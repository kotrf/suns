# Transport lines — first #53 slice

The first slice was merged in #237, main `4fb67cf`, save v102 / orders v42.
The load-policy follow-up merged in #238, main `830bcd1`, **save v103 / orders v43**.
The computer-player follow-up merged in #239, main `db1204d`, with those formats.
The statistics follow-up merged in #240, main `0b67ec1`, **save v104 / orders v44**.
The destination-target follow-up merged in #241, main `7292305`, **save v105 / orders v45**.
The stationary-installation delivery follow-up starts there: **save v106 / orders v46**.

## Playable loop

Open **Logistics — transport lines** from the menu bar. Define a named source,
owned destination colony, one mineral (I/B/G), and Warp; assign one or more
already built cargo fleets, then submit the turn. Each freighter receives an
ordinary **Load All Available → Unload All → repeat** programme. Source can be
an owned colony or an uncolonized mining site with delivered Geological Survey.
Load All consumes the physical surface cache; the line does not create mining,
ships, cargo space, fuel, technology or minerals. Mineral unload does not move
passengers, specimens, xenocrystal or another mineral inadvertently.

The registry is a first-class owner-private catalog: stable owner-local IDs,
monotonic definition revisions, two surveyed endpoints and an intended fleet
roster. No line-definition production/mineral fee or extra technology is added.
Real freighters/fittings and operation pay their existing ordinary costs.
Free flight is unchanged. Map dashed lines show registered HQ definitions,
not compulsory travel lanes, sensor coverage or proof of successful service.

## Dispatch and operation

Line edits dispatch the new programme to its roster. Each ship restarts at the
source when the message arrives; no old cargo is deleted. Pause, deletion and
release send ordinary Stop commands. Resume dispatches the full cycle again.
Pause followed by Resume in the same dispatch batch preserves the player's
last command when both arrive together; overtaken older packets remain suppressed.
Deleting a line does not reuse its ID. Replayed revisions/assignments do not
issue another programme. A fleet belongs to at most one intended roster.

Programmes run aboard without annual HQ selection, including loss of contact
and even absence of the HQ catalog. There is no annual host-side route planner
or secret stock check. Loading, movement, shared holds/gross mass, engine fuel,
coasting, ordinary refuelling, damage, battle, ownership and unloading use the
existing turn phases and arrival actions. Competing transports load in normal
deterministic fleet order. With zero departure minimum a depleted source yields an empty trip. A positive
minimum holds the source waypoint until the selected cargo threshold is met. A captured enemy endpoint cannot surrender stocks
to the old owner; ordinary untargeted arrival fails and continues. A positive
destination target instead holds remaining cargo at a blocked destination until
new orders or restored ownership permit it.
Fuel shortages use existing slow/coasting/stall behavior, with no free rescue.

Manual navigation, stationary work, probe/visitor orders, colonization and gate
commands release the fleet from the HQ roster; their physical effects retain
normal command delay. An explicit assignment is required to join again. Split
offspring are not automatically roster members. The original FleetId keeps its
intended membership if it survives reorganization; an absorbed/lost ID stays
as owner-authored intent until explicitly released. No physical loss is inferred
from hidden host state. Ordinary split/merge programme and cargo rules apply;
no automatic reassignment to another fleet is invented.

## Source reserve and departure minimum

Both values are whole kt of the selected mineral, default zero, bounded at
1,000,000,000. Source reserve limits this loader to `max(0, physical stock -
reserve)`; it neither creates missing stock nor locks minerals against factory
work, other consumers or differently configured lines. Transfer still uses the
real shared hold, and actual fractional stock/cargo stays fractional.

A positive departure minimum compares against the selected mineral already
aboard, including cargo loaded on earlier attempts. The ship loads what fits,
keeps partial cargo and retries this waypoint once each year without new HQ
orders. It advances to unload only after the minimum is aboard. Zero means no
waiting. There is no new idle fuel charge, maintenance discount or free supply.

A minimum that cannot fit because of other cargo or the fitted hold does not
get silently reduced. Waiting continues until the player changes the programme
or physically frees space. A captured source cannot supply the old owner; any
existing cargo is preserved. Once its already-carried selected mineral meets
the minimum, it can depart without extracting anything else there.

