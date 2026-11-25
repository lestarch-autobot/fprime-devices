review rounds: 3 (source: prompt) — design phase, review 1 of 3 — inline
Inline: the campaign prompt forbids child sessions, so no separate session was tried. Local only. Preamble of `phase-review-loop.md` applied: no PR; every line is introduced.
Artifacts: `CameraManager.fpp`, `CameraManagerTypes.fpp`, generated `CameraFeatureTypes.fpp`, `CameraParams.fppi`, `CameraReadBack.fppi`, `docs/sdd.md` sections 3 to 7. Intent input: REQ-CamMgr-001..044 and the campaign prompt.
Reviewer: `architecture-review.agent.md` @ lestarch-autobot/fprime d488d24. Baseline: the new model as a whole. Heuristic 5 and category 4 n/a.

## Classification
`Blackfly.CameraManager`: active; inputs `captureIn`, `pingIn`, `productRecvIn`, `schedIn` and the command all async; `schedIn` is an async `Svc.Sched`. Pattern: event-driven with cyclic notification (hybrid). No sync or guarded input. Classifiable.

## Findings
| Key | Tag | Class | Site | Finding | Rewrite |
|---|---|---|---|---|---|
| arch-undocumented-departure:StreamReader | **must fix** | arch-undocumented-departure | SDD 3.2, 3.8 | The component runs a second task and shares state under a raw mutex (category 6 shape). The SDD gives no reason and no rule for the shared state. | State why the reader is a task (blocking reads, as `Drv.LinuxUartDriver`), what is shared, and that no transport or port call is made with the mutex held. |
| arch-primitive-misuse:close | **could fix** | arch-primitive-misuse | SDD 3.6 | A fault closes the transport while the reader may be inside a read. | Fault paths park the reader before `close()`. |

Categories 1, 2, 3, 5: pass.

## Totals
must fix: 1   could fix: 1   open disagreements: 0

## Disagreement log
| Round | Key | Author position | Reviewer position | Status |
|---|---|---|---|---|

## Close (author)
Both fixed: rationale and locking rule in SDD 3.2; fault order in 3.7.
