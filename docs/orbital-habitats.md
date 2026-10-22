# Orbital habitats — first population slice of #56

Approved in the dialogue on 22 October 2026 (Europe/Moscow). Base main:
`697bec0`, merged #228; #59 is closed. This slice does not expand frozen #63,
radical irreversible terraforming, loyalty, or independent deep-space colonies.

## Purpose and ownership

One completed habitat houses **2,000,000 orbital residents**, separately from the
surface. It shares the colony's owner, mineral store, research allocation and
production queue. It occupies the existing **single engineering slot**: choosing
it competes with mirrors, shades and either magnetosphere. It changes neither
surface environment/Hab/capacity nor mineral concentration.

Start construction over an owned populated surface colony. A finished habitat
starts empty and must receive real colonists. Once populated, it can preserve
the colony when the surface becomes empty. It is a permanent structure on the
planet, not the station's hull or another armed station. Empty structures can be
mothballed without upkeep; dismantling occupied structures is rejected. Paid
work, builder and dates survive interruption/capture. Dismantling refunds nothing.

The main value is demographic capacity, an expedition/colonization population
source in a developed hub, and shelter from surface conditions/bombing. It is
not a superior factory: current population output is only one unit per 500,000
people, while factories cost six work and have no workforce cap. An accessible
good empty world or further surface development can remain a better investment.

## Initial prototype balance

| Requirement | Amount |
| --- | --- |
| Technologies | Construction 18, Energy 16, Biology 14; environment-based campaign |
| Construction | 4,800 work; 80/year maximum, at least 60 years |
| Construction minerals | I/B/G 480/240/480 kt, paid per work |
| Support | Owned surviving Starbase/Fortress with shipyard |
| Occupied annual upkeep | 2 production and I/B/G 1/1/1 kt |
| Emergency reserve | Three prepaid annual life-support charges, included in construction |
| Reserve recharge | At most one charge/year; additional 2 production and I/B/G 1/1/1 |

Ordinary queue priority and research allocation apply to construction. Removing
its row pauses work without refund or loss of investment; duplicate starts cannot
create another installation or multiply the annual assembly allowance. Output
controls are not applicable to housing; pausing construction cannot shut off an
occupied habitat's life support. No scientific specimen or exotic mineral is needed.

Occupied upkeep, then affordable reserve recharge, take priority over research
allocation and optional construction. Both production and all three minerals
must be available for an entire payment. Partial payment is not lost. Minerals
come from physical surface stocks replenished by normal mining/freighters; there
is no food resource, abstract supply network, station fuel tank or free upkeep.

## Population and cargo

Use Planetary engineering's explicit surface/orbit resettlement controls, Cargo's
separate orbital endpoint, or a route's **Orbital colonists** load/unload policy.
Old Colonists and All cargo policies retain their surface meaning. Local
surface/orbit shuttles require working support and the owner's operating tech.
Transports may evacuate orbital residents even after station loss; all residents,
including the last, may leave. Orbital cargo endpoints accept population only.
Unloading beyond habitat capacity is rejected atomically for exact transfers;
arrival policies unload only what fits and retain the remainder aboard.

Interstellar cargo uses the same hold/mass/fuel/gate/split/merge rules as surface
colonists: 100 people occupy 1 kt. Orbital transfers cannot alias surface minerals,
science samples or xenocrystal. There is no automatic migration, rescue ship or
implicit evacuation policy. Surface transfers retain the last resident when no
orbital population remains; moving everyone into an owned habitat is permitted.

While supported and paid, orbital growth at annual end is
`N * 5 * (2,000,000 - N) / (100 * 2,000,000)`, in whole people, with minimum one
for a nonempty growing population. It stops at capacity. At N=200,000 the initial
growth is 9,000/year; at half capacity it is 25,000/year. No starting population
appears on completion. Surface growth keeps existing racial Hab/crowding rules.

Active orbital residents join the surface population **once** for the ordinary
one-output-per-500,000 calculation. The colony pays upkeep before allocating
output to production/research. Orbital residents do not add surface extraction
workforce; mines/geology retain their current rules. Empire and colony history
population totals include both groups, while current UI shows them separately.

## Interruption, combat and capture

Maintenance resolves after movement/combat and ordinary deliveries, before local
production/research. A station finished later in production supports the next
maintenance year. Construction and normal occupation require the owner's own
technology, including after capture. Restored support never creates a free reserve.
The saved annual service date prevents duplicate payment, reserve use or loss
when a resolution date is retried or a save is reloaded.

