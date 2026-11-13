// ======================================================================
// \title  SunCamMgr.cpp
// \brief  cpp file for SunCamMgr component implementation class
// ======================================================================

#include "fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.hpp"

#include <cstring>

#include "Fw/Types/Assert.hpp"
#include "fprime-devices/KissCam/KissCamConfig/FppConstantsAc.hpp"

namespace KissCam {

static_assert(KissCamConfig::REPLY_TIMEOUT_TICKS > 0, "a reply must be allowed at least one run tick");
static_assert(KissCamConfig::TLM_PERIOD_TICKS > 0, "the periodic channel write divides the tick count");

constexpr FwSizeType SunCamMgr::POWER_ON_LINE_SIZE;  // odr-used by the unit test
const U8 SunCamMgr::POWER_ON_LINE[SunCamMgr::POWER_ON_LINE_SIZE] = {0x4d, 0x56, 0x50, 0x20, 0x43, 0x61, 0x6d,
                                                                    0x65, 0x72, 0x61, 0x20, 0x56, 0x32, 0x20,
                                                                    0x52, 0x53, 0x34, 0x32, 0x32, 0x0d, 0x0a};

namespace {

//! Payload length of the acknowledgement of each command (datasheet §7.4); Download is checked separately
bool expectedPayloadLength(U8 cmd, U16& length) {
    switch (cmd) {
        case SunCamMgr::CMD_PING:
            length = 4;
            return true;
        case SunCamMgr::CMD_SET_MODE:
        case SunCamMgr::CMD_SET_EXPOSURE:
            length = 0;
            return true;
        case SunCamMgr::CMD_CAPTURE:
            // the Capture ACK carries no payload (SDD §Behavior); the exposure used is the Exposure channel value
            length = 0;
            return true;
        case SunCamMgr::CMD_READ_REGISTER:
            length = 4;
            return true;
        default:
            return false;
    }
}

U16 readU16(const U8* bytes) {
    return static_cast<U16>((static_cast<U16>(bytes[0]) << 8) | static_cast<U16>(bytes[1]));
}

U8 highByte(U16 value) {
    return static_cast<U8>(value >> 8);
}

U8 lowByte(U16 value) {
    return static_cast<U8>(value & 0xFF);
}

Setting::T settingOfStep(SetupStep::T step) {
    return (step == SetupStep::SET_MODE) ? Setting::CAMERA_MODE : Setting::EXPOSURE;
}

}  // namespace

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

SunCamMgr::SunCamMgr(const char* const compName)
    : SunCamMgrComponentBase(compName),
      m_setupState(SetupState::NOT_SETUP),
      m_operation(Operation::NONE),
      m_commandedMode(Mode::SLEEP),
      m_responsePending(false),
      m_pendingOpCode(0),
      m_pendingCmdSeq(0),
      m_pendingCmd(0),
      m_ticksWaiting(0),
      m_setupStep(SetupStep::PING_MCU),
      m_exposureSent(0),
      m_exposure(0),
      m_modeSent(CaptureMode::SD),
      m_pingType(PingType::MCU),
      m_captureSlot(Slot::SLOT_0),
      m_captureTestPattern(TestPattern::OFF),
      m_downloadPhase(DL_FIRST_LINE),
      m_downloadSlot(Slot::SLOT_0),
      m_downloadFirstSlot(Slot::SLOT_0),
      m_downloadResolution(Resolution::FULL),
      m_downloadImageMode(Mode::SD),
      m_linesPerSlot(0),
      m_pixelsPerLine(0),
      m_linesExpected(0),
      m_linesReceived(0),
      m_lineAttempt(0),
      m_abortRequested(false),
      m_abortOpCode(0),
      m_abortCmdSeq(0),
      m_rxLen(0),
      m_strayCount(0),
      m_powerOnMatch(0),
      m_runQueued(false),
      m_tickCount(0),
      m_modeDirty(false),
      m_exposureDirty(false),
      m_exchanges(0),
      m_cameraErrors(0),
      m_timeouts(0),
      m_discardedBytes(0),
      m_malformedReplies(0),
      m_imagesCaptured(0),
      m_linesDownloaded(0),
      m_productsSent(0),
      m_queueOverflows(0) {
    ::memset(this->m_pendingParams, 0, sizeof this->m_pendingParams);
    ::memset(this->m_lineRecord, 0, sizeof this->m_lineRecord);
    ::memset(this->m_txFrame, 0, sizeof this->m_txFrame);
    ::memset(this->m_rx, 0, sizeof this->m_rx);
    ::memset(this->m_strayHead, 0, sizeof this->m_strayHead);
    ::memset(this->m_drops, 0, sizeof this->m_drops);
}

SunCamMgr::~SunCamMgr() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void SunCamMgr::drvConnected_handler(FwIndexType portNum) {
    // The driver's readiness is not tracked: a frame sent while the port is down fails as SendFailed.
}

void SunCamMgr::drvReceiveIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {
    const U8* const data = buffer.getData();
    const FwSizeType size = buffer.getSize();
    if ((data != nullptr) && (size > 0)) {
        if (status == Drv::ByteStreamStatus::OP_OK) {
            this->assemble(data, size);
        } else {
            this->discard(DiscardCause::MALFORMED, data, FW_MIN(size, FRAME_HEAD_SIZE), static_cast<U32>(size));
        }
    }
    if (data != nullptr) {
        this->drvReceiveReturnOut_out(0, buffer);
    }
}

void SunCamMgr::pingIn_handler(FwIndexType portNum, U32 key) {
    this->pingOut_out(0, key);
}

void SunCamMgr::run_handler(FwIndexType portNum, U32 context) {
    if (!this->m_runQueued.exchange(true)) {
        this->runInternal_internalInterfaceInvoke();
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void SunCamMgr::SETUP_CAMERA_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (!this->acceptCommand(opCode, cmdSeq, Operation::SETUP_CAMERA, Admission::ANY_STATE)) {
        return;
    }
    {
        Os::ScopeLock lock(this->m_lock);
        this->m_modeDirty = false;
        this->m_exposureDirty = false;
    }
    this->log_ACTIVITY_LO_SetupStarted();
    this->log_WARNING_LO_FrameDiscarded_ThrottleClear();
    this->log_WARNING_HI_QueueOverflow_ThrottleClear();
    this->setSetupState(SetupState::SETTING_UP);
    this->sendSetupStep(SetupStep::PING_MCU);
}

void SunCamMgr::SLEEP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (!this->acceptCommand(opCode, cmdSeq, Operation::SLEEP, Admission::ANY_STATE)) {
        return;
    }
    if (!this->sendFrame(CMD_SET_MODE, static_cast<U8>(Mode::SLEEP), 0, 0, 0)) {
        this->exchangeFailed(FailCause::SEND_FAILED);
    }
}

void SunCamMgr::PING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const KissCam::PingType& pingType) {
    if (!this->acceptCommand(opCode, cmdSeq, Operation::PING, Admission::ANY_STATE)) {
        return;
    }
    this->m_pingType = pingType.e;
    if (!this->sendFrame(CMD_PING, static_cast<U8>(pingType.e), 0, 0, 0)) {
        this->exchangeFailed(FailCause::SEND_FAILED);
    }
}

