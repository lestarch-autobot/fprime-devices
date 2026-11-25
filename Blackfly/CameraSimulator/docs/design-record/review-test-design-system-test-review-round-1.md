Bound: 3 rounds (source: campaign prompt, default; supersedes agent-file caps). Reviewer run inline in the campaign session; local only, nothing posted.

# Test-design review, round 1 (inline, Mode A)

Artifact: `### Unit-test list` and `## Tests` of `docs/sdd.md`. CameraSimulator has no system plan of its own: its system tests are rows of the LDC plan, owned by the project campaign. This review checks the unit-test list and the mapping to the LDC plan rows.

## Findings

| Key | Tag | Finding | Closure |
|---|---|---|---|
| TD1-1 | must fix | stamp tests read characters with the renderer's own font: one font fault passes both sides | reader in test code with its own font copy; ten stored checksums |
| TD1-2 | must fix | no test for stream reset, trigger rows, mascot row | StreamReset, SoftwareTrigger, LineTrigger, MascotPresent added |
| TD1-3 | must fix | BadRecord does not send a record cut short | added |
| TD1-4 | could fix | L2-5 was mapped to REQ-CamSim-019, but the bench reads the product, not the leader | mapping removed; unit test only |

Totals: must fix: 3 could fix: 1 open disagreements: 0
