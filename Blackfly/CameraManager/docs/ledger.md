# Campaign ledger — CamMgr (CameraManager)

- Campaign: `CamMgr`, component `Blackfly.CameraManager`, child of the LDC project campaign. Approver: `project-campaign`.
- Lane: DESIGN only. Stops at the Approve SDD hand-off. No implementation, no unit tests, no bench use.
- Inputs: ldc-project `devel` @ aa3b9df (ask, SSDD, sketch, plan); fprime-devices `blackfly-pathfinder` @ ad7ce9f; CamSim SDD @ 1dcfe67; skills lestarch-autobot/fprime @ d488d24; catalog nasa/fprime @ 433db6e.
- Marks: [V] verified in this lane, [I] inference.

## Deliverables

| Item | Path |
|---|---|
| SDD | `docs/sdd.md` |
| Model | `CameraManager.fpp`, `CameraManagerTypes.fpp`, generated `CameraFeatureTypes.fpp`, `CameraParams.fppi`, `CameraReadBack.fppi` |
| Generator of the parameter model | `docs/design-record/gen_model.py` (input: the pathfinder `blackfly_regtable.json`) |
| Review records | `docs/design-record/review-*.md` |
| Checklist, design stage | `docs/checklist.md` |

## Checks

- [V] `fpp-check` of the model with the catalog's `Fw`, `Svc.Sched`, `Svc.Ping` and default configuration models: exit 0 (fprime-fpp 4.0.0a2).
- [V] All 26 allocated REQ-LDC rows are in the `Traces to` column (mapping in `review-requirements-requirements-review-round-3.md`).
- [V] Checklist, design stage: rows 1.1 to 1.6 decided YES; tool NO rows 2.5, 2.6 (no implementation yet) and 5.3 (hardware run) belong to later stages. The clang-tidy plugin was not built: static-analysis rows are UNCHECKED.
- Not done: no code generation or build of the component (DEVELOPMENT lane).

## Review loops (bound 3, source: prompt; all reviews inline, the prompt forbids child sessions)

| Loop | Reviewer | Round 1 | Round 2 | Round 3 (final) |
|---|---|---|---|---|
| Requirements | requirements-review | 7 / 4 / 0 | 2 / 0 / 0 | 0 / 0 / 0 |
| Design | design-review | 4 / 3 / 0 | 1 / 3 / 0 | 1 / 0 / 0 |
| Design | architecture-review | 1 / 1 / 0 | 0 / 1 / 0 | 0 / 0 / 0 |
| Design | design-principles-review | 0 / 3 / 0 | 0 / 0 / 0 | 0 / 0 / 0 |
| Test design | system-test-review Mode A | 6 / 6 / 0 | 1 / 1 / 0 | 0 / 1 / 0 |

Counts are must fix / could fix / open disagreements.

Open at the gate:
1. Routed: `design-needs-human-adjudication:CameraManager` (new component; not counted).
2. `design-intent-mismatch:SocketTransport` (must fix, design round 3): stream records met by a control read at a stop. The author added the rule to SDD 3.2 after the last review; it is unverified.
3. `test-weak:ImagesStored` (could fix, test-design round 3): fixed by the author after the last review; unverified.
4. System-test-review questions 1, 3, 4, 5 concern the LDC system plan and were not asked here.

## Decisions and proposals for the project gate

