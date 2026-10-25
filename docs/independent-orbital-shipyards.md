# Independent orbital shipyards — first orbital founding slice (#54 / #61)

Approved in the dialogue on 25 October 2026 (Europe/Moscow). Starts from main
`0d4ec03` (merged #232, save v96 / orders v39). The user chose a transported
constructor which permanently becomes an orbital yard, rather than a flying
shipyard. Surface colonization is not a prerequisite. Neither issue is closed.

## Playable loop and identity

Design and pay for an Expedition Base with four ordinary engines and one Orbital
Shipyard Kit. Fit a radiation screen and optional cargo, radio/scanner, protection,
fuel-generation, dock and workshop equipment. Build at an existing colony yard;
load real colonists and I/B/G into its shared hold. Fly to a surveyed planet,
then send Deploy through ordinary delayed communications. It must physically
be there, idle, alone in its fleet and below 100% damage when the command arrives.

Deployment permanently anchors the fitted hull at the selected planet. It keeps
its FleetId, cargo, fuel, damage and ordinary detection/combat/telemetry. Like the
existing anchored outposts, it cannot move, split/merge, colonize, enter a gate
or use a wormhole after deployment. An old route packet cannot unanchor it.
Unlike deep-space kits it can deploy at a star; it cannot deploy at arbitrary
empty coordinates. The shared engine represents its orbit at the system position.

There is one station slot per planet, shared with ordinary Orbital Dock / Starbase
/ Fortress objects and other independent yards, including unfinished ones.
Deployment requires an empty orbit and an unowned or friendly surface; surface
Hab is irrelevant. Simultaneous commands resolve in the existing deterministic
turn submission and fleet receipt ordering. Hidden occupancy is checked on site and
reported after delivery; authoring a command is not an occupancy oracle.

Surface ownership, population, mining and engineering are separate. Founding a
friendly surface colony later preserves the base, its workers, investment and
stocks. It continues to use its own queue, rather than granting the surface an
extra copy of its production. It is not a Starbase/Fortress support service for
planetary engineering. Converting it into a normal colony station, decommissioning,
station capture and larger armed orbital hulls are future work; the initial
unarmed Expedition Base can be escorted and fitted with ordinary armor/shields.

## Initial balance

| Item | Requirements | Mass kt | Work | I/B/G kt |
| --- | --- | --- | --- | --- |
| Expedition Base hull (existing) | Construction 12 | 1200 | 900 | 400/180/220 |
| Orbital Shipyard Kit | Energy 8, Construction 14, Biology 8 | 400 | 400 | 160/80/160 |
| Bulk Radiation Screen | Construction 14 | 300 | 50 | 40/10/10 |
| Composite Radiation Screen | Construction 18, Biology 10 | 150 | 120 | 30/20/40 |
| Advanced Radiation Screen | Energy 16, Construction 22, Biology 14 | 80 | 250 | 20/30/80 |

Engines and other fittings add their ordinary technology, mass and price. Kit
and screens fit only Expedition Base hulls; exactly one kit and at most one
screen are allowed. Each uses a mechanical slot, competing with cargo, tanks
and workshops. They give no combat armor/shield bonus. Their mass counts during
transport, fuel consumption, gates and battle; no exotic material is introduced.
The kit includes its receiving collar and three prepaid life-support reserve years.
A mobile constructor still needs an Expedition Dock for external mobile services.

After anchoring, assembly needs **six annual stages**, each costing I/B/G
**20/10/20 kt**, with at least **5,000 actual workers** and paid life support.
No stage is granted on a second invocation in the same year. Pausing stops
assembly and optional construction, but not inhabited life support. Resume keeps
all paid stages. Repeating Deploy cannot refill reserves or reset the project.

## People, supply and manufacturing

Workers are real colonists, using the same shared hold at **100 people/kt**.
Their space competes with minerals, specimens and xenocrystal. There is no free
crew on completion, natural growth, surface extraction or research output.
Capacity is the actual fitted hold; **25,000 workers** provide the full
**40 work/year**, with proportional integer work below that number. Excess
residents do not create more factory capacity. Hab temperature/gravity do not
enter the population or output calculation.

After movement/combat and ordinary delivered base exchanges, each occupied base
pays **1 mg fuel and I/B/G 1/1/1 kt/year**. It can refill at most one missing
reserve year for another identical payment. Payments are atomic; unused fuel
or one missing mineral cannot satisfy a partial charge. Without support, work
stops and one reserve year is spent. After three such years, lose **ceil(10%)**
of surviving residents each unsupported year. Empty bases pay/recharge nothing;
physical fittings and construction remain. The receiving collar accepts supplies
and evacuation even while empty, paused or exhausted.

Once assembled and supported, queue ordinary available owner ship designs.
At most one hull completes per year; unused launch-year work is not carried over.
No new hull/engine/racial access shortcuts are granted. For each work point,
charge the difference in `ceil(total mineral bill * paid work / total work)`
for each mineral, in whole kt. A shortage stops work at the funded point.
This cumulative debit permits repeated supplies for bills larger than the hold,
avoids repeated rounding/debits after reload, and finishes at the exact catalog
bill. Cancel loses paid work/materials and addresses a stable request ID, so a delayed
cancellation cannot remove the next job after an earlier launch. An overtaking
cancel also prevents its older slow build packet from reviving the job. Durable receipts
make replayed explicit IDs idempotent even after completion/cancellation and
reload; separate requests still need separate full payments.

New ships appear beside the base **with zero fuel** and no cargo/population;
refueling transfers finite base stock. Ordinary fuel generation begins under
its existing next-year rules. Remote launches do not appear in the owner's map,
fleet list, AI or exported turn until their first received telemetry.

Refit selects another available Expedition Base design with the kit. It uses
that full design's work/material bill through the same queue; it gives no refund
and retains the old screen/services until completion. It preserves damage,
workers, cargo, identity and anchoring. Completion waits if the new fit cannot
hold the current cargo or fuel. There is no transient capacity deletion.

After maintenance/construction, the existing ordered client service policy can
provide finite refueling and fitted workshop repair. These external services
require completed assembly, workers, support and an enabled policy. The generic
Refuel arrival action can also draw finite fuel above the base's configured
reserve. Ordinary Supply outpost can deliver tanker fuel. No colonized-surface
infinite refueling service is created.

## Radiation

The first fixed orbital background is determined by the star's public class:
Blue-white **90**, White **75**, Yellow-white **60**, Yellow **45**, Orange **30**,
Red **20**, on the 0–100 scale. This is an initial activity proxy, not a new
stellar-flare simulation. It is independent of planetary temperature, gravity,
radiation terraforming and surface magnetic protection.

The chosen screen subtracts **20/40/60** from the background, floored at zero.
Exposure above the owner's racial radiation-range **maximum** loses **0.2% of
residents per excess unit/year**, capped at 20%, rounded upward in whole people.
The minimum of the surface Hab interval is not a requirement for ionizing
radiation. Radiation-immune races suffer no such losses. Radiation losses apply
independently after any life-support attrition, including while assembling or
paused; reserve charges do not protect against radiation. No extra output
penalty is applied beyond the actual reduced workforce.

A Blue-white orbit with a Terran maximum of 40 causes 6% yearly losses with Bulk
(exposure 70), 2% with Composite (50), and no radiation losses with Advanced (30).
Ordinary combat shields do not reduce exposure. Existing colony orbital habitats
are unchanged in this slice. Variable flares and other orbital environments are
future work.

## UI, knowledge and computer players

Open **Independent orbital shipyards** from the menu bar. The dock offers received
constructors/bases, surveyed destination planets, Fly/Deploy, Pause/Resume, ship
queue/cancel/refit and two-way worker/mineral/fuel exchange with a selected
transport. All are ordinary orders. Colonists may be loaded at the origin using
the existing cargo controls. Client repair priorities are available in Mobile
expedition bases. Reports explain deployment, assembly completion, life-support
transitions/population losses and refit completion.

The panel shows the evidence year/age, workers, hold, stocks, assembly stage,
reserves and blocking status. Radiation explicitly shows background → fitted
screen → exposure → predicted yearly loss. Sent and local queue edits are labelled intent;
received stocks are not replaced with promised deliveries. Actual transfers
recheck co-location, ownership, supplies and hold capacity on receipt, after
movement. Hidden stocks, refits, losses and work cannot rewrite old telemetry or
player exports. A new remote ship's existence is withheld until received evidence.

The basic AI receives PlayerView only. It can fund one constructor at an available
colony yard for a surveyed unowned uninhabitable target, load finite workers and
supplies, fly to a radiation-safe orbit, deploy and queue an affordable available
ship after received completion. Base stockpile information and design technology
are never read from hidden host state. This first planner does not maintain a
multi-base rescue/resupply network or automatically refit radiation screens.

## Persistence and acceptance

Save **v97** adds the bounded ORBI tail for physical, confirmed, in-flight and
retained-loss base states, independent command payloads and private new-ship
provenance. Orders **v40** adds OrbitalBaseOrder; the previous supported save v96 /
orders v39 remain readable with no orbital founding state. Component and outpost
IDs append to existing enums. New operating-status reports remain ordinary
immutable delayed messages. A historical v94 migration fixture strips later
tails before reconstructing the genuine old layout.

Acceptance covers supplied assembly/pause/resume, no duplicate stages/launches,
finite per-work mineral debits, cancellation/refit, radiation and emergency
attrition, delivered transfers/evacuation/overfull rejection, permanent anchoring,
one occupied orbit, hostile landing blocking, later friendly surface founding,
physical destruction, received-only AI/Qt controls, hidden-independent exports,
remote launch visibility, delayed commands, save/load and PBEM. Actual full-build
and CTest outcomes and limitations are recorded in project-status and the PR.
