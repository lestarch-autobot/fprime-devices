// ======================================================================
// \title  SunCamMgr.hpp
// \brief  hpp file for SunCamMgr component implementation class
// ======================================================================

#ifndef KissCam_SunCamMgr_HPP
#define KissCam_SunCamMgr_HPP

#include <atomic>

#include "Os/Mutex.hpp"
#include "fprime-devices/KissCam/Components/SunCamMgr/SunCamMgrComponentAc.hpp"
#include "fprime-devices/KissCam/Types/FppConstantsAc.hpp"

namespace KissCam {

//! Manager of one KissCAM V2 camera on a UART byte stream (docs/sdd.md)
class SunCamMgr final : public SunCamMgrComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Protocol constants (KissCAM V2 datasheet §7)
    // ----------------------------------------------------------------------

    static constexpr U8 FRAME_START = 0x40;        //!< '@' first byte of every frame
    static constexpr U8 FRAME_END = 0x0D;          //!< carriage return last byte of every frame
    static constexpr U8 ACK_ERROR = 0xFF;          //!< ACK byte of an error reply
    static constexpr U8 CMD_PING = 0x50;           //!< 'P'
    static constexpr U8 CMD_SET_MODE = 0x4D;       //!< 'M'
    static constexpr U8 CMD_SET_EXPOSURE = 0x45;   //!< 'E'
    static constexpr U8 CMD_CAPTURE = 0x43;        //!< 'C'
    static constexpr U8 CMD_DOWNLOAD = 0x44;       //!< 'D'
    static constexpr U8 CMD_READ_REGISTER = 0x52;  //!< 'R'

    static constexpr FwSizeType COMMAND_FRAME_SIZE = 7;  //!< 0x40 CMD P1 P2 P3 P4 0x0D
    static constexpr FwSizeType REPLY_HEADER_SIZE = 5;   //!< 0x40 ACK MODE LEN_MSB LEN_LSB
    static constexpr FwSizeType REPLY_MAX_SIZE = 648;    //!< header + 642-byte payload + 0x0D
    static constexpr FwSizeType REPLY_MAX_PAYLOAD = REPLY_MAX_SIZE - REPLY_HEADER_SIZE - 1;
    static constexpr FwSizeType FRAME_HEAD_SIZE = 8;  //!< bytes kept in FrameDiscarded

    static constexpr U16 EXPOSURE_REGISTER = 0x3012;          //!< sensor register read back after an exposure push
    static constexpr U16 FULL_PIXELS = KissCam::FULL_PIXELS;  //!< pixels per full-resolution line (SDD Table 4)
    static constexpr U16 PREVIEW_PIXELS = KissCam::PREVIEW_PIXELS;  //!< pixels per preview line
    static constexpr U16 LINE_ECHO_SIZE = 2;                        //!< line number echoed ahead of the pixels
    static constexpr U16 FULL_LINES = KissCam::FULL_LINES;          //!< line requests per full-resolution slot
    static constexpr U16 PREVIEW_LINES = KissCam::PREVIEW_LINES;    //!< line requests per preview slot
    static constexpr FwSizeType SLOT_COUNT = 4;                     //!< camera memory slots; an HD image spans all four
    static constexpr FwSizeType LINE_RECORD_PREFIX = 3;             //!< slot, line MSB, line LSB before the pixels
    static constexpr FwSizeType POWER_ON_LINE_SIZE = 21;            //!< "MVP Camera V2 RS422\r\n" (SDD Table 4)
    static const U8 POWER_ON_LINE[POWER_ON_LINE_SIZE];              //!< the camera's unframed power-on line

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SunCamMgr object
    explicit SunCamMgr(const char* const compName  //!< The component name
    );

    //! Destroy SunCamMgr object
    ~SunCamMgr();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for drvConnected (ignored: no state is kept about the driver)
    void drvConnected_handler(FwIndexType portNum  //!< The port number
                              ) override;

