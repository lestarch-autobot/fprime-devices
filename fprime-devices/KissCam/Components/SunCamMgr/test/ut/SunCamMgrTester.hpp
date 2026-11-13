// ======================================================================
// \title  SunCamMgrTester.hpp
// \author lestarch-autobot
// \brief  hpp file for SunCamMgr component test harness implementation class
// ======================================================================

#ifndef KissCam_SunCamMgrTester_HPP
#define KissCam_SunCamMgrTester_HPP

#include "fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.hpp"
#include "fprime-devices/KissCam/Components/SunCamMgr/SunCamMgrGTestBase.hpp"

namespace KissCam {

class SunCamMgrTester final : public SunCamMgrGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 4096;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Queue depth supplied to the component instance under test
    static const FwSizeType TEST_INSTANCE_QUEUE_DEPTH = 10;

    // KissCAM protocol bytes (datasheet §7)
    static const U8 START = 0x40;
    static const U8 END = 0x0D;
    static const U8 CMD_PING = 0x50;
    static const U8 CMD_SET_MODE = 0x4D;
    static const U8 CMD_SET_EXPOSURE = 0x45;
    static const U8 CMD_CAPTURE = 0x43;
    static const U8 CMD_DOWNLOAD = 0x44;
    static const U8 CMD_READ_REGISTER = 0x52;
    static const U8 CMD_ERROR = 0xFF;
    static const U8 FRAME_SIZE = 7;
    static const FwSizeType MAX_REPLY = 648;
    static const FwSizeType DP_STORE_SIZE = 2 * 1024 * 1024;

    // Camera mode bytes
    static const U8 MODE_SLEEP = 0;
    static const U8 MODE_SD = 1;
    static const U8 MODE_ZOOM = 2;
    static const U8 MODE_HD = 3;

    // Default parameter values
    static const U16 DEFAULT_EXPOSURE = 672;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object SunCamMgrTester
    SunCamMgrTester();

    //! Destroy object SunCamMgrTester
    ~SunCamMgrTester();

  public:
    // ----------------------------------------------------------------------
    // Tests (one per row of the SDD unit-test list)
    // ----------------------------------------------------------------------

    void testSetupNominal();
    void testSetupFailsEachStep();
    void testSetupReentrant();
    void testNoFramesBeforeSetup();
    void testNoCaptureWhenNotSetup();
    void testSleepAndPingNotSetup();
    void testBusyRejection();
    void testParameterPushIdleOnly();
    void testParameterPushFailure();
    void testExposureReadbackMismatch();
    void testCaptureReportsExposure();
    void testCommandFrameBytes();
    void testChannelsFromReplies();
    void testOutcomeEventPerCommand();
    void testDownloadFullAndPreview();
    void testDownloadHdFourSlots();
    void testDownloadPriority();
    void testDownloadLineRetryAndFail();
    void testDownloadFailedSendAndProductFull();
    void testDownloadAbortCommanded();
    void testReplyTimeoutTicks();
    void testCameraErrorReply();
    void testMalformedAndStrayFrames();
    void testPowerOnBanner();
    void testSplitAndCrPayloadReply();
    void testModeMismatch();
    void testReadRegister();
    void testModeChangeRepushesExposure();
    void testSendAndAllocationFailure();
    void testQueueFullHooks();
    void testPeriodicChannelWrite();
    void testExposureRangeCheck();
    void testRunPortSingleQueuedTick();
    void testHealthPingAndDrvConnected();

  private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports
    // ----------------------------------------------------------------------

    //! Captures the bytes of every frame the component sends (the component reuses its frame buffer)
    Drv::ByteStreamStatus from_drvSendOut_handler(FwIndexType portNum, Fw::Buffer& sendBuffer) override;

    // ----------------------------------------------------------------------
    // Handlers for data product ports
    // ----------------------------------------------------------------------

    Fw::Success::T productGet_handler(FwDpIdType id, FwSizeType dataSize, Fw::Buffer& buffer) override;

  private:
    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Dispatch exactly `count` queued messages (doDispatch blocks on an empty queue)
    void dispatch(U32 count = 1);

