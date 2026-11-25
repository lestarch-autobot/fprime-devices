// u3vprobe: USB3 Vision / GenCP pathfinder probe over libusb-1.0.
//
// Not flight code. A sequential "op" interpreter: every command-line word after
// the options is an operation executed in order against one camera, so that an
// experiment is one reproducible command line and one byte-level trace file.
//
//   u3vprobe -s <iSerial> [-t trace.txt] [-m regmap.tsv] [-T ack_timeout_ms] op...
//
// Run "u3vprobe -h" for the list of operations.
#include <errno.h>
#include <inttypes.h>
#include <libusb-1.0/libusb.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "u3v.h"

#define VENDOR_ID 0x1e10
#define MAX_PACKET 65536u
#define MAX_PAYLOAD_TRANSFER (1024u * 1024u)
#define MAX_MAP 4096

typedef struct {
    char name[96];
    uint64_t addr;
    uint32_t len;
    char access[4];
    char type[12]; // u8 u16le u16be u32le u32be u64le u64be i32le i32be f32le f32be f64le f64be str raw
    int lsb;       // -1 when the whole register is the value
    int msb;
} MapEntry;

typedef struct {
    libusb_device_handle* handle;
    uint8_t ep_ctrl_out, ep_ctrl_in, ep_event_in, ep_stream_in;
    int if_ctrl, if_event, if_stream;
    uint16_t request_id;
    uint32_t max_cmd, max_ack;
    unsigned ack_timeout_ms;
    uint64_t sbrm, sirm, eirm, manifest;
    // stream configuration as last written by si-setup
    uint64_t payload_size;
    uint32_t leader_max, trailer_max, transfer_size, transfer_count, final1, final2;
    FILE* trace;
    uint16_t last_status;
    MapEntry* map;
    int map_count;
} Probe;

static uint64_t t0_us;

static uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u - t0_us;
}

static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint64_t rd64(const uint8_t* p) { return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32); }
static void wr16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t* p, uint32_t v) { wr16(p, (uint16_t)v); wr16(p + 2, (uint16_t)(v >> 16)); }
static void wr64(uint8_t* p, uint64_t v) { wr32(p, (uint32_t)v); wr32(p + 4, (uint32_t)(v >> 32)); }

static void say(Probe* pr, const char* fmt, ...) {
    va_list ap;
    char line[1024];
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    printf("%s\n", line);
    fflush(stdout);
    if (pr->trace != NULL) {
        fprintf(pr->trace, "# %s\n", line);
    }
}

// One trace line per USB transfer: time, channel, direction, endpoint, libusb
// result, byte count and (up to hex_limit) bytes.
static void trace_xfer(Probe* pr, const char* chan, const char* dir, uint8_t ep, int rc, const uint8_t* data, int len, int hex_limit) {
    if (pr->trace == NULL) {
        return;
    }
    fprintf(pr->trace, "%10" PRIu64 " %s %s ep=0x%02x rc=%s len=%d :", now_us(), chan, dir, ep, rc == 0 ? "OK" : libusb_error_name(rc), len);
    int n = len < hex_limit ? len : hex_limit;
    for (int i = 0; i < n; i++) {
        fprintf(pr->trace, " %02x", data[i]);
    }
    if (n < len) {
        fprintf(pr->trace, " ...(+%d bytes, see raw file)", len - n);
    }
    fputc('\n', pr->trace);
}

static const char* status_name(uint16_t status) {
    switch (status) {
        case GENCP_SUCCESS: return "SUCCESS";
        case GENCP_NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
        case GENCP_INVALID_PARAMETER: return "INVALID_PARAMETER";
        case GENCP_INVALID_ADDRESS: return "INVALID_ADDRESS";
        case GENCP_WRITE_PROTECT: return "WRITE_PROTECT";
        case GENCP_BAD_ALIGNMENT: return "BAD_ALIGNMENT";
        case GENCP_ACCESS_DENIED: return "ACCESS_DENIED";
        case GENCP_BUSY: return "BUSY";
        case GENCP_MSG_TIMEOUT: return "MSG_TIMEOUT";
        case GENCP_INVALID_HEADER: return "INVALID_HEADER";
        case GENCP_WRONG_CONFIG: return "WRONG_CONFIG";
        case GENCP_GENERIC_ERROR: return "GENERIC_ERROR";
        case U3V_STATUS_RESEND_NOT_SUPPORTED: return "U3V_RESEND_NOT_SUPPORTED";
        case U3V_STATUS_DSI_ENDPOINT_HALTED: return "U3V_DSI_ENDPOINT_HALTED";
        case U3V_STATUS_SI_PAYLOAD_SIZE_NOT_ALIGNED: return "U3V_SI_PAYLOAD_SIZE_NOT_ALIGNED";
        case U3V_STATUS_SI_REGISTERS_INCONSISTENT: return "U3V_SI_REGISTERS_INCONSISTENT";
        case U3V_STATUS_DATA_DISCARDED: return "U3V_DATA_DISCARDED";
        case U3V_STATUS_DATA_OVERRUN: return "U3V_DATA_OVERRUN";
        default: return "?";
    }
}

// ---------------------------------------------------------------------------
// Control channel
// ---------------------------------------------------------------------------

// Send raw bytes on the control OUT endpoint and collect one final ack (pending
// acks are consumed and traced). Returns the ack length, or a negative libusb
// error. The caller interprets the ack.
static int ctrl_exchange(Probe* pr, const uint8_t* cmd, int cmd_len, uint8_t* ack, int ack_cap) {
    int done = 0;
    int rc = libusb_bulk_transfer(pr->handle, pr->ep_ctrl_out, (unsigned char*)cmd, cmd_len, &done, 1000);
    trace_xfer(pr, "CTRL", "OUT", pr->ep_ctrl_out, rc, cmd, done, 1 << 20);
    if (rc != 0) {
        return rc;
    }
    // Ask for exactly the device's maximum ack length: an ack of exactly that
    // size ends on a full USB packet and the camera sends no zero-length packet,
    // so a larger host buffer would wait for a short packet until it times out.
    if (ack_cap > (int)pr->max_ack) {
        ack_cap = (int)pr->max_ack;
    }
    unsigned timeout = pr->ack_timeout_ms;
    for (int pending = 0; pending < 64; pending++) {
        uint64_t t_start = now_us();
        rc = libusb_bulk_transfer(pr->handle, pr->ep_ctrl_in, ack, ack_cap, &done, timeout);
        trace_xfer(pr, "CTRL", "IN ", pr->ep_ctrl_in, rc, ack, done, 1 << 20);
        if (rc != 0) {
            say(pr, "  ack: libusb %s after %" PRIu64 " us", libusb_error_name(rc), now_us() - t_start);
            return rc;
        }
        if (done >= (int)U3V_ACK_HEADER_SIZE && rd16(ack + 6) == GENCP_PENDING_ACK) {
            timeout = rd16(ack + U3V_ACK_HEADER_SIZE + 2);
            say(pr, "  PENDING_ACK: device asks for %u ms more", timeout);
            timeout += pr->ack_timeout_ms;
            continue;
        }
        return done;
    }
    return LIBUSB_ERROR_OTHER;
}

// Check an ack header against the request. Returns 0 when the ack is a
// well-formed SUCCESS for expected_cmd, otherwise -1 (status in last_status).
static int ack_check(Probe* pr, const uint8_t* ack, int ack_len, uint16_t expected_cmd, uint16_t request_id) {
    if (ack_len < (int)U3V_ACK_HEADER_SIZE || rd32(ack) != U3V_MAGIC_CONTROL) {
        say(pr, "  ack: malformed (%d bytes)", ack_len);
        pr->last_status = 0xFFFF;
        return -1;
    }
    pr->last_status = rd16(ack + 4);
    uint16_t cmd = rd16(ack + 6);
    uint16_t len = rd16(ack + 8);
    uint16_t id = rd16(ack + 10);
    if (pr->last_status != GENCP_SUCCESS) {
        say(pr, "  ack: status=0x%04x %s cmd=0x%04x len=%u id=%u", pr->last_status, status_name(pr->last_status), cmd, len, id);
        return -1;
    }
    if (cmd != expected_cmd || id != request_id || (int)len + (int)U3V_ACK_HEADER_SIZE != ack_len) {
        say(pr, "  ack: unexpected cmd=0x%04x id=%u (want 0x%04x id=%u) len=%u total=%d", cmd, id, expected_cmd, request_id, len, ack_len);
        return -1;
    }
    return 0;
}

