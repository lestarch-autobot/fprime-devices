// ======================================================================
// \title  SunCamMgrTester.cpp
// \author lestarch-autobot
// \brief  cpp file for SunCamMgr component test harness implementation class
// ======================================================================

#include "SunCamMgrTester.hpp"

#include <cstring>

#include "Fw/Dp/DpContainer.hpp"
#include "fprime-devices/KissCam/KissCamConfig/FppConstantsAc.hpp"

namespace KissCam {

// Definitions for the static constants that are odr-used by the gtest assertions
const U16 SunCamMgrTester::DEFAULT_EXPOSURE;
const U8 SunCamMgrTester::FRAME_SIZE;
const FwSizeType SunCamMgrTester::MAX_REPLY;

namespace {
const U16 EXPOSURE_REGISTER = 0x3012;

//! Builds the FrameHead array argument of FrameDiscarded
KissCam::FrameHead head(U8 b0, U8 b1, U8 b2, U8 b3, U8 b4, U8 b5, U8 b6, U8 b7) {
    const U8 bytes[8] = {b0, b1, b2, b3, b4, b5, b6, b7};
    return KissCam::FrameHead(bytes);
}
}  // namespace

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

SunCamMgrTester::SunCamMgrTester()
    : SunCamMgrGTestBase("SunCamMgrTester", SunCamMgrTester::MAX_HISTORY_SIZE),
      component("SunCamMgr"),
      m_lastFrameSize(0),
      m_frameCount(0),
      m_setupStateAtClear(SetupState::NOT_SETUP),
      m_sendStatus(Drv::ByteStreamStatus::OP_OK),
      m_productGetStatus(Fw::Success::SUCCESS),
      m_productGetShrink(0) {
    ::memset(this->m_lastFrame, 0, sizeof this->m_lastFrame);
    ::memset(this->m_rxStore, 0, sizeof this->m_rxStore);
    ::memset(this->m_dpStore, 0, sizeof this->m_dpStore);
    this->initComponents();
    this->connectPorts();
    this->component.loadParameters();
}

SunCamMgrTester::~SunCamMgrTester() {}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

Drv::ByteStreamStatus SunCamMgrTester::from_drvSendOut_handler(FwIndexType portNum, Fw::Buffer& sendBuffer) {
    this->pushFromPortEntry_drvSendOut(sendBuffer);
    this->m_lastFrameSize = FW_MIN(sendBuffer.getSize(), static_cast<FwSizeType>(FRAME_SIZE));
    ::memcpy(this->m_lastFrame, sendBuffer.getData(), this->m_lastFrameSize);
    this->m_frameCount++;
    return this->m_sendStatus;
}

// ----------------------------------------------------------------------
// Handlers for data product ports
// ----------------------------------------------------------------------

Fw::Success::T SunCamMgrTester::productGet_handler(FwDpIdType id, FwSizeType dataSize, Fw::Buffer& buffer) {
    this->pushProductGetEntry(id, dataSize);
    if (this->m_productGetStatus != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    // dataSize is the packet size the component asked for (DpContainer header + records + hashes)
    EXPECT_LE(dataSize, sizeof this->m_dpStore);
    if (dataSize > sizeof this->m_dpStore) {
        return Fw::Success::FAILURE;
    }
    buffer = Fw::Buffer(this->m_dpStore, dataSize - this->m_productGetShrink);
    return Fw::Success::SUCCESS;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void SunCamMgrTester::dispatch(U32 count) {
    for (U32 i = 0; i < count; i++) {
        ASSERT_EQ(this->component.doDispatch(), Fw::QueuedComponentBase::MSG_DISPATCH_OK);
    }
}

void SunCamMgrTester::tick(U32 n) {
    for (U32 i = 0; i < n; i++) {
        this->invoke_to_run(0, 0);
        this->dispatch();
    }
}

void SunCamMgrTester::receive(const U8* bytes, FwSizeType size, Drv::ByteStreamStatus status) {
    ASSERT_LE(size, sizeof this->m_rxStore);
    if (size > 0) {
        ::memcpy(this->m_rxStore, bytes, size);
    }
    Fw::Buffer buffer((size > 0) ? this->m_rxStore : nullptr, size);
    const U32 returned = static_cast<U32>(this->fromPortHistory_drvReceiveReturnOut->size());
    this->invoke_to_drvReceiveIn(0, buffer, status);
    this->dispatch();
    // Every receive buffer that carried data comes back; a null notice has nothing to return
    if (size > 0) {
        ASSERT_EQ(this->fromPortHistory_drvReceiveReturnOut->size(), returned + 1);
        ASSERT_EQ(this->fromPortHistory_drvReceiveReturnOut->at(returned).fwBuffer.getData(), this->m_rxStore);
    } else {
        ASSERT_EQ(this->fromPortHistory_drvReceiveReturnOut->size(), returned);
    }
}

void SunCamMgrTester::reply(U8 ack, U8 mode, const U8* payload, U16 length) {
    U8 frame[MAX_REPLY];
    ASSERT_LE(static_cast<FwSizeType>(length) + 6, sizeof frame);
    frame[0] = START;
    frame[1] = ack;
    frame[2] = mode;
    frame[3] = static_cast<U8>(length >> 8);
    frame[4] = static_cast<U8>(length & 0xFF);
    if (length > 0) {
        ::memcpy(&frame[5], payload, length);
    }
    frame[5 + length] = END;
    this->receive(frame, static_cast<FwSizeType>(length) + 6);
}

void SunCamMgrTester::ack(U8 cmd, U8 mode) {
    this->reply(cmd, mode, nullptr, 0);
}

void SunCamMgrTester::errorReply(U8 cmd, U8 errorType, U8 count, U8 mode) {
    const U8 payload[3] = {cmd, errorType, count};
    this->reply(CMD_ERROR, mode, payload, sizeof payload);
}

void SunCamMgrTester::pingReply(U8 valid, U8 invalid, U16 uptime, U8 mode) {
    const U8 payload[4] = {valid, invalid, static_cast<U8>(uptime >> 8), static_cast<U8>(uptime & 0xFF)};
    this->reply(CMD_PING, mode, payload, sizeof payload);
}

void SunCamMgrTester::registerReply(U16 address, U16 value, U8 mode) {
    const U8 payload[4] = {static_cast<U8>(address >> 8), static_cast<U8>(address & 0xFF), static_cast<U8>(value >> 8),
                           static_cast<U8>(value & 0xFF)};
    this->reply(CMD_READ_REGISTER, mode, payload, sizeof payload);
}

void SunCamMgrTester::powerOnLine(FwSizeType split) {
    ASSERT_LE(split, SunCamMgr::POWER_ON_LINE_SIZE);
    if (split > 0) {
        this->receive(SunCamMgr::POWER_ON_LINE, split, Drv::ByteStreamStatus::OP_OK);
        this->receive(&SunCamMgr::POWER_ON_LINE[split], SunCamMgr::POWER_ON_LINE_SIZE - split,
                      Drv::ByteStreamStatus::OP_OK);
    } else {
        this->receive(SunCamMgr::POWER_ON_LINE, SunCamMgr::POWER_ON_LINE_SIZE, Drv::ByteStreamStatus::OP_OK);
    }
}

void SunCamMgrTester::lineReply(U16 line, U16 pixels, U8 fill, U8 mode) {
    U8 payload[2 + FULL_PIXELS];
    ASSERT_LE(pixels, FULL_PIXELS);
    payload[0] = static_cast<U8>(line >> 8);
    payload[1] = static_cast<U8>(line & 0xFF);
    ::memset(&payload[2], fill, pixels);
    this->reply(CMD_DOWNLOAD, mode, payload, static_cast<U16>(pixels + 2));
}

void SunCamMgrTester::assertFrame(U8 cmd, U8 p1, U8 p2, U8 p3, U8 p4, U32 expectedCount) {
    const U8 expected[FRAME_SIZE] = {START, cmd, p1, p2, p3, p4, END};
    ASSERT_EQ(this->m_frameCount, expectedCount);
    ASSERT_EQ(this->m_lastFrameSize, static_cast<FwSizeType>(FRAME_SIZE));
    for (U32 i = 0; i < FRAME_SIZE; i++) {
        ASSERT_EQ(this->m_lastFrame[i], expected[i]) << "frame byte " << i;
    }
}

void SunCamMgrTester::setExposure(U16 value) {
    this->paramSet_EXPOSURE(value, Fw::ParamValid::VALID);
    this->paramSend_EXPOSURE(0, 0);
    ASSERT_EQ(this->cmdResponseHistory->at(this->cmdResponseHistory->size() - 1).response,
              Fw::CmdResponse(Fw::CmdResponse::OK));
    this->cmdResponseHistory->clear();
}

void SunCamMgrTester::setCameraMode(CaptureMode::T mode) {
    this->paramSet_CAMERA_MODE(mode, Fw::ParamValid::VALID);
    this->paramSend_CAMERA_MODE(0, 0);
    ASSERT_EQ(this->cmdResponseHistory->at(this->cmdResponseHistory->size() - 1).response,
              Fw::CmdResponse(Fw::CmdResponse::OK));
    this->cmdResponseHistory->clear();
}

void SunCamMgrTester::clearHistory() {
    if (this->tlmHistory_SetupState->size() > 0) {
        this->m_setupStateAtClear = this->tlmHistory_SetupState->at(this->tlmHistory_SetupState->size() - 1).arg.e;
    }
    SunCamMgrGTestBase::clearHistory();
}

void SunCamMgrTester::assertSetupState(SetupState::T expected) {
    if (this->tlmHistory_SetupState->size() == 0) {
        ASSERT_EQ(this->m_setupStateAtClear, expected);
    } else {
        ASSERT_EQ(this->tlmHistory_SetupState->at(this->tlmHistory_SetupState->size() - 1).arg, SetupState(expected));
    }
}

FwSizeType SunCamMgrTester::imageDataSize(FwSizeType lines, FwSizeType pixels) {
    return SunCamMgr::SIZE_OF_ImageHeader_RECORD + lines * SunCamMgr::SIZE_OF_ImageLine_RECORD(pixels + 3);
}

void SunCamMgrTester::overflowCycle() {
    for (U32 i = 0; i < TEST_INSTANCE_QUEUE_DEPTH; i++) {
        this->invoke_to_pingIn(0, i);
    }
    this->sendCmd_SLEEP(0, 300);
    const U8 bytes[1] = {0x55};
    ::memcpy(this->m_rxStore, bytes, sizeof bytes);
    Fw::Buffer buffer(this->m_rxStore, sizeof bytes);
    this->invoke_to_drvReceiveIn(0, buffer, Drv::ByteStreamStatus::OP_OK);
    this->invoke_to_run(0, 0);
    this->invoke_to_pingIn(0, 99);
    this->dispatch(TEST_INSTANCE_QUEUE_DEPTH);
    this->tick();
}

void SunCamMgrTester::sendSetup(U32 cmdSeq) {
    this->sendCmd_SETUP_CAMERA(0, cmdSeq);
    this->dispatch();
}

void SunCamMgrTester::setupNominal(U8 modeByte, U16 exposure) {
    const U32 base = this->m_frameCount;
    this->sendSetup();
    this->assertFrame(CMD_PING, 0, 0, 0, 0, base + 1);
    this->pingReply(1, 0, 5, MODE_SLEEP);
    this->assertFrame(CMD_SET_MODE, modeByte, 0, 0, 0, base + 2);
    this->ack(CMD_SET_MODE, modeByte);
    this->assertFrame(CMD_SET_EXPOSURE, static_cast<U8>(exposure >> 8), static_cast<U8>(exposure & 0xFF), 0, 0,
                      base + 3);
    this->ack(CMD_SET_EXPOSURE, modeByte);
    this->assertFrame(CMD_READ_REGISTER, 0x30, 0x12, 0, 0, base + 4);
    this->registerReply(EXPOSURE_REGISTER, exposure, modeByte);
    this->assertFrame(CMD_PING, 1, 0, 0, 0, base + 5);
    this->pingReply(5, 0, 6, modeByte);
    ASSERT_EQ(this->tlmHistory_SetupState->at(this->tlmHistory_SetupState->size() - 1).arg, SetupState::READY);
    this->clearHistory();
}

void SunCamMgrTester::startDownload(Slot slot, Resolution resolution, U32 cmdSeq, FwDpPriorityType priority) {
    const U32 base = this->m_frameCount;
    this->sendCmd_DOWNLOAD(0, cmdSeq, slot, resolution, priority);
    this->dispatch();
    this->assertFrame(CMD_DOWNLOAD, static_cast<U8>(slot.e), static_cast<U8>(resolution.e), 0, 0, base + 1);
}

void SunCamMgrTester::downloadPreviewAtPriority(FwDpPriorityType priority, FwDpPriorityType expected, U32 cmdSeq) {
    this->clearHistory();
    this->m_frameCount = 0;
    this->startDownload(Slot::SLOT_1, Resolution::PREVIEW, cmdSeq, priority);
    for (U16 line = 0; line < PREVIEW_LINES; line++) {
        this->lineReply(line, PREVIEW_PIXELS, static_cast<U8>(line));
    }
    ASSERT_EQ(this->productSendHistory->size(), 1u);
    Fw::DpContainer container(0, this->productSendHistory->at(0).buffer);
    ASSERT_EQ(container.deserializeHeader(), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(container.getPriority(), expected);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD, cmdSeq, Fw::CmdResponse::OK);
}

U32 SunCamMgrTester::checkProduct(const KissCam::ImageHeader& expectedHeader,
                                  U16 pixels,
                                  U32 expectedLines,
                                  U8 firstSlot) {
    EXPECT_EQ(this->productSendHistory->size(), 1u);
    if (this->productSendHistory->size() != 1) {
        return 0;
    }
    const Fw::Buffer buffer = this->productSendHistory->at(0).buffer;
    EXPECT_EQ(this->productSendHistory->at(0).id, static_cast<FwDpIdType>(0));
    Fw::DpContainer container(0, buffer);
    EXPECT_EQ(container.deserializeHeader(), Fw::FW_SERIALIZE_OK);
    EXPECT_EQ(container.getPriority(), static_cast<FwDpPriorityType>(KissCamConfig::IMAGE_DP_PRIORITY));
    const FwSizeType dataSize = container.getDataSize();
    const FwSizeType lineRecord = SunCamMgr::SIZE_OF_ImageLine_RECORD(static_cast<FwSizeType>(pixels) + 3);
    EXPECT_EQ(dataSize, SunCamMgr::SIZE_OF_ImageHeader_RECORD + expectedLines * lineRecord);
    Fw::ExternalSerializeBuffer data(&buffer.getData()[Fw::DpContainer::DATA_OFFSET], dataSize);
    EXPECT_EQ(data.setBuffLen(dataSize), Fw::FW_SERIALIZE_OK);
    FwDpIdType id = 0;
    EXPECT_EQ(data.deserializeTo(id), Fw::FW_SERIALIZE_OK);
    EXPECT_EQ(id, static_cast<FwDpIdType>(0));
    KissCam::ImageHeader header;
    EXPECT_EQ(data.deserializeTo(header), Fw::FW_SERIALIZE_OK);
    EXPECT_EQ(header, expectedHeader);
    U32 lines = 0;
    const U16 linesPerSlot = (pixels == FULL_PIXELS) ? static_cast<U16>(FULL_LINES) : static_cast<U16>(PREVIEW_LINES);
    while ((data.getDeserializeSizeLeft() > 0) && (lines < expectedLines)) {
        EXPECT_EQ(data.deserializeTo(id), Fw::FW_SERIALIZE_OK);
        EXPECT_EQ(id, static_cast<FwDpIdType>(1));
        FwSizeStoreType size = 0;
        EXPECT_EQ(data.deserializeTo(size), Fw::FW_SERIALIZE_OK);
        EXPECT_EQ(size, static_cast<FwSizeStoreType>(pixels + 3));
        const U8* record = data.getBuffAddrLeft();
        const U8 expectedSlot = static_cast<U8>(firstSlot + lines / linesPerSlot);
        const U16 expectedLine = static_cast<U16>(lines % linesPerSlot);
        EXPECT_EQ(record[0], expectedSlot) << "line record " << lines;
        EXPECT_EQ(record[1], static_cast<U8>(expectedLine >> 8)) << "line record " << lines;
        EXPECT_EQ(record[2], static_cast<U8>(expectedLine & 0xFF)) << "line record " << lines;
        EXPECT_EQ(record[3], static_cast<U8>(expectedLine & 0xFF)) << "pixel fill of line record " << lines;
        EXPECT_EQ(data.moveDeserToOffset(dataSize - data.getDeserializeSizeLeft() + size), Fw::FW_SERIALIZE_OK);
        lines++;
    }
    EXPECT_EQ(data.getDeserializeSizeLeft(), 0u);
    return lines;
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void SunCamMgrTester::testSetupNominal() {
    this->tick();  // first tick: initial state channels
    ASSERT_TLM_SetupState(0, SetupState::NOT_SETUP);
    ASSERT_TLM_CurrentOperation(0, Operation::NONE);
    this->clearHistory();

    this->sendSetup(3);
    ASSERT_EVENTS_SetupStarted_SIZE(1);
    ASSERT_TLM_SetupState(0, SetupState::SETTING_UP);
    ASSERT_TLM_CurrentOperation(0, Operation::SETUP_CAMERA);
    ASSERT_CMD_RESPONSE_SIZE(0);
    this->assertFrame(CMD_PING, 0, 0, 0, 0, 1);
    this->pingReply(1, 0, 5, MODE_SLEEP);
    ASSERT_EVENTS_PingReply_SIZE(0);  // set-up ping is not reported as a PING command outcome
    this->assertFrame(CMD_SET_MODE, MODE_SD, 0, 0, 0, 2);
    this->ack(CMD_SET_MODE, MODE_SD);
    ASSERT_EVENTS_SettingApplied_SIZE(1);
    ASSERT_EVENTS_SettingApplied(0, Setting::CAMERA_MODE, 1);
    this->assertFrame(CMD_SET_EXPOSURE, 0x02, 0xA0, 0, 0, 3);
    this->ack(CMD_SET_EXPOSURE, MODE_SD);
    ASSERT_EVENTS_SettingApplied_SIZE(2);
    ASSERT_EVENTS_SettingApplied(1, Setting::EXPOSURE, DEFAULT_EXPOSURE);
    this->assertFrame(CMD_READ_REGISTER, 0x30, 0x12, 0, 0, 4);
    this->registerReply(EXPOSURE_REGISTER, DEFAULT_EXPOSURE, MODE_SD);
    ASSERT_EVENTS_ExposureReadback_SIZE(1);
    ASSERT_EVENTS_ExposureReadback(0, DEFAULT_EXPOSURE, DEFAULT_EXPOSURE);
    ASSERT_CMD_RESPONSE_SIZE(0);
    this->assertFrame(CMD_PING, 1, 0, 0, 0, 5);
    this->pingReply(5, 0, 6, MODE_SD);

    ASSERT_EVENTS_SetupComplete_SIZE(1);
    ASSERT_EVENTS_SetupComplete(0, CaptureMode::SD, DEFAULT_EXPOSURE);
    ASSERT_EVENTS_SetupFailed_SIZE(0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_SETUP_CAMERA, 3, Fw::CmdResponse::OK);
    this->assertSetupState(SetupState::READY);
    ASSERT_EQ(this->tlmHistory_CurrentOperation->at(this->tlmHistory_CurrentOperation->size() - 1).arg,
              Operation::NONE);
    ASSERT_EQ(this->tlmHistory_CameraMode->at(this->tlmHistory_CameraMode->size() - 1).arg, Mode::SD);
    ASSERT_EQ(this->tlmHistory_Exposure->at(this->tlmHistory_Exposure->size() - 1).arg, DEFAULT_EXPOSURE);
    ASSERT_TLM_ExposureReadback(0, DEFAULT_EXPOSURE);
    ASSERT_EQ(this->tlmHistory_Exchanges->at(this->tlmHistory_Exchanges->size() - 1).arg, 5u);
    ASSERT_EQ(this->m_frameCount, 5u);
    // SetupComplete reports the mode that was pushed, not the parameter value at completion
    this->clearHistory();
    this->sendSetup(7);
    this->pingReply(1, 0, 5, MODE_SLEEP);
    this->ack(CMD_SET_MODE);
    this->setCameraMode(CaptureMode::HD);
    this->ack(CMD_SET_EXPOSURE);
    this->registerReply(EXPOSURE_REGISTER, DEFAULT_EXPOSURE);
    this->pingReply(5, 0, 6);
    ASSERT_EVENTS_SetupComplete_SIZE(1);
    ASSERT_EVENTS_SetupComplete(0, CaptureMode::SD, DEFAULT_EXPOSURE);
}

void SunCamMgrTester::testSetupFailsEachStep() {
    const U8 cmds[5] = {CMD_PING, CMD_SET_MODE, CMD_SET_EXPOSURE, CMD_READ_REGISTER, CMD_PING};
    const SetupStep::T steps[5] = {SetupStep::PING_MCU, SetupStep::SET_MODE, SetupStep::SET_EXPOSURE,
                                   SetupStep::READ_EXPOSURE, SetupStep::PING_SENSOR};
    for (U32 failAt = 0; failAt < 5; failAt++) {
        for (U32 byTimeout = 0; byTimeout < 2; byTimeout++) {
            this->clearHistory();
            this->m_frameCount = 0;
            this->sendSetup(failAt);
            for (U32 step = 0; step < failAt; step++) {
                switch (cmds[step]) {
                    case CMD_PING:
                        this->pingReply(1, 0, 1, (step == 0) ? MODE_SLEEP : MODE_SD);
                        break;
                    case CMD_READ_REGISTER:
                        this->registerReply(EXPOSURE_REGISTER, DEFAULT_EXPOSURE);
                        break;
                    default:
                        this->ack(cmds[step]);
                        break;
                }
            }
            ASSERT_EQ(this->m_frameCount, failAt + 1);
            ASSERT_EQ(this->m_lastFrame[1], cmds[failAt]);
            if (byTimeout == 1) {
                this->tick(KissCamConfig::REPLY_TIMEOUT_TICKS);
                ASSERT_EVENTS_ReplyTimeout_SIZE(1);
                ASSERT_EVENTS_ReplyTimeout(0, Operation::SETUP_CAMERA, cmds[failAt]);
                ASSERT_EVENTS_SetupFailed(0, steps[failAt], FailCause::TIMEOUT);
            } else {
                this->errorReply(cmds[failAt], 0x03, 7);
                ASSERT_EVENTS_CameraError_SIZE(1);
                ASSERT_EVENTS_CameraError(0, Operation::SETUP_CAMERA, cmds[failAt], ErrorType::WRONG_MODE, 7);
                ASSERT_EVENTS_SetupFailed(0, steps[failAt], FailCause::CAMERA_ERROR);
            }
            ASSERT_EVENTS_SetupFailed_SIZE(1);
            ASSERT_EVENTS_SetupComplete_SIZE(0);
            ASSERT_CMD_RESPONSE_SIZE(1);
            ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_SETUP_CAMERA, failAt, Fw::CmdResponse::EXECUTION_ERROR);
            ASSERT_EQ(this->tlmHistory_SetupState->at(this->tlmHistory_SetupState->size() - 1).arg,
                      SetupState::NOT_SETUP);
            // no further frame after the failure
            ASSERT_EQ(this->m_frameCount, failAt + 1);
        }
    }
    // Out-of-range persisted EXPOSURE fails SET_EXPOSURE locally before any Set Exposure frame
    this->clearHistory();
    this->m_frameCount = 0;
    this->setExposure(1);
    this->sendSetup(9);
    this->pingReply(1, 0, 1, MODE_SLEEP);
    this->ack(CMD_SET_MODE);
    ASSERT_EQ(this->m_frameCount, 2u);
    ASSERT_EVENTS_SettingRejected_SIZE(1);
    ASSERT_EVENTS_SettingRejected(0, Setting::EXPOSURE, 1, KissCamConfig::EXPOSURE_MIN, KissCamConfig::EXPOSURE_MAX);
    ASSERT_EVENTS_SetupFailed_SIZE(1);
    ASSERT_EVENTS_SetupFailed(0, SetupStep::SET_EXPOSURE, FailCause::OUT_OF_RANGE);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_SETUP_CAMERA, 9, Fw::CmdResponse::EXECUTION_ERROR);
    this->assertSetupState(SetupState::NOT_SETUP);
}

void SunCamMgrTester::testSetupReentrant() {
    // after a failed set-up
    this->sendSetup();
    this->tick(KissCamConfig::REPLY_TIMEOUT_TICKS);
    ASSERT_EVENTS_SetupFailed_SIZE(1);
    this->clearHistory();
    this->setupNominal();
    ASSERT_EQ(this->m_frameCount, 6u);
    // while READY: the full sequence again
    this->setupNominal();
    ASSERT_EQ(this->m_frameCount, 11u);
}

void SunCamMgrTester::testNoFramesBeforeSetup() {
    this->tick(3);
    this->invoke_to_drvConnected(0);
    this->dispatch();
    this->setExposure(10);
    this->tick(3);
    ASSERT_EQ(this->m_frameCount, 0u);

    this->sendCmd_CAPTURE(0, 2, Slot::SLOT_1, TestPattern::OFF);
    this->sendCmd_DOWNLOAD(0, 3, Slot::SLOT_1, Resolution::FULL, 0);
    this->sendCmd_READ_REGISTER(0, 4, 0x3012);
    this->dispatch(3);
    ASSERT_EQ(this->m_frameCount, 0u);
    ASSERT_EVENTS_CommandRejected_SIZE(3);
    ASSERT_EVENTS_CommandRejected(0, Operation::CAPTURE, RejectCause::NOT_READY);
    ASSERT_EVENTS_CommandRejected(1, Operation::DOWNLOAD, RejectCause::NOT_READY);
    ASSERT_EVENTS_CommandRejected(2, Operation::READ_REGISTER, RejectCause::NOT_READY);
    ASSERT_CMD_RESPONSE_SIZE(3);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_CAPTURE, 2, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_CMD_RESPONSE(1, SunCamMgr::OPCODE_DOWNLOAD, 3, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_CMD_RESPONSE(2, SunCamMgr::OPCODE_READ_REGISTER, 4, Fw::CmdResponse::EXECUTION_ERROR);
}

void SunCamMgrTester::testSleepAndPingNotSetup() {
    this->sendCmd_PING(0, 1, PingType::MCU_AND_SENSOR);
    this->dispatch();
    this->assertFrame(CMD_PING, 1, 0, 0, 0, 1);
    this->pingReply(3, 1, 77, MODE_SLEEP);
    ASSERT_EVENTS_PingReply_SIZE(1);
    ASSERT_EVENTS_PingReply(0, PingType::MCU_AND_SENSOR, 3, 1, 77);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_PING, 1, Fw::CmdResponse::OK);

    // SLEEP success from NOT_SETUP
    this->sendCmd_SLEEP(0, 2);
    this->dispatch();
    this->assertFrame(CMD_SET_MODE, MODE_SLEEP, 0, 0, 0, 2);
    this->ack(CMD_SET_MODE, MODE_SLEEP);
    ASSERT_EVENTS_SleepEntered_SIZE(1);
    ASSERT_CMD_RESPONSE(1, SunCamMgr::OPCODE_SLEEP, 2, Fw::CmdResponse::OK);
    this->assertSetupState(SetupState::NOT_SETUP);

    // SLEEP from READY, success: NOT_SETUP
    this->setupNominal();
    this->sendCmd_SLEEP(0, 3);
    this->dispatch();
    this->ack(CMD_SET_MODE, MODE_SLEEP);
    ASSERT_EVENTS_SleepEntered_SIZE(1);
    ASSERT_EVENTS_ModeMismatch_SIZE(0);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_SLEEP, 3, Fw::CmdResponse::OK);
    this->assertSetupState(SetupState::NOT_SETUP);

    // SLEEP from READY, failure: still NOT_SETUP
    this->setupNominal();
    this->sendCmd_SLEEP(0, 4);
    this->dispatch();
    this->errorReply(CMD_SET_MODE, 0x04, 1);
    ASSERT_EVENTS_SleepEntered_SIZE(0);
    ASSERT_EVENTS_CameraError_SIZE(1);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_SLEEP, 4, Fw::CmdResponse::EXECUTION_ERROR);
    this->assertSetupState(SetupState::NOT_SETUP);
}

