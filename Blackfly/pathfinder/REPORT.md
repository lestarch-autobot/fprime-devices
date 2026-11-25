# Blackfly S USB3 Vision pathfinder report

Branch `blackfly-pathfinder`, directory `Blackfly/pathfinder/`. Evidence and reference code for the camera manager, USB3 backend and simulator campaigns; none of it is flight code.

Marking: **[V]** observed on the bench hardware in this campaign (trace in `truth/`), **[I]** inferred, read from a document, or a recommendation.

Cameras: two BFS-U3-244S8C, firmware 2103.0.628.0. **C** = `01908168` (decimal 26247528, working imager, lens capped). **B** = `017F4EC3` (decimal 25120451, "broken").

## 1. Deliverables

- [V] `probe/`: `u3vprobe`, one C11 file on libusb-1.0; descriptors, bootstrap, GenICam download, READMEM/WRITEMEM, stream acquisition, raw payload files, transfer trace. Builds without warnings with gcc 13.3.0 on the bench.
- [V] `genicam/`: the GenICam zip from each camera, the XML, the generator, and the generated table as JSON, C header and TSV (109 features).
- [V] `truth/`: 16 experiments as decoded text plus transfer trace; `truth/README.md` gives the result of each.
- [I] Payload bytes are not in the committed traces (24.5 MB per frame); traces keep every control, event, leader and trailer byte and the length and time of each payload transfer.

## 2. Findings

### 2.1 Transport and bootstrap

- [V] Each camera is one SuperSpeed device `1e10:4000` with three USB3 Vision interfaces: control (bulk 0x02 OUT, 0x82 IN), event (bulk 0x83 IN), stream (bulk 0x81 IN).
- [V] The USB iSerial is the serial in hex; the ABRM serial register is the same number in decimal.
- [V] GenCP 1.0. Maximum command and ack length 1024 bytes. MaximumDeviceResponseTime 200 ms. One stream channel.
- [V] ABRM, SBRM (0x10000), SIRM (0x20000), EIRM (0x28000) and the manifest table (0x40000) are readable; HeartbeatTimeout, DeviceSoftwareInterfaceVersion and the IIDC2 address are not implemented and answer INVALID_ADDRESS.
- [V] A read that spans an unimplemented register fails, so the bootstrap has to be read one field at a time.
- [V] An ack of exactly 1024 bytes is not followed by a zero-length packet; the host must read with a buffer of exactly the maximum ack length.
- [V] libusb with the default 16 MB `usbfs_memory_mb` reaches the camera's full rate (385 MB/s) using eight queued 1 MiB transfers. The limit was not raised.

### 2.2 GenICam

- [V] Both cameras deliver a byte-identical GenICam zip (SHA-256 `78df18f2...5180`) containing one XML (SHA-256 `9be43bbd...ffd4ad`), schema 1.1, model `BFS_U3_244S8`.
- [V] The manifest entry carries the SHA-1 of the file, and it matches.
- [I] A backend can check that SHA-1 at connect time to prove the static table matches the camera, without parsing XML.
- [V] All registers used are 4-byte little-endian (strings 64 or 128 bytes) in a regular block: flags, value, min, max, increment, fraction bits.
- [V] 10 of the 109 table features are computed: `Gain` (linear), seven fixed-point floats including `BlackLevel`, `Gamma` and `DeviceTemperature`, and the two frame-rate features (reciprocal of a period register). `PixelSize` is a formula and is not needed. `genicam/README.md` lists them.
- [V] `ExposureTime` is a plain integer register in microseconds.
- [V] Most selectors (`TriggerSelector`, `TestPatternGeneratorSelector`, ...) are not registers; they pick an address. `ChunkSelector` and `UserSetSelector` are registers.
- [V] The conversions for Gain, BlackLevel, Gamma and both frame-rate features gave values consistent with the measured behaviour.

### 2.3 GenCP behaviour