Pause/stop, edits and manual routes use normal delayed commands while waiting;
partial cargo survives cancellation and resume. Policy fields are frozen in
the dispatched route, so editing the HQ definition never changes a distant
ship before receipt. A nonzero reserve or minimum uses the new LoadMinerals
arrival action; zero/zero retains ordinary Load All and its prior behavior.

## Destination stock target

Destination target is an optional whole-kt quantity of the selected I/B/G,
bounded at 1,000,000,000. Zero preserves the previous full-unload programme.
A positive value transfers `min(aboard, max(0, target - actual surface stock))`
at the frozen owned destination. Partial/fractional quantities use existing
physical mineral arithmetic without rounding or discarding cargo. Other
minerals, colonists, specimens and exotic cargo are untouched.

For example, destination stock 70 kt, target 100 kt and cargo 80 kt unloads
30 kt and leaves 50 kt aboard. With no demand the ship waits without generating
empty delivery/arrival messages or consuming new movement fuel. After a real
factory consumes 2 kt, the next arrival phase tops up exactly 2 kt. It repeats
locally without HQ control or contact, then returns to the source once that
selected cargo is empty. Normal source reserve, departure minimum and shared
hold rules apply on the next loading visit. Unrelated cargo does not need to
be emptied to finish the selected-mineral stop.

This is a per-unloader stock ceiling at its existing arrival phase, not a
reservation, empire-wide stock cap or guarantee of a minimum at year's end.
Production consumes supplies later in the turn; mining and other valid cargo
orders may add supplies. Several loaders with the same target inspect stocks
in the usual deterministic fleet order and cannot collectively unload past
that target in that phase. Differently configured lines/other consumers follow
their own ordinary rules. There is no annual HQ stock dispatcher, remote stock
oracle, extra equipment, free supply or new construction/operation price.

A neutral/enemy/wrong destination cannot accept a targeted unload: remaining
cargo stays aboard and the action remains waiting. Battles, damage, docking
and ordinary refuelling retain their phases. Stop, line pause/deletion,
manual override, edits and resume retain cargo and use normal physical command
delay. Edits/resume still restart the line at its source when delivered; an old
programme keeps its old target until then. A large target may need many trips;
a small target can tie a freighter up as a local supply buffer. No automatic
return with excess, demand retasking or emergency diversion is invented.

Logistics exposes the target, received waiting state, age and retained shared
hold occupancy. Geometric projection stops at a targeted unload outcome until
new evidence. The route forecast uses received stock and valid pending orders;
if cargo remains it labels destination-target waiting and leaves later ETA
unknown. It cannot promise future production, consumption or unloading from
stale telemetry. Partial unloads create their own positive immutable receipts,
so unload count is not the number of completed round trips. Existing delayed
statistics and deduplication are unchanged.

## Addressed mineral deliveries to stationary installations

The destination selector also offers owned deployed depots, relays and independent
orbital shipyards from received fleet telemetry. Source remains an owned colony
or surveyed uncolonized mining site. Colony stations use their colony's existing
surface endpoint. Mobile bases, installation sources, fuel-cargo lines and
multiple stops remain later work; ordinary SupplyOutpost fuel actions still work.

A line freezes the exact installation FleetId and received coordinates into its
ordinary unload action. The freighter never pursues an unknown new position,
substitutes another co-located depot or dumps to a planet underneath. Physical
arrival requires that exact owned anchored installation at the waypoint, with
less than 100% damage. Shared free cargo capacity includes every existing cargo.
Only the selected I/B/G transfers; no minerals, hardware or operating fuel appear.

The stock target has the same meaning as for colonies; zero unloads as much as
fits. Any remainder waits aboard and retries locally each year, also at target
zero. Missing, foreign, moved, disabled or full installations retain the cargo.
Stop/pause/edit use ordinary command delivery. A known line can be paused even
if its endpoint contact has since disappeared. Return starts after the selected
cargo is empty. This is finite local supply, not an HQ stock oracle or an
emergency rerouting policy.

Actual positive deliveries produce delayed line receipts naming the installation;
there is no fictitious colony Freight/History entry. Logistics shows received
stock, shared hold, evidence age and waiting. Route forecasts use the same
received state and label later capacity/demand as conditional. Geometry cannot
silently mark a waiting unload complete. No automatic AI policy is expanded.