void SunCamMgrTester::testBusyRejection() {
    this->setupNominal();
    this->sendCmd_READ_REGISTER(0, 1, 0x3012);
    this->dispatch();
    ASSERT_EQ(this->m_frameCount, 6u);

    this->sendCmd_SETUP_CAMERA(0, 10);
    this->sendCmd_SLEEP(0, 11);
    this->sendCmd_PING(0, 12, PingType::MCU);
    this->sendCmd_CAPTURE(0, 13, Slot::SLOT_2, TestPattern::ON);
    this->sendCmd_DOWNLOAD(0, 14, Slot::SLOT_1, Resolution::FULL, 0);
    this->sendCmd_READ_REGISTER(0, 15, 0x3012);
    this->dispatch(6);
    ASSERT_EVENTS_CommandRejected_SIZE(6);
    const Operation::T ops[6] = {Operation::SETUP_CAMERA, Operation::SLEEP,    Operation::PING,
                                 Operation::CAPTURE,      Operation::DOWNLOAD, Operation::READ_REGISTER};
    const FwOpcodeType opcodes[6] = {SunCamMgr::OPCODE_SETUP_CAMERA, SunCamMgr::OPCODE_SLEEP,
                                     SunCamMgr::OPCODE_PING,         SunCamMgr::OPCODE_CAPTURE,
                                     SunCamMgr::OPCODE_DOWNLOAD,     SunCamMgr::OPCODE_READ_REGISTER};
    for (U32 i = 0; i < 6; i++) {
        ASSERT_EVENTS_CommandRejected(i, ops[i], RejectCause::BUSY);
        ASSERT_CMD_RESPONSE(i, opcodes[i], 10 + i, Fw::CmdResponse::BUSY);
    }
    // DOWNLOAD_ABORT during a non-download operation: NO_DOWNLOAD, not BUSY
    this->sendCmd_DOWNLOAD_ABORT(0, 16);
    this->dispatch();
    ASSERT_EVENTS_CommandRejected_SIZE(7);
    ASSERT_EVENTS_CommandRejected(6, Operation::DOWNLOAD_ABORT, RejectCause::NO_DOWNLOAD);
    ASSERT_CMD_RESPONSE(6, SunCamMgr::OPCODE_DOWNLOAD_ABORT, 16, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EQ(this->m_frameCount, 6u);
    // the operation in progress is unaffected
    this->registerReply(0x3012, DEFAULT_EXPOSURE);
    ASSERT_CMD_RESPONSE(7, SunCamMgr::OPCODE_READ_REGISTER, 1, Fw::CmdResponse::OK);
}

void SunCamMgrTester::testParameterPushIdleOnly() {
    // changed while NOT_SETUP: never pushed on its own, applied by SETUP_CAMERA
    this->setExposure(100);
    this->tick(3);
    ASSERT_EQ(this->m_frameCount, 0u);
    this->setupNominal(MODE_SD, 100);
    this->tick(3);
    ASSERT_EQ(this->m_frameCount, 5u);

    // changed during an operation: deferred to the next idle tick
    this->sendCmd_PING(0, 1, PingType::MCU);
    this->dispatch();
    this->setExposure(200);
    this->tick(2);
    ASSERT_EQ(this->m_frameCount, 6u);
    this->pingReply(1, 0, 1);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_PING, 1, Fw::CmdResponse::OK);
    ASSERT_EQ(this->m_frameCount, 6u);
    this->tick();
    this->assertFrame(CMD_SET_EXPOSURE, 0, 200, 0, 0, 7);
    ASSERT_EQ(this->tlmHistory_CurrentOperation->at(this->tlmHistory_CurrentOperation->size() - 1).arg,
              Operation::PARAMETER_PUSH);
    this->ack(CMD_SET_EXPOSURE);
    ASSERT_EVENTS_SettingApplied(0, Setting::EXPOSURE, 200);
    this->assertFrame(CMD_READ_REGISTER, 0x30, 0x12, 0, 0, 8);
    this->registerReply(EXPOSURE_REGISTER, 200);
    ASSERT_EVENTS_ExposureReadback(0, 200, 200);
    ASSERT_EQ(this->tlmHistory_CurrentOperation->at(this->tlmHistory_CurrentOperation->size() - 1).arg,
              Operation::NONE);
    this->assertSetupState(SetupState::READY);
    this->tick(3);
    ASSERT_EQ(this->m_frameCount, 8u);
}

