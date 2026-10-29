# Transport lines — first #53 slice

The first slice was merged in #237, main `4fb67cf`, save v102 / orders v42.
The load-policy follow-up starts from that main and uses **save v103 / orders v43**.

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
to the old owner; the ordinary arrival action fails and the programme continues.
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
recording real deliveries; this slice does not attribute totals to line IDs.

Computer empires retain their existing ordinary freight planner. This slice
adds no automatic staffing of the new shared catalog; a computer-issued line
order goes through the same validation/commands as a human-issued order.

## Persistence and boundary

Bounded **LINE** follows existing CCON/ORLC. It stores only each owner's catalog,
nonreused-ID high-water mark, frozen received endpoint coordinates and roster
issue dates. Physical, received and in-flight route programmes keep their
existing representation. Save v101 loads with an empty catalog and preserves
its ordinary fleet programmes; earlier migration paths remain. Orders v42 introduced SetTransportLineOrder and AssignTransportLineOrder.
Save v103 / orders v43 append reserve/minimum to the versioned LINE/definition
payload and add a conditional payload only for LoadMinerals. Older arrival
actions keep their layouts. A genuine v102 catalog migrates to zero/zero,
preserving its old programmes. Player exports contain only their own catalog.

Limits are 128 lines/owner, 256 assigned IDs/line, 64 UTF-8 bytes/name, distinct
received system coordinates and Warp 1–10. Malformed collection counts,
revisions, duplicate roster identities and truncated tails are rejected.

Remaining #53: multi-stop lines, destination stock targets, richer wait/refuel/service
policies, fleet-base/depot stops, automatic staffing/resupply and honest AI use
of the shared catalog, per-line throughput/utilization/history, interruption
briefings, and explicit emergency diversion/evacuation. Colonists stay physical
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
