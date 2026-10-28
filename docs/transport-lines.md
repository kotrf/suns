# Transport lines — first #53 slice

Selected on 28 October 2026. Starts from main `c957b12`, merged #234,
save v101 / orders v41. This branch uses **save v102 / orders v42**.

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
deterministic fleet order. A depleted source yields an empty trip; there is no
new wait-until-full policy. A captured enemy endpoint cannot surrender stocks
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

## Knowledge, forecasts and interface

Definitions/roster edits are HQ intent and local drafts, not acknowledgements.
Only received fleet telemetry confirms the actual repeating programme. The
Logistics panel shows evidence year/age, last reported target, fuel, hold,
minerals and mismatches with the dispatched programme. It labels received
empty source stocks; unowned mining caches are checked physically on arrival
and are not claimed to be known empty. Missing contacts remain roster entries
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
its ordinary fleet programmes; earlier migration paths remain. Orders v42 add
SetTransportLineOrder and AssignTransportLineOrder; older envelope tags are
unchanged. Player exports contain only their own catalog.

Limits are 128 lines/owner, 256 assigned IDs/line, 64 UTF-8 bytes/name, distinct
received system coordinates and Warp 1–10. Malformed collection counts,
revisions, duplicate roster identities and truncated tails are rejected.

Remaining #53: multi-stop lines, minimum-stock/load targets, wait/refuel/service
policies, fleet-base/depot stops, automatic staffing/resupply and honest AI use
of the shared catalog, per-line throughput/utilization/history, interruption
briefings, and explicit emergency diversion/evacuation. Colonists stay physical
cargo under the existing manual/ordinary-route rules. #53 remains open; this
slice does not expand frozen #63 or decide station capture/irreversible #56.

## Verification

Full GCC 14.2 / Qt 6.12.0 Debug application/test build; Linux/offscreen CTest
**94/94 passed (65.41 s)** and clean `git diff --check`. New core and actual Qt/
save suites verify multi-fleet resource conservation over 20 unattended years,
continued operation without the HQ catalog, delayed commands, same-batch
Pause/Resume, deletion identities, manual override/route-editor precedence,
hidden ownership/stock invariance, player isolation, host continuation, v101
migration, bounded malformed/duplicate/truncated data and PBEM order tags.
No manual campaign balance, cross-platform or sanitizer acceptance is claimed.
