# Mini-3

Implementation of [issue #296](https://github.com/kotrf/suns/issues/296), based on
main `6a59a53` after merged #297. Earlier requests to accumulate the list were
superseded by the user's instruction to implement all twelve items.

| Item | Result |
| --- | --- |
| Selected route | Thin cosmetic solid line, retaining its colour; other routes keep their existing styles. |
| Mineral map modes | Mineral Concentration uses 0–100%; Surface Minerals uses the same kt scale as System Details. Three I/B/G bars show received data only. Unknown data has no bars; confirmed zeros have baseline ticks. Stock tooltips retain exact values and the report year. |
| Ship Designer names | New design stays first; existing designs sort alphabetically. Hull/project selection supplies its name and the first unused Gen2, Gen3, etc. suffix, including pending designs. The proposed name remains editable. |
| Homogeneous fleet portrait | Mouse/keyboard activation opens the received project's read-only slot fitting. |
| Mixed fleet portrait | Two silhouettes open a project/count list; choosing a project opens its slot fitting. |
| Emission messages | Double-click/Show on map use the message's immutable coordinates, even without a temporary marker or star/fleet ID. The selected event is frozen before read-state updates. A single click still explicitly marks an already-selected report as read. |
| Delete design | An existing physical ship blocks deletion, including ships in mixed fleets/in transit or another owner's captured fleet. Unbuilt queue/template references are removed; pending-only drafts are cancelled by name rather than a provisional global ID. |
| Refit | A dock job converts one physical ship to another own project with the same hull. The dialog lists removed/installed equipment, recovery, net minerals and work. |
| Empire Research | Technology catalog stretch 3 versus Research plan 1; the lower table no longer reserves 190px. |
| Resonance reports | Ownership changes do not generate a status report when no laboratory existed before or after. Actual laboratory reports show their recorded work/known shortage and never fall into the ship-mineral-warning formatter. |
| Unload all | Existing waypoints allow Cargo changes and retain them in the order. All of the selected cargo type unloads on actual arrival where allowed; All cargo selects all types. |
| Rejected arrival edits | Restore the saved action and all dependent controls after a rejected change, including wheel changes past Unload all toward Colonize. |

## Dock refit and deletion

Refit uses the ordinary colony production queue and payment/resolution sequence.
One job refits one ship; one active refit job per fleet avoids competing work on
the same physical composition. The ship must remain idle at its owner's operational
shipyard, without a pending fleet command. Departure, missing ships or lost support
pause the job; the regular queue Remove action cancels it. Automatic depot
refuelling waits while the ship remains eligible for its dock refit; departing
fleets retain normal service, and completed ships refuel to their new capacity. Paid resource work is
not refunded on cancellation, matching other production jobs. No replacement hull
is created and the other ships in a mixed fleet remain unchanged.

Compare component multisets. Components retained in the target cost nothing,
including equipment moved to another valid slot. Removed components return 10%
of their original I/B/G cost. Newly installed components cost their normal I/B/G
and resource price, plus 50% of their resource price for labour, rounded up to a
whole resource. Hull cost is never charged again. Pure rearrangements/removals can
therefore have zero resource work. Net mineral bills include recovery per mineral;
negative bills return minerals. Minerals settle on completion, so cancellation
cannot create salvage from equipment still aboard.

The resulting fleet must fit all existing cargo and fuel. Refitting never silently
discards passengers, ordinary cargo, samples, crystals or fuel, and does not repair
damage. Ordinary yearly repair/refuelling still follows normal host rules. Quotes
are bounded component comparisons over received own data, without future-year
simulation or polling; the host rechecks physical eligibility before completion. Newly drafted targets
are addressed by their pending project name, rather than a provisional global ID,
so other empires' designs cannot redirect a PBEM refit. Acceptance rechecks the
current received fleet/dock/project and requires the displayed fitting to match.

Deleted projects are archived internally to keep dated reports and old fitting
references readable. They cannot be selected for new construction or revived by
a later build order. Active physical ships prevent archival. Empty templates left
by removing their last design reference are removed rather than saved as invalid
empty templates. Historical records do not become new project choices.

## Formats and verification

Save 127 / orders 55 / race 3 append DeleteShipDesignOrder and RefitShipOrder as tags 38/39
and append ProductionKind::ShipRefit. Only the new production kind writes its
fleet/source/target payload. A retired project's existing placement-count word
uses its high bit as an archive flag; active projects retain their previous byte
layout. The reader gates this flag to 127+, masks it and bounds the count; old
headers reject retired records/new kinds/new orders. Existing genuine migration
fixtures remain in use; no user save or new binary fixture is published.

New tests exercise actual host refit/ordinary payment/physical composition,
deletion and late-build rejection, no phantom laboratory report, actual arrival
unloading of colonists/Boranium/all, map bars, existing cargo editing, rejected
arrival controls, immutable emission navigation, report text, designer names and
read-only fitting, mixed-fleet selection, refit dialog and save/order boundaries.
Complete GCC 14.2 / Qt 6.12 Debug build; final 16/16 focused tests in 21.51s and full 139/139 CTest in 194.98s passed. Actual Qt fitting/refit/map screenshots and 181 local documentation targets were checked. The main baseline CI43527982470 succeeded.

## PR298 review — 14 November 2026

[Review5903254313](https://github.com/kotrf/suns/pull/298#pullrequestreview-5903254313)
on23e3609 found two P2 issues and one P3 issue. Three new regressions independently
fail at the reported assertions before the fix:

- Queue `[ship B, Factory, Mine]`, Cancel Factory, Delete unused B must leave Mine
  in both received planning and the actual host. Additional cases cover deleting
  first, reordering on either side of deletion and a subsequent cancellation.
  Production planning starts from the received state; its existing shared command
  boundary applies create/delete/refit in order, once.
- A docked ship with a Fuel Tank and 0/10 fuel can refit to smaller tanks. Depot
  refuelling occurs after commands and before movement, excluding eligible refit
  fleets at both service phases. Departing fleets still receive normal fuel
  before movement, while their refit pauses. The actual host test waits two years for minerals,
  retains the original fuel, then completes and refuels to the new capacity. An
  original fuel stock above the target capacity still rejects the order without
  discarding fuel. Existing departure/arrival and service tests remain in the suite.
- Qt viewport hit-testing finds each actual mineral rectangle; its tooltip must
  contain the exact I/B/G stock and received report year. The group alone has no
  hover area, so each visible child now receives the current tooltip too.

Complete GCC 14.2 / Qt 6.12 Debug build; final 15/15 focused tests in 15.82s and full 139/139 CTest in 172.73s passed. All 183 local documentation targets in six related documents were checked. The review fix adds no format or balance changes. Current
source, CI and repeat-review status are tracked in PR298; the three discussions
are left open for reviewer verification.
