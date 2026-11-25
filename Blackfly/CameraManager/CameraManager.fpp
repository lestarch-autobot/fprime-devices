module Blackfly {

  @ Size of a camera feature name in an event
  constant FeatureNameSize = 48

  @ Controls one Blackfly S camera, on USB3 or simulated, and stores its images as RAW data products
  active component CameraManager {

    # ----------------------------------------------------------------------
    # Ports
    # ----------------------------------------------------------------------

    @ Capture start and stop from the coordinator
    async input port captureIn: CaptureControl

    @ Slot state to the coordinator
    output port statusOut: SlotStatus

    @ Notice of an image product, sent before the product
    output port imageStoredOut: ImageStored

    @ Periodic tick: image timeout, buffer retry, telemetry
    async input port schedIn: Svc.Sched drop

    @ Health ping
    async input port pingIn: Svc.Ping

    @ Health ping reply
    output port pingOut: Svc.Ping

    @ Image buffer request
    product request port productRequestOut

    @ Image buffer response
    async product recv port productRecvIn

    @ Image product to the data product manager
    product send port productSendOut

    @ Return of an image buffer that will not become a product
    output port bufferReturnOut: Fw.BufferSend

    @ From the stream reader: one frame was received into the buffer it held
    internal port frameReceived(
      sequence: U32        @< Sequence the frame was read in
      blockId: U64         @< Block ID of the leader
      cameraTimestamp: U64 @< Timestamp of the leader, nanoseconds
      captureTime: Fw.Time @< System time when the trailer arrived
      discard: bool        @< True when the frame failed a check
      reason: DiscardReason @< The failed check
      got: U32             @< Value found
      expected: U32        @< Value expected
    ) block

    @ From the stream reader: frames were read with no buffer to hold them
    internal port framesDropped(
      count: U32 @< Frames dropped since the last report
    ) drop

    @ From the stream reader: the stream ended
    internal port streamFailed(
      sequence: U32      @< Sequence the stream was read in
      fault: StreamFault @< Why
    ) block

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
    # Commands
    # ----------------------------------------------------------------------

    @ Write the control trace to a file in the trace directory
    async command DUMP_CONTROL_TRACE opcode 0

    # ----------------------------------------------------------------------
    # Events
    # ----------------------------------------------------------------------

    @ Capture sequence running
    event CaptureStarted(
      sequence: U32    @< Capture sequence number
      payloadSize: U32 @< Bytes per image
      frameRate: F64   @< Camera's resulting frame rate, Hz
    ) severity activity high id 0 \
      format "Capture sequence {} started: {} bytes per image at {} Hz"

    @ Capture sequence ended by a stop request
    event CaptureStopped(
      sequence: U32  @< Capture sequence number
      stored: U32    @< Images handed to the store
      dropped: U32   @< Images dropped
      discarded: U32 @< Images discarded
    ) severity activity high id 1 \
      format "Capture sequence {} stopped: {} images stored, {} dropped, {} discarded"

    @ Capture start request while capturing
    event CaptureStartIgnored(
      sequence: U32 @< Sequence number of the ignored request
      running: U32  @< Sequence number running
    ) severity warning low id 2 \
      format "Capture start for sequence {} ignored: sequence {} is running"

    @ The transport could not be opened
    event CameraOpenFailed(
      backend: Backend      @< Transport
      status: ControlStatus @< Outcome
    ) severity warning high id 3 \
      format "Camera open failed on {}: {}"

    @ The camera is not the one the register table was generated for
    event CameraIdentityMismatch(
      item: IdentityItem @< Item that differs
    ) severity warning high id 4 \
      format "Camera identity mismatch: {}"

    @ A control exchange of the capture start failed
    event ControlFailed(
      step: StartStep       @< Step of the capture start
      address: U32          @< Register address
      status: ControlStatus @< Outcome
      gencpStatus: U16      @< GenCP status of the acknowledgement, 0 when there was none
    ) severity warning high id 5 \
      format "Control exchange failed in step {} at address {x}: {}, GenCP status {x}"

    @ The camera refused a parameter value
    event ParameterRefused(
      feature: string size FeatureNameSize @< Camera feature
      status: ControlStatus                @< Outcome
      gencpStatus: U16                     @< GenCP status of the acknowledgement, 0 when there was none
    ) severity warning high id 6 \
      format "Camera refused parameter {}: {}, GenCP status {x}"

    @ A parameter was not written because the camera reports its feature locked or unavailable
    event ParameterLocked(
      feature: string size FeatureNameSize @< Camera feature
    ) severity activity low id 7 \
      format "Parameter {} not written: feature locked or unavailable"

    @ AcquisitionStart was not acknowledged with success in time
    event CaptureStartNotAcknowledged(
      status: ControlStatus @< Outcome
      gencpStatus: U16      @< GenCP status of the acknowledgement, 0 when there was none
      timeoutMs: U32        @< ACK_TIMEOUT in use
    ) severity warning high id 8 \
      format "Capture start not acknowledged: {}, GenCP status {x}, timeout {} ms"

    @ No image arrived for the image timeout while capturing
    event ImageTimeout(
      timeoutMs: U32 @< IMAGE_TIMEOUT in use
      stored: U32    @< Images handed to the store in this sequence
    ) severity warning high id 9 \
      format "No image for {} ms after {} images"

    @ The camera or simulator disconnected while capturing
    event CameraDisconnected(
      backend: Backend @< Transport
    ) severity warning high id 10 \
      format "Camera disconnected on {}"

    @ The stream failed while capturing
    event StreamFailed(
      fault: StreamFault @< Why
    ) severity warning high id 11 \
      format "Image stream failed: {}"

    @ AcquisitionStop was not acknowledged with success
    event CaptureStopFailed(
      status: ControlStatus @< Outcome
      gencpStatus: U16      @< GenCP status of the acknowledgement, 0 when there was none
    ) severity warning high id 12 \
      format "Capture stop failed: {}, GenCP status {x}"

    @ An image failed a check and was discarded
    event ImageDiscarded(
      blockId: U64          @< Camera block ID
      reason: DiscardReason @< The failed check
      got: U32              @< Value found
      expected: U32         @< Value expected
    ) severity warning low id 13 \
      format "Image block {} discarded: {} is {}, expected {}" throttle 10

    @ Images arrived with no buffer to store them
    event ImagesDroppedNoBuffer(
      count: U32 @< Images dropped since the last report
    ) severity warning low id 14 \
      format "{} images dropped: no buffer" throttle 10

    @ A read-back value has no entry in the register table
    event ReadBackUnknown(
      feature: string size FeatureNameSize @< Camera feature
      raw: U32                             @< Register value
    ) severity warning low id 15 \
      format "Read-back of {} has unknown value {}"

    @ The control trace was written
    event ControlTraceDumped(
      fileName: string size FileNameStringSize @< File written
      entries: U32                             @< Exchanges written
    ) severity activity high id 16 \
      format "Control trace written to {}: {} exchanges"

    @ The control trace could not be written
    event ControlTraceDumpFailed(
      fileName: string size FileNameStringSize @< File
      status: I32                              @< Os::File status
    ) severity warning high id 17 \
      format "Control trace not written to {}: file status {}"

    @ A capture stop request arrived while not capturing
    event CaptureStopIgnored(
      slotState: SlotState @< State when the request arrived
    ) severity activity low id 18 \
      format "Capture stop ignored in state {}"

    # ----------------------------------------------------------------------
    # Telemetry
    # ----------------------------------------------------------------------

    @ Slot state
    telemetry State: SlotState id 0 update on change

    @ Transport chosen at startup
    telemetry SlotBackend: Backend id 1 update on change

    @ Capture sequence number, current or last
    telemetry Sequence: U32 id 2 update on change

    @ Images handed to the store in the current or last sequence
    telemetry ImagesStored: U32 id 3 update on change

    @ Images dropped for lack of a buffer since startup
    telemetry ImagesDropped: U32 id 4 update on change

    @ Images discarded by a size or status check since startup
    telemetry ImagesDiscarded: U32 id 5 update on change

    @ Frames the camera did not deliver (gaps in the block ID) since startup
    telemetry CameraFramesMissed: U32 id 6 update on change

    @ Parameters not written at the last capture start because the feature was locked or unavailable
    telemetry ParametersLocked: U32 id 7 update on change

    @ Bytes per image read back at the last capture start
    telemetry PayloadSize: U32 id 8 update on change

    @ Camera feature `AcquisitionResultingFrameRate` read back at the last capture start (Hz)
    telemetry ResultingFrameRate: F64 id 9 update on change

    @ Serial number of the camera opened
    telemetry CameraSerial: U32 id 10 update on change

    @ Control exchanges held in the control trace
    telemetry ControlTraceEntries: U32 id 11 update on change

    include "CameraReadBack.fppi"

    # ----------------------------------------------------------------------
    # Parameters
    # ----------------------------------------------------------------------

    include "CameraParams.fppi"

    @ Time allowed for the camera to acknowledge one control command (ms)
    param ACK_TIMEOUT: U32 default 1000 id 0x80

    @ Time allowed between images while capturing (ms)
    param IMAGE_TIMEOUT: U32 default 5000 id 0x81

    # ----------------------------------------------------------------------
    # Data products
    # ----------------------------------------------------------------------

    @ One RAW image
    product container Image id 0 default priority 10

    @ Image metadata
    product record Info: ImageInfo id 0

    @ Pixel data as the camera sent it
    product record Pixels: U8 array id 1

  }

}