// One READMEM. count must fit the device's maximum ack transfer length.
static int readmem_once(Probe* pr, uint64_t addr, uint8_t* out, uint16_t count) {
    uint8_t cmd[U3V_CMD_HEADER_SIZE + U3V_READMEM_SCD_SIZE];
    static uint8_t ack[MAX_PACKET];
    uint16_t id = pr->request_id++;
    wr32(cmd, U3V_MAGIC_CONTROL);
    wr16(cmd + 4, GENCP_FLAG_REQUEST_ACK);
    wr16(cmd + 6, GENCP_READMEM_CMD);
    wr16(cmd + 8, U3V_READMEM_SCD_SIZE);
    wr16(cmd + 10, id);
    wr64(cmd + 12, addr);
    wr16(cmd + 20, 0);
    wr16(cmd + 22, count);
    int n = ctrl_exchange(pr, cmd, sizeof cmd, ack, sizeof ack);
    if (n < 0) {
        pr->last_status = 0xFFFF;
        return -1;
    }
    if (ack_check(pr, ack, n, GENCP_READMEM_ACK, id) != 0) {
        return -1;
    }
    if (n != (int)U3V_ACK_HEADER_SIZE + count) {
        say(pr, "  READMEM returned %d data bytes, wanted %u", n - (int)U3V_ACK_HEADER_SIZE, count);
        return -1;
    }
    memcpy(out, ack + U3V_ACK_HEADER_SIZE, count);
    return 0;
}

static int readmem(Probe* pr, uint64_t addr, uint8_t* out, uint32_t count) {
    uint32_t chunk_max = pr->max_ack - U3V_ACK_HEADER_SIZE;
    while (count > 0) {
        uint32_t chunk = count < chunk_max ? count : chunk_max;
        if (readmem_once(pr, addr, out, (uint16_t)chunk) != 0) {
            return -1;
        }
        addr += chunk;
        out += chunk;
        count -= chunk;
    }
    return 0;
}

static int writemem(Probe* pr, uint64_t addr, const uint8_t* data, uint32_t count) {
    static uint8_t cmd[MAX_PACKET];
    static uint8_t ack[MAX_PACKET];
    if (count + U3V_CMD_HEADER_SIZE + 8 > pr->max_cmd) {
        say(pr, "  WRITEMEM of %u bytes exceeds the maximum command length %u", count, pr->max_cmd);
        return -1;
    }
    uint16_t id = pr->request_id++;
    wr32(cmd, U3V_MAGIC_CONTROL);
    wr16(cmd + 4, GENCP_FLAG_REQUEST_ACK);
    wr16(cmd + 6, GENCP_WRITEMEM_CMD);
    wr16(cmd + 8, (uint16_t)(8 + count));
    wr16(cmd + 10, id);
    wr64(cmd + 12, addr);
    memcpy(cmd + 20, data, count);
    int n = ctrl_exchange(pr, cmd, (int)(20 + count), ack, sizeof ack);
    if (n < 0) {
        pr->last_status = 0xFFFF;
        return -1;
    }
    if (ack_check(pr, ack, n, GENCP_WRITEMEM_ACK, id) != 0) {
        return -1;
    }
    if (n >= (int)U3V_ACK_HEADER_SIZE + 4 && rd16(ack + U3V_ACK_HEADER_SIZE + 2) != count) {
        say(pr, "  WRITEMEM ack reports %u bytes written, sent %u", rd16(ack + U3V_ACK_HEADER_SIZE + 2), count);
    }
    return 0;
}

static int read_u32(Probe* pr, uint64_t addr, uint32_t* v) {
    uint8_t b[4];
    if (readmem(pr, addr, b, 4) != 0) {
        return -1;
    }
    *v = rd32(b);
    return 0;
}

static int read_u64(Probe* pr, uint64_t addr, uint64_t* v) {
    uint8_t b[8];
    if (readmem(pr, addr, b, 8) != 0) {
        return -1;
    }
    *v = rd64(b);
    return 0;
}

static int write_u32(Probe* pr, uint64_t addr, uint32_t v) {
    uint8_t b[4];
    wr32(b, v);
    return writemem(pr, addr, b, 4);
}

// ---------------------------------------------------------------------------
// Device open / descriptors
// ---------------------------------------------------------------------------

static int open_camera(Probe* pr, libusb_context* ctx, const char* serial) {
    libusb_device** list = NULL;
    ssize_t n = libusb_get_device_list(ctx, &list);
    int found = -1;
    for (ssize_t i = 0; i < n && found != 0; i++) {
        struct libusb_device_descriptor dd;
        if (libusb_get_device_descriptor(list[i], &dd) != 0 || dd.idVendor != VENDOR_ID) {
            continue;
        }
        libusb_device_handle* h = NULL;
        if (libusb_open(list[i], &h) != 0) {
            continue;
        }
        unsigned char s[64] = {0};
        libusb_get_string_descriptor_ascii(h, dd.iSerialNumber, s, sizeof s);
        if (serial == NULL || strcmp((const char*)s, serial) == 0) {
            pr->handle = h;
            found = 0;
        } else {
            libusb_close(h);
        }
    }
    libusb_free_device_list(list, 1);
    if (found != 0) {
        fprintf(stderr, "camera %s not found\n", serial == NULL ? "(any)" : serial);
        return -1;
    }
    // Locate the USB3 Vision interfaces by class EF / subclass 05 / protocol 0,1,2.
    struct libusb_config_descriptor* cfg = NULL;
    if (libusb_get_active_config_descriptor(libusb_get_device(pr->handle), &cfg) != 0) {
        return -1;
    }
    pr->if_ctrl = pr->if_event = pr->if_stream = -1;
    for (int i = 0; i < cfg->bNumInterfaces; i++) {
        const struct libusb_interface_descriptor* id = &cfg->interface[i].altsetting[0];
        if (id->bInterfaceClass != 0xEF || id->bInterfaceSubClass != 0x05) {
            continue;
        }
        for (int e = 0; e < id->bNumEndpoints; e++) {
            uint8_t a = id->endpoint[e].bEndpointAddress;
            bool in = (a & 0x80) != 0;
            if (id->bInterfaceProtocol == 0) {
                pr->if_ctrl = id->bInterfaceNumber;
                if (in) pr->ep_ctrl_in = a; else pr->ep_ctrl_out = a;
            } else if (id->bInterfaceProtocol == 1 && in) {
                pr->if_event = id->bInterfaceNumber;
                pr->ep_event_in = a;
            } else if (id->bInterfaceProtocol == 2 && in) {
                pr->if_stream = id->bInterfaceNumber;
                pr->ep_stream_in = a;
            }
        }
    }
    libusb_free_config_descriptor(cfg);
    if (pr->if_ctrl < 0 || pr->if_stream < 0) {
        fprintf(stderr, "no USB3 Vision control/stream interface\n");
        return -1;
    }
    int ifs[3] = {pr->if_ctrl, pr->if_event, pr->if_stream};
    for (int i = 0; i < 3; i++) {
        if (ifs[i] >= 0) {
            int rc = libusb_claim_interface(pr->handle, ifs[i]);
            if (rc != 0) {
                fprintf(stderr, "claim interface %d: %s\n", ifs[i], libusb_error_name(rc));
                return -1;
            }
        }
    }
    return 0;
}

// Dump every descriptor as raw bytes (the truth a simulator must reproduce).
static void op_desc(Probe* pr) {
    uint8_t buf[1024];
    static const struct { uint8_t type; const char* name; } kinds[] = {
        {LIBUSB_DT_DEVICE, "DEVICE"}, {LIBUSB_DT_CONFIG, "CONFIG"}, {LIBUSB_DT_BOS, "BOS"}};
    for (unsigned k = 0; k < 3; k++) {
        int n = libusb_get_descriptor(pr->handle, kinds[k].type, 0, buf, sizeof buf);
        trace_xfer(pr, "EP0 ", "IN ", 0x80, n < 0 ? n : 0, buf, n < 0 ? 0 : n, 1 << 20);
        printf("%s descriptor (%d bytes):", kinds[k].name, n);
        for (int i = 0; i < n; i++) printf(" %02x", buf[i]);
        printf("\n");
    }
    for (uint8_t i = 1; i <= 8; i++) {
        unsigned char s[128] = {0};
        int n = libusb_get_string_descriptor_ascii(pr->handle, i, s, sizeof s);
        if (n > 0) {
            say(pr, "STRING %u: \"%s\"", i, s);
        }
    }
    say(pr, "speed=%d (3=high 4=super 5=super+) bus=%u addr=%u", libusb_get_device_speed(libusb_get_device(pr->handle)),
        libusb_get_bus_number(libusb_get_device(pr->handle)), libusb_get_device_address(libusb_get_device(pr->handle)));
    say(pr, "interfaces: control=%d (out 0x%02x in 0x%02x) event=%d (in 0x%02x) stream=%d (in 0x%02x)", pr->if_ctrl, pr->ep_ctrl_out,
        pr->ep_ctrl_in, pr->if_event, pr->ep_event_in, pr->if_stream, pr->ep_stream_in);
}

