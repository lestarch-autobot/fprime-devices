# u3vprobe: USB3 Vision probe on libusb-1.0

Pathfinder tool, not flight code. One C11 file plus `u3v.h` (protocol constants). It links only the system libusb-1.0 (dynamic).

```sh
sudo apt install libusb-1.0-0-dev pkg-config build-essential
make
./u3vprobe -s 01908168 -t trace.txt -m ../genicam/regmap.tsv desc bootstrap
```

Operations run left to right on one opened camera (`-s` is the USB iSerial):

| Operation | Purpose |
| --- | --- |
| `desc` | USB device, configuration, BOS and string descriptors |
| `bootstrap` | ABRM, SBRM, SIRM, EIRM and manifest table |
| `xml FILE` | download manifest entry 0 (the GenICam zip) |
| `r ADDR LEN`, `r32 ADDR`, `r64 ADDR`, `rlen ADDR LEN` | GenCP READMEM |
| `w32 ADDR VALUE`, `w ADDR HEXBYTES` | GenCP WRITEMEM |
| `raw HEXBYTES` | send arbitrary bytes on the control channel and print the ack |
| `get NAME`, `set NAME VALUE` | access by feature name through `-m regmap.tsv` |
| `si-setup [XFER_SIZE]`, `si-enable`, `si-disable` | program the SIRM for the current PayloadSize, stream enable bit |
| `grab N PREFIX [TMO_MS]` | read N frames synchronously; payloads to `PREFIX_NNNN.raw`; `-` discards and prints byte statistics; `--` discards |
| `grabq N [DEPTH] [TMO_MS]` | read N frames with DEPTH asynchronous transfers queued (needed for full rate); payload discarded |
| `drain EP TMO_MS` | read an IN endpoint until it times out (stream or event) |
| `set-halt EP`, `clear-halt EP`, `ep-status EP` | endpoint halt feature |
| `sleep MS`, `note TEXT` | pacing and trace comments |

`-t FILE` writes every transfer on every endpoint: microseconds since start, channel, direction, endpoint, result, length and bytes. Control, event, leader and
trailer transfers are dumped in full; payload transfers are logged with their length and first 32 bytes (`grab`) or length only (`grabq`), and `grab N PREFIX` writes the complete payload bytes to `PREFIX_NNNN.raw`.

Typical acquisition:

```sh
./u3vprobe -s 01908168 -t t.txt -m ../genicam/regmap.tsv \
    si-setup si-enable set AcquisitionStart 1 grab 3 frame 2000 set AcquisitionStop 1 si-disable
```

Things the backend campaign should lift (all observed on hardware, see `../truth/README.md`):

- Read acks with a buffer of exactly the negotiated maximum ack length (1024); a 1024-byte ack is not followed by a zero-length packet.
- Handle PENDING_ACK (0x0805) by extending the deadline by the timeout it carries.
- Read the bootstrap registers one field at a time; unimplemented ones answer INVALID_ADDRESS.
- Stream reads must follow the programmed SIRM sequence exactly (leader, N payload transfers, final 1, final 2, trailer); `grabq_step_size()` shows it.
- Keep several payload transfers queued. Eight 1 MiB transfers fit the default 16 MB `usbfs_memory_mb` and reach the camera's full 385 MB/s.

Known limits of the probe: one camera per process; `grabq` discards payload bytes; a `drain` with timeout 0 waits forever.