    //! Handler implementation for drvReceiveIn: frames the camera bytes and returns the buffer
    void drvReceiveIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& buffer,
                              const Drv::ByteStreamStatus& status) override;

    //! Handler implementation for pingIn: health ping answered on pingOut
    void pingIn_handler(FwIndexType portNum,  //!< The port number
                        U32 key               //!< Value to return to pinger
                        ) override;

    //! Handler implementation for run (rate-group thread): queues at most one runInternal message
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    void SETUP_CAMERA_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void SLEEP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void PING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const KissCam::PingType& pingType) override;
    void CAPTURE_cmdHandler(FwOpcodeType opCode,
                            U32 cmdSeq,
                            const KissCam::Slot& slot,
                            const KissCam::TestPattern& testPattern) override;
    void DOWNLOAD_cmdHandler(FwOpcodeType opCode,
                             U32 cmdSeq,
                             const KissCam::Slot& slot,
                             const KissCam::Resolution& resolution,
                             FwDpPriorityType priority) override;
    void DOWNLOAD_ABORT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void READ_REGISTER_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U16 address) override;

    // ----------------------------------------------------------------------
    // Handler implementations for user-defined internal interfaces
    // ----------------------------------------------------------------------

    //! The tick on the component thread: timeouts, deferred parameter push, periodic channels
    void runInternal_internalInterfaceHandler() override;

    // ----------------------------------------------------------------------
    // Queue-full hooks (caller's thread: count the drop, touch nothing else)
    // ----------------------------------------------------------------------

    void drvReceiveIn_overflowHook(FwIndexType portNum,
                                   Fw::Buffer& buffer,
                                   const Drv::ByteStreamStatus& status) override;
    void pingIn_overflowHook(FwIndexType portNum, U32 key) override;
    void runInternal_overflowHook() override;
    void SETUP_CAMERA_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) override;
    void SLEEP_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) override;
    void PING_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) override;
    void CAPTURE_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) override;
    void DOWNLOAD_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) override;
    void DOWNLOAD_ABORT_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) override;
    void READ_REGISTER_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) override;

    //! Parameter change: only arms the guarded dirty flag of that parameter
    void parameterUpdated(FwPrmIdType id) override;

    // ----------------------------------------------------------------------
    // Private types
    // ----------------------------------------------------------------------

    //! Phase of a DOWNLOAD operation
    enum DownloadPhase { DL_FIRST_LINE, DL_LINES };  //!< the ImageHeader record is written on the first line reply

    // ----------------------------------------------------------------------
    // Command admission and operation bookkeeping
    // ----------------------------------------------------------------------

    //! Table 2 "Needs READY" column
    enum class Admission { ANY_STATE, READY_ONLY };

    //! Admit a command: false (and the response/rejection sent) when it is refused
    bool acceptCommand(FwOpcodeType opCode, U32 cmdSeq, Operation::T operation, Admission admission);

    //! Record a drop counted by a queue-full hook
    void countDrop(QueueInput::T source);

    //! Answer the pending command response and clear it
    void respond(Fw::CmdResponse::T response);

    //! End the operation in progress
    void endOperation();

    void setSetupState(SetupState::T state);
    void setOperation(Operation::T operation);

    // ----------------------------------------------------------------------
    // Camera exchanges
    // ----------------------------------------------------------------------

    //! Send one 7-byte command frame and arm the reply timeout; false when the driver refused it
    bool sendFrame(U8 cmd, U8 p1, U8 p2, U8 p3, U8 p4);

    //! Reply timeout, 0xFF reply, mode or exposure-readback mismatch, send failure, or the camera power-on line of the
    //! exchange in progress
    void exchangeFailed(FailCause::T cause);

    //! Append received bytes to the assembler and process every complete reply
    void assemble(const U8* bytes, FwSizeType size);

    //! Flush the bytes skipped while resynchronising as one discard
    void flushStray();

    //! Account one byte of a stray run: head, count and the power-on line matcher (REQ-034)
    void strayByte(U8 byte);

    //! Report the stray bytes ahead of the last `keep` (a power-on line or its prefix) as their own discard
    void discardStrayBefore(FwSizeType keep);

    //! Account for a discarded frame
    void discard(DiscardCause::T cause, const U8* bytes, FwSizeType headSize, U32 totalBytes);

    //! Discard whatever the reply assembler holds (a frame that never completed)
    void discardPartialFrame(DiscardCause::T cause);

    //! Validate a complete reply against the exchange in progress, then decode it
    void processReply(const U8* frame, FwSizeType size);

    //! Decode a validated reply payload for the operation in progress
    void handleReply(Mode::T reported, const U8* payload, U16 length);

    // ----------------------------------------------------------------------
    // Set-up, parameter push, download sequencing
    // ----------------------------------------------------------------------

    //! Send the frame of a set-up step (also used by a parameter push)
    void sendSetupStep(SetupStep::T step);
    void finishSetupOrPush();

    //! Check EXPOSURE against EXPOSURE_MIN..EXPOSURE_MAX; emits SettingRejected when it fails
    bool exposureInRange(U16 exposure);
    //! Read the EXPOSURE parameter into m_exposureSent; false (operation ended) when it is out of range
    bool takeExposureParameter();

    //! HD images are named by SLOT_0 only (REQ-035). Call right after a successful acceptCommand: on refusal it
    //! answers CommandRejected(HD_SLOT) + EXECUTION_ERROR, undoes the admission and returns false
    bool slotAllowed(Operation::T operation, Slot::T slot);
    //! Write the ImageHeader record from the first line reply (reported = its mode byte) and announce the download
    void writeImageHeader(Mode::T reported);
    //! The camera's power-on line completed in the stray bytes: discard it; while set up, the camera restarted
    //! (REQ-034)
    void powerOnLineReceived();
    void requestLine();
    void storeLine(const U8* payload, U16 length);
    void finishDownload(bool complete);
    //! Shared tail of finishDownload/failDownload: deliver the product when one exists, answer, end the operation
    void closeDownload(Fw::CmdResponse::T response);
    void failDownload(DownloadFailCause::T cause);

    // ----------------------------------------------------------------------
    // Telemetry
    // ----------------------------------------------------------------------

    void writeStateChannels();
    void writeCounterChannels();

    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    // Component state
    SetupState::T m_setupState;
    Operation::T m_operation;
    Mode::T m_commandedMode;  //!< mode the camera was last told to be in
    bool m_responsePending;
    FwOpcodeType m_pendingOpCode;
    U32 m_pendingCmdSeq;

    // Exchange in progress
    U8 m_pendingCmd;  //!< command byte awaiting a reply, 0 when none
    U8 m_pendingParams[4];
    U32 m_ticksWaiting;

    // Set-up and parameter push
    SetupStep::T m_setupStep;
    U16 m_exposureSent;
    U16 m_exposure;  //!< Exposure channel value: the last exposure the camera acknowledged (m_exposureSent: being
                     //!< pushed)
    //! CAMERA_MODE value pushed by the set-up in progress (reported by SetupComplete)
    CaptureMode::T m_modeSent;
    PingType::T m_pingType;

    // Capture
    Slot::T m_captureSlot;
    TestPattern::T m_captureTestPattern;

    // Download
    DownloadPhase m_downloadPhase;
    Slot::T m_downloadSlot;       //!< slot as commanded
    Slot::T m_downloadFirstSlot;  //!< first slot read (SLOT_0 for an HD image)
    Resolution::T m_downloadResolution;
    Mode::T m_downloadImageMode;
    U16 m_linesPerSlot;
    U16 m_pixelsPerLine;
    U16 m_linesExpected;
    U16 m_linesReceived;
    U8 m_lineAttempt;
    bool m_abortRequested;
    FwOpcodeType m_abortOpCode;
    U32 m_abortCmdSeq;
    DpContainer m_container;
    U8 m_lineRecord[LINE_RECORD_PREFIX + FULL_PIXELS];

    // Framing
    U8 m_txFrame[COMMAND_FRAME_SIZE];
    U8 m_rx[REPLY_MAX_SIZE];
    FwSizeType m_rxLen;
    U32 m_strayCount;
    U8 m_strayHead[FRAME_HEAD_SIZE];
    FwSizeType m_powerOnMatch;  //!< bytes of POWER_ON_LINE matched by the tail of the stray run

    // Tick
    std::atomic<bool> m_runQueued;
    U32 m_tickCount;

    // Parameter dirty flags and queue drops (set from other threads)
    Os::Mutex m_lock;
    bool m_modeDirty;
    bool m_exposureDirty;
    U32 m_drops[QueueInput::NUM_CONSTANTS];

    // Counters (Table 3)
    U32 m_exchanges;
    U32 m_cameraErrors;
    U32 m_timeouts;
    U32 m_discardedBytes;
    U32 m_malformedReplies;
    U32 m_imagesCaptured;
    U32 m_linesDownloaded;
    U32 m_productsSent;
    U32 m_queueOverflows;
};

}  // namespace KissCam

#endif