// ---------------------------------------------------------------------------
// Bootstrap registers
// ---------------------------------------------------------------------------

// Print one bootstrap register, read on its own: the device rejects a READMEM
// that touches an unimplemented register (e.g. ABRM HeartbeatTimeout on USB),
// so the map cannot be fetched as one block.
static void show_reg(Probe* pr, const char* name, uint64_t addr, uint32_t len) {
    uint8_t b[ABRM_STRING_SIZE + 1] = {0};
    if (readmem(pr, addr, b, len) != 0) {
        say(pr, "%-34s @0x%05" PRIx64 " len %2u: status 0x%04x %s", name, addr, len, pr->last_status, status_name(pr->last_status));
    } else if (len == ABRM_STRING_SIZE) {
        say(pr, "%-34s @0x%05" PRIx64 " len %2u: \"%s\"", name, addr, len, (const char*)b);
    } else {
        uint64_t v = len == 8 ? rd64(b) : rd32(b);
        say(pr, "%-34s @0x%05" PRIx64 " len %2u: 0x%0*" PRIx64 " (%" PRIu64 ")", name, addr, len, (int)(2 * len), v, v);
    }
}

// Read the addresses every other operation needs; with verbose, dump every
// bootstrap register.
static int bootstrap(Probe* pr, bool verbose) {
    uint32_t max_cmd = 0, max_ack = 0;
    if (read_u64(pr, ABRM_MANIFEST_TABLE_ADDRESS, &pr->manifest) != 0 || read_u64(pr, ABRM_SBRM_ADDRESS, &pr->sbrm) != 0 ||
        read_u32(pr, pr->sbrm + SBRM_MAX_CMD_TRANSFER_LENGTH, &max_cmd) != 0 || read_u32(pr, pr->sbrm + SBRM_MAX_ACK_TRANSFER_LENGTH, &max_ack) != 0 ||
        read_u64(pr, pr->sbrm + SBRM_SIRM_ADDRESS, &pr->sirm) != 0 || read_u64(pr, pr->sbrm + SBRM_EIRM_ADDRESS, &pr->eirm) != 0) {
        return -1;
    }
    pr->max_cmd = max_cmd < MAX_PACKET ? max_cmd : MAX_PACKET;
    pr->max_ack = max_ack < MAX_PACKET ? max_ack : MAX_PACKET;
    if (!verbose) {
        return 0;
    }
    static const struct { const char* name; uint32_t off; uint32_t len; } abrm[] = {
        {"ABRM GenCPVersion", ABRM_GENCP_VERSION, 4}, {"ABRM ManufacturerName", ABRM_MANUFACTURER_NAME, 64}, {"ABRM ModelName", ABRM_MODEL_NAME, 64},
        {"ABRM FamilyName", ABRM_FAMILY_NAME, 64}, {"ABRM DeviceVersion", ABRM_DEVICE_VERSION, 64}, {"ABRM ManufacturerInfo", ABRM_MANUFACTURER_INFO, 64},
        {"ABRM SerialNumber", ABRM_SERIAL_NUMBER, 64}, {"ABRM UserDefinedName", ABRM_USER_DEFINED_NAME, 64},
        {"ABRM DeviceCapability", ABRM_DEVICE_CAPABILITY, 8}, {"ABRM MaxDeviceResponseTime(ms)", ABRM_MAX_DEVICE_RESPONSE_TIME, 4},
        {"ABRM ManifestTableAddress", ABRM_MANIFEST_TABLE_ADDRESS, 8}, {"ABRM SBRMAddress", ABRM_SBRM_ADDRESS, 8},
        {"ABRM DeviceConfiguration", ABRM_DEVICE_CONFIGURATION, 8}, {"ABRM HeartbeatTimeout", ABRM_HEARTBEAT_TIMEOUT, 4},
        {"ABRM MessageChannelID", ABRM_MESSAGE_CHANNEL_ID, 4}, {"ABRM Timestamp", ABRM_TIMESTAMP, 8}, {"ABRM TimestampLatch", ABRM_TIMESTAMP_LATCH, 4},
        {"ABRM TimestampIncrement", ABRM_TIMESTAMP_INCREMENT, 8}, {"ABRM AccessPrivilege", ABRM_ACCESS_PRIVILEGE, 4},
        {"ABRM ProtocolEndianness", ABRM_PROTOCOL_ENDIANNESS, 4}, {"ABRM ImplementationEndianness", ABRM_IMPLEMENTATION_ENDIANNESS, 4},
        {"ABRM DeviceSoftwareInterfaceVersion", ABRM_DEVICE_SOFTWARE_INTERFACE_VERSION, 64}};
    for (unsigned i = 0; i < sizeof abrm / sizeof abrm[0]; i++) {
        show_reg(pr, abrm[i].name, abrm[i].off, abrm[i].len);
    }
    static const struct { const char* name; uint32_t off; uint32_t len; } sb[] = {
        {"SBRM U3VVersion", SBRM_U3V_VERSION, 4}, {"SBRM U3VCPCapability", SBRM_U3VCP_CAPABILITY, 8}, {"SBRM U3VCPConfiguration", SBRM_U3VCP_CONFIGURATION, 8},
        {"SBRM MaxCmdTransferLength", SBRM_MAX_CMD_TRANSFER_LENGTH, 4}, {"SBRM MaxAckTransferLength", SBRM_MAX_ACK_TRANSFER_LENGTH, 4},
        {"SBRM NumStreamChannels", SBRM_NUM_STREAM_CHANNELS, 4}, {"SBRM SIRMAddress", SBRM_SIRM_ADDRESS, 8}, {"SBRM SIRMLength", SBRM_SIRM_LENGTH, 4},
        {"SBRM EIRMAddress", SBRM_EIRM_ADDRESS, 8}, {"SBRM EIRMLength", SBRM_EIRM_LENGTH, 4}, {"SBRM IIDC2Address", SBRM_IIDC2_ADDRESS, 8},
        {"SBRM CurrentSpeed", SBRM_CURRENT_SPEED, 4}};
    for (unsigned i = 0; i < sizeof sb / sizeof sb[0]; i++) {
        show_reg(pr, sb[i].name, pr->sbrm + sb[i].off, sb[i].len);
    }
    uint8_t sirm[SIRM_SIZE];
    if (readmem(pr, pr->sirm, sirm, sizeof sirm) != 0) {
        return -1;
    }
    say(pr, "SIRM Info                       0x%08x (payload alignment %u bytes)", rd32(sirm + SIRM_INFO), 1u << (rd32(sirm + SIRM_INFO) >> 24));
    say(pr, "SIRM Control                    0x%08x", rd32(sirm + SIRM_CONTROL));
    say(pr, "SIRM RequiredPayloadSize        %" PRIu64, rd64(sirm + SIRM_REQ_PAYLOAD_SIZE));
    say(pr, "SIRM RequiredLeaderSize         %u", rd32(sirm + SIRM_REQ_LEADER_SIZE));
    say(pr, "SIRM RequiredTrailerSize        %u", rd32(sirm + SIRM_REQ_TRAILER_SIZE));
    say(pr, "SIRM MaximumLeaderSize          %u", rd32(sirm + SIRM_MAX_LEADER_SIZE));
    say(pr, "SIRM PayloadTransferSize        %u", rd32(sirm + SIRM_PAYLOAD_TRANSFER_SIZE));
    say(pr, "SIRM PayloadTransferCount       %u", rd32(sirm + SIRM_PAYLOAD_TRANSFER_COUNT));
    say(pr, "SIRM PayloadFinalTransfer1Size  %u", rd32(sirm + SIRM_PAYLOAD_FINAL_TRANSFER1_SIZE));
    say(pr, "SIRM PayloadFinalTransfer2Size  %u", rd32(sirm + SIRM_PAYLOAD_FINAL_TRANSFER2_SIZE));
    say(pr, "SIRM MaximumTrailerSize         %u", rd32(sirm + SIRM_MAX_TRAILER_SIZE));
    if (pr->eirm != 0) {
        uint8_t eirm[EIRM_SIZE];
        if (readmem(pr, pr->eirm, eirm, sizeof eirm) == 0) {
            say(pr, "EIRM Control                    0x%08x", rd32(eirm + EIRM_CONTROL));
            say(pr, "EIRM MaxEventTransferLength     %u", rd32(eirm + EIRM_MAX_EVENT_TRANSFER_LENGTH));
            say(pr, "EIRM EventTestControl           0x%08x", rd32(eirm + EIRM_EVENT_TEST_CONTROL));
        }
    }
    uint64_t entries = 0;
    if (read_u64(pr, pr->manifest, &entries) != 0) {
        return -1;
    }
    say(pr, "MANIFEST entries                %" PRIu64, entries);
    for (uint64_t i = 0; i < entries && i < 8; i++) {
        uint8_t e[MANIFEST_ENTRY_SIZE];
        if (readmem(pr, pr->manifest + 8 + i * MANIFEST_ENTRY_SIZE, e, sizeof e) != 0) {
            return -1;
        }
        uint32_t ver = rd32(e + MANIFEST_ENTRY_FILE_VERSION);
        uint32_t fmt = rd32(e + MANIFEST_ENTRY_FILE_FORMAT);
        char sha[41];
        for (int k = 0; k < 20; k++) snprintf(sha + 2 * k, 3, "%02x", e[MANIFEST_ENTRY_SHA1 + k]);
        say(pr, "MANIFEST[%" PRIu64 "] file version %u.%u.%u format 0x%08x (file type %u: %s, schema %u.%u) address 0x%" PRIx64 " size %" PRIu64 " sha1 %s", i,
            ver >> 24, (ver >> 16) & 0xff, ver & 0xffff, fmt, (fmt >> 10) & 0x3f, ((fmt >> 10) & 0x3f) == 1 ? "zip" : "uncompressed xml", fmt >> 24, (fmt >> 16) & 0xff,
            rd64(e + MANIFEST_ENTRY_ADDRESS), rd64(e + MANIFEST_ENTRY_SIZE_FIELD), sha);
    }
    return 0;
}

