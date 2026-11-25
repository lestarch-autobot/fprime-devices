review rounds: 3 (source: prompt) — requirements phase, review 1 of 3 — inline
Reviewer: `requirements-review.agent.md`, `fprime-requirements-review` @ lestarch-autobot/fprime d488d24. Inline: the campaign prompt forbids child sessions ("do not create child sessions"), so no separate session was tried. Local only.
Artifact: `docs/sdd.md` `## 2. Requirements`, REQ-CamMgr-001..042. Intent input: the campaign prompt and REQ-LDC rows allocated to CameraManager (ldc-project @ aa3b9df).

## Request elements, listed before reading the rows
26 parent rows REQ-LDC-001, 002, 003, 004, 005, 006, 007, 008, 009, 015, 016, 017, 021, 022, 023, 024, 026, 027, 028, 029, 033, 034, 036, 040, 041, 077; "independently configured imaging rates"; "`libusb` is allowed, Spinnaker is not"; "register table ... static at run time"; "same USB3 Vision / GenCP bytes over a local socket below CameraManager's transport interface"; "no allocation after initialization".

## Per-requirement
| ID | RQ-1 | RQ-2 | RQ-3 | RQ-4 |
|---|---|---|---|---|
| 001 | pass | pass | pass | pass |
| 002 | fail → req-not-verifiable:REQ-CamMgr-002 | pass | pass | pass |
| 003–012 | pass | pass | pass | pass |
| 013 | pass | pass | pass | fail → req-no-verification:REQ-CamMgr-013 |
| 014–018 | pass | pass | pass | pass |
| 019 | pass | pass | pass | pass |
| 020–028 | pass | pass | pass | pass |
| 029 | pass | fail → req-unclear:REQ-CamMgr-029 | pass | pass |
| 030 | pass | pass | pass | pass |
| 031 | fail → req-not-verifiable:REQ-CamMgr-031 | pass | pass | pass |
| 032–035 | pass | pass | pass | pass |
| 036 | fail → req-unclear:REQ-CamMgr-036 | pass | pass | pass |
| 037–039 | pass | pass | pass | pass |
| 040 | pass | fail → req-unclear:REQ-CamMgr-040 | pass | pass |
| 041 | pass | pass | pass | pass |
| 042 | pass | fail → req-unclear:REQ-CamMgr-042 | fail → req-no-rationale:REQ-CamMgr-042 | pass |

RQ-3 reads the `Traces to` column as the rationale (a parent REQ-ID).

## Request trace (RS-1)
| Request element (quoted) | REQ-IDs | Covered? |
|---|---|---|
| REQ-LDC-001 | 012, 022 | pass |
| REQ-LDC-002 | 001 | pass |
| REQ-LDC-003 | 002, 031, 032, 036, 037, 040 | pass |
| REQ-LDC-004 | 003, 004 | pass |
| REQ-LDC-005 | 005 | pass |
| REQ-LDC-006 | 005, 006 | pass |
| REQ-LDC-007 | 008 | pass |
| REQ-LDC-008 | 009, 010, 011 | pass |
| REQ-LDC-009 | 006, 034, 035 | pass |
| REQ-LDC-015 | 012, 014 | pass |
| REQ-LDC-016 | 013, 015, 041 | fail → req-no-verification:REQ-CamMgr-013 |
| REQ-LDC-017 | 012, 038, 039 | pass |
| REQ-LDC-021 | 016 | pass (disabled is reported by CameraCoordinator, set row) |
| REQ-LDC-022 | 017 | pass |
| REQ-LDC-023 | 018 | pass |
| REQ-LDC-024 | 019 | pass |
| REQ-LDC-026 | 015, 020, 021, 040 | pass |
| REQ-LDC-027 | 027, 037 | pass |
| REQ-LDC-028 | 001, 002 | pass |
| REQ-LDC-029 | 002, 031, 032, 033 | pass |
| REQ-LDC-033 | 022, 023, 030 | pass |
| REQ-LDC-034 | 024, 025 | pass |
| REQ-LDC-036 | 026 | pass |
| REQ-LDC-040 | 029, 030 | pass |
| REQ-LDC-041 | 028, 029 | pass |
| REQ-LDC-077 | 007 | pass |
| "independently configured imaging rates" | 006, 034 | pass |
| "same USB3 Vision / GenCP bytes over a local socket" | 001, 002 | pass |
| "register table ... static at run time" | 003 | pass |
| "no allocation after initialization" | 042 | pass |
| "`libusb` is allowed, Spinnaker is not" | — | pass (waived: a build constraint, External References names `libusb`) |

