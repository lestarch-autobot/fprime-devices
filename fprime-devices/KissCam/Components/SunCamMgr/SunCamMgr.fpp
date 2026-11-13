module KissCam {

    @ Manager of an MVP Aerospace KissCAM V2 sun camera over a UART byte-stream driver.
    @ One camera operation at a time; settings (parameters) applied by SETUP_CAMERA; images as data products.
    active component SunCamMgr {

        # ----------------------------------------------------------------------
        # UART driver (Drv.ByteStreamDriver client, receive made asynchronous)
        # ----------------------------------------------------------------------

        @ Driver (re)connected: handler is empty, no state is kept, no probe is sent (the camera may be off)
        async input port drvConnected: Drv.ByteStreamReady drop

        @ Bytes from the camera, framed on the component thread (one reply arrives in several buffers).
        @ Queue-full hook, in the driver's thread: returns the buffer and counts the drop — nothing else
        async input port drvReceiveIn: Drv.ByteStreamData hook

        @ Returns the driver's receive buffers
        output port drvReceiveReturnOut: Fw.BufferSend

        @ Sends one seven-byte command frame; the component keeps ownership of the frame buffer
        output port drvSendOut: Drv.ByteStreamSend

        # ----------------------------------------------------------------------
        # Framework services
        # ----------------------------------------------------------------------

        @ Rate-group tick (10 Hz), synchronous: the handler test-and-sets the atomic "run queued" flag and, when it
        @ was clear, invokes runInternal — at most one run message is ever on the queue
        sync input port run: Svc.Sched

        @ The tick work on the component thread: reply timeouts (KissCamConfig.REPLY_TIMEOUT_TICKS), deferred
        @ parameter push, periodic channels. The handler clears the atomic; the queue-full hook clears it too
        @ and counts the drop (QueueInput.RUN_TICK)
        internal port runInternal hook

        @ Health ping; queue-full hook only counts the drop
        async input port pingIn: Svc.Ping hook

        @ Health ping reply
        output port pingOut: Svc.Ping

        @ Data product allocation (synchronous)
        product get port productGetOut

        @ Data product delivery
        product send port productSendOut

        command recv port cmdIn
        command reg port cmdRegOut
        command resp port cmdResponseOut
        event port eventOut
        text event port textEventOut
        telemetry port tlmOut
        time get port timeGetOut
        param get port prmGetOut
        param set port prmSetOut

        # ----------------------------------------------------------------------
        # Data products (SDD Table 4)
        # ----------------------------------------------------------------------

        @ First record of an Image product
        product record ImageHeader: KissCam.ImageHeader id 0

        @ One image line: slot (1 byte), line number (2 bytes, MSB first), then 640 (FULL) or 64 (PREVIEW) pixel bytes; in HD the
        @ slot tag places the line in its quadrant (0 TL, 1 TR, 2 BL, 3 BR)
        product record ImageLine: U8 array id 1

        @ One product per image: the one slot downloaded (SD, Zoom) or slots 0..3 in order (HD)
        product container Image id 0 default priority KissCamConfig.IMAGE_DP_PRIORITY

        # ----------------------------------------------------------------------
        # Parameters (SDD Table 1) — pushed by SETUP_CAMERA and, while READY, on change
        # ----------------------------------------------------------------------

        @ Capture mode pushed with Set Mode: SD / ZOOM store one 640x480 slot, HD stores 1280x960 across slots 0..3
        param CAMERA_MODE: CaptureMode default CaptureMode.SD

        @ Exposure pushed with Set Exposure, in steps of 89.5 us, 2..960 (camera default 672)
        param EXPOSURE: U16 default 672

        # ----------------------------------------------------------------------
        # Commands (SDD Table 2). Queue-full hooks run in the dispatcher's thread and only answer
        # the command BUSY and count the drop; the component thread reports QueueOverflow on its next tick.
        # ----------------------------------------------------------------------

        @ Ping MCU, Set Mode (CAMERA_MODE), Set Exposure (EXPOSURE), Read Register 0x3012, Ping MCU+sensor; then READY
        async command SETUP_CAMERA hook

        @ Set Mode Sleep; the component returns to NOT_SETUP. Accepted when not set up
        async command SLEEP hook

        @ Ping the camera; reply counters and uptime go to events and telemetry. Accepted when not set up
        async command PING(pingType: PingType) hook

        @ Capture an image into a slot, optionally the test pattern (needs READY; in HD only SLOT_0, the camera fills slots 0..3).
        @ The ACK carries no payload (datasheet §7.4.5 response figure); CaptureComplete echoes the slot argument and the
        @ reply mode and reports the Exposure channel value. Sent only while READY (REQ-036)
        async command CAPTURE(slot: Slot, testPattern: TestPattern) hook

        @ Download an image into one Image data product (needs READY): 480 (FULL) or 48 (PREVIEW) line requests in order
        @ of the commanded slot, or of slots 0, 1, 2, 3 in HD (slot must be SLOT_0); the header mode is the first reply's mode byte.
        @ priority is set on the product at allocation: 1..2^32-1 as commanded (lower is sent first by Svc.DpCatalog),
        @ 0 selects KissCamConfig.IMAGE_DP_PRIORITY
        async command DOWNLOAD(slot: Slot, resolution: Resolution, $priority: FwDpPriorityType) hook

        @ Stop the download in progress after the pending line; the lines received are still delivered (ACTIVITY_HI)
        async command DOWNLOAD_ABORT hook

        @ Read a 16-bit image sensor register (needs READY)
        async command READ_REGISTER(address: U16) hook

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ SETUP_CAMERA accepted; exchanges begin
        event SetupStarted severity activity low format "Camera set-up started"

        @ Every set-up exchange succeeded; settings applied and read back
        event SetupComplete(mode: CaptureMode, exposure: U16) severity activity high \
            format "Camera set-up complete: mode {} exposure {}"

        @ A set-up exchange failed; the component is NOT_SETUP
        event SetupFailed(step: SetupStep, cause: FailCause) severity warning high \
            format "Camera set-up failed at {}: {}; component NOT_SETUP"

        @ A Table 1 setting was acknowledged by the camera
        event SettingApplied(setting: Setting, value: U16) severity activity high \
            format "Camera setting {} applied: {}"

        @ A parameter value outside its Table 1 range was refused before any camera frame; the applied value stands
        event SettingRejected(setting: Setting, value: U16, min: U16, max: U16) severity warning low \
            format "Camera setting {} value {} rejected: range {}..{}"

        @ A parameter push outside set-up failed; the component is NOT_SETUP
        event SettingPushFailed(setting: Setting, cause: FailCause) severity warning high \
            format "Camera setting {} push failed: {}; component NOT_SETUP"

        @ Exposure register 0x3012 read back after an exposure push
        event ExposureReadback(pushed: U16, readback: U16) severity activity low \
            format "Exposure pushed {} read back {}"

        @ Set Mode Sleep acknowledged; the component is NOT_SETUP
        event SleepEntered severity activity high format "Camera in Sleep; component NOT_SETUP"

        @ Ping reply
        event PingReply(pingType: PingType, validCmds: U8, invalidCmds: U8, uptime: U16) severity activity low \
            format "Camera ping {}: {} valid, {} invalid commands, uptime {} s"

        @ Capture acknowledged; exposure = the Exposure channel value (the last pushed or read-back exposure; the ACK carries none)
        event CaptureComplete(slot: Slot, testPattern: TestPattern, exposure: U16, mode: Mode) severity activity high \
            format "Captured slot {} (test pattern {}) at exposure {} in mode {}"

        @ First line reply received: imageMode = its mode byte, ImageHeader record written (product allocated at the command)
        event DownloadStarted(slot: Slot, imageMode: Mode, resolution: Resolution, lines: U16) severity activity low \
            format "Download of slot {} ({} image) at {} started: {} lines"

        @ Every line received; product sent
        event DownloadComplete(
            slot: Slot,
            imageMode: Mode,
            resolution: Resolution,
            lines: U16,
            bytes: U32 @< Image bytes received: lines x 640 (FULL) or x 64 (PREVIEW), all slots of an HD image; not the serialized record bytes
        ) severity activity high \
            format "Download of slot {} ({} image) at {} complete: {} lines, {} bytes"

        @ DOWNLOAD_ABORT ended the download; lines received so far were delivered
        event DownloadAborted(slot: Slot, linesReceived: U16) severity activity high \
            format "Download of slot {} aborted by command after {} lines"

        @ The download ended early on a fault; lines received so far were delivered when a product existed
        event DownloadFailed(slot: Slot, linesReceived: U16, cause: DownloadFailCause) severity warning high \
            format "Download of slot {} failed after {} lines: {}"

        @ An image line is being re-requested
        event LineRetry(line: U16, attempt: U8) severity diagnostic format "Re-requesting line {} (attempt {})"

        @ Read Register reply
        event RegisterRead(address: U16, value: U16) severity activity high \
            format "Register 0x{x} = {}"

        @ A ground command was rejected: NOT_READY, BUSY, NO_DOWNLOAD, HD_SLOT (response EXECUTION_ERROR, or BUSY for cause BUSY)
        event CommandRejected(operation: Operation, cause: RejectCause) severity warning low \
            format "{} rejected: {}"

        @ The camera answered with an error reply
        event CameraError(operation: Operation, cmdByte: U8, errorType: ErrorType, count: U8) severity warning high \
            format "Camera error during {}: command 0x{x} {} (invalid count {})"

        @ No reply within REPLY_TIMEOUT_TICKS
        event ReplyTimeout(operation: Operation, cmdByte: U8) severity warning high \
            format "Camera reply timeout during {} (command 0x{x})"

        @ Received bytes discarded; throttled, SETUP_CAMERA clears the throttle
        event FrameDiscarded(cause: DiscardCause, bytes: U32, head: FrameHead) severity warning low \
            format "Discarded {} bytes: {} head {}" throttle 10

        @ The camera's 21-byte power-on line (SDD Table 4 POWER_ON_LINE) arrived while READY or SETTING_UP: the camera restarted
        @ in Sleep; the operation in progress (NONE when idle) failed with CAMERA_RESTART and the component is NOT_SETUP
        event CameraRestarted(operation: Operation) severity warning high \
            format "Camera power-on line received during {}; component NOT_SETUP"
        @ The camera reported a mode other than the commanded one while READY
        event ModeMismatch(commanded: Mode, reported: Mode) severity warning high \
            format "Camera mode {} expected, {} reported; component NOT_SETUP"

        @ The UART driver refused a command frame
        event SendFailed(operation: Operation, status: Drv.ByteStreamStatus) severity warning high \
            format "UART send failed during {}: {}"

        @ No data-product container of the needed size was available
        event ProductAllocationFailed(bytes: U32) severity warning high \
            format "Image data product allocation of {} bytes failed"

        @ Queue-full drops since the last tick, reported from the component thread; SETUP_CAMERA clears the throttle
        event QueueOverflow(source: QueueInput, dropped: U32) severity warning high \
            format "Queue full: {} dropped {} times" throttle 10

        # ----------------------------------------------------------------------
        # Telemetry (SDD Table 3)
        # ----------------------------------------------------------------------

        @ Component set-up state
        telemetry SetupState: SetupState
        @ Operation in progress; NONE when idle
        telemetry CurrentOperation: Operation
        @ Camera mode from the mode byte of the last reply
        telemetry CameraMode: Mode
        @ EXPOSURE value the camera acknowledged, or the exposure of the last Capture reply (steps of 89.5 us)
        telemetry Exposure: U16
        @ Sensor register 0x3012 from the last readback (steps of 89.5 us)
        telemetry ExposureReadback: U16
        @ Camera uptime from the last Ping reply (s, rolls over at 65536)
        telemetry CameraUptime: U16 format "{} s"
        @ Valid commands counted by the camera since its boot (last Ping reply; rolls over at 256)
        telemetry CameraValidCmds: U8
        @ Invalid commands counted by the camera since its boot (last Ping or error reply; rolls over at 256)
        telemetry CameraInvalidCmds: U8
        @ Last Read Register reply
        telemetry RegisterValue: RegisterValue
        @ Last camera error reply
        telemetry LastCameraError: CameraErrorInfo
        @ Command/reply exchanges completed with an acknowledgement (since boot)
        telemetry Exchanges: U32
        @ Error replies received (since boot)
        telemetry CameraErrors: U32
        @ Reply timeouts (since boot)
        telemetry Timeouts: U32
        @ Bytes discarded outside an operation, malformed or for another command (since boot)
        telemetry DiscardedBytes: U32
        @ Discarded frames (since boot)
        telemetry MalformedReplies: U32
        @ Capture acknowledgements (since boot)
        telemetry ImagesCaptured: U32
        @ Image lines stored in products (since boot)
        telemetry LinesDownloaded: U32
        @ Image data products sent (since boot)
        telemetry ProductsSent: U32
        @ Inputs dropped by queue-full hooks (since boot): sum of the per-source mutex-guarded counts, read on the run tick
        telemetry QueueOverflows: U32
    }
}