void SunCamMgrTester::testParameterPushFailure() {
    // timeout
    this->setupNominal();
    this->setExposure(300);
    this->tick();
    this->assertFrame(CMD_SET_EXPOSURE, 0x01, 0x2C, 0, 0, 6);
    this->tick(KissCamConfig::REPLY_TIMEOUT_TICKS);
    ASSERT_EVENTS_ReplyTimeout(0, Operation::PARAMETER_PUSH, CMD_SET_EXPOSURE);
    ASSERT_EVENTS_SettingPushFailed_SIZE(1);
    ASSERT_EVENTS_SettingPushFailed(0, Setting::EXPOSURE, FailCause::TIMEOUT);
    this->assertSetupState(SetupState::NOT_SETUP);
    this->tick(3);
    ASSERT_EQ(this->m_frameCount, 6u);

    // camera error on the mode push
    this->setupNominal(MODE_SD, 300);
    this->setCameraMode(CaptureMode::HD);
    this->tick();
    this->assertFrame(CMD_SET_MODE, MODE_HD, 0, 0, 0, 12);
    this->errorReply(CMD_SET_MODE, 0x04, 2, MODE_SD);
    ASSERT_EVENTS_CameraError(0, Operation::PARAMETER_PUSH, CMD_SET_MODE, ErrorType::SENSOR_PROBLEM, 2);
    ASSERT_EVENTS_SettingPushFailed_SIZE(1);
    ASSERT_EVENTS_SettingPushFailed(0, Setting::CAMERA_MODE, FailCause::CAMERA_ERROR);
    this->assertSetupState(SetupState::NOT_SETUP);
    ASSERT_CMD_RESPONSE_SIZE(0);
}

void SunCamMgrTester::testExposureReadbackMismatch() {
    // at set-up
    this->sendSetup(1);
    this->pingReply(1, 0, 1, MODE_SLEEP);
    this->ack(CMD_SET_MODE);
    this->ack(CMD_SET_EXPOSURE);
    this->registerReply(EXPOSURE_REGISTER, 0x02A1);
    ASSERT_EVENTS_ExposureReadback_SIZE(1);
    ASSERT_EVENTS_ExposureReadback(0, DEFAULT_EXPOSURE, 0x02A1);
    ASSERT_TLM_ExposureReadback(0, 0x02A1);
    ASSERT_EVENTS_SetupFailed(0, SetupStep::READ_EXPOSURE, FailCause::EXPOSURE_MISMATCH);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_SETUP_CAMERA, 1, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EQ(this->m_frameCount, 4u);
    this->assertSetupState(SetupState::NOT_SETUP);
    // on a push
    this->setupNominal();
    this->setExposure(50);
    this->tick();
    this->ack(CMD_SET_EXPOSURE);
    this->registerReply(EXPOSURE_REGISTER, 49);
    ASSERT_EVENTS_ExposureReadback(0, 50, 49);
    ASSERT_EVENTS_SettingPushFailed(0, Setting::EXPOSURE, FailCause::EXPOSURE_MISMATCH);
    this->assertSetupState(SetupState::NOT_SETUP);
}

void SunCamMgrTester::testCaptureReportsExposure() {
    this->setupNominal();
    this->sendCmd_CAPTURE(0, 1, Slot::SLOT_3, TestPattern::ON);
    this->dispatch();
    this->assertFrame(CMD_CAPTURE, 3, 1, 0, 0, 6);
    this->ack(CMD_CAPTURE);  // the Capture ACK carries no payload
    ASSERT_EVENTS_CaptureComplete_SIZE(1);
    ASSERT_EVENTS_CaptureComplete(0, Slot::SLOT_3, TestPattern::ON, DEFAULT_EXPOSURE, Mode::SD);
    ASSERT_TLM_ImagesCaptured(0, 1);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_CAPTURE, 1, Fw::CmdResponse::OK);
    // the exposure reported is the Exposure channel value: the last the camera acknowledged
    this->clearHistory();
    this->setExposure(10);
    this->tick();
    this->ack(CMD_SET_EXPOSURE);
    this->registerReply(EXPOSURE_REGISTER, 10);
    ASSERT_TLM_Exposure(0, 10);
    this->clearHistory();
    // an ACK with a 2-byte payload is malformed: no CaptureComplete, the exchange times out
    this->sendCmd_CAPTURE(0, 2, Slot::SLOT_0, TestPattern::OFF);
    this->dispatch();
    const U8 payload[2] = {0x00, 0x0A};
    this->reply(CMD_CAPTURE, MODE_SD, payload, sizeof payload);
    ASSERT_EVENTS_FrameDiscarded_SIZE(1);
    ASSERT_EQ(this->eventHistory_FrameDiscarded->at(0).cause, DiscardCause::MALFORMED);
    ASSERT_EVENTS_CaptureComplete_SIZE(0);
    ASSERT_CMD_RESPONSE_SIZE(0);
    this->tick(KissCamConfig::REPLY_TIMEOUT_TICKS);
    ASSERT_EVENTS_ReplyTimeout_SIZE(1);
    ASSERT_EVENTS_ReplyTimeout(0, Operation::CAPTURE, CMD_CAPTURE);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_CAPTURE, 2, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CaptureComplete_SIZE(0);
    ASSERT_EQ(this->tlmHistory_ImagesCaptured->at(this->tlmHistory_ImagesCaptured->size() - 1).arg, 1u);
    this->clearHistory();
    this->sendCmd_CAPTURE(0, 3, Slot::SLOT_0, TestPattern::OFF);
    this->dispatch();
    this->ack(CMD_CAPTURE);
    ASSERT_EVENTS_CaptureComplete(0, Slot::SLOT_0, TestPattern::OFF, 10, Mode::SD);
    ASSERT_TLM_ImagesCaptured(0, 2);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_CAPTURE, 3, Fw::CmdResponse::OK);
}

void SunCamMgrTester::testCommandFrameBytes() {
    this->sendCmd_PING(0, 0, PingType::MCU);
    this->dispatch();
    this->assertFrame(CMD_PING, 0x00, 0, 0, 0, 1);
    this->pingReply(1, 0, 1, MODE_SLEEP);
    this->sendCmd_SLEEP(0, 0);
    this->dispatch();
    this->assertFrame(CMD_SET_MODE, 0x00, 0, 0, 0, 2);
    this->ack(CMD_SET_MODE, MODE_SLEEP);
    this->setCameraMode(CaptureMode::HD);
    this->setExposure(0x1234 & 0x03FF);   // 0x0234 = 564
    this->setupNominal(MODE_HD, 0x0234);  // Set Mode 03, Set Exposure 02 34, Read 30 12, Ping 01
    this->sendCmd_CAPTURE(0, 0, Slot::SLOT_0, TestPattern::OFF);
    this->dispatch();
    this->assertFrame(CMD_CAPTURE, 0x00, 0x00, 0, 0, 8);
    this->ack(CMD_CAPTURE, MODE_HD);
    this->sendCmd_READ_REGISTER(0, 0, 0xBEEF);
    this->dispatch();
    this->assertFrame(CMD_READ_REGISTER, 0xBE, 0xEF, 0, 0, 9);
    this->registerReply(0xBEEF, 1, MODE_HD);
    // HD preview download: slot byte 0..3, resolution byte 1, line number 16-bit MSB first
    this->sendCmd_DOWNLOAD(0, 0, Slot::SLOT_0, Resolution::PREVIEW, 0);
    this->dispatch();
    this->assertFrame(CMD_DOWNLOAD, 0x00, 0x01, 0x00, 0x00, 10);
    this->lineReply(0, PREVIEW_PIXELS, 0, MODE_HD);
    this->assertFrame(CMD_DOWNLOAD, 0x00, 0x01, 0x00, 0x01, 11);
    for (U16 line = 1; line < PREVIEW_LINES - 1; line++) {
        this->lineReply(line, PREVIEW_PIXELS, static_cast<U8>(line), MODE_HD);
    }
    this->assertFrame(CMD_DOWNLOAD, 0x00, 0x01, 0x00, PREVIEW_LINES - 1, 10 + PREVIEW_LINES - 1);
    this->lineReply(PREVIEW_LINES - 1, PREVIEW_PIXELS, 0, MODE_HD);
    this->assertFrame(CMD_DOWNLOAD, 0x01, 0x01, 0x00, 0x00, 10 + PREVIEW_LINES);  // slot 1 follows slot 0
    this->sendCmd_DOWNLOAD_ABORT(0, 0);
    this->dispatch();
    this->lineReply(0, PREVIEW_PIXELS, 0, MODE_HD);
    ASSERT_EVENTS_DownloadAborted_SIZE(1);
    // a full-resolution line request past 255 carries the MSB
    this->sendCmd_DOWNLOAD(0, 0, Slot::SLOT_0, Resolution::FULL, 0);
    this->dispatch();
    for (U16 line = 0; line < 0x0100; line++) {
        this->lineReply(line, FULL_PIXELS, static_cast<U8>(line), MODE_HD);
    }
    this->assertFrame(CMD_DOWNLOAD, 0x00, 0x00, 0x01, 0x00, 10 + PREVIEW_LINES + 1 + 0x0100);
}