// Download manifest entry 0 (the GenICam file, usually a zip) to a file.
static int op_xml(Probe* pr, const char* path) {
    uint8_t e[MANIFEST_ENTRY_SIZE];
    if (readmem(pr, pr->manifest + 8, e, sizeof e) != 0) {
        return -1;
    }
    uint64_t addr = rd64(e + MANIFEST_ENTRY_ADDRESS);
    uint64_t size = rd64(e + MANIFEST_ENTRY_SIZE_FIELD);
    if (size == 0 || size > (64u << 20)) {
        say(pr, "implausible manifest file size %" PRIu64, size);
        return -1;
    }
    uint8_t* data = malloc(size);
    if (data == NULL || readmem(pr, addr, data, (uint32_t)size) != 0) {
        free(data);
        return -1;
    }
    FILE* f = fopen(path, "wb");
    if (f == NULL || fwrite(data, 1, size, f) != size) {
        perror(path);
        free(data);
        return -1;
    }
    fclose(f);
    free(data);
    say(pr, "wrote %" PRIu64 " bytes from 0x%" PRIx64 " to %s", size, addr, path);
    return 0;
}

// ---------------------------------------------------------------------------
// Stream channel
// ---------------------------------------------------------------------------

static uint32_t align_up(uint32_t v, uint32_t a) { return (v + a - 1) / a * a; }

// Program the SIRM transfer geometry for the camera's current payload size.
// transfer_size 0 selects the default (1 MiB, or the whole payload if smaller).
static int op_si_setup(Probe* pr, uint32_t transfer_size) {
    uint8_t sirm[SIRM_SIZE];
    if (readmem(pr, pr->sirm, sirm, sizeof sirm) != 0) {
        return -1;
    }
    uint32_t align = 1u << (rd32(sirm + SIRM_INFO) >> 24);
    pr->payload_size = rd64(sirm + SIRM_REQ_PAYLOAD_SIZE);
    pr->leader_max = align_up(rd32(sirm + SIRM_REQ_LEADER_SIZE), 1024);
    pr->trailer_max = align_up(rd32(sirm + SIRM_REQ_TRAILER_SIZE), 1024);
    if (transfer_size == 0) {
        transfer_size = MAX_PAYLOAD_TRANSFER;
    }
    transfer_size -= transfer_size % align;
    pr->transfer_size = transfer_size;
    pr->transfer_count = (uint32_t)(pr->payload_size / transfer_size);
    uint32_t rest = (uint32_t)(pr->payload_size % transfer_size);
    pr->final1 = rest - rest % align;
    pr->final2 = rest % align != 0 ? align : 0;
    say(pr, "si-setup: payload %" PRIu64 " align %u -> leader_max %u trailer_max %u transfer %u x %u final1 %u final2 %u", pr->payload_size, align,
        pr->leader_max, pr->trailer_max, pr->transfer_size, pr->transfer_count, pr->final1, pr->final2);
    if (write_u32(pr, pr->sirm + SIRM_MAX_LEADER_SIZE, pr->leader_max) != 0 || write_u32(pr, pr->sirm + SIRM_PAYLOAD_TRANSFER_SIZE, pr->transfer_size) != 0 ||
        write_u32(pr, pr->sirm + SIRM_PAYLOAD_TRANSFER_COUNT, pr->transfer_count) != 0 ||
        write_u32(pr, pr->sirm + SIRM_PAYLOAD_FINAL_TRANSFER1_SIZE, pr->final1) != 0 ||
        write_u32(pr, pr->sirm + SIRM_PAYLOAD_FINAL_TRANSFER2_SIZE, pr->final2) != 0 || write_u32(pr, pr->sirm + SIRM_MAX_TRAILER_SIZE, pr->trailer_max) != 0) {
        return -1;
    }
    return 0;
}

typedef struct {
    uint64_t sum;
    uint64_t sumsq;
    uint64_t count;
    uint8_t min;
    uint8_t max;
} ByteStats;

static void stats_add(ByteStats* st, const uint8_t* p, int n) {
    for (int i = 0; i < n; i++) {
        uint8_t v = p[i];
        st->sum += v;
        st->sumsq += (uint64_t)v * v;
        if (v < st->min) st->min = v;
        if (v > st->max) st->max = v;
    }
    st->count += (uint64_t)n;
}