void SunCamMgr::CAPTURE_cmdHandler(FwOpcodeType opCode,
                                   U32 cmdSeq,
                                   const KissCam::Slot& slot,
                                   const KissCam::TestPattern& testPattern) {
    if (!this->acceptCommand(opCode, cmdSeq, Operation::CAPTURE, Admission::READY_ONLY)) {
        return;
    }
    if (!this->slotAllowed(Operation::CAPTURE, slot.e)) {
        return;
    }
    this->m_captureSlot = slot.e;
    this->m_captureTestPattern = testPattern.e;
    if (!this->sendFrame(CMD_CAPTURE, static_cast<U8>(slot.e), static_cast<U8>(testPattern.e), 0, 0)) {
        this->exchangeFailed(FailCause::SEND_FAILED);
    }
}

void SunCamMgr::DOWNLOAD_cmdHandler(FwOpcodeType opCode,
                                    U32 cmdSeq,
                                    const KissCam::Slot& slot,
                                    const KissCam::Resolution& resolution,
                                    FwDpPriorityType priority) {
    if (!this->acceptCommand(opCode, cmdSeq, Operation::DOWNLOAD, Admission::READY_ONLY)) {
        return;
    }
    if (!this->slotAllowed(Operation::DOWNLOAD, slot.e)) {
        return;
    }
    // SD and Zoom images occupy the commanded slot; an HD image occupies slots 0..3 as quadrants
    const bool hd = (this->m_commandedMode == Mode::HD);
    const bool preview = (resolution.e == Resolution::PREVIEW);
    this->m_downloadSlot = slot.e;
    this->m_downloadFirstSlot = hd ? Slot::SLOT_0 : slot.e;
    this->m_downloadResolution = resolution.e;
    this->m_linesPerSlot = preview ? PREVIEW_LINES : FULL_LINES;
    this->m_pixelsPerLine = preview ? PREVIEW_PIXELS : FULL_PIXELS;
    this->m_linesExpected = static_cast<U16>(this->m_linesPerSlot * (hd ? SLOT_COUNT : 1));
    this->m_linesReceived = 0;
    this->m_lineAttempt = 0;
    this->m_abortRequested = false;
    const FwSizeType dataSize =
        SIZE_OF_ImageHeader_RECORD + static_cast<FwSizeType>(this->m_linesExpected) *
                                         SIZE_OF_ImageLine_RECORD(LINE_RECORD_PREFIX + this->m_pixelsPerLine);
    if (this->dpGet_Image(dataSize, this->m_container) != Fw::Success::SUCCESS) {
        this->log_WARNING_HI_ProductAllocationFailed(static_cast<U32>(dataSize));
        this->respond(Fw::CmdResponse::EXECUTION_ERROR);
        this->endOperation();
        return;
    }
    if (priority != 0) {
        this->m_container.setPriority(priority);
    }
    this->m_downloadPhase = DL_FIRST_LINE;
    this->requestLine();
}

bool SunCamMgr::slotAllowed(Operation::T operation, Slot::T slot) {
    if ((this->m_commandedMode == Mode::HD) && (slot != Slot::SLOT_0)) {
        this->log_WARNING_LO_CommandRejected(operation, RejectCause::HD_SLOT);
        this->respond(Fw::CmdResponse::EXECUTION_ERROR);
        this->endOperation();
        return false;
    }
    return true;
}

void SunCamMgr::DOWNLOAD_ABORT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (this->m_operation != Operation::DOWNLOAD) {
        this->log_WARNING_LO_CommandRejected(Operation::DOWNLOAD_ABORT, RejectCause::NO_DOWNLOAD);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    if (this->m_abortRequested) {
        this->log_WARNING_LO_CommandRejected(Operation::DOWNLOAD_ABORT, RejectCause::BUSY);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return;
    }
    // The download stops once the pending line reply (or its timeout) has been handled
    this->m_abortRequested = true;
    this->m_abortOpCode = opCode;
    this->m_abortCmdSeq = cmdSeq;
}