void SunCamMgrTester::testChannelsFromReplies() {
    this->setupNominal(MODE_SD);
    this->sendCmd_PING(0, 0, PingType::MCU);
    this->dispatch();
    this->pingReply(12, 3, 0xABCD, MODE_SD);
    ASSERT_TLM_CameraUptime(0, 0xABCD);
    ASSERT_TLM_CameraValidCmds(0, 12);
    ASSERT_TLM_CameraInvalidCmds(0, 3);
    ASSERT_TLM_CameraMode(0, Mode::SD);

    this->sendCmd_READ_REGISTER(0, 0, 0x3012);
    this->dispatch();
    this->registerReply(0x3012, 0x0123);
    ASSERT_TLM_RegisterValue_SIZE(1);
    ASSERT_TLM_RegisterValue(0, RegisterValue(0x3012, 0x0123));

    this->sendCmd_READ_REGISTER(0, 0, 0x3000);
    this->dispatch();
    this->errorReply(CMD_READ_REGISTER, 0x02, 9);
    ASSERT_TLM_LastCameraError_SIZE(1);
    ASSERT_TLM_LastCameraError(0, CameraErrorInfo(CMD_READ_REGISTER, ErrorType::INVALID_PARAMETER, 9));
    ASSERT_TLM_CameraInvalidCmds(1, 9);
    ASSERT_EQ(this->tlmHistory_CameraErrors->at(this->tlmHistory_CameraErrors->size() - 1).arg, 1u);
    ASSERT_EQ(this->tlmHistory_Exchanges->at(this->tlmHistory_Exchanges->size() - 1).arg, 7u);
}

void SunCamMgrTester::testOutcomeEventPerCommand() {
    // OK outcomes
    this->setupNominal();  // SetupComplete checked in testSetupNominal
    this->sendCmd_PING(0, 1, PingType::MCU);
    this->dispatch();
    this->pingReply(1, 2, 3);
    ASSERT_EVENTS_PingReply(0, PingType::MCU, 1, 2, 3);
    this->sendCmd_CAPTURE(0, 2, Slot::SLOT_1, TestPattern::OFF);
    this->dispatch();
    this->ack(CMD_CAPTURE);
    ASSERT_EVENTS_CaptureComplete(0, Slot::SLOT_1, TestPattern::OFF, DEFAULT_EXPOSURE, Mode::SD);
    this->sendCmd_READ_REGISTER(0, 3, 0x3012);
    this->dispatch();
    this->registerReply(0x3012, DEFAULT_EXPOSURE);
    ASSERT_EVENTS_RegisterRead_SIZE(1);
    ASSERT_EVENTS_RegisterRead(0, 0x3012, DEFAULT_EXPOSURE);
    this->sendCmd_SLEEP(0, 4);
    this->dispatch();
    this->ack(CMD_SET_MODE, MODE_SLEEP);
    ASSERT_EVENTS_SleepEntered_SIZE(1);
    ASSERT_CMD_RESPONSE_SIZE(4);
    for (U32 i = 0; i < 4; i++) {
        ASSERT_EQ(this->cmdResponseHistory->at(i).response, Fw::CmdResponse(Fw::CmdResponse::OK));
        ASSERT_EQ(this->cmdResponseHistory->at(i).cmdSeq, i + 1);
    }
    // rejected outcomes are reported by CommandRejected with the operation name
    this->clearHistory();
    this->sendCmd_CAPTURE(0, 6, Slot::SLOT_1, TestPattern::OFF);
    this->sendCmd_DOWNLOAD_ABORT(0, 7);
    this->dispatch(2);
    ASSERT_EVENTS_CommandRejected_SIZE(2);
    ASSERT_EVENTS_CommandRejected(0, Operation::CAPTURE, RejectCause::NOT_READY);
    ASSERT_EVENTS_CommandRejected(1, Operation::DOWNLOAD_ABORT, RejectCause::NO_DOWNLOAD);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_CAPTURE, 6, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_CMD_RESPONSE(1, SunCamMgr::OPCODE_DOWNLOAD_ABORT, 7, Fw::CmdResponse::EXECUTION_ERROR);
}

void SunCamMgrTester::testDownloadFullAndPreview() {
    this->setupNominal();
    // FULL, SD: the product is allocated at the command, the header written on the first reply
    this->startDownload(Slot::SLOT_2, Resolution::FULL, 1);
    ASSERT_PRODUCT_GET_SIZE(1);
    ASSERT_PRODUCT_GET(0, 0, Fw::DpContainer::getPacketSizeForDataSize(imageDataSize(FULL_LINES, FULL_PIXELS)));
    ASSERT_EVENTS_DownloadStarted_SIZE(0);
    for (U16 line = 0; line < FULL_LINES; line++) {
        this->assertFrame(CMD_DOWNLOAD, 2, 0, static_cast<U8>(line >> 8), static_cast<U8>(line & 0xFF), 6 + line);
        this->lineReply(line, FULL_PIXELS, static_cast<U8>(line));
        ASSERT_EVENTS_DownloadStarted_SIZE(1);
    }
    ASSERT_EVENTS_DownloadStarted(0, Slot::SLOT_2, Mode::SD, Resolution::FULL, FULL_LINES);
    ASSERT_EQ(this->m_frameCount, 6u + FULL_LINES - 1);
    ASSERT_EVENTS_DownloadComplete_SIZE(1);
    ASSERT_EVENTS_DownloadFailed_SIZE(0);
    const ImageHeader fullHeader(Slot::SLOT_2, Mode::SD, Resolution::FULL, 640, 480, DEFAULT_EXPOSURE, FULL_LINES);
    ASSERT_EQ(this->checkProduct(fullHeader, FULL_PIXELS, FULL_LINES, 2), FULL_LINES);
    ASSERT_EVENTS_DownloadComplete(0, Slot::SLOT_2, Mode::SD, Resolution::FULL, FULL_LINES,
                                   static_cast<U32>(FULL_LINES) * FULL_PIXELS);
    ASSERT_TLM_ProductsSent(0, 1);
    ASSERT_EQ(this->tlmHistory_LinesDownloaded->at(this->tlmHistory_LinesDownloaded->size() - 1).arg, FULL_LINES);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD, 1, Fw::CmdResponse::OK);
    ASSERT_EQ(this->tlmHistory_CurrentOperation->at(this->tlmHistory_CurrentOperation->size() - 1).arg,
              Operation::NONE);

    // PREVIEW, ZOOM
    this->clearHistory();
    this->m_frameCount = 0;
    this->setCameraMode(CaptureMode::ZOOM);
    this->tick();
    this->assertFrame(CMD_SET_MODE, MODE_ZOOM, 0, 0, 0, 1);
    this->ack(CMD_SET_MODE, MODE_ZOOM);
    this->ack(CMD_SET_EXPOSURE, MODE_ZOOM);
    this->registerReply(EXPOSURE_REGISTER, DEFAULT_EXPOSURE, MODE_ZOOM);
    this->assertSetupState(SetupState::READY);
    this->clearHistory();
    this->m_frameCount = 0;
    // the header carries the Exposure channel value, here changed to 10 before the download
    this->setExposure(10);
    this->tick();
    this->ack(CMD_SET_EXPOSURE, MODE_ZOOM);
    this->registerReply(EXPOSURE_REGISTER, 10, MODE_ZOOM);
    ASSERT_TLM_Exposure(0, 10);
    this->clearHistory();
    this->m_frameCount = 0;
    this->startDownload(Slot::SLOT_3, Resolution::PREVIEW, 2);
    ASSERT_PRODUCT_GET(0, 0, Fw::DpContainer::getPacketSizeForDataSize(imageDataSize(PREVIEW_LINES, PREVIEW_PIXELS)));
    for (U16 line = 0; line < PREVIEW_LINES; line++) {
        this->assertFrame(CMD_DOWNLOAD, 3, 1, 0, static_cast<U8>(line), 1 + line);
        this->lineReply(line, PREVIEW_PIXELS, static_cast<U8>(line), MODE_ZOOM);
    }
    ASSERT_EVENTS_DownloadStarted(0, Slot::SLOT_3, Mode::ZOOM, Resolution::PREVIEW, PREVIEW_LINES);
    const ImageHeader previewHeader(Slot::SLOT_3, Mode::ZOOM, Resolution::PREVIEW, 64, 48, 10, PREVIEW_LINES);
    ASSERT_EQ(this->checkProduct(previewHeader, PREVIEW_PIXELS, PREVIEW_LINES, 3), PREVIEW_LINES);
    ASSERT_EVENTS_DownloadComplete_SIZE(1);
    ASSERT_EVENTS_DownloadComplete(0, Slot::SLOT_3, Mode::ZOOM, Resolution::PREVIEW, PREVIEW_LINES,
                                   static_cast<U32>(PREVIEW_LINES) * PREVIEW_PIXELS);
    ASSERT_TLM_ProductsSent(0, 2);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD, 2, Fw::CmdResponse::OK);
}

void SunCamMgrTester::testDownloadHdFourSlots() {
    this->setCameraMode(CaptureMode::HD);
    this->setupNominal(MODE_HD);
    // in HD only SLOT_0 names the image (REQ-035)
    this->sendCmd_CAPTURE(0, 1, Slot::SLOT_1, TestPattern::OFF);
    this->sendCmd_DOWNLOAD(0, 2, Slot::SLOT_3, Resolution::PREVIEW, 0);
    this->dispatch(2);
    ASSERT_EVENTS_CommandRejected_SIZE(2);
    ASSERT_EVENTS_CommandRejected(0, Operation::CAPTURE, RejectCause::HD_SLOT);
    ASSERT_EVENTS_CommandRejected(1, Operation::DOWNLOAD, RejectCause::HD_SLOT);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_CAPTURE, 1, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_CMD_RESPONSE(1, SunCamMgr::OPCODE_DOWNLOAD, 2, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_PRODUCT_GET_SIZE(0);
    ASSERT_EQ(this->m_frameCount, 5u);
    this->clearHistory();
    // PREVIEW: slots 0, 1, 2, 3 in order into one product
    this->sendCmd_DOWNLOAD(0, 3, Slot::SLOT_0, Resolution::PREVIEW, 0);
    this->dispatch();
    const U16 hdLines = 4 * PREVIEW_LINES;
    ASSERT_PRODUCT_GET(0, 0, Fw::DpContainer::getPacketSizeForDataSize(imageDataSize(hdLines, PREVIEW_PIXELS)));
    for (U16 i = 0; i < hdLines; i++) {
        const U8 slot = static_cast<U8>(i / PREVIEW_LINES);
        const U16 line = static_cast<U16>(i % PREVIEW_LINES);
        this->assertFrame(CMD_DOWNLOAD, slot, 1, 0, static_cast<U8>(line), 6 + i);
        this->lineReply(line, PREVIEW_PIXELS, static_cast<U8>(line), MODE_HD);
    }
    ASSERT_EVENTS_DownloadStarted(0, Slot::SLOT_0, Mode::HD, Resolution::PREVIEW, hdLines);
    const ImageHeader header(Slot::SLOT_0, Mode::HD, Resolution::PREVIEW, 128, 96, DEFAULT_EXPOSURE, hdLines);
    ASSERT_EQ(this->checkProduct(header, PREVIEW_PIXELS, hdLines, 0), hdLines);
    ASSERT_EVENTS_DownloadComplete(0, Slot::SLOT_0, Mode::HD, Resolution::PREVIEW, hdLines,
                                   static_cast<U32>(hdLines) * PREVIEW_PIXELS);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD, 3, Fw::CmdResponse::OK);
    // FULL HD header geometry and line count
    this->clearHistory();
    this->sendCmd_DOWNLOAD(0, 4, Slot::SLOT_0, Resolution::FULL, 0);
    this->dispatch();
    ASSERT_PRODUCT_GET(0, 0, Fw::DpContainer::getPacketSizeForDataSize(imageDataSize(4 * FULL_LINES, FULL_PIXELS)));
    this->sendCmd_DOWNLOAD_ABORT(0, 5);
    this->dispatch();
    this->lineReply(0, FULL_PIXELS, 0, MODE_HD);
    ASSERT_EVENTS_DownloadStarted(0, Slot::SLOT_0, Mode::HD, Resolution::FULL, 4 * FULL_LINES);
    const ImageHeader fullHeader(Slot::SLOT_0, Mode::HD, Resolution::FULL, 1280, 960, DEFAULT_EXPOSURE, 4 * FULL_LINES);
    ASSERT_EQ(this->checkProduct(fullHeader, FULL_PIXELS, 1, 0), 1u);
}