- [V] Status codes seen: 0x0000 SUCCESS, 0x8002 INVALID_PARAMETER, 0x8003 INVALID_ADDRESS, 0x8004 WRITE_PROTECT, 0x8006 ACCESS_DENIED, 0x8007 BUSY.
- [V] Out-of-range, off-increment and invalid enumeration values are rejected with 0x8002 and leave the register unchanged. The camera never clamps.
- [V] Accepted values can read back rounded (ExposureTime 1000 -> 997 us; Gain raw 12345 -> 12245).
- [V] Limits depend on other features (OffsetX.max on Width, maximum frame rate on exposure, height, pixel format and throughput limit), and a write that violates the current limit is rejected.
- [V] Width, Height, PixelFormat, TestPattern, IspEnable and AdcBitDepth answer 0x8006 while acquiring. Exposure, gain, black level, gamma, auto modes, trigger mode, frame-rate enable and throughput limit can be written while acquiring.
- [V] Commands the camera does not understand (unknown id, bad magic, no request-ack flag) get no answer; the next good command works.
- [V] Slow commands send PENDING_ACK (0x0805) every 100 ms with a 200 ms extension. AcquisitionStart always does, and completes in 163 to 225 ms.
- [V] A command sent while another is still running gets 0x8007 BUSY.
- [V] Typical ack times: 0.3 to 2 ms for plain registers, 17 to 24 ms for ROI, pixel format and mode switches.

### 2.4 Stream

- [V] A frame is a 52-byte leader, payload transfers, and a 32-byte trailer (36 bytes with chunk data).
- [V] The leader has block id, payload type, a nanosecond timestamp on the `TimestampLatchValue` clock, pixel format, size and offset. The trailer has block id, status, valid payload size and line count.
- [V] The camera sends no zero-length packets. The host must request exactly the sequence it programmed into the SIRM.
- [V] If the payload does not fit the programmed SIRM sizes, the camera truncates it and reports trailer status 0xA101 with the delivered size; the stream stays in step.
- [V] The camera queues up to 9 frames. When the host is too slow, new frames are dropped, block ids show gaps and `TransferQueueOverflowCount` increases. Delivered frames are always complete with status 0x0000.
- [V] AcquisitionStop discards queued frames.
- [V] CLEAR_FEATURE(HALT) on the stream endpoint stops the stream, even when it was healthy; AcquisitionStop then AcquisitionStart brings it back.
- [V] Clearing the SIRM stream-enable bit while acquiring ends the acquisition; a new AcquisitionStart is required.
- [V] Block id continued across stop/start while the stream stayed enabled and started at 0 after the stream was enabled again and acquisition started.
- [I] Block id restarts when the stream is enabled while no acquisition is active.
- [V] AcquisitionStart with the stream-enable bit clear was accepted and frames could still be read.
- [I] The backend should not rely on the enable bit to gate data; it should always stop acquisition first.
- [V] Chunk mode adds 20 bytes (image chunk id and length, CRC, CRC chunk id and length) and changes the payload type to 0x4001.

### 2.5 Rates

- [V] Full resolution (5320x4600), exposure 1 ms, throughput limit 500000000: BayerRG8 15.75 fps (385 MB/s), BayerRG16 7.87, BayerRG10p 10.49, BayerRG12p 10.49, Mono8 4.96, RGB8Packed 4.96.
- [V] Both cameras were found with `DeviceLinkThroughputLimit` 43000000, which holds BayerRG8 full frames to 1.76 fps. After a DeviceReset camera B read 380000000.
- [I] 43000000 was written by an earlier user of the bench and 380000000 is the power-up value; this was not confirmed on C.
- [V] Rate scales with Height and not with Width (5320x480 and 640x480 both 123 fps; 640x4600 15.7 fps).
- [V] `AcquisitionResultingFrameRate` matched every measured rate, so a manager can read the achievable rate instead of computing it.
- [V] Software trigger gives one frame per `TriggerSoftware`; triggers arriving during exposure or readout are dropped without an error.
- [V] First leader arrives 165 to 205 ms plus exposure after the AcquisitionStart command is sent.
- [V] Mono8 and RGB8Packed switch `IspEnable` on and it stays on when returning to a Bayer format.

### 2.6 The two cameras