void SunCamMgr::READ_REGISTER_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U16 address) {
    if (!this->acceptCommand(opCode, cmdSeq, Operation::READ_REGISTER, Admission::READY_ONLY)) {
        return;
    }
    if (!this->sendFrame(CMD_READ_REGISTER, highByte(address), lowByte(address), 0, 0)) {
        this->exchangeFailed(FailCause::SEND_FAILED);
    }
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined internal interfaces
// ----------------------------------------------------------------------

void SunCamMgr::runInternal_internalInterfaceHandler() {
    this->m_runQueued = false;

    // Drops counted by the queue-full hooks since the last tick
    U32 drops[QueueInput::NUM_CONSTANTS] = {};
    {
        Os::ScopeLock lock(this->m_lock);
        ::memcpy(drops, this->m_drops, sizeof drops);
        ::memset(this->m_drops, 0, sizeof this->m_drops);
    }
    U32 dropped = 0;
    for (FwSizeType i = 0; i < QueueInput::NUM_CONSTANTS; i++) {
        if (drops[i] > 0) {
            this->log_WARNING_HI_QueueOverflow(static_cast<QueueInput::T>(i), drops[i]);
            dropped += drops[i];
        }
    }
    if (dropped > 0) {
        this->m_queueOverflows += dropped;
        this->tlmWrite_QueueOverflows(this->m_queueOverflows);
    }

    // Reply timeout of the exchange in progress
    if (this->m_pendingCmd != 0) {
        this->m_ticksWaiting++;
        if (this->m_ticksWaiting >= static_cast<U32>(KissCamConfig::REPLY_TIMEOUT_TICKS)) {
            this->log_WARNING_HI_ReplyTimeout(this->m_operation, this->m_pendingCmd);
            this->m_timeouts++;
            this->tlmWrite_Timeouts(this->m_timeouts);
            this->exchangeFailed(FailCause::TIMEOUT);
        }
    }

    // Deferred parameter push, only while READY and idle
    if ((this->m_setupState == SetupState::READY) && (this->m_operation == Operation::NONE)) {
        bool modeDirty = false;
        bool exposureDirty = false;
        {
            Os::ScopeLock lock(this->m_lock);
            modeDirty = this->m_modeDirty;
            exposureDirty = this->m_exposureDirty;
            this->m_modeDirty = false;
            this->m_exposureDirty = false;
        }
        if (modeDirty) {
            this->setOperation(Operation::PARAMETER_PUSH);
            this->sendSetupStep(SetupStep::SET_MODE);
        } else if (exposureDirty) {
            this->setOperation(Operation::PARAMETER_PUSH);
            this->sendSetupStep(SetupStep::SET_EXPOSURE);
        }
    }

    // Periodic re-write of the component state and counter channels
    if ((this->m_tickCount % static_cast<U32>(KissCamConfig::TLM_PERIOD_TICKS)) == 0) {
        this->writeStateChannels();
        this->writeCounterChannels();
    }
    this->m_tickCount++;
}

// ----------------------------------------------------------------------
// Queue-full hooks
// ----------------------------------------------------------------------

void SunCamMgr::drvReceiveIn_overflowHook(FwIndexType portNum,
                                          Fw::Buffer& buffer,
                                          const Drv::ByteStreamStatus& status) {
    this->countDrop(QueueInput::UART_RECEIVE);
    if (buffer.getData() != nullptr) {
        this->drvReceiveReturnOut_out(0, buffer);
    }
}

void SunCamMgr::pingIn_overflowHook(FwIndexType portNum, U32 key) {
    this->countDrop(QueueInput::HEALTH_PING);
}

void SunCamMgr::runInternal_overflowHook() {
    this->m_runQueued = false;
    this->countDrop(QueueInput::RUN_TICK);
}

void SunCamMgr::SETUP_CAMERA_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) {
    this->countDrop(QueueInput::COMMAND);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
}

void SunCamMgr::SLEEP_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) {
    this->countDrop(QueueInput::COMMAND);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
}

void SunCamMgr::PING_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) {
    this->countDrop(QueueInput::COMMAND);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
}

void SunCamMgr::CAPTURE_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) {
    this->countDrop(QueueInput::COMMAND);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
}

void SunCamMgr::DOWNLOAD_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) {
    this->countDrop(QueueInput::COMMAND);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
}

void SunCamMgr::DOWNLOAD_ABORT_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) {
    this->countDrop(QueueInput::COMMAND);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
}

void SunCamMgr::READ_REGISTER_cmdOverflowHook(FwOpcodeType opCode, U32 cmdSeq) {
    this->countDrop(QueueInput::COMMAND);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
}

void SunCamMgr::parameterUpdated(FwPrmIdType id) {
    Os::ScopeLock lock(this->m_lock);
    if (id == PARAMID_CAMERA_MODE) {
        this->m_modeDirty = true;
    } else if (id == PARAMID_EXPOSURE) {
        this->m_exposureDirty = true;
    }
}

// ----------------------------------------------------------------------
// Command admission and operation bookkeeping
// ----------------------------------------------------------------------