void SunCamMgrTester::testDownloadPriority() {
    this->setupNominal();
    this->downloadPreviewAtPriority(7, 7, 1);
    this->downloadPreviewAtPriority(1, 1, 2);
    this->downloadPreviewAtPriority(0xFFFFFFFF, 0xFFFFFFFF, 3);
    this->downloadPreviewAtPriority(0, static_cast<FwDpPriorityType>(KissCamConfig::IMAGE_DP_PRIORITY), 4);
}

void SunCamMgrTester::testDownloadLineRetryAndFail() {
    this->setupNominal();
    this->startDownload(Slot::SLOT_1, Resolution::PREVIEW, 1);
    this->lineReply(0, PREVIEW_PIXELS, 0);
    this->lineReply(1, PREVIEW_PIXELS, 1);
    this->assertFrame(CMD_DOWNLOAD, 1, 1, 0, 2, 8);
    // a line reply with the wrong pixel count is discarded, not stored, and the line is re-requested on timeout
    this->lineReply(2, PREVIEW_PIXELS - 1, 2);
    this->lineReply(2, PREVIEW_PIXELS + 1, 2);
    ASSERT_EVENTS_FrameDiscarded_SIZE(2);
    ASSERT_EVENTS_FrameDiscarded(0, DiscardCause::MALFORMED, 71,
                                 head(0x40, CMD_DOWNLOAD, 0x01, 0x00, 0x41, 0x00, 0x02, 0x02));
    ASSERT_EVENTS_FrameDiscarded(1, DiscardCause::MALFORMED, 73,
                                 head(0x40, CMD_DOWNLOAD, 0x01, 0x00, 0x43, 0x00, 0x02, 0x02));
    ASSERT_EQ(this->tlmHistory_MalformedReplies->at(this->tlmHistory_MalformedReplies->size() - 1).arg, 2u);
    ASSERT_EQ(this->tlmHistory_DiscardedBytes->at(this->tlmHistory_DiscardedBytes->size() - 1).arg, 144u);
    ASSERT_EVENTS_LineRetry_SIZE(0);
    this->assertFrame(CMD_DOWNLOAD, 1, 1, 0, 2, 8);
    for (U8 attempt = 1; attempt <= KissCamConfig::LINE_RETRIES; attempt++) {
        this->tick(KissCamConfig::REPLY_TIMEOUT_TICKS);
        ASSERT_EVENTS_ReplyTimeout_SIZE(attempt);
        ASSERT_EVENTS_LineRetry_SIZE(attempt);
        ASSERT_EVENTS_LineRetry(attempt - 1, 2, attempt);
        this->assertFrame(CMD_DOWNLOAD, 1, 1, 0, 2, 8u + attempt);
        ASSERT_EVENTS_DownloadFailed_SIZE(0);
    }
    // an error reply on the last retry fails the download
    this->errorReply(CMD_DOWNLOAD, 0x02, 4);
    ASSERT_EVENTS_LineRetry_SIZE(KissCamConfig::LINE_RETRIES);
    ASSERT_EVENTS_DownloadFailed_SIZE(1);
    ASSERT_EVENTS_DownloadFailed(0, Slot::SLOT_1, 2, DownloadFailCause::LINE_FAILED);
    ASSERT_EVENTS_DownloadComplete_SIZE(0);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD, 1, Fw::CmdResponse::EXECUTION_ERROR);
    const ImageHeader header(Slot::SLOT_1, Mode::SD, Resolution::PREVIEW, 64, 48, DEFAULT_EXPOSURE, PREVIEW_LINES);
    ASSERT_EQ(this->checkProduct(header, PREVIEW_PIXELS, 2, 1), 2u);
    ASSERT_EQ(this->tlmHistory_ProductsSent->at(this->tlmHistory_ProductsSent->size() - 1).arg, 1u);
    ASSERT_EQ(this->m_frameCount, 8u + KissCamConfig::LINE_RETRIES);
    ASSERT_EQ(this->tlmHistory_CurrentOperation->at(this->tlmHistory_CurrentOperation->size() - 1).arg,
              Operation::NONE);
    this->assertSetupState(SetupState::READY);
}

void SunCamMgrTester::testNoCaptureWhenNotSetup() {
    // before set-up
    this->sendCmd_CAPTURE(0, 1, Slot::SLOT_0, TestPattern::OFF);
    this->dispatch();
    ASSERT_EVENTS_CommandRejected_SIZE(1);
    ASSERT_EVENTS_CommandRejected(0, Operation::CAPTURE, RejectCause::NOT_READY);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_CAPTURE, 1, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EQ(this->m_frameCount, 0u);
    // after SLEEP
    this->setupNominal();
    this->sendCmd_SLEEP(0, 2);
    this->dispatch();
    this->ack(CMD_SET_MODE, MODE_SLEEP);
    this->assertSetupState(SetupState::NOT_SETUP);
    this->clearHistory();
    this->sendCmd_CAPTURE(0, 3, Slot::SLOT_0, TestPattern::ON);
    this->dispatch();
    ASSERT_EVENTS_CommandRejected(0, Operation::CAPTURE, RejectCause::NOT_READY);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_CAPTURE, 3, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EQ(this->m_frameCount, 6u);
    // after CameraRestarted
    this->setupNominal();
    this->powerOnLine();
    ASSERT_EVENTS_CameraRestarted_SIZE(1);
    this->assertSetupState(SetupState::NOT_SETUP);
    this->clearHistory();
    this->sendCmd_CAPTURE(0, 4, Slot::SLOT_0, TestPattern::OFF);
    this->dispatch();
    ASSERT_EVENTS_CommandRejected(0, Operation::CAPTURE, RejectCause::NOT_READY);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_CAPTURE, 4, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EQ(this->m_frameCount, 11u);
    ASSERT_TLM_ImagesCaptured_SIZE(0);
}

void SunCamMgrTester::testPowerOnBanner() {
    const FrameHead bannerHead = head(0x4d, 0x56, 0x50, 0x20, 0x43, 0x61, 0x6d, 0x65);  // "MVP Came"
    // NOT_SETUP, idle: the line is stray bytes (REQ-020) and nothing else
    this->powerOnLine();
    ASSERT_EVENTS_FrameDiscarded_SIZE(1);
    ASSERT_EVENTS_FrameDiscarded(0, DiscardCause::MALFORMED, 21, bannerHead);
    ASSERT_TLM_DiscardedBytes(0, 21);
    ASSERT_EVENTS_CameraRestarted_SIZE(0);
    // split across two receive buffers: still one discard of 21 bytes
    this->powerOnLine(10);
    ASSERT_EVENTS_FrameDiscarded_SIZE(2);
    ASSERT_EVENTS_FrameDiscarded(1, DiscardCause::MALFORMED, 21, bannerHead);
    ASSERT_TLM_DiscardedBytes(1, 42);
    // noise before the line, line split across buffers: the noise is its own discard
    U8 mixed[12] = {0xAA, 0x55};
    ::memcpy(&mixed[2], SunCamMgr::POWER_ON_LINE, 10);
    this->receive(mixed, sizeof mixed, Drv::ByteStreamStatus::OP_OK);
    ASSERT_EVENTS_FrameDiscarded_SIZE(3);
    ASSERT_EVENTS_FrameDiscarded(2, DiscardCause::MALFORMED, 2, head(0xAA, 0x55, 0, 0, 0, 0, 0, 0));
    this->receive(&SunCamMgr::POWER_ON_LINE[10], 11, Drv::ByteStreamStatus::OP_OK);
    ASSERT_EVENTS_FrameDiscarded_SIZE(4);
    ASSERT_EVENTS_FrameDiscarded(3, DiscardCause::MALFORMED, 21, bannerHead);
    ASSERT_TLM_DiscardedBytes(3, 65);
    // a malformed frame (oversize length) immediately followed by the line in one buffer
    U8 bad[5 + 21] = {START, CMD_PING, MODE_SD, 0xFF, 0xFF};
    ::memcpy(&bad[5], SunCamMgr::POWER_ON_LINE, 21);
    this->receive(bad, sizeof bad, Drv::ByteStreamStatus::OP_OK);
    ASSERT_EVENTS_FrameDiscarded_SIZE(6);
    ASSERT_EVENTS_FrameDiscarded(4, DiscardCause::MALFORMED, 5, head(START, CMD_PING, MODE_SD, 0xFF, 0xFF, 0, 0, 0));
    ASSERT_EVENTS_FrameDiscarded(5, DiscardCause::MALFORMED, 21, bannerHead);
    ASSERT_TLM_DiscardedBytes(5, 91);
    ASSERT_EVENTS_CameraRestarted_SIZE(0);
    ASSERT_EQ(this->m_frameCount, 0u);
    // READY, idle
    this->setupNominal();
    this->powerOnLine();
    ASSERT_EVENTS_FrameDiscarded(0, DiscardCause::MALFORMED, 21, bannerHead);
    ASSERT_EVENTS_CameraRestarted_SIZE(1);
    ASSERT_EVENTS_CameraRestarted(0, Operation::NONE);
    this->assertSetupState(SetupState::NOT_SETUP);
    // mid-download, split across two buffers
    this->setupNominal();
    this->startDownload(Slot::SLOT_1, Resolution::PREVIEW, 1);
    this->lineReply(0, PREVIEW_PIXELS, 0);
    this->lineReply(1, PREVIEW_PIXELS, 1);
    this->powerOnLine(5);
    ASSERT_EVENTS_FrameDiscarded(0, DiscardCause::MALFORMED, 21, bannerHead);
    ASSERT_EVENTS_CameraRestarted_SIZE(1);
    ASSERT_EVENTS_CameraRestarted(0, Operation::DOWNLOAD);
    ASSERT_EVENTS_DownloadFailed_SIZE(1);
    ASSERT_EVENTS_DownloadFailed(0, Slot::SLOT_1, 2, DownloadFailCause::CAMERA_RESTART);
    const ImageHeader header(Slot::SLOT_1, Mode::SD, Resolution::PREVIEW, 64, 48, DEFAULT_EXPOSURE, PREVIEW_LINES);
    ASSERT_EQ(this->checkProduct(header, PREVIEW_PIXELS, 2, 1), 2u);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD, 1, Fw::CmdResponse::EXECUTION_ERROR);
    this->assertSetupState(SetupState::NOT_SETUP);
    ASSERT_EQ(this->tlmHistory_CurrentOperation->at(this->tlmHistory_CurrentOperation->size() - 1).arg,
              Operation::NONE);
    // mid-set-up
    this->clearHistory();
    this->sendSetup(2);
    this->pingReply(1, 0, 1, MODE_SLEEP);
    this->powerOnLine();
    ASSERT_EVENTS_CameraRestarted(0, Operation::SETUP_CAMERA);
    ASSERT_EVENTS_SetupFailed_SIZE(1);
    ASSERT_EVENTS_SetupFailed(0, SetupStep::SET_MODE, FailCause::CAMERA_RESTART);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_SETUP_CAMERA, 2, Fw::CmdResponse::EXECUTION_ERROR);
    this->assertSetupState(SetupState::NOT_SETUP);
    // a framed reply following the line in the same buffer is still parsed
    this->clearHistory();
    this->sendCmd_PING(0, 3, PingType::MCU);
    this->dispatch();
    U8 bytes[SunCamMgr::POWER_ON_LINE_SIZE + 10];
    ::memcpy(bytes, SunCamMgr::POWER_ON_LINE, SunCamMgr::POWER_ON_LINE_SIZE);
    const U8 ping[10] = {0x40, CMD_PING, MODE_SLEEP, 0x00, 0x04, 0x01, 0x00, 0x00, 0x07, 0x0D};
    ::memcpy(&bytes[SunCamMgr::POWER_ON_LINE_SIZE], ping, sizeof ping);
    this->receive(bytes, sizeof bytes, Drv::ByteStreamStatus::OP_OK);
    ASSERT_EVENTS_FrameDiscarded_SIZE(1);
    ASSERT_EVENTS_FrameDiscarded(0, DiscardCause::MALFORMED, 21, bannerHead);
    ASSERT_EVENTS_CameraRestarted_SIZE(0);
    ASSERT_EVENTS_PingReply_SIZE(1);
    ASSERT_EVENTS_PingReply(0, PingType::MCU, 1, 0, 7);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_PING, 3, Fw::CmdResponse::OK);
    // a run ending in a false prefix ("M") at the buffer end is kept until the next framed byte drops it
    const U8 falsePrefix[2] = {0x11, 0x4D};
    this->receive(falsePrefix, sizeof falsePrefix, Drv::ByteStreamStatus::OP_OK);
    ASSERT_EVENTS_FrameDiscarded_SIZE(2);
    ASSERT_EVENTS_FrameDiscarded(1, DiscardCause::MALFORMED, 1, head(0x11, 0, 0, 0, 0, 0, 0, 0));
    this->sendCmd_PING(0, 4, PingType::MCU);
    this->dispatch();
    this->pingReply(1, 0, 1, MODE_SLEEP);
    ASSERT_EVENTS_FrameDiscarded_SIZE(3);
    ASSERT_EVENTS_FrameDiscarded(2, DiscardCause::MALFORMED, 1, head(0x4D, 0, 0, 0, 0, 0, 0, 0));
    ASSERT_EVENTS_PingReply_SIZE(2);
    ASSERT_EVENTS_CameraRestarted_SIZE(0);
    ASSERT_CMD_RESPONSE(1, SunCamMgr::OPCODE_PING, 4, Fw::CmdResponse::OK);
}

