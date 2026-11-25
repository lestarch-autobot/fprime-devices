Bound: 3 rounds (source: campaign prompt, default; supersedes agent-file caps). Reviewer run inline in the campaign session; local only, nothing posted.

# Test-design review, round 2 (inline, Mode A)

Artifact after round 1 fixes.

## Findings

| Key | Tag | Finding | Closure |
|---|---|---|---|
| TD2-1 | must fix | no test restarts the program after a kill, which L5-1 depends on | ProcessStaleSocket added; L5-1 mapped to REQ-CamSim-034 |
| TD2-2 | could fix | ProcessFrameRate takes 30 s of wall time | accepted; the requirement states 10 s |

Totals: must fix: 1 could fix: 1 open disagreements: 0