bool SunCamMgr::acceptCommand(FwOpcodeType opCode, U32 cmdSeq, Operation::T operation, Admission admission) {
    if (this->m_operation != Operation::NONE) {
        this->log_WARNING_LO_CommandRejected(operation, RejectCause::BUSY);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return false;
    }
    if ((admission == Admission::READY_ONLY) && (this->m_setupState != SetupState::READY)) {
        this->log_WARNING_LO_CommandRejected(operation, RejectCause::NOT_READY);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return false;
    }
    this->m_responsePending = true;
    this->m_pendingOpCode = opCode;
    this->m_pendingCmdSeq = cmdSeq;
    this->setOperation(operation);
    return true;
}

void SunCamMgr::countDrop(QueueInput::T source) {
    Os::ScopeLock lock(this->m_lock);
    FW_ASSERT(static_cast<FwSizeType>(source) < QueueInput::NUM_CONSTANTS, static_cast<FwAssertArgType>(source));
    this->m_drops[source]++;
}

void SunCamMgr::respond(Fw::CmdResponse::T response) {
    if (this->m_responsePending) {
        this->m_responsePending = false;
        this->cmdResponse_out(this->m_pendingOpCode, this->m_pendingCmdSeq, response);
    }
}

void SunCamMgr::endOperation() {
    this->m_pendingCmd = 0;
    this->m_abortRequested = false;
    this->setOperation(Operation::NONE);
}

void SunCamMgr::setSetupState(SetupState::T state) {
    if (this->m_setupState != state) {
        this->m_setupState = state;
        this->tlmWrite_SetupState(state);
    }
}

void SunCamMgr::setOperation(Operation::T operation) {
    if (this->m_operation != operation) {
        this->m_operation = operation;
        this->tlmWrite_CurrentOperation(operation);
    }
}

// ----------------------------------------------------------------------
// Camera exchanges
// ----------------------------------------------------------------------

bool SunCamMgr::sendFrame(U8 cmd, U8 p1, U8 p2, U8 p3, U8 p4) {
    FW_ASSERT(this->m_pendingCmd == 0, this->m_pendingCmd);
    // a fragment received while no exchange was pending can only be noise or a late reply
    this->discardPartialFrame(DiscardCause::NO_OPERATION);
    this->m_txFrame[0] = FRAME_START;
    this->m_txFrame[1] = cmd;
    this->m_txFrame[2] = p1;
    this->m_txFrame[3] = p2;
    this->m_txFrame[4] = p3;
    this->m_txFrame[5] = p4;
    this->m_txFrame[6] = FRAME_END;
    Fw::Buffer frame(this->m_txFrame, COMMAND_FRAME_SIZE);
    const Drv::ByteStreamStatus status = this->drvSendOut_out(0, frame);
    if (status != Drv::ByteStreamStatus::OP_OK) {
        this->log_WARNING_HI_SendFailed(this->m_operation, status);
        return false;
    }
    this->m_pendingCmd = cmd;
    this->m_pendingParams[0] = p1;
    this->m_pendingParams[1] = p2;
    this->m_pendingParams[2] = p3;
    this->m_pendingParams[3] = p4;
    this->m_ticksWaiting = 0;
    return true;
}

void SunCamMgr::exchangeFailed(FailCause::T cause) {
    this->m_pendingCmd = 0;
    // a reply left incomplete by the failed exchange must not frame the next one
    this->discardPartialFrame(DiscardCause::MALFORMED);
    switch (this->m_operation) {
        case Operation::SETUP_CAMERA:
            this->log_WARNING_HI_SetupFailed(this->m_setupStep, cause);
            this->setSetupState(SetupState::NOT_SETUP);
            this->respond(Fw::CmdResponse::EXECUTION_ERROR);
            this->endOperation();
            break;
        case Operation::PARAMETER_PUSH:
            this->log_WARNING_HI_SettingPushFailed(settingOfStep(this->m_setupStep), cause);
            this->setSetupState(SetupState::NOT_SETUP);
            this->endOperation();
            break;
        case Operation::SLEEP:
            this->setSetupState(SetupState::NOT_SETUP);
            this->respond(Fw::CmdResponse::EXECUTION_ERROR);
            this->endOperation();
            break;
        case Operation::DOWNLOAD:
            if ((cause == FailCause::MODE_MISMATCH) || (cause == FailCause::CAMERA_RESTART)) {
                this->setSetupState(SetupState::NOT_SETUP);
            }
            if (cause == FailCause::SEND_FAILED) {
                this->failDownload(DownloadFailCause::SEND_FAILED);
            } else if (cause == FailCause::CAMERA_RESTART) {
                this->failDownload(DownloadFailCause::CAMERA_RESTART);
            } else if (this->m_abortRequested) {
                this->finishDownload(false);
            } else if ((cause == FailCause::MODE_MISMATCH) ||
                       (this->m_lineAttempt >= static_cast<U8>(KissCamConfig::LINE_RETRIES))) {
                this->failDownload(DownloadFailCause::LINE_FAILED);
            } else {
                this->m_lineAttempt++;
                this->log_DIAGNOSTIC_LineRetry(static_cast<U16>(this->m_linesReceived % this->m_linesPerSlot),
                                               this->m_lineAttempt);
                this->requestLine();
            }
            break;
        case Operation::PING:
        case Operation::CAPTURE:
        case Operation::READ_REGISTER:
            if ((cause == FailCause::MODE_MISMATCH) || (cause == FailCause::CAMERA_RESTART)) {
                this->setSetupState(SetupState::NOT_SETUP);
            }
            this->respond(Fw::CmdResponse::EXECUTION_ERROR);
            this->endOperation();
            break;
        default:
            // NONE and DOWNLOAD_ABORT never own an exchange
            FW_ASSERT(0, static_cast<FwAssertArgType>(this->m_operation));
            break;
    }
}

