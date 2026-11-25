review rounds: 3 (source: prompt) — design phase, review 2 of 3 — inline
Inputs as round 1. Scope: elements changed by the round 1 close (internal ports, `StreamFault`, SDD 3.2, 3.5 to 3.8, 3.10) and each reviewer's whole-model checks.
Reviewer: `architecture-review.agent.md`.

## Classification
Unchanged: active, event-driven with cyclic notification. No sync or guarded input added.

## Closed from round 1 (verified)
arch-undocumented-departure:StreamReader (SDD 3.2 gives the reason, the shared record and the locking rule), arch-primitive-misuse:close (SDD 3.7 fault order; 3.2 "`close()` is called only after the reader has parked").

## Findings
| Key | Tag | Class | Site | Finding | Rewrite |
|---|---|---|---|---|---|
| arch-primitive-misuse:SocketTransport | **could fix** | arch-primitive-misuse | SDD 3.2 | Two more locks (socket read and write) are added by the round 1 and 2 closes; the SDD should bound how long one is held. Same site as design-intent-mismatch:SocketTransport. | "held for one poll of at most 10 ms at a time". |

## Totals
must fix: 0   could fix: 1   open disagreements: 0

## Disagreement log
| Round | Key | Author position | Reviewer position | Status |
|---|---|---|---|---|

## Close (author)
Fixed in SDD 3.2.