// Acquire frames from the stream endpoint. The stream must already be enabled
// and the acquisition started (by earlier ops). prefix "-" discards payloads;
// otherwise each payload is written to <prefix>_<n>.raw.
// Returns the number of complete frames (leader + trailer) received.
static int op_grab(Probe* pr, int frames, const char* prefix, unsigned timeout_ms) {
    if (pr->transfer_size == 0) {
        say(pr, "grab: run si-setup first");
        return -1;
    }
    uint32_t cap = pr->transfer_size > pr->leader_max ? pr->transfer_size : pr->leader_max;
    if (cap < pr->trailer_max) cap = pr->trailer_max;
    if (cap < pr->final1) cap = pr->final1;
    uint8_t* buf = malloc(cap);
    if (buf == NULL) {
        return -1;
    }
    bool with_stats = strcmp(prefix, "--") != 0; // "--": discard and skip the byte statistics (fastest reader)
    bool save = with_stats && strcmp(prefix, "-") != 0;
    int complete = 0;
    uint64_t t_begin = now_us(), t_first = 0, t_last = 0, bytes_total = 0;
    say(pr, "grab: %d frames, timeout %u ms, start t=%" PRIu64 " us", frames, timeout_ms, t_begin);
    for (int f = 0; f < frames; f++) {
        // --- leader ---
        int n = 0;
        int rc = libusb_bulk_transfer(pr->handle, pr->ep_stream_in, buf, (int)pr->leader_max, &n, timeout_ms);
        uint64_t t_leader = now_us();
        trace_xfer(pr, "STRM", "IN ", pr->ep_stream_in, rc, buf, n, 256);
        if (rc != 0) {
            say(pr, "frame %d: leader read failed: %s after %" PRIu64 " us (%d bytes)", f, libusb_error_name(rc), t_leader - t_begin, n);
            break;
        }
        if (n < (int)LEADER_BLOCK_ID + 8 || rd32(buf) != U3V_MAGIC_LEADER) {
            say(pr, "frame %d: expected leader, got %d bytes starting 0x%08x (resynchronising)", f, n, n >= 4 ? rd32(buf) : 0);
            f--;
            if (now_us() - t_begin > (uint64_t)timeout_ms * 1000u * 4u) break;
            continue;
        }
        uint64_t block_id = rd64(buf + LEADER_BLOCK_ID);
        uint16_t ptype = rd16(buf + LEADER_PAYLOAD_TYPE);
        uint64_t ts = n >= (int)LEADER_TIMESTAMP + 8 ? rd64(buf + LEADER_TIMESTAMP) : 0;
        uint32_t pixfmt = 0, sx = 0, sy = 0, ox = 0, oy = 0;
        if (n >= (int)LEADER_IMAGE_SIZE) {
            pixfmt = rd32(buf + LEADER_PIXEL_FORMAT);
            sx = rd32(buf + LEADER_SIZE_X);
            sy = rd32(buf + LEADER_SIZE_Y);
            ox = rd32(buf + LEADER_OFFSET_X);
            oy = rd32(buf + LEADER_OFFSET_Y);
        }
        if (t_first == 0) t_first = t_leader;
        // --- payload ---
        FILE* out = NULL;
        if (save) {
            char path[512];
            snprintf(path, sizeof path, "%s_%04d.raw", prefix, f);
            out = fopen(path, "wb");
        }
        ByteStats st = {0, 0, 0, 255, 0};
        uint64_t got = 0;
        bool have_trailer = false;
        uint32_t steps = pr->transfer_count + (pr->final1 != 0 ? 1u : 0u) + (pr->final2 != 0 ? 1u : 0u);
        for (uint32_t s = 0; s <= steps && !have_trailer; s++) {
            uint32_t want = s < pr->transfer_count ? pr->transfer_size
                            : (s == pr->transfer_count && pr->final1 != 0) ? pr->final1
                            : (s < steps) ? pr->final2 : pr->trailer_max;
            rc = libusb_bulk_transfer(pr->handle, pr->ep_stream_in, buf, (int)want, &n, timeout_ms);
            bool is_trailer = rc == 0 && n >= 4 && n <= (int)pr->trailer_max && rd32(buf) == U3V_MAGIC_TRAILER && (s == steps || n < (int)want);
            trace_xfer(pr, "STRM", "IN ", pr->ep_stream_in, rc, buf, n, is_trailer ? 256 : 32);
            if (rc != 0) {
                say(pr, "frame %d: transfer %u/%u failed: %s (%d bytes)", f, s, steps, libusb_error_name(rc), n);
                break;
            }
            if (is_trailer) {
                have_trailer = true;
                if (s != steps) {
                    say(pr, "frame %d: trailer arrived early at transfer %u/%u", f, s, steps);
                }
                break;
            }
            if (s == steps) {
                say(pr, "frame %d: expected trailer, got %d bytes starting 0x%08x", f, n, n >= 4 ? rd32(buf) : 0);
                break;
            }
            if (with_stats) stats_add(&st, buf, n);
            if (out != NULL) fwrite(buf, 1, (size_t)n, out);
            got += (uint64_t)n;
        }
        if (out != NULL) fclose(out);
        uint64_t t_trailer = now_us();
        bytes_total += got;
        if (!have_trailer) {
            say(pr, "frame %d: block %" PRIu64 " INCOMPLETE payload %" PRIu64 " bytes", f, block_id, got);
            if (rc != 0) break;
            continue;
        }
        uint16_t tstatus = n >= (int)TRAILER_STATUS + 2 ? rd16(buf + TRAILER_STATUS) : 0xFFFF;
        uint64_t valid = n >= (int)TRAILER_VALID_PAYLOAD_SIZE + 8 ? rd64(buf + TRAILER_VALID_PAYLOAD_SIZE) : 0;
        uint32_t tsy = n >= (int)TRAILER_IMAGE_SIZE ? rd32(buf + TRAILER_SIZE_Y) : 0;
        double mean = st.count != 0 ? (double)st.sum / (double)st.count : 0.0;
        double var = st.count != 0 ? (double)st.sumsq / (double)st.count - mean * mean : 0.0;
        say(pr,
            "frame %d: block %" PRIu64 " type 0x%04x ts %" PRIu64 " fmt 0x%08x %ux%u+%u+%u payload %" PRIu64 " | trailer block %" PRIu64
            " status 0x%04x valid %" PRIu64 " size_y %u | host leader %" PRIu64 " us trailer %" PRIu64 " us | bytes min %u max %u mean %.3f var %.3f",
            f, block_id, ptype, ts, pixfmt, sx, sy, ox, oy, got, rd64(buf + TRAILER_BLOCK_ID), tstatus, valid, tsy, t_leader, t_trailer, st.min, st.max, mean,
            var);
        complete++;
        t_last = t_leader; // rate is measured leader to leader
    }
    free(buf);
    if (complete > 1) {
        double span = (double)(t_last - t_first) / 1e6;
        say(pr, "grab: %d complete frames, first leader %" PRIu64 " us after grab start, %.3f fps, %.1f MB/s over %.3f s", complete, t_first - t_begin,
            (double)(complete - 1) / span, (double)bytes_total / (double)complete * (double)(complete - 1) / 1e6 / span, span);
    } else {
        say(pr, "grab: %d complete frames%s", complete, complete == 1 ? "" : " (none)");
        if (complete == 1) say(pr, "grab: first leader %" PRIu64 " us after grab start", t_first - t_begin);
    }
    return complete;
}

// ---- queued (asynchronous) stream reader ----
// Keeps DEPTH bulk transfers pending on the stream endpoint so the host does not throttle the camera.
// The transfers follow the fixed per-frame sequence programmed into the SIRM: leader (leader_max), transfer_count
// transfers of transfer_size, final1, final2 (when non-zero), trailer (trailer_max). The sequence matters: the
// camera sends no zero-length packet, so a payload tail that is a multiple of the packet size only ends a
// transfer of exactly that size. Payload bytes are discarded; only leaders and trailers are reported.
typedef struct {
    Probe* pr;
    int frames_wanted, complete, leaders, in_flight;
    bool stop, in_frame;
    uint64_t t_begin, t_first, t_last, bytes_frame, bytes_total, block_id, ts, t_leader;
    uint32_t steps;       // transfers per frame, leader and trailer included
    uint32_t submit_step; // position in the frame of the next transfer to submit
    uint32_t done_step;   // position in the frame of the next transfer to complete
    int errors;
} GrabQ;

static uint32_t grabq_step_size(const GrabQ* q, uint32_t step) {
    const Probe* pr = q->pr;
    if (step == 0) return pr->leader_max;
    if (step == q->steps - 1) return pr->trailer_max;
    if (step <= pr->transfer_count) return pr->transfer_size;
    if (step == pr->transfer_count + 1 && pr->final1 != 0) return pr->final1;
    return pr->final2;
}

static int grabq_submit(GrabQ* q, struct libusb_transfer* xfer) {
    xfer->length = (int)grabq_step_size(q, q->submit_step);
    int rc = libusb_submit_transfer(xfer);
    if (rc == 0) {
        q->submit_step = (q->submit_step + 1) % q->steps;
        q->in_flight++;
    }
    return rc;
}