void SunCamMgrTester::testDownloadFailedSendAndProductFull() {
    this->setupNominal();
    // Download frame refused by the driver: the product allocated at the command is delivered empty
    this->m_sendStatus = Drv::ByteStreamStatus::OTHER_ERROR;
    this->sendCmd_DOWNLOAD(0, 1, Slot::SLOT_1, Resolution::PREVIEW, 0);
    this->dispatch();
    this->m_sendStatus = Drv::ByteStreamStatus::OP_OK;
    ASSERT_EVENTS_SendFailed_SIZE(1);
    ASSERT_EVENTS_SendFailed(0, Operation::DOWNLOAD, Drv::ByteStreamStatus::OTHER_ERROR);
    ASSERT_EVENTS_DownloadFailed_SIZE(1);
    ASSERT_EVENTS_DownloadFailed(0, Slot::SLOT_1, 0, DownloadFailCause::SEND_FAILED);
    ASSERT_EVENTS_DownloadStarted_SIZE(0);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD, 1, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_PRODUCT_GET_SIZE(1);
    ASSERT_PRODUCT_SEND_SIZE(1);
    {
        Fw::DpContainer container(0, this->productSendHistory->at(0).buffer);
        ASSERT_EQ(container.deserializeHeader(), Fw::FW_SERIALIZE_OK);
        ASSERT_EQ(container.getDataSize(), 0u);  // no line reply arrived: no header, no lines
    }
    this->clearHistory();

    // Container full: a product one byte short of the requested size cannot take the last line
    this->m_productGetShrink = 1;
    this->startDownload(Slot::SLOT_1, Resolution::PREVIEW, 3);
    const U16 linesThatFit = PREVIEW_LINES - 1;
    for (U16 line = 0; line <= linesThatFit; line++) {
        ASSERT_EVENTS_DownloadFailed_SIZE(0);
        this->lineReply(line, PREVIEW_PIXELS, static_cast<U8>(line));
    }
    this->m_productGetShrink = 0;
    ASSERT_EVENTS_DownloadFailed_SIZE(1);
    ASSERT_EVENTS_DownloadFailed(0, Slot::SLOT_1, linesThatFit, DownloadFailCause::PRODUCT_FULL);
    const ImageHeader header(Slot::SLOT_1, Mode::SD, Resolution::PREVIEW, 64, 48, DEFAULT_EXPOSURE, PREVIEW_LINES);
    ASSERT_EQ(this->checkProduct(header, PREVIEW_PIXELS, linesThatFit, 1), linesThatFit);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD, 3, Fw::CmdResponse::EXECUTION_ERROR);
}

void SunCamMgrTester::testDownloadAbortCommanded() {
    this->setupNominal();
    this->sendCmd_DOWNLOAD_ABORT(0, 1);
    this->dispatch();
    ASSERT_EVENTS_CommandRejected(0, Operation::DOWNLOAD_ABORT, RejectCause::NO_DOWNLOAD);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD_ABORT, 1, Fw::CmdResponse::EXECUTION_ERROR);
    this->clearHistory();

    this->startDownload(Slot::SLOT_3, Resolution::FULL, 2);
    this->lineReply(0, FULL_PIXELS, 0);
    this->lineReply(1, FULL_PIXELS, 1);
    // the pixel-count check follows the full-resolution line length too
    {
        const U32 frames = this->m_frameCount;
        this->lineReply(2, FULL_PIXELS - 1, 2);
        ASSERT_EVENTS_FrameDiscarded_SIZE(1);
        ASSERT_EQ(this->eventHistory_FrameDiscarded->at(0).cause, DiscardCause::MALFORMED);
        ASSERT_EQ(this->m_frameCount, frames);
    }
    this->sendCmd_DOWNLOAD_ABORT(0, 3);
    this->dispatch();
    ASSERT_CMD_RESPONSE_SIZE(0);  // the pending line is still awaited
    ASSERT_EQ(this->m_frameCount, 8u);
    this->lineReply(2, FULL_PIXELS, 2);
    ASSERT_EQ(this->m_frameCount, 8u);  // no further request
    ASSERT_EVENTS_DownloadAborted_SIZE(1);
    ASSERT_EVENTS_DownloadAborted(0, Slot::SLOT_3, 3);
    ASSERT_EVENTS_DownloadFailed_SIZE(0);
    ASSERT_CMD_RESPONSE_SIZE(2);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD, 2, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(1, SunCamMgr::OPCODE_DOWNLOAD_ABORT, 3, Fw::CmdResponse::OK);
    const ImageHeader header(Slot::SLOT_3, Mode::SD, Resolution::FULL, 640, 480, DEFAULT_EXPOSURE, FULL_LINES);
    ASSERT_EQ(this->checkProduct(header, FULL_PIXELS, 3, 3), 3u);
    ASSERT_EQ(this->tlmHistory_CurrentOperation->at(this->tlmHistory_CurrentOperation->size() - 1).arg,
              Operation::NONE);
    // abort while the pending line times out: no retry, abort completes
    this->clearHistory();
    this->startDownload(Slot::SLOT_1, Resolution::PREVIEW, 4);
    this->sendCmd_DOWNLOAD_ABORT(0, 5);
    this->dispatch();
    this->tick(KissCamConfig::REPLY_TIMEOUT_TICKS);
    ASSERT_EVENTS_LineRetry_SIZE(0);
    ASSERT_EVENTS_DownloadAborted(0, Slot::SLOT_1, 0);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD, 4, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(1, SunCamMgr::OPCODE_DOWNLOAD_ABORT, 5, Fw::CmdResponse::OK);
    // abort before the first line reply; a second abort while one is pending is refused BUSY
    this->clearHistory();
    this->sendCmd_DOWNLOAD(0, 6, Slot::SLOT_2, Resolution::PREVIEW, 0);
    this->dispatch();
    this->sendCmd_DOWNLOAD_ABORT(0, 7);
    this->sendCmd_DOWNLOAD_ABORT(0, 8);
    this->dispatch(2);
    ASSERT_EVENTS_CommandRejected_SIZE(1);
    ASSERT_EVENTS_CommandRejected(0, Operation::DOWNLOAD_ABORT, RejectCause::BUSY);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD_ABORT, 8, Fw::CmdResponse::BUSY);
    this->lineReply(0, PREVIEW_PIXELS, 0);
    ASSERT_EVENTS_DownloadAborted_SIZE(1);
    ASSERT_EVENTS_DownloadAborted(0, Slot::SLOT_2, 1);
    ASSERT_PRODUCT_SEND_SIZE(1);
    ASSERT_CMD_RESPONSE(1, SunCamMgr::OPCODE_DOWNLOAD, 6, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(2, SunCamMgr::OPCODE_DOWNLOAD_ABORT, 7, Fw::CmdResponse::OK);
    this->assertSetupState(SetupState::READY);
}

void SunCamMgrTester::testReplyTimeoutTicks() {
    this->setupNominal();
    this->sendCmd_PING(0, 1, PingType::MCU);
    this->dispatch();
    this->tick(KissCamConfig::REPLY_TIMEOUT_TICKS - 1);
    ASSERT_EVENTS_ReplyTimeout_SIZE(0);
    ASSERT_CMD_RESPONSE_SIZE(0);
    this->tick();
    ASSERT_EVENTS_ReplyTimeout_SIZE(1);
    ASSERT_EVENTS_ReplyTimeout(0, Operation::PING, CMD_PING);
    ASSERT_EQ(this->tlmHistory_Timeouts->at(this->tlmHistory_Timeouts->size() - 1).arg, 1u);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_PING, 1, Fw::CmdResponse::EXECUTION_ERROR);
    // a reply arriving on the last tick before expiry is accepted
    this->clearHistory();
    this->sendCmd_PING(0, 2, PingType::MCU);
    this->dispatch();
    this->tick(KissCamConfig::REPLY_TIMEOUT_TICKS - 1);
    this->pingReply(1, 0, 1);
    ASSERT_EVENTS_ReplyTimeout_SIZE(0);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_PING, 2, Fw::CmdResponse::OK);
    // the timeout counter restarts per exchange
    this->tick(KissCamConfig::REPLY_TIMEOUT_TICKS * 2);
    ASSERT_EVENTS_ReplyTimeout_SIZE(0);
    // a reply left incomplete when the exchange times out is discarded and does not frame the next reply
    this->clearHistory();
    this->sendCmd_PING(0, 3, PingType::MCU);
    this->dispatch();
    const U8 partial[5] = {0x40, 0x50, 0x01, 0x00, 0x04};
    this->receive(partial, sizeof partial, Drv::ByteStreamStatus::OP_OK);
    ASSERT_EVENTS_FrameDiscarded_SIZE(0);
    this->tick(KissCamConfig::REPLY_TIMEOUT_TICKS);
    ASSERT_EVENTS_ReplyTimeout_SIZE(1);
    ASSERT_EVENTS_FrameDiscarded_SIZE(1);
    ASSERT_EVENTS_FrameDiscarded(0, DiscardCause::MALFORMED, 5, head(0x40, 0x50, 0x01, 0x00, 0x04, 0, 0, 0));
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_PING, 3, Fw::CmdResponse::EXECUTION_ERROR);
    this->clearHistory();
    this->sendCmd_PING(0, 4, PingType::MCU);
    this->dispatch();
    this->pingReply(1, 0, 1);
    ASSERT_EVENTS_FrameDiscarded_SIZE(0);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_PING, 4, Fw::CmdResponse::OK);
    // a fragment received while idle is discarded when the next exchange starts
    this->clearHistory();
    this->receive(partial, sizeof partial, Drv::ByteStreamStatus::OP_OK);
    ASSERT_EVENTS_FrameDiscarded_SIZE(0);
    this->sendCmd_PING(0, 5, PingType::MCU);
    this->dispatch();
    ASSERT_EVENTS_FrameDiscarded_SIZE(1);
    ASSERT_EVENTS_FrameDiscarded(0, DiscardCause::NO_OPERATION, 5, head(0x40, 0x50, 0x01, 0x00, 0x04, 0, 0, 0));
    this->pingReply(1, 0, 1);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_PING, 5, Fw::CmdResponse::OK);
}

void SunCamMgrTester::testCameraErrorReply() {
    this->setupNominal();
    this->sendCmd_CAPTURE(0, 1, Slot::SLOT_1, TestPattern::OFF);
    this->dispatch();
    this->errorReply(CMD_CAPTURE, 0x03, 42);
    ASSERT_EVENTS_CameraError_SIZE(1);
    ASSERT_EVENTS_CameraError(0, Operation::CAPTURE, CMD_CAPTURE, ErrorType::WRONG_MODE, 42);
    ASSERT_TLM_LastCameraError(0, CameraErrorInfo(CMD_CAPTURE, ErrorType::WRONG_MODE, 42));
    ASSERT_TLM_CameraErrors(0, 1);
    ASSERT_EVENTS_CaptureComplete_SIZE(0);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_CAPTURE, 1, Fw::CmdResponse::EXECUTION_ERROR);
    this->assertSetupState(SetupState::READY);
    // an error reply naming another command is not this exchange's reply
    this->sendCmd_PING(0, 2, PingType::MCU);
    this->dispatch();
    this->errorReply(CMD_CAPTURE, 0x01, 43);
    ASSERT_EVENTS_CameraError_SIZE(1);
    ASSERT_EVENTS_FrameDiscarded_SIZE(1);
    ASSERT_CMD_RESPONSE_SIZE(1);
    this->pingReply(1, 0, 1);
    ASSERT_CMD_RESPONSE(1, SunCamMgr::OPCODE_PING, 2, Fw::CmdResponse::OK);
}