| # | Proposal | Basis |
|---|---|---|
| P1 | Only the capture-defining parameters have defaults and are always written: `THROUGHPUT_LIMIT` 500000000, `PIXEL_FORMAT` BayerRG8, `EXPOSURE_AUTO` Off, `EXPOSURE_TIME` 1000 us, `GAIN_AUTO` Off, `GAIN` 0 dB, `FRAME_RATE_ENABLE` true, `FRAME_RATE` 1 Hz. Every other parameter has no default and is written only once set; the camera keeps its own value. Alternative: a default for all 72. | [I] the pathfinder's full-rate configuration; the camera powers up with automatic modes on |
| P2 | Two timeout parameters, `ACK_TIMEOUT` (ms, 1000) and `IMAGE_TIMEOUT` (ms, 5000). | [V] REQ-LDC-022 and 023 each name a timeout parameter; the plan uses `ACK_TIMEOUT` |
| P3 | A feature the camera reports locked or unavailable is skipped, counted and reported with `ParameterLocked`; it is not a refusal (REQ-CamMgr-011). | [V] pathfinder: `ExposureTime` is locked while `ExposureAuto` is Continuous |
| P4 | A camera whose device-description SHA-1 differs from the pinned one is refused (`CameraIdentityMismatch`). | [I] the static table is only right for that description |
| P5 | `SlotStatus.slotState` is the enum `Blackfly.SlotState` {DISABLED, IDLE, CAPTURING, FAULTED}; the sketch had `U8`. CameraManager never reports DISABLED. | change to the sketch's port shape |
| P6 | New output port `bufferReturnOut: Fw.BufferSend` returns an image buffer that will hold no product. ImageStore must expose its buffer manager's return port. | new connection in the topology |
| P7 | 33 parameters come from the SSDD's Table 1 but are not in the committed 109-feature register table; their names, types and enumeration entries are [I] (the device description is not on this machine) and marked "Table 1 name" in the SDD. The SSDD counts 14 AutoExposure features; this design has 13. First DEVELOPMENT step: regenerate the table from the device description and correct the model. CameraSimulator must use the same regenerated table, or it answers these reads with an address error and L2-4 fails. | [V] table has 109 features |
| P8 | No control traffic between `AcquisitionStart` and the stop request; a parameter set during capture is stored only. | REQ-LDC-077; keeps the control trace deterministic for REQ-LDC-029 |
| P9 | No component-level system plan. `## 9. Tests` lists the LDC system tests that exercise the component; the component's own evidence is the unit-test list. | same form as CamSim |
| P10 | Capture time is the system time read by the stream reader when the trailer arrives. | REQ-LDC-034 "system time at image arrival" |
| P11 | REQ-CamMgr-042 covers the component's own code; `libusb` allocates inside its calls. | [V] `libusb` 1.0.27 allocates transfers internally |
| P12 | `AasRoi*` parameters use the automatic-exposure selector entry only; `TEST_PATTERN` uses the sensor generator. | [I] |
| P13 | A capture start is limited to 10 s of transport waits (`START_BUDGET_MS`); beyond it the start fails with `ControlFailed` `TIMEOUT`. The health ping period must be longer, or the budget lowered. | design review round 1 |
| P14 | REQ-CamMgr-043: a stop is answered within `ACK_TIMEOUT` plus 500 ms (1.5 s at the default, inside the system's 2 s). An `ACK_TIMEOUT` above 1500 ms breaks REQ-LDC-016. | requirements review round 1 |
| P15 | The socket transport uses the CameraSimulator record format unchanged (CamSim P1): 8-byte header, one record per transfer, HELLO, RESET, `/tmp/blackfly-camsim-N.sock`. No difference. | CamSim SDD @ 1dcfe67 |

## Observations for the project campaign

- Plan L5-2 sets `TRIGGER_SOURCE` to a source that never fires. The camera only waits for a trigger when `TRIGGER_MODE` is On [V: CamSim SDD; I: camera]. The procedure also needs `TRIGGER_MODE_PRM_SET(On)`. No name is changed.
- Risk [I]: the framework computes the product checksum over 24.5 MB per image on the component thread; at 15 images per second this may limit REQ-LDC-010. To be measured in development.
- No plan name was changed: `DUMP_CONTROL_TRACE`, `FRAME_RATE`, `EXPOSURE_TIME`, `GAIN`, `TRIGGER_SOURCE`, `ACK_TIMEOUT` are as in the plan.
- Vendor material: none committed. The device description is named by hash only.

## Changes to the sketch

| Sketch | Now | Why |
|---|---|---|
| `SlotStatus(slot: U8, slotState: U8)` | `SlotStatus(slot: U8, slotState: SlotState)` | typed state (P5) |
| — | `bufferReturnOut` | unused buffer (P6) |
| element boundaries | unchanged | — |

## Gate

Gate 1 (Approve SDD): handed to the project campaign with the final message of this session. Not approved until the project campaign says so.