static void LIBUSB_CALL grabq_cb(struct libusb_transfer* xfer) {
    GrabQ* q = xfer->user_data;
    Probe* pr = q->pr;
    const uint8_t* b = xfer->buffer;
    int n = xfer->actual_length;
    uint32_t step = q->done_step;
    q->in_flight--;
    if (xfer->status != LIBUSB_TRANSFER_COMPLETED) {
        if (xfer->status != LIBUSB_TRANSFER_CANCELLED) {
            say(pr, "grabq: transfer status %d (%s) after %d bytes at step %u/%u", (int)xfer->status,
                xfer->status == LIBUSB_TRANSFER_TIMED_OUT ? "timeout" : xfer->status == LIBUSB_TRANSFER_STALL ? "stall" : "error", n, step, q->steps);
            q->errors++;
        }
        q->stop = true;
        return;
    }
    q->done_step = (step + 1) % q->steps;
    bool is_leader = n >= (int)LEADER_TIMESTAMP + 8 && n <= (int)pr->leader_max && rd32(b) == U3V_MAGIC_LEADER;
    bool is_trailer = n >= (int)TRAILER_VALID_PAYLOAD_SIZE + 8 && n <= (int)pr->trailer_max && rd32(b) == U3V_MAGIC_TRAILER;
    if (step == 0 && is_leader) {
        trace_xfer(pr, "STRM", "IN ", pr->ep_stream_in, 0, b, n, 256);
        q->in_frame = true;
        q->bytes_frame = 0;
        q->block_id = rd64(b + LEADER_BLOCK_ID);
        q->ts = rd64(b + LEADER_TIMESTAMP);
        q->t_leader = now_us();
        if (q->t_first == 0) q->t_first = q->t_leader;
        q->leaders++;
    } else if (step == q->steps - 1 && is_trailer) {
        trace_xfer(pr, "STRM", "IN ", pr->ep_stream_in, 0, b, n, 256);
        uint64_t t = now_us();
        say(pr, "frame %d: block %" PRIu64 " ts %" PRIu64 " payload %" PRIu64 " | trailer block %" PRIu64 " status 0x%04x valid %" PRIu64
                " | host leader %" PRIu64 " us trailer %" PRIu64 " us",
            q->complete, q->block_id, q->ts, q->bytes_frame, rd64(b + TRAILER_BLOCK_ID), rd16(b + TRAILER_STATUS), rd64(b + TRAILER_VALID_PAYLOAD_SIZE),
            q->t_leader, t);
        q->in_frame = false;
        q->bytes_total += q->bytes_frame;
        q->complete++;
        q->t_last = q->t_leader; // rate is measured leader to leader
        if (q->complete >= q->frames_wanted) q->stop = true;
    } else if (step == 0 || step == q->steps - 1 || is_leader) {
        trace_xfer(pr, "STRM", "IN ", pr->ep_stream_in, 0, b, n, 64);
        say(pr, "grabq: out of sequence at step %u/%u: %d bytes starting 0x%08x (block %" PRIu64 ", %" PRIu64 " payload bytes so far)", step, q->steps, n,
            n >= 4 ? rd32(b) : 0, q->block_id, q->bytes_frame);
        q->errors++;
        q->stop = true;
    } else {
        trace_xfer(pr, "STRM", "IN ", pr->ep_stream_in, 0, b, n, 0);
        q->bytes_frame += (uint64_t)n;
    }
    if (!q->stop) (void)grabq_submit(q, xfer);
}

static int op_grabq(Probe* pr, libusb_context* ctx, int frames, int depth, unsigned timeout_ms) {
    if (pr->transfer_size == 0) {
        say(pr, "grabq: run si-setup first");
        return -1;
    }
    if (depth < 1) depth = 1;
    if (depth > 64) depth = 64;
    struct libusb_transfer* xf[64] = {0};
    GrabQ q = {0};
    q.pr = pr;
    q.frames_wanted = frames;
    q.steps = 2 + pr->transfer_count + (pr->final1 != 0 ? 1u : 0u) + (pr->final2 != 0 ? 1u : 0u);
    uint32_t cap = pr->transfer_size;
    if (cap < pr->leader_max) cap = pr->leader_max;
    if (cap < pr->trailer_max) cap = pr->trailer_max;
    if (cap < pr->final1) cap = pr->final1;
    if (cap < pr->final2) cap = pr->final2;
    q.t_begin = now_us();
    say(pr, "grabq: %d frames, %d transfers queued (payload transfers %u bytes), timeout %u ms, start t=%" PRIu64 " us", frames, depth, pr->transfer_size, timeout_ms, q.t_begin);
    for (int i = 0; i < depth; i++) {
        xf[i] = libusb_alloc_transfer(0);
        uint8_t* b = malloc(cap);
        if (xf[i] == NULL || b == NULL) return -1;
        libusb_fill_bulk_transfer(xf[i], pr->handle, pr->ep_stream_in, b, (int)cap, grabq_cb, &q, timeout_ms);
        int rc = grabq_submit(&q, xf[i]);
        if (rc != 0) {
            say(pr, "grabq: submit %d failed: %s (raise /sys/module/usbcore/parameters/usbfs_memory_mb?)", i, libusb_error_name(rc));
            q.stop = true;
            break;
        }
    }
    bool cancelled = false;
    while (q.in_flight > 0) {
        struct timeval tv = {0, 100000};
        libusb_handle_events_timeout(ctx, &tv);
        if (q.stop && !cancelled) {
            for (int i = 0; i < depth; i++)
                if (xf[i] != NULL) libusb_cancel_transfer(xf[i]);
            cancelled = true;
        }
    }
    for (int i = 0; i < depth; i++) {
        if (xf[i] != NULL) {
            free(xf[i]->buffer);
            libusb_free_transfer(xf[i]);
        }
    }
    if (q.complete > 1) {
        double span = (double)(q.t_last - q.t_first) / 1e6;
        say(pr, "grabq: %d complete frames (%d leaders), first leader %" PRIu64 " us after start, %.3f fps, %.1f MB/s over %.3f s", q.complete, q.leaders,
            q.t_first - q.t_begin, (double)(q.complete - 1) / span, (double)q.bytes_total / (double)q.complete * (double)(q.complete - 1) / 1e6 / span, span);
    } else {
        say(pr, "grabq: %d complete frames (%d leaders)", q.complete, q.leaders);
    }
    return q.complete;
}

