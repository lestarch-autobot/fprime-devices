# Campaign ledger — CamSim (CameraSimulator)

Campaign: `fprime-development-campaign`, id `CamSim`, child of the LDC project campaign. Approver: `project-campaign`. Lane: DESIGN only. Review bound: 3 rounds (campaign prompt). Reviews are inline and local.

Marks: [V] verified in a file or by a command in this session; [I] inference.

## Inputs

- [V] Skills: lestarch-autobot/fprime `system-campaign-skills` @ d488d245.
- [V] Ask and SSDD: lestarch-autobot/ldc-project `devel` @ aa3b9df3. Allocated: REQ-LDC-028, 029, 030, 031.
- [V] Base: lestarch-autobot/fprime-devices `blackfly-pathfinder` @ ad7ce9fb; pathfinder report, traces and generated register table read.
- [V] `campaign/CamMgr` did not exist on the remote when this lane ran. The CameraManager SDD was not read.
- [V] No bench access. No vendor file committed.

## Steps

| Step | Result |
|---|---|
| Requirements | 34 rows `REQ-CamSim-001` to `034` in `docs/sdd.md`; every row traces to an allocated system requirement |
| FPP design | not applied: ground-support executable, no FPP model (SSDD Inventory and Red flags) |
| Design | `## Design` of `docs/sdd.md`: process, socket protocol, control, stream, image, log |
| Test design | 33 unit tests in `### Unit-test list`; `## Tests` maps LDC plan rows L2-4, L2-5, L5-1 |
| Design-stage checklist | not created: `fprime-auto-checklist` works from an FPP module; none exists here (P10) |
| Language | C++14 in the F Prime style |

## Trace

| System requirement | Component requirements |
|---|---|
| REQ-LDC-028 | REQ-CamSim-001, 002, 008, 009, 010, 011, 012, 013, 014, 022, 023, 024, 025, 026, 028, 029, 030, 031, 032, 034 |
| REQ-LDC-029 | REQ-CamSim-003, 004, 005, 006, 007 |
| REQ-LDC-030 | REQ-CamSim-015, 016, 020, 021, 027, 033 |
| REQ-LDC-031 | REQ-CamSim-017, 018, 019, 020, 021 |

Component-kind map rows delegated to this campaign: REQ-LDC-030 (unit tests SlotsDiffer, MascotPresent, RenderPure) and REQ-LDC-031 (StampReadBack, StampHeight).

## Review loops

| Loop | Reviewer | Rounds | must fix / could fix / open disagreements, by round | Final |
|---|---|---|---|---|
| Requirements | requirements-review, inline | 3 | 5/4/0, 1/1/0, 0/1/0 | 0 / 1 / 0 |
| Design | design-review, architecture-review, design-principles-review, inline, on the SDD design text (no FPP model) | 3 | 3/3/0, 2/1/0, 0/1/0 | 0 / 1 / 0 |
| Test design | system-test-review Mode A, inline | 3 | 3/1/0, 1/1/0, 0/1/0 | 0 / 1 / 0 |

Records: `docs/design-record/review-<phase>-<reviewer>-round-N.md`. All must-fix findings are closed in the SDD. Limit of an inline review: the author and the reviewer are one session; [I] an independent reviewer may find more.

## Proposals for the project gate

| # | Proposal | Basis |
|---|---|---|
| P1 | The socket record format in `### Socket protocol` (8-byte header: magic, channel, kind, length; one record per USB transfer; RESET and HELLO records; Unix stream socket, default `/tmp/blackfly-camsim-N.sock`) is the socket side of CameraManager's transport interface. CameraManager's SDD adopts it, or the project gate names the differences and this SDD follows. | [V] SSDD: CameraManager's SDD defines the interface; [V] that SDD was not available |
| P2 | Simulator serial numbers are `9990000N`. All other identity values equal the capped camera's. | [I] CameraManager may select a device by serial |
| P3 | "Image index" in the stamp is the leader's block ID. It restarts at 0 when the stream is enabled again and has gaps after dropped frames, so it can differ from the product's image index. | [V] pathfinder report on block ID |
| P4 | "Capture time" in the stamp is the simulator's device clock (leader timestamp), not the product's capture time, which is system time at arrival (REQ-LDC-034). L2-5 and L7-1 read the time from the stamp. To compute an image again, the auditor reads slot, index and time from the stamp and takes format and geometry from the product. | [V] REQ-LDC-034; [I] the simulator cannot know the arrival time |
| P5 | Stamp characters are 70 pixels high or more, so images under 800 by 256 pixels cannot hold the stamp. System tests of simulator slots keep the image at or above that size. | [V] REQ-LDC-031 needs 64 pixels |
| P6 | Two slot images differ in at least 25 percent of pixels. The system requirement gives no number. | [I] |
| P7 | Nine simulators at full resolution and full rate move about 3.4 GB/s through sockets and render on nine cores. The plan's rates on simulator slots should be checked against the LDC box in BASE DEPLOYMENT. Not a simulator requirement. | [I] 9 × 15.75 × 24.5 MB |
| P8 | With `TriggerMode` on and a line source the simulator sends no frame. A slot faulted by "a trigger source that never fires" can therefore be shown on a simulator too. | [V] SSDD bench note uses the capped camera for this |
| P9 | REQ-CamSim-029 (no allocation after startup) traces to REQ-LDC-028 for want of a better parent; its source is the catalog coding rule. | [V] campaign prompt |
| P10 | No design-stage checklist file. The requirements and design rows that do not need an FPP model were covered by the three review loops. | [V] SSDD Inventory: checklist rows tied to an FPP model not applied |
| P11 | The device description zip is served only when a local copy is given with `--genicam-zip`. Without it, reads of the file are answered INVALID_ADDRESS and logged `UNMODELLED`, not `REJECTED`. CameraManager uses the static table and should not read the file. | [V] human ruling: static table, no vendor file committed |
| P12 | A valid command for an unmodelled address is logged `UNMODELLED`. For REQ-LDC-028 evidence, the project should count `UNMODELLED` lines with `REJECTED` lines: both must be zero in L2-4. | [I] |
| P13 | Rounded read-back of accepted values (for example exposure 1000 read as 997) is not modelled: the simulator reads back the written value. REQ-LDC-029 compares status and length only. | [V] pathfinder report |

## Observations for the project session

- [V] SSDD `## Tests` row L2-5 lists REQ-LDC-028 and REQ-LDC-033; the map gives REQ-LDC-030 and 031 to the L2-5 evidence. One of the two looks wrong.
- [V] The plan's bench actions `simulator_logs`, `simulator_kill_2`, `simulator_start_2`, `read_images` need: the `--log` file, SIGKILL then a start with the same `--slot` (REQ-CamSim-034), and the stamp reader.

## Plan names

The simulator has no commands, events or channels. No name of the system plan was changed.

## Gate

Approve SDD: handed to the project campaign. DEVELOPMENT not started.