void SunCamMgr::assemble(const U8* bytes, FwSizeType size) {
    FW_ASSERT(bytes != nullptr);
    for (FwSizeType i = 0; i < size; i++) {
        const U8 byte = bytes[i];
        if (this->m_rxLen == 0) {
            if (byte != FRAME_START) {
                // resynchronising: skipped bytes are reported as one discard
                this->strayByte(byte);
                continue;
            }
            this->flushStray();
        }
        FW_ASSERT(this->m_rxLen < REPLY_MAX_SIZE, static_cast<FwAssertArgType>(this->m_rxLen));
        this->m_rx[this->m_rxLen] = byte;
        this->m_rxLen++;
        if (this->m_rxLen < REPLY_HEADER_SIZE) {
            continue;
        }
        const U16 length = readU16(&this->m_rx[3]);
        bool malformed = false;
        if (length > REPLY_MAX_PAYLOAD) {
            malformed = true;
        } else if (this->m_rxLen == REPLY_HEADER_SIZE + static_cast<FwSizeType>(length) + 1) {
            if (byte == FRAME_END) {
                // the frame is consumed before processing: a failed exchange may reset the assembler
                const FwSizeType frameSize = this->m_rxLen;
                this->m_rxLen = 0;
                this->processReply(this->m_rx, frameSize);
            } else {
                malformed = true;
            }
        }
        if (malformed) {
            // the bytes of the bad frame start a resynchronisation run (they may hold the power-on line)
            const FwSizeType badLen = this->m_rxLen;
            this->m_rxLen = 0;
            for (FwSizeType k = 0; k < badLen; k++) {
                this->strayByte(this->m_rx[k]);
            }
        }
    }
    // a stray run ending in a prefix of the power-on line may continue in the next buffer: keep that tail
    if (this->m_powerOnMatch > 0) {
        this->discardStrayBefore(this->m_powerOnMatch);
    } else {
        this->flushStray();
    }
}

void SunCamMgr::discardStrayBefore(FwSizeType keep) {
    FW_ASSERT(static_cast<FwSizeType>(this->m_strayCount) >= keep, static_cast<FwAssertArgType>(this->m_strayCount),
              static_cast<FwAssertArgType>(keep));
    const FwSizeType before = static_cast<FwSizeType>(this->m_strayCount) - keep;
    if (before > 0) {
        this->discard(DiscardCause::MALFORMED, this->m_strayHead, FW_MIN(before, FRAME_HEAD_SIZE),
                      static_cast<U32>(before));
    }
    this->m_strayCount = static_cast<U32>(keep);
    ::memcpy(this->m_strayHead, POWER_ON_LINE, FW_MIN(keep, FRAME_HEAD_SIZE));
}

void SunCamMgr::strayByte(U8 byte) {
    if (this->m_strayCount < FRAME_HEAD_SIZE) {
        this->m_strayHead[this->m_strayCount] = byte;
    }
    this->m_strayCount++;
    // the camera's power-on line is unframed: recognise it anywhere in the stray run (REQ-034)
    if (byte == POWER_ON_LINE[this->m_powerOnMatch]) {
        this->m_powerOnMatch++;
    } else {
        this->m_powerOnMatch = (byte == POWER_ON_LINE[0]) ? 1 : 0;
    }
    if (this->m_powerOnMatch == POWER_ON_LINE_SIZE) {
        this->powerOnLineReceived();
    }
}

void SunCamMgr::flushStray() {
    if (this->m_strayCount > 0) {
        this->discard(DiscardCause::MALFORMED, this->m_strayHead,
                      FW_MIN(static_cast<FwSizeType>(this->m_strayCount), FRAME_HEAD_SIZE), this->m_strayCount);
        this->m_strayCount = 0;
    }
    this->m_powerOnMatch = 0;
}

void SunCamMgr::powerOnLineReceived() {
    // stray bytes before the line are their own discard; the line is one discard of 21 bytes (REQ-020)
    this->discardStrayBefore(POWER_ON_LINE_SIZE);
    this->flushStray();
    // nothing else while the camera was not set up
    if (this->m_setupState == SetupState::NOT_SETUP) {
        return;
    }
    this->log_WARNING_HI_CameraRestarted(this->m_operation);
    this->m_commandedMode = Mode::SLEEP;
    if (this->m_operation != Operation::NONE) {
        this->exchangeFailed(FailCause::CAMERA_RESTART);
    }
    this->setSetupState(SetupState::NOT_SETUP);
}

void SunCamMgr::discardPartialFrame(DiscardCause::T cause) {
    if (this->m_rxLen > 0) {
        this->discard(cause, this->m_rx, FW_MIN(this->m_rxLen, FRAME_HEAD_SIZE), static_cast<U32>(this->m_rxLen));
        this->m_rxLen = 0;
    }
}

void SunCamMgr::discard(DiscardCause::T cause, const U8* bytes, FwSizeType headSize, U32 totalBytes) {
    FW_ASSERT(headSize <= FRAME_HEAD_SIZE, static_cast<FwAssertArgType>(headSize));
    U8 headBytes[FRAME_HEAD_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0};
    ::memcpy(headBytes, bytes, headSize);
    this->m_discardedBytes += totalBytes;
    this->tlmWrite_DiscardedBytes(this->m_discardedBytes);
    if (cause == DiscardCause::MALFORMED) {
        this->m_malformedReplies++;
        this->tlmWrite_MalformedReplies(this->m_malformedReplies);
    }
    this->log_WARNING_LO_FrameDiscarded(cause, totalBytes, FrameHead(headBytes));
}

