review rounds: 3 (source: prompt) — design phase, review 2 of 3 — inline
Inputs as round 1. Scope: elements changed by the round 1 close (internal ports, `StreamFault`, SDD 3.2, 3.5 to 3.8, 3.10) and each reviewer's whole-model checks.
Reviewer: `design-review.agent.md`, categories 1, 3, 4, 6, 7, 9.

## Routed
| Key | Tag | Class | Site | Finding | Rewrite |
|---|---|---|---|---|---|
| design-needs-human-adjudication:CameraManager | **must fix** (routed, not counted) | design-needs-human-adjudication | component | New component. | Carried to the project gate. |

## Closed from round 1 (verified)
design-intent-mismatch:frameReceived (both ports carry `sequence`; `fpp-check` passes), design-intent-mismatch:captureIn-stop, design-intent-mismatch:capture-start-duration, design-intent-mismatch:READ_BACK, design-missed-reuse:SDD-3.2, design-margin-erosion:SDD-3, design-intent-mismatch:productRecvIn.

## Findings
| Key | Tag | Class | Site | Finding | Rewrite |
|---|---|---|---|---|---|
| design-intent-mismatch:SocketTransport | **must fix** | design-intent-mismatch | SDD 3.2 | The socket protocol puts control and stream records on one connection. The component thread (acknowledgement of `AcquisitionStop`) and the reader (stream records) both read it; either can take the other's record. REQ-CamMgr-001 and REQ-CamMgr-013 fail on the socket transport. | Read under a lock; hold a record of the other channel in a per-channel area of one record; bound the lock time. |
| design-margin-erosion:READER_PARK_MS | **could fix** | design-margin-erosion | SDD 3.5, 3.6 | `ACK_TIMEOUT` plus a 500 ms park wait leaves no margin to the bound of REQ-CamMgr-043. | Park wait of 300 ms. |
| design-intent-mismatch:CaptureStopFailed | **could fix** | design-intent-mismatch | SDD 5.4 | The event table does not list the reader that did not park. | Add it to "Raised when". |
| design-intent-mismatch:StreamFault | **could fix** | design-intent-mismatch | `StreamFault`, SDD 3.8 | A frame begun and never finished has no fault value. | Add `STALLED`. |

## Totals
must fix: 1   could fix: 3   open disagreements: 0

## Disagreement log
| Round | Key | Author position | Reviewer position | Status |
|---|---|---|---|---|

## Close (author)
All 4 fixed: SDD 3.2 (read lock of at most 10 ms a poll, holding areas, write lock), `READER_PARK_MS` 300, events table, `StreamFault.STALLED`. `fpp-check` passes.
