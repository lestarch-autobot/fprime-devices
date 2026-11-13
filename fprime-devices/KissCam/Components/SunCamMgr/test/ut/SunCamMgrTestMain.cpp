// ======================================================================
// \title  SunCamMgrTestMain.cpp
// \author lestarch-autobot
// \brief  cpp file for SunCamMgr component test main function
// ======================================================================

#include "SunCamMgrTester.hpp"

#define SUNCAM_TEST(name)                \
    TEST(SunCamMgr, name) {              \
        KissCam::SunCamMgrTester tester; \
        tester.name();                   \
    }

SUNCAM_TEST(testSetupNominal)
SUNCAM_TEST(testSetupFailsEachStep)
SUNCAM_TEST(testSetupReentrant)
SUNCAM_TEST(testNoFramesBeforeSetup)
SUNCAM_TEST(testNoCaptureWhenNotSetup)
SUNCAM_TEST(testSleepAndPingNotSetup)
SUNCAM_TEST(testBusyRejection)
SUNCAM_TEST(testParameterPushIdleOnly)
SUNCAM_TEST(testParameterPushFailure)
SUNCAM_TEST(testExposureReadbackMismatch)
SUNCAM_TEST(testCaptureReportsExposure)
SUNCAM_TEST(testCommandFrameBytes)
SUNCAM_TEST(testChannelsFromReplies)
SUNCAM_TEST(testOutcomeEventPerCommand)
SUNCAM_TEST(testDownloadFullAndPreview)
SUNCAM_TEST(testDownloadHdFourSlots)
SUNCAM_TEST(testDownloadPriority)
SUNCAM_TEST(testDownloadLineRetryAndFail)
SUNCAM_TEST(testDownloadFailedSendAndProductFull)
SUNCAM_TEST(testDownloadAbortCommanded)
SUNCAM_TEST(testReplyTimeoutTicks)
SUNCAM_TEST(testCameraErrorReply)
SUNCAM_TEST(testMalformedAndStrayFrames)
SUNCAM_TEST(testPowerOnBanner)
SUNCAM_TEST(testSplitAndCrPayloadReply)
SUNCAM_TEST(testModeMismatch)
SUNCAM_TEST(testReadRegister)
SUNCAM_TEST(testModeChangeRepushesExposure)
SUNCAM_TEST(testSendAndAllocationFailure)
SUNCAM_TEST(testQueueFullHooks)
SUNCAM_TEST(testPeriodicChannelWrite)
SUNCAM_TEST(testExposureRangeCheck)
SUNCAM_TEST(testRunPortSingleQueuedTick)
SUNCAM_TEST(testHealthPingAndDrvConnected)

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