    //! Invoke run and dispatch the queued tick, n times
    void tick(U32 n = 1);

    //! Deliver bytes on drvReceiveIn with the given status and dispatch
    void receive(const U8* bytes, FwSizeType size, Drv::ByteStreamStatus status = Drv::ByteStreamStatus::OP_OK);

    //! Deliver a well-formed reply: @ ack mode lenHi lenLo payload... \r
    void reply(U8 ack, U8 mode, const U8* payload, U16 length);

    //! Deliver an empty-payload ACK for cmd
    void ack(U8 cmd, U8 mode = MODE_SD);

    //! Deliver an error reply for cmd
    void errorReply(U8 cmd, U8 errorType, U8 count, U8 mode = MODE_SD);

    //! Deliver a Ping reply
    void pingReply(U8 valid, U8 invalid, U16 uptime, U8 mode = MODE_SD);

    //! Deliver a Read Register reply
    void registerReply(U16 address, U16 value, U8 mode = MODE_SD);

    //! Deliver the camera's 21-byte power-on line, in one receive buffer or split after `split` bytes
    void powerOnLine(FwSizeType split = 0);

    //! Deliver a Download line reply with pixel fill value
    void lineReply(U16 line, U16 pixels, U8 fill, U8 mode = MODE_SD);

    //! Assert the last frame sent, byte for byte, and the number of frames sent so far
    void assertFrame(U8 cmd, U8 p1, U8 p2, U8 p3, U8 p4, U32 expectedCount);

    //! Drive the five nominal set-up exchanges after SETUP_CAMERA; leaves the component READY
    void setupNominal(U8 modeByte = MODE_SD, U16 exposure = DEFAULT_EXPOSURE);

    //! Send SETUP_CAMERA and dispatch it
    void sendSetup(U32 cmdSeq = 0);
    //! Fill the queue with health pings, overflow every input once, drain, tick (four QueueOverflow events)
    void overflowCycle();
    //! Data size of an Image container: one header record and one line record (slot, line, pixels) per line
    static FwSizeType imageDataSize(FwSizeType lines, FwSizeType pixels);
    //! Clear the histories, remembering the last SetupState written (the channel is written on change)
    void clearHistory();
    //! Assert the component's set-up state: the last SetupState written, or the one before the last clear
    void assertSetupState(SetupState::T expected);
    //! Set a parameter through its PRM_SET command; the parameter command response is dropped from the history
    void setExposure(U16 value);
    void setCameraMode(CaptureMode::T mode);

    //! Parse the last sent product: header record then line records; returns the number of line records
    U32 checkProduct(const KissCam::ImageHeader& expectedHeader, U16 pixels, U32 expectedLines, U8 firstSlot);

    //! Send DOWNLOAD and dispatch it; asserts the first line request (slot, resolution, line 0) left
    void startDownload(Slot slot, Resolution resolution, U32 cmdSeq = 0, FwDpPriorityType priority = 0);

    //! Download SLOT_1 PREVIEW at the commanded priority; asserts the sent product carries the expected one
    void downloadPreviewAtPriority(FwDpPriorityType priority, FwDpPriorityType expected, U32 cmdSeq);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    SunCamMgr component;

    //! Last frame bytes sent by the component
    U8 m_lastFrame[FRAME_SIZE];

    //! Size of the last frame sent
    FwSizeType m_lastFrameSize;

    //! Number of frames sent
    U32 m_frameCount;
    SetupState::T m_setupStateAtClear;

    //! Status returned to drvSendOut
    Drv::ByteStreamStatus m_sendStatus;

    //! Status returned to productGetOut
    Fw::Success::T m_productGetStatus;
    //! Bytes withheld from each product buffer handed out (0: the requested size)
    FwSizeType m_productGetShrink;

    //! Backing store for receive buffers handed to the component
    U8 m_rxStore[MAX_REPLY * 2];

    //! Backing store for data products
    U8 m_dpStore[DP_STORE_SIZE];
};

}  // namespace KissCam

#endif