- [V] C (capped) returns near-black but not zero images: mean 0.39 at 1 ms and 0 dB, with hot pixels at 255; mean 60 at 48 dB; the power-up auto modes raise exposure and gain while streaming.
- [V] C produces test patterns: `SensorTestPattern` on the sensor generator, `Increment` and `VariableFrameSequence` (all zero) on the pipeline generator. The patterns are static and pass through white balance.
- [V] B answers everything on the control and event channels like C.
- [V] B's first AcquisitionStart after power-up or reset succeeds and delivers exactly one complete frame of non-image data (mean 126.474, variance 11262.087 both times), then nothing.
- [V] B's later starts either succeed without any frame or are refused with 0x8006 ACCESS_DENIED; after the reset every later start was refused.
- [V] B differs from C at idle in `BlackLevelClampingEnable` (0 against 1), in a lower temperature (45.9 C against 66.8 C) and in auto exposure and gain never moving.
- [I] The sensor in B is not running; a manager can detect this as "AcquisitionStart refused" or "no second leader within the frame period plus margin". The idle differences are hints, not proof.
- [V] One camera streaming at full rate does not disturb control traffic on the other (worst ack 14.7 ms).
- [I] Two cameras streaming at once is untested, because B does not stream. Two full-rate BayerRG8 streams need 770 MB/s, more than one 5 Gbit/s host controller port can carry; the ten-camera system needs a throughput budget per controller.

## 3. Recommended transport interface

[I] The manager should see one camera through the operations below. The libusb backend maps them to bulk transfers; the simulator backend maps them to a local socket carrying the same bytes. GenCP framing, request ids, pending-ack handling and the leader/payload/trailer state machine sit above this interface so that both backends exercise them.

| Operation | libusb backend | Socket simulator |
| --- | --- | --- |
| `open(serial)` / `close()` | find `1e10:4000` by iSerial, claim interfaces 0 to 2 | connect to the instance's socket |
| `controlWrite(bytes, timeout)` | bulk OUT 0x02 | send on the control channel |
| `controlRead(buffer, maxAckLength, timeout)` -> length or timeout | bulk IN 0x82, buffer exactly `maxAckLength` | receive one ack |
| `streamRead(buffer, requestLength, timeout)` -> length or timeout | bulk IN 0x81, one transfer | receive one transfer |
| `streamQueue(buffers, lengths)` / `streamReap(timeout)` | asynchronous transfers, at least 8 MiB queued | same calls, no real queue needed |
| `eventRead(buffer, timeout)` | bulk IN 0x83 | receive one event |
| `streamReset()` | cancel transfers, clear halt on 0x81 | drop pending stream data |
| `info()` | USB strings, speed, bus address | simulator instance identity |

- [I] The socket needs transfer boundaries (a length prefix per transfer, or a sequenced-packet socket) because the protocol depends on them: a short transfer ends a GenCP ack and the SIRM sequence defines the stream.
- [I] The simulator should return the same statuses and timing classes as section 2.3 and enforce the same ordering as section 2.4; `truth/` holds the byte patterns.
- [I] Descriptor reads stay inside the libusb backend; the manager only needs the identity from `info()` and the bootstrap registers.
- [I] `streamReset()` must always be followed by AcquisitionStop and AcquisitionStart, because clearing a halt stops the stream.

## 4. Recommended persistent F Prime parameters

[I] Per camera. Values are validated against the limit registers before writing, and the read-back value is reported, because the camera rejects rather than clamps and rounds what it accepts.

| Parameter | Feature | Note |
| --- | --- | --- |
| Enable | none | camera participates or not |
| Frame rate (Hz) | `AcquisitionFrameRateEnable`, `AcquisitionFrameRate` | the independently configured imaging rate; must not exceed `AcquisitionResultingFrameRate` |
| Exposure mode and time (us) | `ExposureAuto`, `ExposureTime` | power-up default is auto Continuous |
| Gain mode and gain (dB) | `GainAuto`, `Gain` | power-up default is auto Continuous |
| Black level (%) | `BlackLevel` | |
| Gamma enable and value | `GammaEnable`, `Gamma` | default on, 0.8 |
| White balance mode and ratios | `BalanceWhiteAuto`, `BalanceRatio[Red]`, `BalanceRatio[Blue]` | default auto Continuous |
| Pixel format | `PixelFormat` | sets payload size and maximum rate |
| ADC bit depth | `AdcBitDepth` | |
| ROI | `Width`, `Height`, `OffsetX`, `OffsetY` | write offsets to 0 first, then size, then offsets |
| Binning and decimation | `BinningHorizontal`, `BinningVertical`, `DecimationHorizontal`, `DecimationVertical` | |
| Throughput limit (bytes/s) | `DeviceLinkThroughputLimit` | the per-camera share of the USB budget |
| Trigger mode and source | `TriggerMode`, `TriggerSource` | software trigger is an alternative way to set the rate |
| Chunk data enable | `ChunkModeActive` | frame id, CRC and timestamp in the payload |
| Test pattern | `TestPattern[Sensor]`, `TestPattern[PipelineStart]` | for checkout |