// Read whatever arrives on an IN endpoint for a while (stale stream data, events).
static void op_drain(Probe* pr, uint8_t ep, unsigned timeout_ms) {
    static uint8_t buf[MAX_PAYLOAD_TRANSFER];
    const char* chan = ep == pr->ep_event_in ? "EVT " : ep == pr->ep_stream_in ? "STRM" : "CTRL";
    for (int i = 0; i < 4096; i++) {
        int n = 0;
        int rc = libusb_bulk_transfer(pr->handle, ep, buf, sizeof buf, &n, timeout_ms);
        trace_xfer(pr, chan, "IN ", ep, rc, buf, n, 256);
        say(pr, "drain ep 0x%02x: %s %d bytes%s", ep, rc == 0 ? "OK" : libusb_error_name(rc), n,
            n >= 4 && rd32(buf) == U3V_MAGIC_LEADER ? " (leader)" : n >= 4 && rd32(buf) == U3V_MAGIC_TRAILER ? " (trailer)" : "");
        if (rc != 0) {
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Register map (generated by genicam/gen_regtable.py) for name based access
// ---------------------------------------------------------------------------

static int load_map(Probe* pr, const char* path) {
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        perror(path);
        return -1;
    }
    pr->map = calloc(MAX_MAP, sizeof(MapEntry));
    char line[512];
    while (pr->map != NULL && fgets(line, sizeof line, f) != NULL && pr->map_count < MAX_MAP) {
        MapEntry* e = &pr->map[pr->map_count];
        char lsb[16], msb[16];
        if (line[0] == '#' || sscanf(line, "%95s %" SCNx64 " %u %3s %11s %15s %15s", e->name, &e->addr, &e->len, e->access, e->type, lsb, msb) != 7) {
            continue;
        }
        e->lsb = strcmp(lsb, "-") == 0 ? -1 : atoi(lsb);
        e->msb = strcmp(msb, "-") == 0 ? -1 : atoi(msb);
        pr->map_count++;
    }
    fclose(f);
    return 0;
}

static const MapEntry* find_entry(Probe* pr, const char* name) {
    for (int i = 0; i < pr->map_count; i++) {
        if (strcmp(pr->map[i].name, name) == 0) {
            return &pr->map[i];
        }
    }
    say(pr, "no register named %s in the map", name);
    return NULL;
}

static uint64_t bytes_to_uint(const uint8_t* b, uint32_t len, bool big) {
    uint64_t v = 0;
    for (uint32_t i = 0; i < len && i < 8; i++) {
        v |= (uint64_t)b[big ? len - 1 - i : i] << (8 * i);
    }
    return v;
}

static void uint_to_bytes(uint8_t* b, uint32_t len, bool big, uint64_t v) {
    for (uint32_t i = 0; i < len && i < 8; i++) {
        b[big ? len - 1 - i : i] = (uint8_t)(v >> (8 * i));
    }
}

static int op_get(Probe* pr, const char* name) {
    const MapEntry* e = find_entry(pr, name);
    uint8_t b[256] = {0};
    if (e == NULL || e->len > sizeof b - 1) {
        return -1;
    }
    if (readmem(pr, e->addr, b, e->len) != 0) {
        say(pr, "get %s @0x%" PRIx64 ": FAILED status 0x%04x %s", name, e->addr, pr->last_status, status_name(pr->last_status));
        return -1;
    }
    bool big = strstr(e->type, "be") != NULL;
    if (strcmp(e->type, "str") == 0) {
        say(pr, "get %s @0x%" PRIx64 " = \"%s\"", name, e->addr, (const char*)b);
    } else if (e->type[0] == 'f') {
        double d;
        if (e->len == 4) {
            uint32_t u = (uint32_t)bytes_to_uint(b, 4, big);
            float fl;
            memcpy(&fl, &u, 4);
            d = fl;
        } else {
            uint64_t u = bytes_to_uint(b, 8, big);
            memcpy(&d, &u, 8);
        }
        say(pr, "get %s @0x%" PRIx64 " = %.9g", name, e->addr, d);
    } else if (strcmp(e->type, "raw") == 0) {
        char hex[3 * 64 + 1] = "";
        for (uint32_t i = 0; i < e->len && i < 64; i++) snprintf(hex + 3 * i, 4, "%02x ", b[i]);
        say(pr, "get %s @0x%" PRIx64 " = %s", name, e->addr, hex);
    } else {
        uint64_t v = bytes_to_uint(b, e->len, big);
        uint64_t raw = v;
        if (e->lsb >= 0) {
            int width = e->msb - e->lsb + 1;
            v = (v >> e->lsb) & (width >= 64 ? ~0ull : ((1ull << width) - 1));
        }
        if (e->type[0] == 'i' && e->lsb < 0 && e->len < 8 && (v >> (8 * e->len - 1)) != 0) {
            v |= ~0ull << (8 * e->len);
        }
        say(pr, "get %s @0x%" PRIx64 " = %" PRId64 " (0x%" PRIx64 ", register 0x%" PRIx64 ")", name, e->addr, (int64_t)v, v, raw);
    }
    return 0;
}

static int op_set(Probe* pr, const char* name, const char* value) {
    const MapEntry* e = find_entry(pr, name);
    uint8_t b[256] = {0};
    if (e == NULL || e->len > sizeof b) {
        return -1;
    }
    bool big = strstr(e->type, "be") != NULL;
    if (strcmp(e->type, "str") == 0) {
        strncpy((char*)b, value, e->len);
    } else if (e->type[0] == 'f') {
        double d = strtod(value, NULL);
        if (e->len == 4) {
            float fl = (float)d;
            uint32_t u;
            memcpy(&u, &fl, 4);
            uint_to_bytes(b, 4, big, u);
        } else {
            uint64_t u;
            memcpy(&u, &d, 8);
            uint_to_bytes(b, 8, big, u);
        }
    } else {
        uint64_t v = (uint64_t)strtoll(value, NULL, 0);
        if (e->lsb >= 0) {
            if (readmem(pr, e->addr, b, e->len) != 0) {
                return -1;
            }
            int width = e->msb - e->lsb + 1;
            uint64_t mask = (width >= 64 ? ~0ull : ((1ull << width) - 1)) << e->lsb;
            v = (bytes_to_uint(b, e->len, big) & ~mask) | ((v << e->lsb) & mask);
        }
        uint_to_bytes(b, e->len, big, v);
    }
    uint64_t t = now_us();
    int rc = writemem(pr, e->addr, b, e->len);
    say(pr, "set %s @0x%" PRIx64 " <- %s : %s status 0x%04x %s (%" PRIu64 " us)", name, e->addr, value, rc == 0 ? "OK" : "FAILED", pr->last_status,
        status_name(pr->last_status), now_us() - t);
    return rc;
}

// ---------------------------------------------------------------------------
// Op interpreter
// ---------------------------------------------------------------------------

static int parse_hex(const char* s, uint8_t* out, int cap) {
    int n = 0;
    while (s[0] != '\0' && s[1] != '\0' && n < cap) {
        unsigned v;
        if (sscanf(s, "%2x", &v) != 1) {
            return -1;
        }
        out[n++] = (uint8_t)v;
        s += 2;
    }
    return n;
}

static void hexdump(Probe* pr, uint64_t addr, const uint8_t* b, uint32_t len) {
    for (uint32_t off = 0; off < len; off += 16) {
        char line[128];
        int p = snprintf(line, sizeof line, "%08" PRIx64 ":", addr + off);
        for (uint32_t i = off; i < off + 16 && i < len; i++) p += snprintf(line + p, sizeof line - (size_t)p, " %02x", b[i]);
        p += snprintf(line + p, sizeof line - (size_t)p, "  ");
        for (uint32_t i = off; i < off + 16 && i < len; i++) p += snprintf(line + p, sizeof line - (size_t)p, "%c", b[i] >= 32 && b[i] < 127 ? b[i] : '.');
        say(pr, "%s", line);
    }
}

static void usage(void) {
    puts("usage: u3vprobe -s <iSerial> [-t trace.txt] [-m regmap.tsv] [-T ack_timeout_ms] op...\n"
         "ops (executed in order; numbers accept 0x.., a failed op does not stop the run):\n"
         "  desc                    dump USB descriptors\n"
         "  bootstrap               dump ABRM, SBRM, SIRM, EIRM and the manifest table\n"
         "  xml FILE                download manifest entry 0 (GenICam file) to FILE\n"
         "  r ADDR LEN              READMEM, hex dump\n"
         "  r32 ADDR | r64 ADDR     READMEM, little-endian integer\n"
         "  w32 ADDR VALUE          WRITEMEM, little-endian u32\n"
         "  w ADDR HEXBYTES         WRITEMEM of arbitrary bytes\n"
         "  rlen ADDR LEN           one READMEM with an explicit count field (no chunking)\n"
         "  raw HEXBYTES            send raw bytes on the control channel, print the ack\n"
         "  get NAME | set NAME V   name based access through the -m register map\n"
         "  si-setup [XFER_SIZE]    program SIRM transfer sizes for the current payload size\n"
         "  si-enable | si-disable  write SIRM control stream-enable bit\n"
         "  grab N PREFIX [TMO_MS]  read N frames from the stream endpoint (PREFIX - = discard, -- = discard, no statistics)\n"
         "  grabq N [DEPTH] [TMO_MS] read N frames with DEPTH queued asynchronous transfers, payload discarded\n"
         "  drain EP TMO_MS         read an IN endpoint until it times out\n"
         "  set-halt EP | clear-halt EP   SET_FEATURE / CLEAR_FEATURE(ENDPOINT_HALT)\n"
         "  ep-status EP            GET_STATUS of an endpoint\n"
         "  sleep MS                wait\n"
         "  note TEXT               write a comment into the trace");
}

int main(int argc, char** argv) {
    Probe pr;
    memset(&pr, 0, sizeof pr);
    pr.max_cmd = 1024;
    pr.max_ack = 1024;
    pr.ack_timeout_ms = 1000;
    const char* serial = NULL;
    int a = 1;
    for (; a + 1 < argc && argv[a][0] == '-' && argv[a][1] != '\0' && strlen(argv[a]) == 2; a += 2) {
        switch (argv[a][1]) {
            case 's': serial = argv[a + 1]; break;
            case 't': pr.trace = fopen(argv[a + 1], "w"); break;
            case 'm': if (load_map(&pr, argv[a + 1]) != 0) return 2; break;
            case 'T': pr.ack_timeout_ms = (unsigned)strtoul(argv[a + 1], NULL, 0); break;
            default: usage(); return 2;
        }
    }
    if (a >= argc || strcmp(argv[a], "-h") == 0) {
        usage();
        return 2;
    }
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t0_us = (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
    libusb_context* ctx = NULL;
    if (libusb_init(&ctx) != 0 || open_camera(&pr, ctx, serial) != 0) {
        return 1;
    }
    const struct libusb_version* lv = libusb_get_version();
    say(&pr, "u3vprobe serial=%s libusb %u.%u.%u.%u", serial == NULL ? "(first)" : serial, lv->major, lv->minor, lv->micro, lv->nano);
    int failures = 0;
    if (bootstrap(&pr, false) != 0) {
        say(&pr, "bootstrap read failed");
        failures++;
    }
#define NEED(n) if (a + (n) >= argc) { fprintf(stderr, "op %s needs %d argument(s)\n", op, (n)); failures++; break; }
#define NUM(i) strtoull(argv[a + (i)], NULL, 0)
    for (; a < argc; a++) {
        const char* op = argv[a];
        uint64_t t_op = now_us();
        int rc = 0;
        static uint8_t b[MAX_PACKET];
        if (strcmp(op, "desc") == 0) {
            op_desc(&pr);
        } else if (strcmp(op, "bootstrap") == 0) {
            rc = bootstrap(&pr, true);
        } else if (strcmp(op, "xml") == 0) {
            NEED(1);
            rc = op_xml(&pr, argv[++a]);
        } else if (strcmp(op, "r") == 0) {
            NEED(2);
            uint64_t addr = NUM(1);
            uint32_t len = (uint32_t)NUM(2);
            a += 2;
            if (len > sizeof b) len = sizeof b;
            rc = readmem(&pr, addr, b, len);
            if (rc == 0) hexdump(&pr, addr, b, len);
            else say(&pr, "r 0x%" PRIx64 " %u: FAILED status 0x%04x %s", addr, len, pr.last_status, status_name(pr.last_status));
        } else if (strcmp(op, "rlen") == 0) {
            NEED(2);
            uint64_t addr = NUM(1);
            uint32_t len = (uint32_t)NUM(2);
            a += 2;
            rc = readmem_once(&pr, addr, b, (uint16_t)len);
            say(&pr, "rlen 0x%" PRIx64 " %u: %s status 0x%04x %s", addr, len, rc == 0 ? "OK" : "FAILED", pr.last_status, status_name(pr.last_status));
        } else if (strcmp(op, "r32") == 0 || strcmp(op, "r64") == 0) {
            NEED(1);
            uint64_t addr = NUM(1);
            a += 1;
            uint64_t v = 0;
            uint32_t v32 = 0;
            rc = op[1] == '3' ? read_u32(&pr, addr, &v32) : read_u64(&pr, addr, &v);
            if (op[1] == '3') v = v32;
            if (rc == 0) say(&pr, "%s 0x%" PRIx64 " = 0x%" PRIx64 " (%" PRIu64 ")", op, addr, v, v);
            else say(&pr, "%s 0x%" PRIx64 ": FAILED status 0x%04x %s", op, addr, pr.last_status, status_name(pr.last_status));
        } else if (strcmp(op, "w32") == 0) {
            NEED(2);
            uint64_t addr = NUM(1);
            uint32_t v = (uint32_t)NUM(2);
            a += 2;
            rc = write_u32(&pr, addr, v);
            say(&pr, "w32 0x%" PRIx64 " <- 0x%x: %s status 0x%04x %s (%" PRIu64 " us)", addr, v, rc == 0 ? "OK" : "FAILED", pr.last_status,
                status_name(pr.last_status), now_us() - t_op);
        } else if (strcmp(op, "w") == 0) {
            NEED(2);
            uint64_t addr = NUM(1);
            int n = parse_hex(argv[a + 2], b, sizeof b);
            a += 2;
            rc = n < 0 ? -1 : writemem(&pr, addr, b, (uint32_t)n);
            say(&pr, "w 0x%" PRIx64 " (%d bytes): %s status 0x%04x %s (%" PRIu64 " us)", addr, n, rc == 0 ? "OK" : "FAILED", pr.last_status,
                status_name(pr.last_status), now_us() - t_op);
        } else if (strcmp(op, "raw") == 0) {
            NEED(1);
            static uint8_t ack[MAX_PACKET];
            int n = parse_hex(argv[++a], b, sizeof b);
            int got = n < 0 ? -1 : ctrl_exchange(&pr, b, n, ack, sizeof ack);
            if (got >= (int)U3V_ACK_HEADER_SIZE) {
                say(&pr, "raw: ack %d bytes status 0x%04x %s cmd 0x%04x len %u id %u (%" PRIu64 " us)", got, rd16(ack + 4), status_name(rd16(ack + 4)),
                    rd16(ack + 6), rd16(ack + 8), rd16(ack + 10), now_us() - t_op);
            } else {
                say(&pr, "raw: no usable ack (%d) after %" PRIu64 " us", got, now_us() - t_op);
                rc = -1;
            }
        } else if (strcmp(op, "get") == 0) {
            NEED(1);
            rc = op_get(&pr, argv[++a]);
        } else if (strcmp(op, "set") == 0) {
            NEED(2);
            rc = op_set(&pr, argv[a + 1], argv[a + 2]);
            a += 2;
        } else if (strcmp(op, "si-setup") == 0) {
            uint32_t xfer = 0;
            if (a + 1 < argc && argv[a + 1][0] >= '0' && argv[a + 1][0] <= '9') xfer = (uint32_t)NUM(1), a++;
            rc = op_si_setup(&pr, xfer);
        } else if (strcmp(op, "si-enable") == 0 || strcmp(op, "si-disable") == 0) {
            uint32_t v = op[3] == 'e' ? SIRM_CONTROL_STREAM_ENABLE : 0;
            rc = write_u32(&pr, pr.sirm + SIRM_CONTROL, v);
            say(&pr, "%s: %s status 0x%04x %s (%" PRIu64 " us)", op, rc == 0 ? "OK" : "FAILED", pr.last_status, status_name(pr.last_status), now_us() - t_op);
        } else if (strcmp(op, "grab") == 0) {
            NEED(2);
            int frames = (int)NUM(1);
            const char* prefix = argv[a + 2];
            a += 2;
            unsigned tmo = 2000;
            if (a + 1 < argc && argv[a + 1][0] >= '0' && argv[a + 1][0] <= '9') tmo = (unsigned)NUM(1), a++;
            rc = op_grab(&pr, frames, prefix, tmo) == frames ? 0 : -1;
        } else if (strcmp(op, "grabq") == 0) {
            NEED(1);
            int frames = (int)NUM(1);
            a += 1;
            int depth = 8;
            unsigned tmo = 2000;
            if (a + 1 < argc && argv[a + 1][0] >= '0' && argv[a + 1][0] <= '9') depth = (int)NUM(1), a++;
            if (a + 1 < argc && argv[a + 1][0] >= '0' && argv[a + 1][0] <= '9') tmo = (unsigned)NUM(1), a++;
            rc = op_grabq(&pr, ctx, frames, depth, tmo) == frames ? 0 : -1;
        } else if (strcmp(op, "drain") == 0) {
            NEED(2);
            op_drain(&pr, (uint8_t)NUM(1), (unsigned)NUM(2));
            a += 2;
        } else if (strcmp(op, "set-halt") == 0 || strcmp(op, "clear-halt") == 0 || strcmp(op, "ep-status") == 0) {
            NEED(1);
            uint8_t ep = (uint8_t)NUM(1);
            a += 1;
            if (op[0] == 's') {
                rc = libusb_control_transfer(pr.handle, 0x02, LIBUSB_REQUEST_SET_FEATURE, 0 /* ENDPOINT_HALT */, ep, NULL, 0, 1000);
            } else if (op[0] == 'c') {
                rc = libusb_clear_halt(pr.handle, ep);
            } else {
                uint8_t st[2] = {0, 0};
                rc = libusb_control_transfer(pr.handle, 0x82, LIBUSB_REQUEST_GET_STATUS, 0, ep, st, 2, 1000);
                say(&pr, "ep-status 0x%02x: %02x %02x (bit0 = halted)", ep, st[0], st[1]);
                rc = rc == 2 ? 0 : rc;
            }
            say(&pr, "%s 0x%02x: %s", op, ep, rc >= 0 ? "OK" : libusb_error_name(rc));
        } else if (strcmp(op, "sleep") == 0) {
            NEED(1);
            struct timespec d = {(time_t)(NUM(1) / 1000), (long)(NUM(1) % 1000) * 1000000L};
            nanosleep(&d, NULL);
            a += 1;
        } else if (strcmp(op, "note") == 0) {
            NEED(1);
            say(&pr, "NOTE %s", argv[++a]);
        } else {
            fprintf(stderr, "unknown op %s\n", op);
            rc = -1;
        }
        if (rc != 0) {
            failures++;
        }
    }
    if (pr.if_stream >= 0) libusb_release_interface(pr.handle, pr.if_stream);
    if (pr.if_event >= 0) libusb_release_interface(pr.handle, pr.if_event);
    libusb_release_interface(pr.handle, pr.if_ctrl);
    libusb_close(pr.handle);
    libusb_exit(ctx);
    if (pr.trace != NULL) fclose(pr.trace);
    return failures == 0 ? 0 : 1;
}
