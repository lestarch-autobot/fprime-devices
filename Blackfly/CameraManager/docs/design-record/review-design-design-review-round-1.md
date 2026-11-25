review rounds: 3 (source: prompt) — design phase, review 1 of 3 — inline
Inline: the campaign prompt forbids child sessions, so no separate session was tried. Local only. Preamble of `phase-review-loop.md` applied: no PR; every line is introduced.
Artifacts: `CameraManager.fpp`, `CameraManagerTypes.fpp`, generated `CameraFeatureTypes.fpp`, `CameraParams.fppi`, `CameraReadBack.fppi`, `docs/sdd.md` sections 3 to 7. Intent input: REQ-CamMgr-001..044 and the campaign prompt.
Reviewer: `design-review.agent.md` @ lestarch-autobot/fprime d488d24, categories 1, 3, 4, 6, 7, 9 (2, 5, 8 n/a: no implementation).

## Routed
| Key | Tag | Class | Site | Finding | Rewrite |
|---|---|---|---|---|---|
| design-needs-human-adjudication:CameraManager | **must fix** (routed, not counted) | design-needs-human-adjudication | component | New component, new ports, new transport interface. | Carried to the project gate. |

## Findings
| Key | Tag | Class | Site | Finding | Rewrite |
|---|---|---|---|---|---|
| design-intent-mismatch:frameReceived | **must fix** | design-intent-mismatch | `frameReceived`, `streamFailed`; SDD 3.6 | SDD admits a frame "when the frame's sequence is current", but the ports carry no sequence. A frame queued before a stop can be stored in the next sequence (REQ-CamMgr-026). | Add `sequence: U32` to both internal ports. |
| design-intent-mismatch:captureIn-stop | **must fix** | design-intent-mismatch | SDD 3.6 stop, 3.8 | Stop "waits for the reader to park" with no bound; payload and trailer reads have no stated time limit. REQ-CamMgr-043 cannot be shown. | Stop calls `streamReset()`, waits at most 500 ms for the park, else faults; payload and trailer reads wait 100 ms at a time. |
| design-intent-mismatch:capture-start-duration | **must fix** | design-intent-mismatch | SDD 3.7 | A start makes hundreds of exchanges, each allowed `ACK_TIMEOUT`; a slow camera can hold the component thread for minutes, past any health ping. | A budget for the waits of one start (`START_BUDGET_MS`); beyond it the exchange ends as `TIMEOUT` (REQ-CamMgr-040). |
| design-intent-mismatch:READ_BACK | **must fix** | design-intent-mismatch | SDD 3.7 `READ_BACK` | Reading a feature the camera reports unavailable fails and would fault a start that REQ-CamMgr-011 lets through. | Read back only features the flag register reports available. |
| design-missed-reuse:SDD-3.2 | **could fix** | design-missed-reuse | SDD 3.2 | The SDD does not say which framework facilities carry the task, lock, file and socket, or why `Drv/Ip` is not used. | Name `Os::Task`, `Os::Mutex`, `Os::ConditionVariable`, `Os::File`; state that `Drv/Ip` has no Unix stream socket. |
| design-margin-erosion:SDD-3 | **could fix** | design-margin-erosion | SDD 3 | No statement of memory, queue, task and time use per slot. | Add a Resources table. |
| design-intent-mismatch:productRecvIn | **could fix** | design-intent-mismatch | SDD 3.6 `productRecvIn` | A buffer asked for in an earlier sequence with a smaller payload can be armed for a larger one. | Arm only a buffer of the current payload size; return any other. |

Category 3: the stream reader task is examined by the architecture reviewer. Category 4: 72 parameters follow the parent's Table 1; no element outside the requirements found. Category 6: n/a, new component.

## Totals
must fix: 4   could fix: 3   open disagreements: 0

## Disagreement log
| Round | Key | Author position | Reviewer position | Status |
|---|---|---|---|---|

## Close (author)
All 7 fixed as proposed. `fpp-check` passes after the change. The start budget is new behaviour inside REQ-CamMgr-040; recorded as proposal P13.
