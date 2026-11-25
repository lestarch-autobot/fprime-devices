module Blackfly {

  @ State of one camera slot
  enum SlotState {
    DISABLED  @< Slot not enabled: reported by the coordinator only, never by CameraManager
    IDLE      @< Not capturing
    CAPTURING @< Capture sequence running
    FAULTED   @< Capture failed: stays until the next capture start
  } default IDLE

  @ Transport of one slot, chosen at startup
  enum Backend {
    USB3   @< Blackfly camera on USB3 (libusb)
    SOCKET @< Camera simulator on a local socket
  }

  @ Outcome of one transport call or control exchange
  enum ControlStatus {
    OK           @< Acknowledged with GenCP status SUCCESS
    REFUSED      @< Acknowledged with another GenCP status
    TIMEOUT      @< No acknowledgement within ACK_TIMEOUT
    MALFORMED    @< Acknowledgement does not match the command
    IO_ERROR     @< Transport error
    DISCONNECTED @< Camera or simulator gone
  }

  @ Step of a capture start
  enum StartStep {
    OPEN       @< Open the transport
    BOOTSTRAP  @< Read the bootstrap registers
    RESET      @< Stop any acquisition left running and reset the stream
    FIXED      @< Write the features fixed by the system
    PARAMETERS @< Write the parameters
    READ_BACK  @< Read the features back
    STREAM     @< Program the stream interface registers
    START      @< Write AcquisitionStart
  }

  @ Identity item checked when a camera is opened
  enum IdentityItem {
    GENCP_VERSION @< GenCP version register
    U3V_VERSION   @< USB3 Vision version register
    MODEL         @< Model name
    DESCRIPTION   @< SHA-1 of the device description in the manifest table
    LIMITS        @< Maximum command or acknowledgement length
  }

  @ Why an image was discarded
  enum DiscardReason {
    LEADER_SIZE    @< Leader width, height or pixel format differ from the read-back values
    PAYLOAD_SIZE   @< Trailer valid payload size differs from PayloadSize
    TRAILER_STATUS @< Trailer status is not success
    BLOCK_ID       @< Trailer block ID differs from the leader's
  }

  @ Why the stream reader stopped
  enum StreamFault {
    DISCONNECTED @< Camera or simulator gone
    IO_ERROR     @< Transport error
    OUT_OF_STEP  @< A transfer was not the leader or trailer expected
    STALLED      @< A frame was begun and no more data came
  }

  @ Metadata record of one image product
  struct ImageInfo {
    slot: U8                   @< Camera slot, 0 to 9
    sequence: U32              @< Capture sequence number
    imageIndex: U32            @< Index of the image within the sequence, from 0
    captureTime: Fw.TimeValue  @< System time when the image trailer arrived
    width: U32                 @< Image width in pixels
    height: U32                @< Image height in pixels
    offsetX: U32               @< Horizontal offset of the image on the sensor
    offsetY: U32               @< Vertical offset of the image on the sensor
    pixelFormat: PixelFormat   @< Pixel format
    pixelFormatCode: U32       @< Pixel format code of the leader (PFNC)
    blockId: U64               @< Camera block ID of the frame
    cameraTimestamp: U64       @< Camera timestamp of the frame, nanoseconds
  }

  @ Capture control from the coordinator to one slot
  port CaptureControl(
    start: bool   @< True starts a capture sequence, false stops it
    sequence: U32 @< Capture sequence number (start only)
  )

  @ State of one slot
  port SlotStatus(
    slot: U8             @< Camera slot, 0 to 9
    slotState: SlotState @< New state
  )

  @ One image product about to be handed to the data product writer (not yet on disk)
  port ImageStored(
    slot: U8             @< Camera slot, 0 to 9
    sequence: U32        @< Capture sequence number
    index: U32           @< Image index within the sequence
    captureTime: Fw.Time @< Capture time: the time tag of the product
    isJpeg: bool         @< False for a RAW product
  )

}
