review rounds: 3 (source: prompt) — test-design phase, review 2 of 3 — inline
Reviewer and inputs as round 1. Scope: changed rows; questions 2 and 6 over the whole list.

## Closed from round 1 (verified)
All 12 keys: rows FailedStartStoresNothing, StartStopsRunningCamera, StartChannels, StartBudget, StaleFrame, SocketInterleaved, SocketStopDiscards, StreamStalled, SocketNoHello, CounterLimit present with values; Ping remapped; sentence on REQ-CamMgr-043 present.

## Question 2
Every REQ-CamMgr row with a unit-test method has a row. 034: L2-2, L2-3. 042: inspection. Pass.

## Question 6
| Check | Result |
|---|---|
| Every named failure path has a row | fail → test-gap:read-back-unavailable |
| Assertions name values | fail → test-weak:values-2 |
| Every event and channel has a row | pass after test-weak:values-2 (`CaptureStopIgnored`, `ControlTraceDumped`, `ParameterLocked` are not named in a row) |

## Findings
| Key | Tag | Class | Site | Finding | Rewrite |
|---|---|---|---|---|---|
| test-gap:read-back-unavailable | **must fix** | test-gap | list | SDD 3.7 reads back only available features. No row; a component that faults on such a feature passes. | Row ReadBackUnavailable. |
| test-weak:values-2 | **could fix** | test-weak | ParameterLocked, StopWhileNotCapturing, DumpControlTrace | The rows do not name the event or channel. | Name `ParameterLocked` and `ParametersLocked`; `CaptureStopIgnored`; `ControlTraceDumped` and `ControlTraceEntries`. |

## Totals
must fix: 1   could fix: 1   open disagreements: 0

## Disagreement log
| Round | Key | Author position | Reviewer position | Status |
|---|---|---|---|---|

## Close (author)
Both fixed.