void SunCamMgr::processReply(const U8* frame, FwSizeType size) {
    FW_ASSERT(size >= REPLY_HEADER_SIZE + 1, static_cast<FwAssertArgType>(size));
    const U8 ack = frame[1];
    const U8 modeByte = frame[2];
    const U8* const payload = &frame[REPLY_HEADER_SIZE];
    const U16 length = static_cast<U16>(size - REPLY_HEADER_SIZE - 1);
    const FwSizeType headSize = FW_MIN(size, FRAME_HEAD_SIZE);
    const U32 bytes = static_cast<U32>(size);
    const U8 cmd = this->m_pendingCmd;

    if (cmd == 0) {
        this->discard(DiscardCause::NO_OPERATION, frame, headSize, bytes);
        return;
    }
    if (ack == ACK_ERROR) {
        if (length != 3) {
            this->discard(DiscardCause::MALFORMED, frame, headSize, bytes);
            return;
        }
        if (payload[0] != cmd) {
            this->discard(DiscardCause::WRONG_COMMAND, frame, headSize, bytes);
            return;
        }
        if (payload[1] >= ErrorType::NUM_CONSTANTS) {
            this->discard(DiscardCause::MALFORMED, frame, headSize, bytes);
            return;
        }
        const ErrorType::T errorType = static_cast<ErrorType::T>(payload[1]);
        const U8 count = payload[2];
        this->log_WARNING_HI_CameraError(this->m_operation, cmd, errorType, count);
        this->tlmWrite_LastCameraError(CameraErrorInfo(cmd, errorType, count));
        this->tlmWrite_CameraInvalidCmds(count);
        this->m_cameraErrors++;
        this->tlmWrite_CameraErrors(this->m_cameraErrors);
        if (modeByte < Mode::NUM_CONSTANTS) {
            this->tlmWrite_CameraMode(static_cast<Mode::T>(modeByte));
        }
        this->exchangeFailed(FailCause::CAMERA_ERROR);
        return;
    }
    if (ack != cmd) {
        this->discard(DiscardCause::WRONG_COMMAND, frame, headSize, bytes);
        return;
    }
    U16 expected = 0;
    if (cmd == CMD_DOWNLOAD) {
        // line echo then exactly one line of pixels; anything else is not stored and the line
        // is re-requested when the reply times out
        if (length != static_cast<U16>(LINE_ECHO_SIZE + this->m_pixelsPerLine)) {
            this->discard(DiscardCause::MALFORMED, frame, headSize, bytes);
            return;
        }
        if ((payload[0] != this->m_pendingParams[2]) || (payload[1] != this->m_pendingParams[3])) {
            this->discard(DiscardCause::WRONG_COMMAND, frame, headSize, bytes);
            return;
        }
    } else if (!expectedPayloadLength(cmd, expected) || (length != expected)) {
        this->discard(DiscardCause::MALFORMED, frame, headSize, bytes);
        return;
    } else if ((cmd == CMD_READ_REGISTER) &&
               ((payload[0] != this->m_pendingParams[0]) || (payload[1] != this->m_pendingParams[1]))) {
        this->discard(DiscardCause::WRONG_COMMAND, frame, headSize, bytes);
        return;
    }
    if (modeByte >= Mode::NUM_CONSTANTS) {
        this->discard(DiscardCause::MALFORMED, frame, headSize, bytes);
        return;
    }
    const Mode::T reported = static_cast<Mode::T>(modeByte);
    this->tlmWrite_CameraMode(reported);
    if ((this->m_setupState == SetupState::READY) && (this->m_operation != Operation::SLEEP)) {
        const Mode::T commanded =
            (cmd == CMD_SET_MODE) ? static_cast<Mode::T>(this->m_pendingParams[0]) : this->m_commandedMode;
        if (reported != commanded) {
            this->log_WARNING_HI_ModeMismatch(commanded, reported);
            this->exchangeFailed(FailCause::MODE_MISMATCH);
            return;
        }
    }
    this->m_exchanges++;
    this->tlmWrite_Exchanges(this->m_exchanges);
    this->m_pendingCmd = 0;
    this->handleReply(reported, payload, length);
}

