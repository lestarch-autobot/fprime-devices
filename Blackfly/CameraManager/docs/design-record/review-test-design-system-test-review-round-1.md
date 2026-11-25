review rounds: 3 (source: prompt) — test-design phase, review 1 of 3 — inline
Reviewer: `system-test-review.agent.md` Mode A @ lestarch-autobot/fprime d488d24. Inline: the campaign prompt forbids child sessions. Local only.
Artifact: `docs/sdd.md` "8. Unit-test list" and "9. Tests". Intent input: REQ-CamMgr-001..044, SDD sections 3 to 7, the FPP model.
Questions 1, 3, 4, 5 (pass rules, outside evidence, fault menu, deployment) are about a system plan. This component has none of its own: section 9 points to the LDC plan, owned and reviewed by the project campaign. They are answered there. Questions 2 and 6 are answered here.

## Question 2 — requirement with no test and no waiver
| REQ | Unit-test rows | System tests | Result |
|---|---|---|---|
| 001–009, 011–038, 040, 041, 044 | present | as section 9 | pass, except the keys below |
| 010 | ParameterRefused | — | fail → test-gap:REQ-CamMgr-010 |
| 039 | StartCaptures | — | fail → test-gap:REQ-CamMgr-039 |
| 034 | none (timing) | L2-2, L2-3 | pass |
| 042 | inspection | — | pass |
| 043 | StopBounded | L2-1 | fail → test-weak:REQ-CamMgr-043 |

## Question 6 — unit-test list
| Check | Result |
|---|---|
| Every unit-test REQ has a row naming values | fail → test-gap:REQ-CamMgr-010, test-gap:REQ-CamMgr-039, test-weak:values |
| Every interface element has a row | fail → test-gap:start-channels (`CameraSerial`, `PayloadSize`, `ResultingFrameRate`, `Sequence`) |
| Every named failure path has a row | fail → test-gap:start-budget, test-gap:stale-frame, test-gap:socket-interleave, test-weak:stalled, test-weak:hello, test-weak:counter-limit |
| No row restates mechanics | fail → test-weak:Ping |

## Findings
| Key | Tag | Class | Site | Finding | Rewrite |
|---|---|---|---|---|---|
| test-gap:REQ-CamMgr-010 | **must fix** | test-gap | ParameterRefused | The row asserts the fault, not "no acquisition, no image". A component that faults and still starts the camera passes. | Row FailedStartStoresNothing: no `AcquisitionStart` write, no product, for each kind of failed start. |
| test-gap:REQ-CamMgr-039 | **must fix** | test-gap | StartCaptures | "steps in order" does not name the stop of a running camera. | Row StartStopsRunningCamera naming the first writes. |
| test-gap:start-channels | **must fix** | test-gap | list | Four channels have no row. | Row StartChannels. |
| test-gap:start-budget | **must fix** | test-gap | list | SDD 3.7 start budget has no row. | Row StartBudget with the values. |
| test-gap:stale-frame | **must fix** | test-gap | list | SDD 3.6 "sequence is current" has no row; the fault it guards (index of the next sequence) would not be seen. | Row StaleFrame. |
| test-gap:socket-interleave | **must fix** | test-gap | list | SDD 3.2 two-thread socket reads and the discard at stop have no row. | Rows SocketInterleaved, SocketStopDiscards. |
| test-weak:values | **could fix** | test-weak | ImageProduct, DiscardWrongSize, DropNoBuffer, BlockIdGaps, StartCaptures, LateBufferReturned | Assertions name no values. | Name counts, bytes and channels. |
| test-weak:REQ-CamMgr-043 | **could fix** | test-weak | StopBounded, L2-1 | The unit test sums scripted waits; L2-1 checks 2 s. The SDD should say which shows what. | One sentence under the list. |
| test-weak:stalled | **could fix** | test-weak | list | `STALLED` has no row. | Row StreamStalled. |
| test-weak:hello | **could fix** | test-weak | list | The 1 s HELLO limit has no row. | Row SocketNoHello. |
| test-weak:counter-limit | **could fix** | test-weak | list | "Counters stop at their largest value" has no row. | Row CounterLimit. |
| test-weak:Ping | **could fix** | test-weak | Ping | Mapped to REQ-CamMgr-016, which it does not show. | Map to "none: interface element" and name the key. |

## Totals
must fix: 6   could fix: 6   open disagreements: 0

## Disagreement log
| Round | Key | Author position | Reviewer position | Status |
|---|---|---|---|---|

## Close (author)
All 12 fixed with the rows above.