void SunCamMgrTester::testMalformedAndStrayFrames() {
    this->setupNominal();
    // no operation in progress
    this->pingReply(1, 0, 1);
    ASSERT_EVENTS_FrameDiscarded_SIZE(1);
    ASSERT_EVENTS_FrameDiscarded(0, DiscardCause::NO_OPERATION, 10,
                                 head(0x40, 0x50, 0x01, 0x00, 0x04, 0x01, 0x00, 0x00));
    ASSERT_TLM_DiscardedBytes(0, 10);

    this->sendCmd_PING(0, 1, PingType::MCU);
    this->dispatch();
    // wrong command byte
    this->ack(CMD_SET_MODE);
    ASSERT_EVENTS_FrameDiscarded_SIZE(2);
    ASSERT_EVENTS_FrameDiscarded(1, DiscardCause::WRONG_COMMAND, 6, head(0x40, 0x4D, 0x01, 0x00, 0x00, 0x0D, 0, 0));
    ASSERT_TLM_DiscardedBytes(1, 16);
    // bad length: declared length larger than the assembler
    const U8 tooLong[6] = {START, CMD_PING, MODE_SD, 0x02, 0x83, END};
    this->receive(tooLong, sizeof tooLong);
    ASSERT_EVENTS_FrameDiscarded_SIZE(3);
    ASSERT_EVENTS_FrameDiscarded(2, DiscardCause::MALFORMED, 6, head(0x40, 0x50, 0x01, 0x02, 0x83, 0x0D, 0, 0));
    ASSERT_TLM_MalformedReplies(0, 1);
    // missing terminator: the frame ends on a byte other than 0x0D, then resync on the next 0x40
    const U8 noEnd[10] = {START, CMD_PING, MODE_SD, 0x00, 0x04, 1, 2, 3, 4, 0x55};
    this->receive(noEnd, sizeof noEnd);
    ASSERT_EVENTS_FrameDiscarded_SIZE(4);
    ASSERT_EVENTS_FrameDiscarded(3, DiscardCause::MALFORMED, 10, head(0x40, 0x50, 0x01, 0x00, 0x04, 1, 2, 3));
    ASSERT_TLM_MalformedReplies(1, 2);
    // stray bytes before a start byte are discarded; the frame after them is accepted
    const U8 stray[13] = {0x11, 0x22, 0x33, START, CMD_PING, MODE_SD, 0x00, 0x04, 9, 8, 0x00, 0x07, END};
    this->receive(stray, sizeof stray);
    ASSERT_EVENTS_FrameDiscarded_SIZE(5);
    ASSERT_EVENTS_FrameDiscarded(4, DiscardCause::MALFORMED, 3, head(0x11, 0x22, 0x33, 0, 0, 0, 0, 0));
    ASSERT_EVENTS_PingReply_SIZE(1);
    ASSERT_EVENTS_PingReply(0, PingType::MCU, 9, 8, 7);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_PING, 1, Fw::CmdResponse::OK);
    // late reply after the exchange timed out
    this->sendCmd_PING(0, 2, PingType::MCU);
    this->dispatch();
    this->tick(KissCamConfig::REPLY_TIMEOUT_TICKS);
    this->pingReply(1, 0, 1);
    ASSERT_EVENTS_FrameDiscarded_SIZE(6);
    ASSERT_EVENTS_FrameDiscarded(5, DiscardCause::NO_OPERATION, 10,
                                 head(0x40, 0x50, 0x01, 0x00, 0x04, 0x01, 0x00, 0x00));
    // non-OK buffer with data: discarded and returned; a null notice has nothing to return
    const U8 junk[4] = {1, 2, 3, 4};
    this->receive(junk, sizeof junk, Drv::ByteStreamStatus::OTHER_ERROR);
    ASSERT_EVENTS_FrameDiscarded_SIZE(7);
    ASSERT_EVENTS_FrameDiscarded(6, DiscardCause::MALFORMED, 4, head(1, 2, 3, 4, 0, 0, 0, 0));
    this->receive(nullptr, 0, Drv::ByteStreamStatus::RECV_NO_DATA);
    ASSERT_EVENTS_FrameDiscarded_SIZE(7);
    ASSERT_EQ(this->tlmHistory_DiscardedBytes->at(this->tlmHistory_DiscardedBytes->size() - 1).arg, 49u);
    // replies that cannot be decoded are malformed and leave the exchange pending: an error reply of the
    // wrong length or with an unknown error type, an acknowledgement of the wrong length, a mode byte that
    // is not a Mode, a Download line without its line number
    this->sendCmd_PING(0, 3, PingType::MCU);
    this->dispatch();
    const U8 shortError[2] = {CMD_PING, 0x01};
    this->reply(CMD_ERROR, MODE_SD, shortError, sizeof shortError);
    const U8 unknownType[3] = {CMD_PING, 0x05, 1};
    this->reply(CMD_ERROR, MODE_SD, unknownType, sizeof unknownType);
    const U8 shortPing[3] = {1, 2, 3};
    this->reply(CMD_PING, MODE_SD, shortPing, sizeof shortPing);
    const U8 pingPayload[4] = {1, 2, 0, 3};
    this->reply(CMD_PING, 0x04, pingPayload, sizeof pingPayload);
    ASSERT_EQ(this->tlmHistory_MalformedReplies->at(this->tlmHistory_MalformedReplies->size() - 1).arg, 8u);
    ASSERT_EVENTS_CameraError_SIZE(0);
    ASSERT_CMD_RESPONSE_SIZE(2);
    this->pingReply(1, 0, 1);
    ASSERT_CMD_RESPONSE(2, SunCamMgr::OPCODE_PING, 3, Fw::CmdResponse::OK);
    this->startDownload(Slot::SLOT_1, Resolution::PREVIEW, 4);
    const U8 shortLine[1] = {0};
    this->reply(CMD_DOWNLOAD, MODE_SD, shortLine, sizeof shortLine);
    ASSERT_EQ(this->tlmHistory_MalformedReplies->at(this->tlmHistory_MalformedReplies->size() - 1).arg, 9u);
    ASSERT_EVENTS_DownloadFailed_SIZE(0);
    this->lineReply(0, PREVIEW_PIXELS, 0);
    ASSERT_EQ(this->tlmHistory_LinesDownloaded->at(this->tlmHistory_LinesDownloaded->size() - 1).arg, 1u);
    // FrameDiscarded is throttled after ten; SETUP_CAMERA clears the throttle
    ASSERT_EVENTS_FrameDiscarded_SIZE(10);
    for (U16 line = 1; line < PREVIEW_LINES; line++) {
        this->lineReply(line, PREVIEW_PIXELS, 0);
    }
    ASSERT_EVENTS_DownloadComplete_SIZE(1);
    this->sendSetup(5);
    this->ack(CMD_SET_MODE);  // not the reply to the pending Ping
    ASSERT_EVENTS_FrameDiscarded_SIZE(11);
    ASSERT_EVENTS_FrameDiscarded(10, DiscardCause::WRONG_COMMAND, 6, head(0x40, 0x4D, 0x01, 0x00, 0x00, 0x0D, 0, 0));
}

void SunCamMgrTester::testSplitAndCrPayloadReply() {
    this->setupNominal();
    this->startDownload(Slot::SLOT_1, Resolution::PREVIEW, 1);
    // one line reply (72 bytes) whose pixels are all 0x0D, delivered in three buffers
    U8 frame[72];
    frame[0] = START;
    frame[1] = CMD_DOWNLOAD;
    frame[2] = MODE_SD;
    frame[3] = 0x00;
    frame[4] = 0x42;
    frame[5] = 0x00;
    frame[6] = 0x00;
    ::memset(&frame[7], END, PREVIEW_PIXELS);
    frame[71] = END;
    this->receive(frame, 3);
    this->receive(&frame[3], 40);
    ASSERT_EQ(this->m_frameCount, 6u);  // still waiting for the rest of the line
    ASSERT_EVENTS_FrameDiscarded_SIZE(0);
    this->receive(&frame[43], 29);
    ASSERT_EVENTS_FrameDiscarded_SIZE(0);
    this->assertFrame(CMD_DOWNLOAD, 1, 1, 0, 1, 7);
    ASSERT_EQ(this->tlmHistory_LinesDownloaded->at(this->tlmHistory_LinesDownloaded->size() - 1).arg, 1u);
    // two replies in one buffer: the second line follows the first
    U8 two[2 * 72];
    ::memcpy(two, frame, 72);
    ::memcpy(&two[72], frame, 72);
    two[6] = 0x01;
    two[72 + 6] = 0x02;
    this->receive(two, sizeof two);
    this->assertFrame(CMD_DOWNLOAD, 1, 1, 0, 3, 9);
    ASSERT_EQ(this->tlmHistory_LinesDownloaded->at(this->tlmHistory_LinesDownloaded->size() - 1).arg, 3u);
    // a reply echoing the wrong line number is not this exchange's reply
    this->lineReply(7, PREVIEW_PIXELS, 0);
    ASSERT_EVENTS_FrameDiscarded_SIZE(1);
    ASSERT_EVENTS_FrameDiscarded(0, DiscardCause::WRONG_COMMAND, 72,
                                 head(0x40, 0x44, 0x01, 0x00, 0x42, 0x00, 0x07, 0x00));
    this->assertFrame(CMD_DOWNLOAD, 1, 1, 0, 3, 9);
}

void SunCamMgrTester::testModeMismatch() {
    this->setupNominal();
    this->sendCmd_PING(0, 1, PingType::MCU);
    this->dispatch();
    this->pingReply(1, 0, 1, MODE_HD);
    ASSERT_EVENTS_ModeMismatch_SIZE(1);
    ASSERT_EVENTS_ModeMismatch(0, Mode::SD, Mode::HD);
    ASSERT_EVENTS_PingReply_SIZE(0);
    ASSERT_TLM_CameraMode(0, Mode::HD);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_PING, 1, Fw::CmdResponse::EXECUTION_ERROR);
    this->assertSetupState(SetupState::NOT_SETUP);
    // during set-up the mode is not yet commanded: the first ping may report Sleep (see setupNominal)
    // a push reply reporting the wrong mode fails the push with MODE_MISMATCH
    this->setupNominal();
    this->setExposure(10);
    this->tick();
    this->ack(CMD_SET_EXPOSURE, MODE_SLEEP);
    ASSERT_EVENTS_ModeMismatch(0, Mode::SD, Mode::SLEEP);
    ASSERT_EVENTS_SettingPushFailed(0, Setting::EXPOSURE, FailCause::MODE_MISMATCH);
    this->assertSetupState(SetupState::NOT_SETUP);
    // a line reply in the wrong mode ends the download and the READY state
    this->setupNominal(MODE_SD, 10);
    this->startDownload(Slot::SLOT_1, Resolution::PREVIEW, 3);
    this->lineReply(0, PREVIEW_PIXELS, 0, MODE_HD);
    ASSERT_EVENTS_ModeMismatch(0, Mode::SD, Mode::HD);
    ASSERT_EVENTS_DownloadFailed(0, Slot::SLOT_1, 0, DownloadFailCause::LINE_FAILED);
    ASSERT_PRODUCT_SEND_SIZE(1);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD, 3, Fw::CmdResponse::EXECUTION_ERROR);
    this->assertSetupState(SetupState::NOT_SETUP);
}

void SunCamMgrTester::testReadRegister() {
    this->setupNominal();
    this->sendCmd_READ_REGISTER(0, 2, 0x3012);
    this->dispatch();
    this->assertFrame(CMD_READ_REGISTER, 0x30, 0x12, 0, 0, 6);
    // address echo mismatch: discarded, exchange still pending
    this->registerReply(0x3013, 0x0100);
    ASSERT_EVENTS_FrameDiscarded_SIZE(1);
    ASSERT_EVENTS_FrameDiscarded(0, DiscardCause::WRONG_COMMAND, 10,
                                 head(0x40, 0x52, 0x01, 0x00, 0x04, 0x30, 0x13, 0x01));
    ASSERT_EVENTS_RegisterRead_SIZE(0);
    this->registerReply(0x3012, 0x0100);
    ASSERT_EVENTS_RegisterRead(0, 0x3012, 0x0100);
    ASSERT_TLM_RegisterValue(0, RegisterValue(0x3012, 0x0100));
    ASSERT_TLM_ExposureReadback_SIZE(0);  // a commanded read is not the exposure readback
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_READ_REGISTER, 2, Fw::CmdResponse::OK);
}

void SunCamMgrTester::testModeChangeRepushesExposure() {
    this->setupNominal();
    this->setCameraMode(CaptureMode::HD);
    this->tick();
    this->assertFrame(CMD_SET_MODE, MODE_HD, 0, 0, 0, 6);
    this->ack(CMD_SET_MODE, MODE_HD);
    ASSERT_EVENTS_SettingApplied(0, Setting::CAMERA_MODE, MODE_HD);
    this->assertFrame(CMD_SET_EXPOSURE, 0x02, 0xA0, 0, 0, 7);
    this->ack(CMD_SET_EXPOSURE, MODE_HD);
    ASSERT_EVENTS_SettingApplied(1, Setting::EXPOSURE, DEFAULT_EXPOSURE);
    this->assertFrame(CMD_READ_REGISTER, 0x30, 0x12, 0, 0, 8);
    this->registerReply(EXPOSURE_REGISTER, DEFAULT_EXPOSURE, MODE_HD);
    ASSERT_EVENTS_ExposureReadback(0, DEFAULT_EXPOSURE, DEFAULT_EXPOSURE);
    this->assertSetupState(SetupState::READY);
    this->tick(3);
    ASSERT_EQ(this->m_frameCount, 8u);
    // the new mode is now the commanded mode for the mode check
    this->sendCmd_PING(0, 1, PingType::MCU);
    this->dispatch();
    this->pingReply(1, 0, 1, MODE_HD);
    ASSERT_EVENTS_ModeMismatch_SIZE(0);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_PING, 1, Fw::CmdResponse::OK);
    // every CAMERA_MODE push (ZOOM, then back to SD) is followed by the exposure push and readback
    const U8 modes[2] = {MODE_ZOOM, MODE_SD};
    const CaptureMode::T captureModes[2] = {CaptureMode::ZOOM, CaptureMode::SD};
    for (U32 i = 0; i < 2; i++) {
        this->clearHistory();
        const U32 base = this->m_frameCount;
        this->setCameraMode(captureModes[i]);
        this->tick();
        this->assertFrame(CMD_SET_MODE, modes[i], 0, 0, 0, base + 1);
        this->ack(CMD_SET_MODE, modes[i]);
        this->assertFrame(CMD_SET_EXPOSURE, 0x02, 0xA0, 0, 0, base + 2);
        this->ack(CMD_SET_EXPOSURE, modes[i]);
        this->assertFrame(CMD_READ_REGISTER, 0x30, 0x12, 0, 0, base + 3);
        this->registerReply(EXPOSURE_REGISTER, DEFAULT_EXPOSURE, modes[i]);
        ASSERT_EVENTS_SettingApplied(0, Setting::CAMERA_MODE, modes[i]);
        ASSERT_EVENTS_SettingApplied(1, Setting::EXPOSURE, DEFAULT_EXPOSURE);
        this->assertSetupState(SetupState::READY);
    }
}