void SunCamMgr::handleReply(Mode::T reported, const U8* payload, U16 length) {
    switch (this->m_txFrame[1]) {
        case CMD_PING: {
            const U8 validCmds = payload[0];
            const U8 invalidCmds = payload[1];
            const U16 uptime = readU16(&payload[2]);
            this->tlmWrite_CameraValidCmds(validCmds);
            this->tlmWrite_CameraInvalidCmds(invalidCmds);
            this->tlmWrite_CameraUptime(uptime);
            if (this->m_operation == Operation::PING) {
                this->log_ACTIVITY_LO_PingReply(this->m_pingType, validCmds, invalidCmds, uptime);
                this->respond(Fw::CmdResponse::OK);
                this->endOperation();
            } else if (this->m_setupStep == SetupStep::PING_MCU) {
                this->sendSetupStep(SetupStep::SET_MODE);
            } else {
                this->finishSetupOrPush();
            }
            break;
        }
        case CMD_SET_MODE: {
            const Mode::T mode = static_cast<Mode::T>(this->m_pendingParams[0]);
            this->m_commandedMode = mode;
            if (this->m_operation == Operation::SLEEP) {
                this->log_ACTIVITY_HI_SleepEntered();
                this->setSetupState(SetupState::NOT_SETUP);
                this->respond(Fw::CmdResponse::OK);
                this->endOperation();
            } else {
                this->log_ACTIVITY_HI_SettingApplied(Setting::CAMERA_MODE, static_cast<U16>(mode));
                this->sendSetupStep(SetupStep::SET_EXPOSURE);
            }
            break;
        }
        case CMD_SET_EXPOSURE:
            this->log_ACTIVITY_HI_SettingApplied(Setting::EXPOSURE, this->m_exposureSent);
            this->m_exposure = this->m_exposureSent;
            this->tlmWrite_Exposure(this->m_exposure);
            this->sendSetupStep(SetupStep::READ_EXPOSURE);
            break;
        case CMD_READ_REGISTER: {
            const U16 address = readU16(&payload[0]);
            const U16 value = readU16(&payload[2]);
            if (this->m_operation == Operation::READ_REGISTER) {
                this->log_ACTIVITY_HI_RegisterRead(address, value);
                this->tlmWrite_RegisterValue(RegisterValue(address, value));
                this->respond(Fw::CmdResponse::OK);
                this->endOperation();
            } else {
                this->log_ACTIVITY_LO_ExposureReadback(this->m_exposureSent, value);
                this->tlmWrite_ExposureReadback(value);
                if (value != this->m_exposureSent) {
                    this->exchangeFailed(FailCause::EXPOSURE_MISMATCH);
                } else if (this->m_operation == Operation::SETUP_CAMERA) {
                    this->sendSetupStep(SetupStep::PING_SENSOR);
                } else {
                    this->finishSetupOrPush();
                }
            }
            break;
        }
        case CMD_CAPTURE: {
            // the ACK carries no exposure: the one used is the last the camera acknowledged (REQ-033)
            this->log_ACTIVITY_HI_CaptureComplete(this->m_captureSlot, this->m_captureTestPattern, this->m_exposure,
                                                  reported);
            this->m_imagesCaptured++;
            this->tlmWrite_ImagesCaptured(this->m_imagesCaptured);
            this->respond(Fw::CmdResponse::OK);
            this->endOperation();
            break;
        }
        case CMD_DOWNLOAD:
            if (this->m_downloadPhase == DL_FIRST_LINE) {
                this->writeImageHeader(reported);
            }
            this->storeLine(payload, length);
            break;
        default:
            FW_ASSERT(0, this->m_txFrame[1]);
            break;
    }
}

// ----------------------------------------------------------------------
// Set-up, parameter push, download sequencing
// ----------------------------------------------------------------------

void SunCamMgr::sendSetupStep(SetupStep::T step) {
    this->m_setupStep = step;
    if ((step == SetupStep::SET_EXPOSURE) && !this->takeExposureParameter()) {
        return;
    }
    bool sent = false;
    switch (step) {
        case SetupStep::PING_MCU:
            sent = this->sendFrame(CMD_PING, static_cast<U8>(PingType::MCU), 0, 0, 0);
            break;
        case SetupStep::PING_SENSOR:
            sent = this->sendFrame(CMD_PING, static_cast<U8>(PingType::MCU_AND_SENSOR), 0, 0, 0);
            break;
        case SetupStep::SET_MODE: {
            Fw::ParamValid valid;  // an INVALID parameter reads as its FPP default, which is a legal mode
            this->m_modeSent = this->paramGet_CAMERA_MODE(valid).e;
            sent = this->sendFrame(CMD_SET_MODE, static_cast<U8>(this->m_modeSent), 0, 0, 0);
            break;
        }
        case SetupStep::SET_EXPOSURE:
            sent =
                this->sendFrame(CMD_SET_EXPOSURE, highByte(this->m_exposureSent), lowByte(this->m_exposureSent), 0, 0);
            break;
        case SetupStep::READ_EXPOSURE:
            sent = this->sendFrame(CMD_READ_REGISTER, highByte(EXPOSURE_REGISTER), lowByte(EXPOSURE_REGISTER), 0, 0);
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(step));
            break;
    }
    if (!sent) {
        this->exchangeFailed(FailCause::SEND_FAILED);
    }
}

bool SunCamMgr::takeExposureParameter() {
    Fw::ParamValid valid;  // an INVALID parameter reads as its FPP default, which is in range
    const U16 exposure = this->paramGet_EXPOSURE(valid);
    if (this->exposureInRange(exposure)) {
        this->m_exposureSent = exposure;
        return true;
    }
    if (this->m_operation == Operation::SETUP_CAMERA) {
        this->log_WARNING_HI_SetupFailed(SetupStep::SET_EXPOSURE, FailCause::OUT_OF_RANGE);
        this->setSetupState(SetupState::NOT_SETUP);
        this->respond(Fw::CmdResponse::EXECUTION_ERROR);
    } else {
        // a refused push ends without a frame; the applied value stands and READY is kept
    }
    this->endOperation();
    return false;
}

void SunCamMgr::finishSetupOrPush() {
    if (this->m_operation == Operation::SETUP_CAMERA) {
        this->log_ACTIVITY_HI_SetupComplete(this->m_modeSent, this->m_exposureSent);
        this->setSetupState(SetupState::READY);
        this->respond(Fw::CmdResponse::OK);
    }
    this->endOperation();
}

bool SunCamMgr::exposureInRange(U16 exposure) {
    if ((exposure < static_cast<U16>(KissCamConfig::EXPOSURE_MIN)) ||
        (exposure > static_cast<U16>(KissCamConfig::EXPOSURE_MAX))) {
        this->log_WARNING_LO_SettingRejected(Setting::EXPOSURE, exposure, static_cast<U16>(KissCamConfig::EXPOSURE_MIN),
                                             static_cast<U16>(KissCamConfig::EXPOSURE_MAX));
        return false;
    }
    return true;
}