Without support, technology, minerals or production for upkeep, orbital growth
and output stop. Each unsupported year spends one reserve charge. Three such
years cause no population loss; subsequent unsupported years lose **ceil(10%)**
of surviving residents. The physical structure and investment remain. A return
to paid service stops attrition and can recharge one reserve year at its real cost.
An empty habitat has no upkeep and cannot refill reserves for free.

Hostile fleet presence is not a new abstract blockade toggle. Ordinary combat
can kill supply transports or the station; existing stocks and support otherwise
continue to work. Destroying/refitting away the station's shipyard removes
support, not the habitat or its population instantly. Surface bombardment affects
surface residents/infrastructure. It must not abandon/delete a colony while
orbital residents survive. Separate targeted habitat combat is outside this slice.

Any hostile station must still be destroyed before invasion. Defense uses the
**combined surface + orbital population**, preserving the existing invasion
probability and casualty rule. Winning-defender casualties are distributed
proportionally, deterministically and in whole people. On defeat, the defender's
population is destroyed under the existing war abstraction; surviving attackers
settle the surface and inherit an empty habitat and preserved work. A lone
colonist cannot bypass a populated orbit just because the surface is empty.
Station capture, surrender, prisoners and loyalty are not introduced here.

## Received evidence, UI and computer empires

The owner receives immutable orbital/deeper observations of both populations,
service/reserve/loss state, production/mining inputs, stocks, queue and supporting
station fit/damage and shipyard availability. These are one coherent baseline
for UI and forecasts. Older saves have no habitat baseline; fresh owner reports
supply it without inventing historical orbital state. Foreign observations
show the engineering structure's public progress/status, without internal population,
reserves, supplies or scheduling. Already dispatched packets remain unchanged.
Delivered battle evidence updates known station loss/damage independently. Its
date and turn phase survive a later-arriving older surface report, including a
pre-combat observation from the same year, and cannot overwrite a later
final-phase refit observation.

Planetary engineering provides build/pause/dismantle and resettlement controls;
System Details and colony summaries expose orbital population. Ship-building
controls and order authoring use the same received station snapshot as the
engineering panel, including delivered station-loss evidence; physical execution
still checks the current host shipyard. Evidence year/age and forecast labels
apply. Cargo requires current received endpoint
information for immediate orbital transfers; historical visibility is not a live
transfer permit. Routes use received planning data but execute against real stock,
capacity, ownership and combat. One-year upkeep/output/growth forecasts first
add ordinary mining from received colony inputs, then apply annual service to a
copy. They do not mutate the observation or assume future freight arrivals. The
queue forecaster already mines each simulated year and uses the service resolver
directly, so mining is not added twice. Future arrivals are not guaranteed supply.

The basic AI works only through PlayerView and ordinary paid orders. It can choose
a habitat at a productive, near-capacity colony with a free slot, technologies,
received support and stocks covering construction plus a reserve margin. It seeds
an acknowledged completed habitat from the surface. After receiving an emergency
status it can evacuate with an available local unassigned transport to a reachable
friendly surface colony, with ordinary cargo and fuel costs. The rescue uses a delayed ordinary
load-and-return route. Stale telemetry does not authorize an immediate transfer. Automatic remote rescue-network scheduling
remains future work.

Life-support transitions, reserve changes and yearly casualties use delayed
engineering messages and the existing History plumbing; ordinary engineering
reports cover construction milestones, capture and dismantling.

## Persistence and acceptance

Formats: **save v94 / orders v38**. HABI is a bounded tail after SCIC, preserving
physical habitat state, annual debit guard and private received/in-flight economic
snapshots. Earlier saves start with no habitats; old pending cargo keeps surface
meaning. Existing enums append values; ordinary bodies remain readable through
version gates. Bulk cargo packets add explicit orbital endpoint flags; route
cargo adds OrbitalColonists. Special cargo rejects orbital aliases.

The core/editor suites cover paid finite construction and interruption, technology
requirements, separate growth/output, reserve/attrition/recharge, atomic cargo,
physical route evacuation after station loss, orbital-only defense/capture,
received knowledge and fair AI, stale-transfer rejection, station battle/survey
ordering, surface bombing with orbital survivors, GUI order authoring, v93 migration, corruption,
annual guard save/load, private exports and PBEM payloads. Full validation results
are recorded at the top of project-status before the PR is prepared.

Future #56 work includes independent orbital founding, additional housing sizes,
dedicated habitat combat, atmospheric/ocean/climate projects and bounded system
engineering. Radical irreversible transformation stays deferred for a separate
preparation/evacuation/point-of-no-return/capture discussion. #56 stays open.
