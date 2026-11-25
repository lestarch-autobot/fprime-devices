Bound: 3 rounds (source: campaign prompt, default; supersedes agent-file caps). Reviewer run inline in the campaign session; local only, nothing posted.

# Design review, round 2 (inline)

Scope as round 1. Artifact: `## Design` after round 1 fixes.

## Findings

| Key | Tag | Reviewer | Finding | Closure |
|---|---|---|---|---|
| DS2-1 | must fix | design-review | a killed instance leaves its socket file; the next start would fail to bind | connect probe and replace, written in Socket protocol |
| DS2-2 | must fix | design-principles (bounded resources) | record length limit of 1 MiB, but the payload transfer size is client-programmed | register maximum set to the limit, stated |
| DS2-3 | could fix | architecture-review | nine simulators at full rate move about 3.4 GB/s through sockets on one box; no margin shown | not a simulator design change; proposal P7 |

Totals: must fix: 2 could fix: 1 open disagreements: 0