void SunCamMgr::writeImageHeader(Mode::T reported) {
    // the image mode is the mode byte of the first line reply; HD quadrants double both dimensions
    const bool hd = (this->m_linesExpected > this->m_linesPerSlot);
    const U16 sides = hd ? 2 : 1;
    this->m_downloadImageMode = reported;
    const ImageHeader header(this->m_downloadSlot, reported, this->m_downloadResolution,
                             static_cast<U16>(this->m_pixelsPerLine * sides),
                             static_cast<U16>(this->m_linesPerSlot * sides), this->m_exposure, this->m_linesExpected);
    const Fw::SerializeStatus status = this->m_container.serializeRecord_ImageHeader(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));
    this->m_downloadPhase = DL_LINES;
    this->log_ACTIVITY_LO_DownloadStarted(this->m_downloadSlot, reported, this->m_downloadResolution,
                                          this->m_linesExpected);
}

void SunCamMgr::requestLine() {
    FW_ASSERT(this->m_linesPerSlot > 0);
    const U8 slot =
        static_cast<U8>(static_cast<U16>(this->m_downloadFirstSlot) + this->m_linesReceived / this->m_linesPerSlot);
    const U16 line = static_cast<U16>(this->m_linesReceived % this->m_linesPerSlot);
    const U8 preview = (this->m_downloadResolution == Resolution::PREVIEW) ? 1 : 0;
    if (!this->sendFrame(CMD_DOWNLOAD, slot, preview, highByte(line), lowByte(line))) {
        this->exchangeFailed(FailCause::SEND_FAILED);
    }
}

void SunCamMgr::storeLine(const U8* payload, U16 length) {
    FW_ASSERT(length >= 2, length);
    const FwSizeType pixels = static_cast<FwSizeType>(length) - 2;
    FW_ASSERT(LINE_RECORD_PREFIX + pixels <= sizeof this->m_lineRecord, static_cast<FwAssertArgType>(pixels));
    this->m_lineRecord[0] = this->m_pendingParams[0];  // slot as requested
    this->m_lineRecord[1] = payload[0];                // line number echo, MSB first
    this->m_lineRecord[2] = payload[1];
    ::memcpy(&this->m_lineRecord[LINE_RECORD_PREFIX], &payload[2], pixels);
    const Fw::SerializeStatus status =
        this->m_container.serializeRecord_ImageLine(this->m_lineRecord, LINE_RECORD_PREFIX + pixels);
    if (status != Fw::FW_SERIALIZE_OK) {
        this->failDownload(DownloadFailCause::PRODUCT_FULL);
        return;
    }
    this->m_linesReceived++;
    this->m_linesDownloaded++;
    this->tlmWrite_LinesDownloaded(this->m_linesDownloaded);
    this->m_lineAttempt = 0;
    if (this->m_abortRequested) {
        this->finishDownload(false);
    } else if (this->m_linesReceived >= this->m_linesExpected) {
        this->finishDownload(true);
    } else {
        this->requestLine();
    }
}

void SunCamMgr::finishDownload(bool complete) {
    if (complete) {
        this->log_ACTIVITY_HI_DownloadComplete(this->m_downloadSlot, this->m_downloadImageMode,
                                               this->m_downloadResolution, this->m_linesExpected,
                                               static_cast<U32>(this->m_linesReceived) * this->m_pixelsPerLine);
    } else {
        this->log_ACTIVITY_HI_DownloadAborted(this->m_downloadSlot, this->m_linesReceived);
    }
    this->closeDownload(Fw::CmdResponse::OK);
}

void SunCamMgr::closeDownload(Fw::CmdResponse::T response) {
    // the product was allocated at the command: whatever was received is delivered
    this->dpSend(this->m_container);
    this->m_productsSent++;
    this->tlmWrite_ProductsSent(this->m_productsSent);
    this->respond(response);
    if (this->m_abortRequested) {
        this->cmdResponse_out(this->m_abortOpCode, this->m_abortCmdSeq, Fw::CmdResponse::OK);
    }
    this->endOperation();
}

void SunCamMgr::failDownload(DownloadFailCause::T cause) {
    this->log_WARNING_HI_DownloadFailed(this->m_downloadSlot, this->m_linesReceived, cause);
    this->closeDownload(Fw::CmdResponse::EXECUTION_ERROR);
}

// ----------------------------------------------------------------------
// Telemetry
// ----------------------------------------------------------------------

void SunCamMgr::writeStateChannels() {
    this->tlmWrite_SetupState(this->m_setupState);
    this->tlmWrite_CurrentOperation(this->m_operation);
}

void SunCamMgr::writeCounterChannels() {
    this->tlmWrite_Exchanges(this->m_exchanges);
    this->tlmWrite_CameraErrors(this->m_cameraErrors);
    this->tlmWrite_Timeouts(this->m_timeouts);
    this->tlmWrite_DiscardedBytes(this->m_discardedBytes);
    this->tlmWrite_MalformedReplies(this->m_malformedReplies);
    this->tlmWrite_ImagesCaptured(this->m_imagesCaptured);
    this->tlmWrite_LinesDownloaded(this->m_linesDownloaded);
    this->tlmWrite_ProductsSent(this->m_productsSent);
    this->tlmWrite_QueueOverflows(this->m_queueOverflows);
}

}  // namespace KissCam
