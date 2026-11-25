# Hardware truth: Blackfly S BFS-U3-244S8C over USB3 Vision

Captured with `../probe/u3vprobe` on the LDC bench (Ubuntu 24.04, libusb 1.0.27), firmware 2103.0.628.0 on both cameras.

- `01908168` (decimal serial 26247528): working imager, lens capped. Called **C** below.
- `017F4EC3` (decimal serial 25120451): "broken" imager. Called **B** below.

Each experiment is a `.txt` (the probe's decoded output, with `NOTE` lines naming each step) and a `.trace`
(every USB transfer: microseconds since probe start, channel, direction, endpoint, libusb result, length, bytes).

Trace conventions:

- Control transfers (`CTRL`) and event transfers (`EVT`) are dumped in full.
- Stream leaders and trailers (`STRM`, lengths 52 / 32 / 36) are dumped in full.
- Payload transfers appear with timestamp and length; their hex bytes are not kept (a full-resolution frame is 24.5 MB). Lines for the bulk 1 MiB payload transfers are removed from most traces to keep the repository small; the `.txt` still shows the per-frame byte count. The probe itself can write full payloads (`grab N PREFIX`); two small sample payloads are kept (`13-*.raw`).
- `10b-*.txt` has consecutive identical lines collapsed to one line plus `[repeated N times]`.
- Multi-byte fields are little-endian. GenCP prefix: `55 33 56 43` magic, u16 flags (command) or status (ack), u16 command id, u16 payload length, u16 request id.

Unless a file says otherwise the camera was at: BayerRG8, 5320x4600, offset 0, autos off, ExposureTime 1000 us (reads 997), Gain 0, `DeviceLinkThroughputLimit` 500000000.

## Index

| File | What it shows |
| --- | --- |
| `01-enumeration-<serial>` | USB device, configuration, BOS and string descriptors; interface and endpoint discovery. |
| `02-bootstrap-<serial>` | ABRM, SBRM, SIRM, EIRM and manifest table, read field by field. |
| `regsnap-<serial>.txt` | Every table feature read at idle, as found at the start of the campaign. |
| `regsnap-final-<serial>.txt` | The same after the campaign and after restoring settings. |
| `03-gencp-status-01908168` | Ack status for bad address, bad length, write to read-only, out-of-range and invalid enum values. |
| `03b-gencp-malformed-01908168` | Malformed commands: unknown command id, no request-ack flag, bad magic, inconsistent lengths. |
| `04-pending-ack-01908168` | `TestPendingAck` 0..500 ms and the PENDING_ACK packets it produces. |
| `04b-pending-ack-short-timeout-01908168` | Host gives up after 50 ms and sends the next command: BUSY. |
| `05-readback-01908168` | Readback after write: rounding, rejection, dependent limits, payload size per pixel format. |
| `06-rate-<case>-01908168` | Achieved frame rate and USB throughput per pixel format, throughput limit, exposure, frame-rate setting and ROI. |
| `07-leader-trailer-chunk-01908168`, `07-chunk-tail-01908168.txt` | Leader and trailer bytes, timestamp against `TimestampLatchValue`, block id, chunk mode layout. |
| `08-overrun-underrun-01908168` | Host stops reading (device queue overflow) and host reads faster than frames arrive. |
| `09-endpoint-halt-01908168` | SET_FEATURE / CLEAR_FEATURE(ENDPOINT_HALT) on stream and control endpoints. |
| `10a-access-while-streaming-01908168` | Which registers can be written while acquiring; SingleFrame and MultiFrame. Sections C to J of this file ran with a left-over SingleFrame mode; use `10b` for ordering. |
| `10b-start-stop-ordering-01908168` | Start/stop/stream-enable ordering, recovery after clear-halt, SIRM programmed too small. Section L only shows continuous streaming and proves nothing else. |
| `11-software-trigger-01908168` | TriggerMode On with TriggerSource Software. |
| `12-capped-black-statistics-01908168` | Payload statistics of the capped camera against exposure, gain, black level, bit depth and the power-up auto modes. |
| `13-test-pattern-01908168`, `13-increment-640x480-BayerRG8.raw`, `13-increment-640x480-BayerRG16.raw` | Test pattern generators and two sample payloads. |
| `14-first-frame-latency-01908168` | Time from AcquisitionStart to the first leader. |
| `15-dual-C-01908168`, `15-dual-B-017F4EC3` | C streaming at full rate while B handles 600 control commands and an acquisition attempt. |
| `16-broken-camera-acquire-017F4EC3` | B asked to acquire after earlier start attempts. |
| `16b-broken-camera-after-reset-017F4EC3` (`.part1.trace` is the reset command itself) | B's first eight starts after one `DeviceReset`. |

## Results

### 01 Enumeration

- USB 3.1 SuperSpeed device `1e10:4000`, class EF/02/01 (IAD), one configuration, three interfaces of class EF/05 (USB3 Vision): 0 control (bulk OUT 0x02, bulk IN 0x82), 1 event (bulk IN 0x83), 2 stream (bulk IN 0x81, max burst 15).
- Strings: 1 `FLIR`, 2 `Blackfly S BFS-U3-244S8C`, 3 the hex serial, 4 `USB3 Vision Device`, 5 `1E10` + serial (GUID), 8 `GEN3-USB`.
- The two cameras differ only in the serial strings.

### 02 Bootstrap

- GenCP version 0x00010000; manufacturer `FLIR`; model `Blackfly S BFS-U3-244S8C`; family `GEN3-USB`; device version `2103.0.628.0`; ABRM serial is the **decimal** serial, the USB iSerial is the same number in hex.
- Manifest table at 0x40000, SBRM 0x10000, SIRM 0x20000, EIRM 0x28000. DeviceCapability 0xF0D. MaximumDeviceResponseTime 200 ms. TimestampIncrement 1000 ns.
- Not implemented (ack 0x8003 INVALID_ADDRESS): HeartbeatTimeout 0x1E8, DeviceSoftwareInterfaceVersion 0x210, SBRM IIDC2 address. TimestampLatch 0x1F8 is write-only (read gives 0x8002).
- A READMEM that spans an unimplemented register returns 0x8003 **and** the valid leading bytes, so the bootstrap must be read field by field.
- SBRM: maximum command and acknowledge transfer length 1024 bytes, one stream channel, current speed 8 (SuperSpeed).
- SIRM: payload size alignment 8 (info 0x03000000), required payload 24472000, required leader 52, required trailer 36. All host-written SIRM fields are 0 after power-up.
- Manifest: one entry, file version 1.0.0, zip of a schema 1.1 XML, address 0x60000000, size 67623, SHA-1 in the entry equals the SHA-1 of the downloaded file.

### 03 GenCP status codes (C)

| Request | Ack status |
| --- | --- |
| READMEM / WRITEMEM at an unmapped address | 0x8003 INVALID_ADDRESS |
| READMEM at register address + 1 | 0x8003 INVALID_ADDRESS |
| READMEM of 0, 2 or 3 bytes of a 4-byte register | 0x0000 with that many bytes |
| READMEM of 1012 bytes (largest that fits a 1024-byte ack) | 0x0000 |
| READMEM of 1013 bytes | 0x0000, ack cut to 1024 bytes, length field still says 1013 |
| READMEM of 4000 bytes | 0x8002 INVALID_PARAMETER |
| READMEM of a write-only register | 0x8002 INVALID_PARAMETER |
| WRITEMEM to a read-only register (WidthMax, ABRM manufacturer, PayloadSize) | 0x8004 WRITE_PROTECT |
| WRITEMEM of 8 bytes across a value and its read-only neighbour | 0x8004 WRITE_PROTECT |
| WRITEMEM of 2 bytes to the 4-byte Width register | 0x0000, and the register took the short value |
| Value out of range, off increment, unknown enum value, unavailable enum entry | 0x8002 INVALID_PARAMETER, register unchanged |
| Configuration register while acquiring (see 10a) | 0x8006 ACCESS_DENIED |
| New command while the previous one is still executing (see 04b) | 0x8007 BUSY |

Malformed commands (03b): unknown command id, missing request-ack flag, wrong magic and command id 0x0804 get **no ack at all** (host timeout); the control channel works again on the next good command without any halt clear. Inconsistent length fields are tolerated and answered with SUCCESS.

Timing: a plain register read or write is acknowledged in 0.3 to 2 ms; ROI writes take about 19 ms, PixelFormat about 24 ms, auto and enable switches about 17 ms; rejected ROI writes 6 to 8 ms.

### 04 Pending ack and timeout (C)

- `TestPendingAck` (0xF0000404, 0..500 ms) delays the ack of the write by the written value.
- For delays above 100 ms the camera sends PENDING_ACK (command id 0x0805, same request id, payload: u16 reserved, u16 timeout = 200 ms) every 100 ms, then the WRITEMEM ack. At exactly 100 ms there is no pending ack.
- The register keeps its value, so every later write to it is delayed again.
- `AcquisitionStart` always answers one PENDING_ACK and then SUCCESS after 163 to 225 ms.
- If the host stops waiting (50 ms) and sends another command, that command is answered 0x8007 BUSY with its own request id; the abandoned command still completes in the camera.

### 05 Readback (C)

- Nothing is clamped. A value outside min/max or off the increment is rejected with 0x8002 and the register keeps its old value.
- Accepted values are rounded to the sensor grid: ExposureTime 1000 -> 997, 12345 -> 12341, 17 -> 16, 20000000 -> 20000003 (range 16 to 30000003 us). Gain raw 12345 -> 12245 (maximum 130608 = 47.99 dB). BlackLevel and Gamma keep the written raw value.
- Limits follow other features: Width 1920 makes OffsetX.max 3400; Width cannot go back to 5320 while OffsetX is 1000. Width and OffsetX step 4 (as read from the increment registers), Height step 2.
- `AcquisitionFrameRate` raw is a period: fps = 125000000 / raw. With `AcquisitionFrameRateEnable` 0 it reads the current maximum. A rate above the current maximum is rejected.
- `AcquisitionResultingFrameRate` = 37125000 / raw.
- `DeviceLinkThroughputLimit` accepts 0 to 500000000 bytes/s.
- `PayloadSize` at 5320x4600: BayerRG8 and Mono8 24472000; BayerRG10p 30590000; BayerRG12p 36708000; BayerRG16 48944000; RGB8Packed 73416000.
- Selecting Mono8 or RGB8Packed sets `IspEnable` to 1 and it stays 1 after returning to BayerRG8.

### 06 Frame rate (C)

Measured leader to leader over 40 to 100 frames with the asynchronous reader (`grabq`, 8 transfers of 1 MiB queued), exposure 1 ms. No frame was lost in any of these runs.

| Case | Frames/s | USB payload MB/s |
| --- | --- | --- |
| BayerRG8 5320x4600 | 15.747 | 385.4 |
| BayerRG16 | 7.865 | 384.9 |
| BayerRG10p | 10.486 | 320.8 |
| BayerRG12p | 10.490 | 385.1 |
| Mono8 (ISP on) | 4.956 | 121.3 |
| RGB8Packed (ISP on) | 4.955 | 363.8 |
| BayerRG8, throughput limit 43000000 (as found) | 1.758 | 43.0 |
| BayerRG8, limit 100000000 | 4.084 | 99.9 |
| BayerRG8, limit 200000000 | 8.168 | 199.9 |
| BayerRG8, exposure 100 ms | 9.876 | |
| BayerRG8, exposure 500 ms | 1.995 | |
| BayerRG8, AcquisitionFrameRate 5 | 5.002 | |
| BayerRG8 2660x2300 | 30.682 | |
| BayerRG8 1920x1200 | 56.132 | |
| BayerRG8 640x480 | 122.706 | |
| BayerRG8 5320x480 | 122.870 | 313.8 |
| BayerRG8 640x4600 | 15.745 | |

- The camera's own `AcquisitionResultingFrameRate` agreed with every measurement (BayerRG8 full: 37125000 / 2358274 = 15.74).
- Rate depends on Height, not Width.
- With long exposures the rate is about 1 / (exposure + 1.3 ms).
- A slow host (synchronous 1 MiB reads plus per-byte statistics, about 150 to 270 MB/s) makes `TransferQueueOverflowCount` climb; see 08.

### 07 Leader, trailer, chunk data (C)

- Leader, 52 bytes: `55 33 56 4C` magic, u16 reserved, u16 leader size 0x34, u64 block id, u16 reserved, u16 payload type (1 image, 0x4001 image with chunks), u64 timestamp in ns, u32 pixel format, u32 size x, u32 size y, u32 offset x, u32 offset y, u16 padding x, u16 reserved.
- Trailer, 32 bytes: `55 33 56 54` magic, u16 reserved, u16 trailer size 0x20, u64 block id, u16 status, u16 reserved, u64 valid payload size, u32 size y. In chunk mode it is 36 bytes with a trailing u32 chunk layout id.
- The frame timestamp uses the same clock as `TimestampLatchValue` (ns, latch resolution 1000 ns).
- Payload transfers: the camera does not send a zero-length packet. The host must request exactly the SIRM sequence: leader, `count` transfers of the transfer size, final transfer 1, final transfer 2, trailer.
- `ChunkModeActive` 1: PayloadSize 24472020, payload type 0x4001, valid payload 24472020. After the image come (little-endian) the image chunk id and length, a u32 CRC, and the CRC chunk id and length 4. Image and CRC are the chunks enabled by default.
- With chunks the last payload bytes do not fit the alignment: final transfer 1 is rounded down to a multiple of 8 and final transfer 2 carries the rest.

### 08 Overrun and underrun (C)

- The camera queues at most 9 frames (`TransferQueueCurrentBlockCount`).
- Host not reading for 2 s at 15.7 fps: the queue fills, `TransferQueueOverflowCount` goes up by one per dropped frame. Afterwards the host receives the oldest queued frames (blocks 0 to 8) and then frames with gaps in the block id (34, 37, 39, ...). New frames are dropped while the queue is full.
- Every delivered frame is complete and has trailer status 0x0000. An overrun is visible only in the block id gaps and in `TransferQueueOverflowCount`, which counts since power-up.
- AcquisitionStop with frames queued: the frame being transferred is completed, the rest of the queue is discarded.
- Underrun: a host read that times out returns 0 bytes and loses nothing.

### 09 Endpoint halt (C)

- GET_STATUS is `00 00` on all endpoints at idle.
- After SET_FEATURE(HALT) on 0x81 the status reads `01 00` and bulk reads time out (libusb did not report a STALL).
- After CLEAR_FEATURE(HALT) the status is `00 00`, but no frames arrive until acquisition is stopped and started again (10b section J shows stop and start are sufficient).
- CLEAR_FEATURE(HALT) on a healthy streaming endpoint also stops the stream.
- The control endpoints report the halt bit but keep answering GenCP commands.

### 10 Access while streaming, start/stop ordering (C)

While acquiring (`Width.locked` bit = 1):

- Rejected with 0x8006 ACCESS_DENIED: Width, Height, PixelFormat, TestPattern, IspEnable, AdcBitDepth.
- Accepted: ExposureTime, Gain, BlackLevel, Gamma, ExposureAuto, AcquisitionMode, AcquisitionFrameRateEnable, TriggerMode, DeviceLinkThroughputLimit, BlackLevelClampingEnable, TimestampLatch, and also ChunkModeActive.
- SIRM: writing MaximumLeaderSize while the stream is enabled gives 0x8004 WRITE_PROTECT; PayloadTransferSize was accepted.
- If the payload grows beyond what the SIRM was programmed for (ChunkModeActive switched on mid-stream, or a transfer count programmed too small), the camera sends what fits and the trailer has **status 0xA101** with the truncated valid payload size and the number of complete lines in size y. The stream stays in step.

Ordering:

- Order used everywhere: program SIRM sizes, set SIRM stream enable, AcquisitionStart; AcquisitionStop, clear stream enable.
- AcquisitionStart while acquiring: SUCCESS within 1 ms, no effect. AcquisitionStop twice, stop when idle, stream enable or disable twice: all SUCCESS.
- After AcquisitionStop no further frame arrives (when the host was keeping up).
- Stop then start with the stream still enabled: frames resume and the block id continues (one id is skipped).
- Clearing stream enable while acquiring stops the stream. Setting it again brings no frames; a new AcquisitionStart is needed, and the block id then starts at 0.
- AcquisitionStart with the stream disabled is accepted, the camera fills its queue, and frames could be read from the endpoint even with the enable bit clear.
- SingleFrame gives one frame per AcquisitionStart; MultiFrame with AcquisitionFrameCount 3 gives exactly three.

### 11 Software trigger (C)

- TriggerMode 1 with TriggerSource 31 (Software): no frame until `TriggerSoftware` is written; one frame per trigger; frame timestamp about 2 ms after the write.
- Three triggers 300 ms apart without reading: three frames queued, timestamps 301 ms apart.
- Five triggers within 3 ms: one frame. Triggers during exposure or readout are dropped although each write is acknowledged SUCCESS.
- `TriggerSoftware` with TriggerMode 0: 0x8006 ACCESS_DENIED. After AcquisitionStop: SUCCESS, no frame.

### 12 Capped camera (C)

Payload byte statistics, BayerRG8 full frame, lens cap on:

| Setting | Mean | Variance | Min | Max |
| --- | --- | --- | --- | --- |
| Exposure 1 ms, gain 0 dB | 0.39 | 1.35 | 0 | 255 |
| Exposure 1 s, gain 0 dB | 2.7 | 26 | 0 | 255 |
| Exposure 1 ms, gain 47.99 dB | 60 | 3200 | 0 | 255 |
| BlackLevel 5 % | 21.83 | 1.29 | 9 to 13 | 255 |
| Power-up autos (ExposureAuto, GainAuto Continuous), after 3 s | 1 to 4 | 3.6 to 15 | 0 | 255 |

- The image is not all zero: a few hot pixels reach 255 in every frame.
- With the power-up auto modes the camera raises exposure and gain while streaming (13323 us and 17.2 dB after about 3 s); 54 % of the bytes are still 0.
- BayerRG16: 16-bit little-endian samples, 90 % exactly 0, maximum 65530, only 193 distinct values.

### 13 Test patterns (C)

`TestPatternGeneratorSelector` is not a register; it selects which register `TestPattern` uses (Sensor 0xF0004208, PipelineStart 0xF0004228).

- Sensor generator: accepts Off (0) and SensorTestPattern (3); 1, 2 and 4 give 0x8002.
- PipelineStart generator: accepts Off (0), Increment (1) and VariableFrameSequence (4); 3 gives 0x8002.
- SensorTestPattern and Increment are static: every frame is byte-identical (full-frame mean 156.209 and 157.430).
- Increment is a diagonal ramp that passes through the white-balance gains, so red, green and blue sites differ (row 0 starts 0, 3, 6, 7, 10, 11; row 1 starts 148, 241, 149, 244 at 640x480).
- The Increment pattern is the same at ROI offset 0,0 and 1000,1000.
- VariableFrameSequence delivered all-zero frames.

### 14 Time to first frame (C)

- AcquisitionStart is acknowledged after 163 to 202 ms (one PENDING_ACK on the way).
- The first leader arrives 1 to 2 ms plus the exposure time after that ack; the first frame timestamp is 163 to 205 ms plus exposure after a timestamp latched just before the start command.
- Leader to trailer takes about 94 ms at full resolution and 6.4 ms at 640x480.

### 15 Two cameras at once

- C streamed 100 full-resolution frames at 15.746 fps (385.3 MB/s) without loss while B answered 600 GenCP commands (ack after 0.29 ms on average, 14.7 ms worst case) and refused an acquisition start.
- Two cameras streaming image data at the same time could not be captured because B delivers no stream.

### 16 The broken camera (B)

- Enumeration, bootstrap, GenICam file, register reads and writes, ROI and pixel format changes, PayloadSize, pending ack, timestamp latch and the event channel (a 24-byte test event on 0x83) all behave like C.
- First AcquisitionStart after power-up or DeviceReset: SUCCESS with a pending ack like C, `Width.locked` becomes 1, and exactly **one** complete frame arrives (block 0, trailer status 0x0000, full valid payload). Its content is not an image: byte mean 126.474, variance 11262.087, the same numbers after power-up and after the reset. No second leader follows (host timeout), the queue is empty and the overflow count stays 0.
- Later starts: observed either SUCCESS with no frame at all (five times in the first session), or 0x8006 ACCESS_DENIED within 1 ms (every start after that, and starts 2 to 8 after the reset). `Width.locked` reads 0 in the ACCESS_DENIED state.
- Software trigger and the pipeline test pattern do not produce frames either.
- Differences from C that are readable at idle: `BlackLevelClampingEnable` is 0 (C: 1); ExposureTime and Gain never change under the auto modes; DeviceTemperature raw 367 (45.9 C) against 534 (66.8 C) on C.
- `DeviceReset` (0x42004 <- 1): SUCCESS ack after 0.24 ms, the camera left the bus and was back within about 4 s with a new device address. After the reset `DeviceLinkThroughputLimit` read 380000000; both cameras had 43000000 when the campaign started.
