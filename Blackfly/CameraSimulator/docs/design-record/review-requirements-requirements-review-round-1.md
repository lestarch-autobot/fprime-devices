Bound: 3 rounds (source: campaign prompt, default; supersedes agent-file caps). Reviewer run inline in the campaign session; local only, nothing posted.

# Requirements review, round 1 (inline)

Artifact: `docs/sdd.md` `## Requirements`, first draft (29 rows).

## Intent elements (listed before reading the draft)

E1 accept every USB3 Vision control and stream message the system sends (REQ-LDC-028).
E2 start-to-stop acknowledgements match the capped camera in status and length (REQ-LDC-029).
E3 image pattern differs per instance (REQ-LDC-030).
E4 slot, image index, capture time in characters of 64 pixels or more (REQ-LDC-031).
E5 working camera only. E6 separate process, local socket, same bytes. E7 whimsical, identifiable image.
E8 logs every received message; rejected messages can be counted (map evidence, plan action `simulator_logs`).
E9 killed and restarted in L5-1. E10 images can be computed again from slot, index and time (L7-1).
E11 system issues `TriggerSoftware` and sets `TriggerMode`, `TriggerSource` (SSDD Table 1).
E12 stream reset operation of the transport (pathfinder report section 3).

## Per-requirement (RQ-1 to RQ-4)

| Row | Result |
|---|---|
| 001 | pass; one compliance row as the skill asks |
| 007 | fail RQ-1: "within 250 ms" has no start point |
| 017 | fail RQ-1: "show" names no observable |
| 018, 019 | "shown" depends on 017 |
| 024 | fail RQ-2: three outcomes in 19 words |
| 009, 011, 014, 016, 021, 027, 028 | over 15 words |
| others | pass |

## Request trace (RS-1)

E1 002, 009-014. E2 003-007. E3 015, 016. E4 017-019. E5 none needed (negative scope). E6 design. E7 none. E8 022. E9 024 partly. E10 020, 021. E11 none. E12 none.

## Fault scenarios (RS-2)

Covered: bad command, bad record, second client, disconnect, slow client, log failure, bad argument, SIGTERM. Missing: record cut short by a closing client.

## Consistency (RS-3)

006 (no answer) and 026 (close connection) act on different layers; no conflict.

## Findings

| Key | Tag | Class | Row | Finding | Rewrite |
|---|---|---|---|---|---|
| RQ1-1 | must fix | req-not-verifiable | 007 | no start point for 250 ms | "within 250 ms of the command" |
| RQ1-2 | must fix | req-not-verifiable | 017-019 | "show" has no observable | define the stamp in a table; "carry the Table 7 stamp" |
| RQ1-3 | must fix | req-missing | E11 | no trigger behaviour; the system sends `TriggerSoftware` | add software-trigger and other-source rows |
| RQ1-4 | must fix | req-missing | E12 | stream reset has no row | add a row |
| RQ1-5 | must fix | req-unclear | 024 | three outcomes | one outcome: a new client is served from power-up values |
| RQ1-6 | could fix | req-missing | E7 | "whimsical, identifiable" has no row | one row pointing at a mascot table |
| RQ1-7 | could fix | req-fault-gap | 026 | incomplete record not named | "bad or incomplete" |
| RQ1-8 | could fix | req-unclear | 7 rows | over 15 words | shorten |
| RQ1-9 | could fix | req-no-rationale | 029 | trace to REQ-LDC-028 is nominal; the source is the catalog coding rule | keep; record in ledger |

Totals: must fix: 5 could fix: 4 open disagreements: 0

## Disagreement log

None.
