Bound: 3 rounds (source: campaign prompt, default; supersedes agent-file caps). Reviewer run inline in the campaign session; local only, nothing posted.

# Design review, round 1 (inline)

Scope: CameraSimulator is not an F Prime component and has no FPP model. The reviewers of the design phase (design-review, architecture-review, design-principles-review) were applied to the `## Design` text of `docs/sdd.md`. Checks tied to an FPP model (port kinds, command and event definitions, `fpp-check`) do not apply and were not run.

## Findings

| Key | Tag | Reviewer | Finding | Closure |
|---|---|---|---|---|
| DS1-1 | must fix | design-review | no trigger behaviour in the design while requirements need it | paragraph added to Control behaviour |
| DS1-2 | must fix | design-principles (bounded resources) | drawing a 24 MB image from nothing each frame on the one thread delays control answers and cannot hold 15.75 frames per second | background drawn once per geometry; per frame one copy plus mascot and stamp |
| DS1-3 | must fix | architecture-review | the stamp time is the device clock, the product's capture time is system time at arrival (REQ-LDC-034); the design did not say which is shown or how an image is computed again | stated; recompute path written; proposal P4 |
| DS1-4 | could fix | design-review | SIGTERM handling inside a poll loop not stated | signal descriptor added |
| DS1-5 | could fix | architecture-review | mascot table belongs with the requirement that points at it | moved to Table 8 |
| DS1-6 | could fix | architecture-review | the socket protocol is defined here, yet the SSDD says CameraManager's SDD defines the interface; `campaign/CamMgr` does not exist on the remote yet | proposal P1 |

Totals: must fix: 3 could fix: 3 open disagreements: 0