Save v106 / orders v46 append FleetIds only to definitions/actions whose planet
ID is zero, and to installation receipt/report payloads. Exactly one destination
identity is required. Ordinary planetary payloads keep their previous layout;
v105 and older supported migrations default to planetary endpoints. Forged old
envelopes cannot accept installation payloads. Physical, received, draft and
in-flight definitions and receipts all preserve the exact receiver.

## Knowledge, forecasts and interface

Definitions/roster edits are HQ intent and local drafts, not acknowledgements.
Only received fleet telemetry confirms the actual repeating programme. The
Logistics panel shows evidence year/age, last reported target, fuel, hold,
minerals and mismatches with the dispatched programme. It labels received
empty source stocks; unowned mining caches are checked physically on arrival
and are not claimed to be known empty. Received telemetry distinguishes waiting
for a minimum from a reported hold that cannot fit it. Geometry cannot assume
a future loading threshold was fulfilled: the map projection stops at that
source waypoint until received telemetry/radar supplies new evidence. Route
forecasts label a received-stock wait and leave later ETA unknown rather than
promising future supplies. Missing contacts remain roster entries
that can be released, including after their loss is received.

Draft definitions/assignments participate in shared command projections and
the ordinary Route Program preview. Manual route edits after an assignment are
last in the command sequence, rather than modifying an older superseded draft.
Physical hidden captures, construction, losses and stocks cannot rewrite the
catalog or old exported player evidence. Host-only pending command results are
not status evidence. Normal Freight messages and receipt-year History keep
recording real deliveries. The statistics follow-up adds line attribution
without another economic delivery event.

## Received statistics and interruption diagnostics

Newly dispatched programmes use UnloadTransportLine: ordinary selected-mineral
Unload All plus frozen owner-local line ID, revision and destination ID. Only a
positive physical unload at that exact owned destination produces a line
receipt. Unowned/enemy/wrong endpoints and empty cycles cannot create credited
volume. The mechanism does not require the current HQ roster/catalog to match:
an older programme can still deliver after an edit, pause, release or deletion.
Reorganization keeps its existing idle-only rules: Stop before a normal split
or merge, with real cargo conserved; split children have no programme until
explicitly assigned. Arrival merges retain their normal destination programme.
Attribution follows the
executing action rather than inferring a line from names or current fleet IDs.
Existing cargo, payment, ownership and phase behavior are unchanged.

The receipt uses the ordinary report routing/outbox/delay. It freezes fleet,
planet, line/revision, mineral manifest and observation year; receipt year is
set only when delivered. Logistics shows I/B/G totals and unload count received
in the last 20 calendar receipt years, plus received mean kt/year over that
window. This is reported flow into HQ knowledge, not guaranteed current route
throughput: a batch of late reports can spike it, reports in transit are absent,
and no receipt does not prove that no delivery happened. Both dates and the
old programme revision are visible for recent receipts. Changes of mineral or
endpoints do not relabel prior manifests as the current cargo.

Receipt identity is (observation year, line, revision, fleet, destination).
Repeated packets neither add volume nor refresh the first receipt date, even
after the displayed window expires or after save/load. Up to 4096 receipts per
owner are retained, ordered by observation identity. At the bound the whole
oldest observation year is discarded and a durable watermark rejects replay
or delayed observations through that year. The UI exposes this omission and
labels volumes a retained lower bound; it never silently presents complete
coverage. Deleting a line does not reuse IDs or reattribute its late packets.
This bounded diagnostic history is separate from the ordinary colony economic
ledger and does not create a second Freight/History event.

The calculated round trip is twice `ceil(distance / Warp distance)`, a
flight-only lower bound excluding loading/waiting/service, fuel shortage and
future interruptions. Confirmed matching post-dispatch programme telemetry
supplies reported shared-hold occupancy, en-route programmes and source loading
waits, with oldest/newest evidence years and ages. Hold occupancy is not a time
utilization percentage. Unconfirmed, paused and missing roster entries do not
become zero-load confirmed ships. Source/destination report dates accompany
received colony stocks; uncolonized caches are not invented as known empty.