- [I] Not parameters: identity, temperature, power, link and queue counters (telemetry); AcquisitionStart/Stop, TimestampLatch, DeviceReset (commands).
- [I] Persistence should live in the F Prime parameter database only. Camera user sets were not used, by bench rule, and would create a second copy of the configuration.
- [I] Configuration parameters that the camera refuses while acquiring (ROI, pixel format, ADC bit depth, test pattern) need a stop, apply, restart sequence in the manager.

## 5. Provenance and licences

- [V] libusb 1.0.27 (Ubuntu 24.04 package `libusb-1.0-0` 2:1.0.27-1, runtime string 1.0.27.11882), dynamically linked as `/lib/x86_64-linux-gnu/libusb-1.0.so.0`.
- [I] libusb is LGPL-2.1-or-later; dynamic linking to the unmodified system library places no licence condition on the probe or on a backend's source.
- [V] Spinnaker was not installed, linked or run. No Spinnaker header or source was copied.
- [I] Protocol constants and layouts in `probe/u3v.h` come from the public USB3 Vision and GenCP register layouts and were confirmed on the wire; the FLIR technical reference on the bench was read for feature meanings only.
- [V] `genicam/*.zip` and `public_camxml.xml` are FLIR's device description, read from the cameras. The XML carries no licence text.
- [I] Redistribution terms for that XML are unknown; see open question 1.
- [V] The generator uses the Python 3 standard library only. Probe, generator and documents were written for this task.
- [V] Build tools: gcc 13.3.0, make, pkg-config from Ubuntu 24.04.

## 6. Bench changes

- [V] Installed with apt: `libusb-1.0-0-dev`, `pkg-config`, `poppler-utils` (to read the technical reference PDF).
- [V] Created `/mnt/sata/blackfly-dp/pathfinder/` (probe build, traces); large captures were deleted as they were made.
- [V] `usbfs_memory_mb` was left at 16. Nothing was changed in udev, the kernel, the hub or the watchdog. No reboot.
- [V] No camera non-volatile memory was written (no UserSetSave, no firmware, `DeviceUserID` untouched).
- [V] One `DeviceReset` was sent to camera B, once, to see its fresh-start behaviour. It re-enumerated in about 4 s with a new USB address.
- [V] All volatile settings were restored: `truth/regsnap-final-*.txt` differs from the as-found `truth/regsnap-*.txt` only in temperature, uptime, supply readings, counters and the timestamp latch. B's throughput limit was set back to the as-found 43000000 after its reset.
- [V] C's `TransferQueueOverflowCount` is 602 from the overrun tests; it clears at power-up.

## 7. Open questions

1. [I] May FLIR's GenICam XML be kept in this repository if the fork is ever made public? If not, keep only the hashes and the generated table.
2. [I] Is 43000000 or 380000000 the intended `DeviceLinkThroughputLimit`, and who set 43000000? The flight value per camera depends on how the ten cameras are spread over host controllers.
3. [I] B's failure is characterised only from outside. Is one garbage frame followed by refusal the behaviour the simulator should reproduce for a "failed imager", or should it model a family of faults?
4. [I] Two cameras streaming together could not be tested. A second working camera, or uncapping C alone, is needed for that and for real image content.
5. [I] The one-frame-then-denied sequence on B was seen after one power-up and one reset; the "start succeeds, no frame" variant was seen only in the first session. The trigger for each variant is unknown.
6. [I] Trailer status 0xA101 is the only non-zero stream status seen. Other status values (for example for a sensor fault) are unknown.
7. [I] Hardware trigger, exposure-end events, sequencer, and binning or decimation rates were not exercised.
8. [I] `grabq` in the probe discards payloads; full-rate capture to disk was not attempted and is a backend design topic (buffer ownership, zero copy).
9. [I] Should a backend verify the manifest SHA-1 and refuse a camera with different firmware, or only warn?
