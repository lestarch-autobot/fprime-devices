Bound: 3 rounds (source: campaign prompt, default; supersedes agent-file caps). Reviewer run inline in the campaign session; local only, nothing posted.

# Requirements review, round 2 (inline)

Artifact: `docs/sdd.md` `## Requirements` after round 1 fixes (33 rows).

Closure of round 1: RQ1-1 to RQ1-8 fixed in the table. RQ1-9 kept as written, recorded in the ledger (proposal P9); no disagreement.

## Per-requirement

All rows state stimulus, observable and bound. All rows are 15 words or fewer.

## Request trace (RS-1)

E9: L5-1 kills the simulator with a signal it cannot catch (`simulator_kill_2`). The socket file stays. No row says the restarted instance must start. All other elements are covered.

## Findings

| Key | Tag | Class | Row | Finding | Rewrite |
|---|---|---|---|---|---|
| RQ2-1 | must fix | req-missing | E9 | restart after a kill is not required | add "shall replace a socket file left by an ended instance" |
| RQ2-2 | could fix | req-weak-verification | 024 | Integration Test named, but L5-1 restarts the process and does not reconnect to a live one | Unit Test only; L5-1 verifies the new row |

Totals: must fix: 1 could fix: 1 open disagreements: 0

## Disagreement log

None.