Roster reasons distinguish awaiting/mismatched programme evidence, delayed
Stop acknowledgement, received ownership blocks, disabling damage, empty tank
(with possible drift/fuel-free flight), impossible shared hold, departure-minimum
waiting and source stock at/below its reserve. All use delivered evidence.
Last target/status uses actual telemetry, not a geometrically projected dot.
No yearly hidden-state poll, automatic dispatch, new AI policy, future-stock
promise or automatic evacuation is introduced.

## Computer-player supply lines

Computer empires can create the same catalog entries through ordinary
SetTransportLineOrder / AssignTransportLineOrder. One existing empty, undamaged,
idle freighter can be assigned per planning year; no free ships or cargo are
created. The first policy serves populated owned colonies with received refuel
depots at both ends. It chooses a received source with at least 300 kt and a
destination below 100 kt of I/B/G; the programme leaves 200 kt at source and
waits for 50 kt aboard. These are AI heuristics, not new loading limits for
human-authored lines. Minimum destination stock, then approach plus round-trip
distance and stable endpoint/cargo/fleet IDs break ties deterministically.

Candidate ships must have at least 50 kt of fitted hold. Scouts, colonizers,
miners, probes, constructors, mobile bases, bombers/armed ships, loaded ships
and fleets with existing missions or current orders are not requisitioned.
Military planning has first choice of unassigned ships; a catalog roster or
new assignment then reserves the ship against military, exotic-material,
expansion and habitat cargo planners, including while old idle telemetry persists.
Paused/missing roster entries remain intent. The AI does not silently release
them, resume paused lines, reassign lost transports or infer command failure
from a missing acknowledgement.

Warp selection budgets actual received fuel for the initial approach and a
full fitted hold for every leg. A complete direct cycle must fit the fitted
tank; both endpoints need received refuelling service. Received medium damage
and engine overdrive reject unsafe direct paths. No imaginary bypass stops,
future cargo unloading, scoop generation or future source production are
credited. Existing fuel-free engine speeds remain usable when fuel is absent.
Physical fuel, stocks, ownership, equipment and service are still
checked by the ordinary processor; stale evidence cannot grant supplies.

At most eight catalog entries are created by this conservative policy (all
existing entries count), with one intended supplier per destination/mineral.
The stored catalog is durable dispatch intent, so an old remote idle snapshot
does not cause annual duplicate definitions or assignments. It survives host
save/load without another memory field. Results use normal telemetry and
Freight/History reports; AI reasoning names the chosen line and thresholds.
Automatic transport construction, staffing existing empty definitions, mining
cache/depot/base endpoints, changed-demand retasking and interruption recovery
remain later work. This adds no automatic population evacuation.

## Persistence and boundary

Bounded **LINE** follows existing CCON/ORLC. It stores only each owner's catalog,
nonreused-ID high-water mark, frozen received endpoint coordinates and roster
issue dates. Physical, received and in-flight route programmes keep their
existing representation. Save v101 loads with an empty catalog and preserves
its ordinary fleet programmes; earlier migration paths remain. Orders v42 introduced SetTransportLineOrder and AssignTransportLineOrder.
Save v103 / orders v43 appended reserve/minimum to the versioned LINE/definition
payload and add a conditional payload only for LoadMinerals. Older arrival
actions keep their layouts. A genuine v102 catalog migrates to zero/zero,
preserving its old programmes. Player exports contain only their own catalog
and received statistics. Save v104 appends bounded **TLST** after LINE and
conditional line-report/action payloads; orders v44 carries the new unload
action. Existing actions keep their binary layout. Genuine v103 saves start
with no invented statistics; old Unload All programmes continue unchanged and
are recognized as legacy telemetry. An explicit line edit/reassignment sends
the attributed programme through normal delayed communication. Save v105 /
orders v45 add a destination target to versioned LINE/definition and attributed
unload payloads. Genuine v104 programs default to zero and keep full unloading;
v103/v102 migrations remain. PBEM rejects trailing bytes as well as malformed
or forged old payloads instead of silently ignoring an appended target.

Limits are 128 lines/owner, 256 assigned IDs/line, 64 UTF-8 bytes/name, distinct
received system coordinates and Warp 1–10. Malformed collection counts,
revisions, duplicate roster identities and truncated tails are rejected.