void SunCamMgrTester::testSendAndAllocationFailure() {
    // send failure at set-up
    this->m_sendStatus = Drv::ByteStreamStatus::SEND_RETRY;
    this->sendSetup(1);
    ASSERT_EVENTS_SendFailed(0, Operation::SETUP_CAMERA, Drv::ByteStreamStatus::SEND_RETRY);
    ASSERT_EVENTS_SetupFailed(0, SetupStep::PING_MCU, FailCause::SEND_FAILED);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_SETUP_CAMERA, 1, Fw::CmdResponse::EXECUTION_ERROR);
    this->m_sendStatus = Drv::ByteStreamStatus::OP_OK;
    this->clearHistory();
    this->setupNominal();
    // send failure on a command
    this->m_sendStatus = Drv::ByteStreamStatus::OTHER_ERROR;
    this->sendCmd_PING(0, 2, PingType::MCU);
    this->dispatch();
    ASSERT_EVENTS_SendFailed(0, Operation::PING, Drv::ByteStreamStatus::OTHER_ERROR);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_PING, 2, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EQ(this->tlmHistory_CurrentOperation->at(this->tlmHistory_CurrentOperation->size() - 1).arg,
              Operation::NONE);
    // the first frame of every READY command refused; SLEEP refused ends in NOT_SETUP
    this->sendCmd_CAPTURE(0, 4, Slot::SLOT_1, TestPattern::OFF);
    this->sendCmd_DOWNLOAD(0, 5, Slot::SLOT_1, Resolution::FULL, 0);
    this->sendCmd_READ_REGISTER(0, 6, 0x3012);
    this->dispatch(3);
    ASSERT_EVENTS_SendFailed_SIZE(4);
    ASSERT_EVENTS_DownloadFailed_SIZE(1);
    ASSERT_EVENTS_DownloadFailed(0, Slot::SLOT_1, 0, DownloadFailCause::SEND_FAILED);
    ASSERT_PRODUCT_GET_SIZE(1);  // the product allocated at the command is delivered empty
    ASSERT_PRODUCT_SEND_SIZE(1);
    ASSERT_CMD_RESPONSE_SIZE(4);
    for (U32 i = 1; i < 4; i++) {
        ASSERT_EQ(this->cmdResponseHistory->at(i).cmdSeq, i + 3);
        ASSERT_EQ(this->cmdResponseHistory->at(i).response, Fw::CmdResponse(Fw::CmdResponse::EXECUTION_ERROR));
    }
    this->assertSetupState(SetupState::READY);
    this->sendCmd_SLEEP(0, 7);
    this->dispatch();
    ASSERT_EVENTS_SendFailed(4, Operation::SLEEP, Drv::ByteStreamStatus::OTHER_ERROR);
    ASSERT_CMD_RESPONSE(4, SunCamMgr::OPCODE_SLEEP, 7, Fw::CmdResponse::EXECUTION_ERROR);
    this->assertSetupState(SetupState::NOT_SETUP);
    this->m_sendStatus = Drv::ByteStreamStatus::OP_OK;
    this->setupNominal();
    this->m_sendStatus = Drv::ByteStreamStatus::OTHER_ERROR;
    // send failure on a push
    this->setExposure(10);
    this->tick();
    ASSERT_EVENTS_SettingPushFailed(0, Setting::EXPOSURE, FailCause::SEND_FAILED);
    this->assertSetupState(SetupState::NOT_SETUP);
    this->m_sendStatus = Drv::ByteStreamStatus::OP_OK;
    this->clearHistory();
    // product allocation failure
    this->setupNominal(MODE_SD, 10);
    this->m_productGetStatus = Fw::Success::FAILURE;
    const U32 frames = this->m_frameCount;
    this->sendCmd_DOWNLOAD(0, 3, Slot::SLOT_1, Resolution::FULL, 0);
    this->dispatch();
    ASSERT_EVENTS_ProductAllocationFailed_SIZE(1);
    ASSERT_EVENTS_ProductAllocationFailed(0, static_cast<U32>(imageDataSize(FULL_LINES, FULL_PIXELS)));
    ASSERT_EVENTS_DownloadStarted_SIZE(0);
    ASSERT_PRODUCT_SEND_SIZE(0);
    ASSERT_CMD_RESPONSE(0, SunCamMgr::OPCODE_DOWNLOAD, 3, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EQ(this->m_frameCount, frames);  // no Download frame
    this->assertSetupState(SetupState::READY);
}

void SunCamMgrTester::testQueueFullHooks() {
    // fill the queue with ticks that will not be dispatched, then overflow each input
    for (U32 i = 0; i < TEST_INSTANCE_QUEUE_DEPTH; i++) {
        this->sendCmd_PING(0, i, PingType::MCU);
    }
    this->sendCmd_SETUP_CAMERA(0, 100);
    this->sendCmd_SLEEP(0, 101);
    this->sendCmd_PING(0, 102, PingType::MCU);
    this->sendCmd_CAPTURE(0, 103, Slot::SLOT_1, TestPattern::OFF);
    this->sendCmd_DOWNLOAD(0, 104, Slot::SLOT_1, Resolution::FULL, 0);
    this->sendCmd_DOWNLOAD_ABORT(0, 105);
    this->sendCmd_READ_REGISTER(0, 106, 0x3012);
    const FwOpcodeType opcodes[7] = {SunCamMgr::OPCODE_SETUP_CAMERA, SunCamMgr::OPCODE_SLEEP,
                                     SunCamMgr::OPCODE_PING,         SunCamMgr::OPCODE_CAPTURE,
                                     SunCamMgr::OPCODE_DOWNLOAD,     SunCamMgr::OPCODE_DOWNLOAD_ABORT,
                                     SunCamMgr::OPCODE_READ_REGISTER};
    ASSERT_CMD_RESPONSE_SIZE(7);
    for (U32 i = 0; i < 7; i++) {
        ASSERT_CMD_RESPONSE(i, opcodes[i], 100 + i, Fw::CmdResponse::BUSY);
    }
    const U8 bytes[3] = {1, 2, 3};
    Fw::Buffer buffer(this->m_rxStore, sizeof bytes);
    ::memcpy(this->m_rxStore, bytes, sizeof bytes);
    this->invoke_to_drvReceiveIn(0, buffer, Drv::ByteStreamStatus::OP_OK);
    ASSERT_from_drvReceiveReturnOut_SIZE(1);
    this->invoke_to_pingIn(0, 7);
    this->invoke_to_pingIn(0, 8);
    this->invoke_to_pingIn(0, 9);
    this->invoke_to_run(0, 0);
    this->invoke_to_run(0, 0);  // the hook cleared the atomic, so this tick is attempted and dropped too
    this->dispatch(TEST_INSTANCE_QUEUE_DEPTH);
    ASSERT_from_pingOut_SIZE(0);
    ASSERT_EVENTS_QueueOverflow_SIZE(0);
    // the next tick reports the drops per source
    this->tick();
    ASSERT_EVENTS_QueueOverflow_SIZE(4);
    ASSERT_EVENTS_QueueOverflow(0, QueueInput::COMMAND, 7);
    ASSERT_EVENTS_QueueOverflow(1, QueueInput::UART_RECEIVE, 1);
    ASSERT_EVENTS_QueueOverflow(2, QueueInput::RUN_TICK, 2);
    ASSERT_EVENTS_QueueOverflow(3, QueueInput::HEALTH_PING, 3);
    ASSERT_EQ(this->tlmHistory_QueueOverflows->at(this->tlmHistory_QueueOverflows->size() - 1).arg, 13u);
    this->tick();
    ASSERT_EVENTS_QueueOverflow_SIZE(4);
    // QueueOverflow is throttled after ten; SETUP_CAMERA clears the throttle
    this->tick(KissCamConfig::REPLY_TIMEOUT_TICKS);  // the accepted PING times out: no operation in progress
    this->clearHistory();
    for (U32 cycle = 0; cycle < 3; cycle++) {
        this->overflowCycle();
    }
    ASSERT_EVENTS_QueueOverflow_SIZE(6);  // 16 fired since boot, the throttle stops at 10 (4 before clearHistory)
    ASSERT_EQ(this->tlmHistory_QueueOverflows->at(this->tlmHistory_QueueOverflows->size() - 1).arg, 13u + 12u);
    this->sendSetup(200);
    this->clearHistory();
    this->overflowCycle();
    ASSERT_EVENTS_QueueOverflow_SIZE(4);
    ASSERT_EVENTS_QueueOverflow(0, QueueInput::COMMAND, 1);
}

void SunCamMgrTester::testPeriodicChannelWrite() {
    this->tick();
    ASSERT_TLM_SetupState_SIZE(1);
    ASSERT_TLM_CurrentOperation_SIZE(1);
    ASSERT_TLM_Exchanges_SIZE(1);
    ASSERT_TLM_Exchanges(0, 0);
    ASSERT_TLM_QueueOverflows(0, 0);
    ASSERT_TLM_CameraMode_SIZE(0);  // camera channels only on reply
    this->tick(KissCamConfig::TLM_PERIOD_TICKS - 1);
    ASSERT_TLM_SetupState_SIZE(1);
    this->tick();
    ASSERT_TLM_SetupState_SIZE(2);
    ASSERT_TLM_CurrentOperation_SIZE(2);
    ASSERT_TLM_Exchanges_SIZE(2);
    ASSERT_TLM_CameraErrors_SIZE(2);
    ASSERT_TLM_Timeouts_SIZE(2);
    ASSERT_TLM_DiscardedBytes_SIZE(2);
    ASSERT_TLM_MalformedReplies_SIZE(2);
    ASSERT_TLM_ImagesCaptured_SIZE(2);
    ASSERT_TLM_LinesDownloaded_SIZE(2);
    ASSERT_TLM_ProductsSent_SIZE(2);
    ASSERT_TLM_QueueOverflows_SIZE(2);
    ASSERT_TLM_CameraMode_SIZE(0);
    // on change
    this->clearHistory();
    this->sendSetup();
    ASSERT_TLM_SetupState_SIZE(1);
    ASSERT_TLM_SetupState(0, SetupState::SETTING_UP);
    ASSERT_TLM_CurrentOperation(0, Operation::SETUP_CAMERA);
    ASSERT_TLM_Exchanges_SIZE(0);  // counted on acknowledgement, not on send
}

void SunCamMgrTester::testExposureRangeCheck() {
    this->setupNominal();
    const U16 rejected[2] = {KissCamConfig::EXPOSURE_MIN - 1, KissCamConfig::EXPOSURE_MAX + 1};
    for (U32 i = 0; i < 2; i++) {
        this->setExposure(rejected[i]);
        this->tick(2);
        ASSERT_EVENTS_SettingRejected_SIZE(i + 1);
        ASSERT_EVENTS_SettingRejected(i, Setting::EXPOSURE, rejected[i], KissCamConfig::EXPOSURE_MIN,
                                      KissCamConfig::EXPOSURE_MAX);
        ASSERT_EQ(this->m_frameCount, 5u);
        this->assertSetupState(SetupState::READY);
    }
    const U16 accepted[2] = {KissCamConfig::EXPOSURE_MIN, KissCamConfig::EXPOSURE_MAX};
    for (U32 i = 0; i < 2; i++) {
        this->setExposure(accepted[i]);
        this->tick();
        this->assertFrame(CMD_SET_EXPOSURE, static_cast<U8>(accepted[i] >> 8), static_cast<U8>(accepted[i] & 0xFF), 0,
                          0, 6 + 2 * i);
        this->ack(CMD_SET_EXPOSURE);
        this->registerReply(EXPOSURE_REGISTER, accepted[i]);
    }
    ASSERT_EVENTS_SettingApplied_SIZE(2);
    ASSERT_EVENTS_SettingRejected_SIZE(2);
    // persisted out-of-range value fails set-up at SET_EXPOSURE
    this->setExposure(KissCamConfig::EXPOSURE_MAX + 1);
    this->clearHistory();
    this->sendSetup(5);
    this->pingReply(1, 0, 1);
    this->ack(CMD_SET_MODE);
    ASSERT_EVENTS_SettingRejected_SIZE(1);
    ASSERT_EVENTS_SetupFailed(0, SetupStep::SET_EXPOSURE, FailCause::OUT_OF_RANGE);
    ASSERT_EQ(this->m_frameCount, 11u);
}

void SunCamMgrTester::testRunPortSingleQueuedTick() {
    for (U32 i = 0; i < 5; i++) {
        this->invoke_to_run(0, 0);
    }
    // exactly one tick message is queued: one dispatch runs it (first-tick channel write) ...
    this->dispatch();
    ASSERT_TLM_SetupState_SIZE(1);
    // ... and the next message in the queue is the health ping queued afterwards, not another tick
    this->invoke_to_pingIn(0, 1);
    this->dispatch();
    ASSERT_from_pingOut_SIZE(1);
    // once dispatched, every later run queues a tick again: tick TLM_PERIOD_TICKS + 1 re-writes the channels
    for (U32 i = 0; i < KissCamConfig::TLM_PERIOD_TICKS; i++) {
        this->invoke_to_run(0, 0);
        this->dispatch();
    }
    ASSERT_TLM_SetupState_SIZE(2);
    // the overflow hook clears the atomic: a tick dropped from a full queue does not block later ticks
    for (U32 i = 0; i < TEST_INSTANCE_QUEUE_DEPTH; i++) {
        this->invoke_to_pingIn(0, i);
    }
    this->invoke_to_run(0, 0);  // dropped by the hook
    this->dispatch(TEST_INSTANCE_QUEUE_DEPTH);
    this->invoke_to_run(0, 0);
    this->dispatch();
    ASSERT_EVENTS_QueueOverflow_SIZE(1);
    ASSERT_EVENTS_QueueOverflow(0, QueueInput::RUN_TICK, 1);
}

void SunCamMgrTester::testHealthPingAndDrvConnected() {
    this->invoke_to_pingIn(0, 0xCAFE);
    this->dispatch();
    ASSERT_from_pingOut_SIZE(1);
    ASSERT_from_pingOut(0, 0xCAFE);
    this->invoke_to_drvConnected(0);
    this->dispatch();
    ASSERT_EQ(this->m_frameCount, 0u);
    ASSERT_EVENTS_SIZE(0);
    this->setupNominal();
    this->invoke_to_drvConnected(0);
    this->dispatch();
    ASSERT_EQ(this->m_frameCount, 5u);
    this->assertSetupState(SetupState::READY);
}

}  // namespace KissCam