## Fault scenarios (RS-2)
| Interface / input / resource | Scenario | REQ-ID or waiver | Covered? |
|---|---|---|---|
| capture request | start while capturing | 014 | pass |
| capture request | stop while not capturing | 015 | pass |
| capture request | full queue | — | fail → req-set-no-fault-coverage:scheduler-and-request-queue/full |
| parameter | value the camera refuses | 009, 010 | pass |
| parameter | value outside its type | waiver, section 7: `Svc.CmdDispatcher`, `Svc.PrmDb` | pass (waived) |
| parameter | locked by the camera | 011 | pass |
| camera control | no acknowledgement | 017, 040, 041 | pass |
| camera control | wrong acknowledgement | 036 | pass |
| camera control | error status | 009, 017, 040 | pass |
| camera open | cannot open, wrong camera | 020, 004 | pass |
| camera | already acquiring at start | 039 | pass |
| camera stream | no image | 018 | pass |
| camera stream | wrong size | 027 | pass |
| camera stream | out of step | 037 | pass |
| camera stream | read error that is not a disconnect | — | fail → req-set-no-fault-coverage:camera-stream/read-error |
| transport | disconnect | 019 | pass |
| image buffer | none, refused | 028, 029 | pass |
| product write | fails | waiver, section 7: ImageCatalog, `Svc.DpWriter` | pass (waived) |
| trace file | cannot be written | 033 | pass |
| trace memory | more exchanges than kept | — | fail → req-not-verifiable:REQ-CamMgr-031 |
| component thread | stops answering | waiver, section 7: `Svc.Health` | pass (waived) |

## Consistency (RS-3)
IDs unique. REQ-CamMgr-013 ("within 1 s") cannot be met together with REQ-CamMgr-041 at the default `ACK_TIMEOUT` of 1000 ms: a stop that waits a full timeout answers after 1 s. → req-set-inconsistent:REQ-CamMgr-013+REQ-CamMgr-041.

## Findings
| Key | Tag | Class | Row / element | Finding | Rewrite |
|---|---|---|---|---|---|
| req-not-verifiable:REQ-CamMgr-002 | **must fix** | req-not-verifiable | 002 | "comply with" two whole standards has no pass criterion. | "The component shall send GenCP 1.0 ReadMem and WriteMem commands and accept USB3 Vision 1.0 image leaders, payloads and trailers." |
| req-no-verification:REQ-CamMgr-013 | **must fix** | req-no-verification | 013 | "within 1 s" is real timing; a unit test cannot observe it, and it is listed first. | Remove the time from 013; add a row for the time with Integration Test first. |
| req-set-inconsistent:REQ-CamMgr-013+REQ-CamMgr-041 | **must fix** | req-set-inconsistent | 013, 041 | 1 s bound conflicts with an `ACK_TIMEOUT` of 1000 ms. | "A capture stop request shall be answered with an idle or faulted report within `ACK_TIMEOUT` plus 500 ms." |
| req-not-verifiable:REQ-CamMgr-031 | **must fix** | req-not-verifiable | 031 | No bound on the exchanges recorded; memory is bounded. | "... of the first 1024 control exchanges since the capture start request." |
| req-set-no-fault-coverage:scheduler-and-request-queue/full | **must fix** | req-set-no-fault-coverage | set | No row or waiver for a full queue. | "A scheduler tick arriving at a full queue shall be dropped, capture continuing." |
| req-set-no-fault-coverage:camera-stream/read-error | **must fix** | req-set-no-fault-coverage | 019 | A stream read error that is not a disconnect has no row. | "A transport disconnect or read error during capture shall fault the slot with a `WARNING_HI` event." |
| req-no-rationale:REQ-CamMgr-042 | **must fix** | req-no-rationale | 042 | "request: flight-grade code" quotes no request element. | Trace to the quoted element "no allocation after initialization". |
| req-unclear:REQ-CamMgr-042 | **could fix** | req-unclear | 042 | "take memory" is not crisp. | "The component's code shall allocate no memory after startup configuration." |
| req-unclear:REQ-CamMgr-036 | **could fix** | req-unclear | 036 | "ignored" and "does not match" are loose. | "An acknowledgement whose request ID or command differs from the command sent shall not complete the exchange." |
| req-unclear:REQ-CamMgr-040 | **could fix** | req-unclear | 040 | "Any other" depends on row order. | "A failed control exchange of a capture start, other than a parameter write or `AcquisitionStart`, shall fault ..." |
| req-unclear:REQ-CamMgr-029 | **could fix** | req-unclear | 029 | "tick" is undefined in the row. | "... at the next scheduler tick ..." |

## Totals
must fix: 7   could fix: 4   open disagreements: 0

## Disagreement log
| Round | Key | Author position | Reviewer position | Status |
|---|---|---|---|---|

## Close (author)
All 11 fixed with the rewrites above. New rows: REQ-CamMgr-043 (stop time), REQ-CamMgr-044 (full queue). A full queue on a capture request, a command or a buffer answer asserts (F Prime `assert` queue policy); recorded as a waiver in section 7.
