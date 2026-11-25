review rounds: 3 (source: prompt) — design phase, review 3 of 3 — inline
Inputs as round 1. Scope: elements changed by the round 2 close (`StreamFault`, SDD 3.2, 3.5, 3.6, 3.8, 3.10, 5.4) and each reviewer's whole-model checks.
Reviewer: `design-review.agent.md`, categories 1, 3, 4, 6, 7, 9.

## Routed
| Key | Tag | Class | Site | Finding | Rewrite |
|---|---|---|---|---|---|
| design-needs-human-adjudication:CameraManager | **must fix** (routed, not counted) | design-needs-human-adjudication | component | New component. | Carried to the project gate. |

## Closed from round 2 (verified)
design-margin-erosion:READER_PARK_MS, design-intent-mismatch:CaptureStopFailed (the event's `status` argument carries `TIMEOUT` for a reader not parked), design-intent-mismatch:StreamFault.

## Findings
| Key | Tag | Class | Site | Finding | Rewrite |
|---|---|---|---|---|---|
| design-intent-mismatch:SocketTransport | **must fix** | design-intent-mismatch | SDD 3.2 | Not closed. At a stop the reader has its stop flag set and no longer takes stream records. The simulator still sends the rest of the frame. The control read then finds the stream holding area full, waits out its time limit, and the stop faults (REQ-CamMgr-013). | "While the reader's stop flag is set or the reader is parked, a stream record met by a control read is discarded." |

## Totals
must fix: 1   could fix: 0   open disagreements: 0

## Disagreement log
| Round | Key | Author position | Reviewer position | Status |
|---|---|---|---|---|

## Close (author) — after the last review, UNVERIFIED
design-intent-mismatch:SocketTransport: the rewrite was added to SDD 3.2. No review remains inside the bound to verify it; it is listed as open at the gate.