Remaining #53: multi-stop lines, richer wait/refuel/service
policies, mobile-base stops and installation sources, fuel lines, broader automatic staffing/resupply,
observed cycle duration, deeper time utilization/operational history and
interruption briefings.
Further automatic AI staffing/construction is deferred in favor of these
player-facing features; the first received statistics/diagnostics are above.
**Automatic emergency diversion/population evacuation is frozen by user decision
of 29 October 2026, pending more actual play; it is not the next slice.** Colonists stay physical
cargo under the existing manual/ordinary-route rules. #53 remains open; this
slice does not expand frozen #63 or decide station capture/irreversible #56.

## Verification

Full GCC 14.2 / Qt 6.12.0 Debug application/test build; Linux/offscreen CTest
**94/94 passed (65.65 s)** and clean `git diff --check`. New core and actual Qt/
save suites verify multi-fleet resource conservation over 20 unattended years,
continued operation without the HQ catalog, delayed commands, same-batch
Pause/Resume, deletion identities, manual override/route-editor precedence,
hidden ownership/stock invariance, player isolation, host continuation, v101
migration, bounded malformed/duplicate/truncated data and PBEM order tags.
The policy follow-up also covers source reserve floors, gradual unattended
loading, impossible holds, competing fleets, capture, cancellation, preserved
partial cargo, stale geometry/conditional forecasts, delayed policy packets,
received/physical save state and genuine v102 zero-policy migration.
No manual campaign balance, cross-platform or sanitizer acceptance is claimed.

Computer-policy follow-up: full application/test build and Linux/offscreen
CTest **94/94 passed (69.71 s)**; `git diff --check` clean. New cases exercise
ordinary AI creation/dispatch, real unattended delivery and conservation,
source floors, fitted fuel use and fuel-free slowdown, guards and deterministic
input reordering, hidden stock/ownership/depot/fuel/cargo planning invariance,
exactly one assignment through a 34-year command delay, military reservation,
paused intent, and host save/load with identical controller/fleet continuation.
Existing Qt, migration and PBEM payload/export tests remain passing; no new
format or computer-specific privileged packet is introduced.

Statistics follow-up: full GCC 14.2 / Qt 6.12.0 Debug application/test build
succeeded; Linux/offscreen CTest **94/94 passed (82.09 s)** and clean
`git diff --check`. New scenarios cover actual attributed unloads, ordinary
manual unload exclusion, neutral/enemy/wrong destinations, Stop/split and both
merge paths followed by explicit assignment, old-revision reports arriving
after a 34-year delay, replay/save/load/window-expiry deduplication, bounded
history watermark, received status invariant under projected positions,
hidden-world byte-identical player exports, real Qt metrics/history controls,
pending/delivered receipt persistence, PBEM action payload and forged v43
rejection, and genuine v103 legacy programme loading. Existing migrations,
controls, forecasts and other suites pass. No manual campaign balance,
cross-platform or sanitizer acceptance is claimed.

Destination-target follow-up: full GCC 14.2 / Qt 6.12.0 Debug application/test
build succeeded; Linux/offscreen CTest **94/94 passed (80.45 s)** and clean
`git diff --check`. New scenarios cover actual factory consumption, unattended
multi-year top-ups with/without an HQ catalog, mineral conservation, competing
fractional unloads and unrelated cargo, no empty receipt/movement-fuel churn,
capture/neutral blockers, pause/resume, 34-year delayed edits, stale projection,
invalid targets, real Qt controls and draft/conditional forecasts, partial and
in-flight save/load, hidden-event byte-identical player exports, PBEM targets,
forged old envelopes/trailing bytes and genuine v104 zero-target continuation.
Existing v103/v102 and older migration/cargo/route/controller suites pass.
No manual campaign balance, cross-platform or sanitizer acceptance is claimed.


Stationary-installation delivery follow-up: GCC 14.2 / Qt 6.12.0 full Debug
application/test build succeeded; offscreen CTest **94/94 passed (40.05 s)**.
Existing core/Qt/save suites cover depot/independent-yard delivery, physical
shared capacity and fractional cargo, target waiting/conservation/no HQ,
identity/ownership/loss/disabled blockers, received-only state/export, real UI
selection, physical/draft/in-flight programmes, received and pending far-away
receipts, PBEM and old-envelope rejection, and v105 planetary continuation.
Older migration and other suites pass. Prepared for review, not merged; no
manual campaign/balance or other-platform/sanitizer acceptance is claimed.
