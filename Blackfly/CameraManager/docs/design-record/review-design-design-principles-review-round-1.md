review rounds: 3 (source: prompt) — design phase, review 1 of 3 — inline
Inline: the campaign prompt forbids child sessions, so no separate session was tried. Local only. Preamble of `phase-review-loop.md` applied: no PR; every line is introduced.
Artifacts: `CameraManager.fpp`, `CameraManagerTypes.fpp`, generated `CameraFeatureTypes.fpp`, `CameraParams.fppi`, `CameraReadBack.fppi`, `docs/sdd.md` sections 3 to 7. Intent input: REQ-CamMgr-001..044 and the campaign prompt.
Reviewer: `design-principles-review.agent.md` @ lestarch-autobot/fprime d488d24, `jpl-design-principles`.

## Ledger
| Rule | Result |
|---|---|
| P-2.3.1 Design for ops | pass |
| P-2.3.2 Unambiguous | pass |
| P-2.3.3 Fault avoidance | pass |
| P-2.3.6 Graceful degradation | pass: a fault stays in its slot |
| P-2.3.7 Fault recovery | pass: next capture start |
| DR-4.4.6.1, DR-4.4.6.4 Visibility of state | pass: `State`, read-back channels |
| DR-4.4.6.8 Visibility to reconstruct faults | pass: events name step, address, status; control trace |
| DR-4.4.7.1 Rollover | fail → jpl:DR-4.4.7.1:counters |
| DR-4.9.3.1 In-flight commandability | pass |
| DR-4.11.1.1.1 Architecture documented | pass |
| DR-4.11.1.1.2 Quality attributes | pass |
| DR-4.11.1.3 Semantics of data interfaces | pass: units in the tables |
| DR-4.11.1.4 COTS tools | n/a |
| DR-4.11.1.5 Demonstrable correctness | pass: state diagram |
| DR-4.11.2.1 Nominal initialization | pass |
| DR-4.11.2.2 Multiple restart | pass: `RESET` step |
| DR-4.11.2.3 Minimalist boot | pass: nothing sent before a request |
| DR-4.11.2.4 Initialization trace | pass: `SlotBackend`, `CameraSerial` |
| DR-4.11.3.1 Standards | pass |
| DR-4.11.3.3 Parameter specification | pass |
| DR-4.11.4.3 Unintended modification | n/a |
| DR-4.11.4.7 Time-outs | fail → jpl:DR-4.11.4.7:CameraTransport.open |
| DR-4.11.4.11 Memory use | n/a (needs implementation): fixed arrays, no allocation after `configure` |
| DR-4.11.4.12 Data set consistency | pass: each parameter is read once per start |
| DR-4.11.4.13 Thread safety | fail → jpl:DR-4.11.4.13:m_reader (same site as arch-undocumented-departure:StreamReader) |
| DR-4.11.4.14 State transitions | pass |
| DR-4.11.5.2 Incremental verification | pass: transport interface |
| DR-4.11.5.4 Stress testing | n/a (needs implementation) |
| DR-4.11.6.1 Self-test | pass: identity check |
| DR-4.11.6.2 Fault diagnostics | pass |
| Skipped (design-review 7, 8) | DR-6.3.5.3, DR-4.11.6.3, DR-4.11.4.2, DR-4.11.4.5, DR-4.11.4.6, DR-4.9.1.2 |

## Findings
| Key | Tag | Class | Site | Finding | Rewrite |
|---|---|---|---|---|---|
| jpl:DR-4.4.7.1:counters | **could fix** | jpl-DR-4.4.7.1 | SDD 3.8, telemetry | Nothing says what counters and the request ID do at their largest value. | "Counters stop at their largest value." "The request ID goes from 1 to 65535 and then to 1 again." |
| jpl:DR-4.11.4.7:CameraTransport.open | **could fix** | jpl-DR-4.11.4.7 | SDD 3.2 | `open()` reads the HELLO record with no stated time limit. | "`open()` waits at most 1 s for the HELLO record." |
| jpl:DR-4.11.4.13:m_reader | **could fix** | jpl-DR-4.11.4.13 | SDD 3.2, 3.4 | The SDD does not say how the shared reader record is protected or that `streamReset()` may run beside a read. | State the mutex rule and the concurrent use of `streamReset()`. |

## Totals
must fix: 0   could fix: 3   open disagreements: 0

## Disagreement log
| Round | Key | Author position | Reviewer position | Status |
|---|---|---|---|---|

## Close (author)
All 3 fixed in SDD 3.2, 3.7 and 3.8.
