# Orbital Civilization (#50): Alternate Reality-inspired design

Status: selected direction, 7 November 2026. The user chose a station-dwelling
civilization inspired by Stars! Alternate Reality. This replaces the earlier
choice between Habitat Civilization and Remote Logistics Specialist. The reserved
`HabitatCivilization` identity is the implementation candidate; it is not yet
playable. This PR remains a design draft, not a gameplay implementation.

## Direction supplied by the user

- Population lives on orbital stations. Destruction of its inhabited station in
  battle kills every native resident still aboard: this is the central vulnerability.
- Its strength is developed orbital technology and stronger station hulls. Exact
  hull statistics, prices and research prerequisites are still balance work.
- Mining is exclusively remote, available from the start. The starting economy
  must include working mining ships and equipment, not a late research promise.
- Surface gravity and temperature do not constrain this civilization's orbital
  population; radiation must affect it. This follows the previously discussed
  orbital direction, not an assumption that living in orbit makes radiation safe.

On 7 November the user explicitly accepted the total-loss rule for residents
remaining on a destroyed station. Residents physically evacuated beforehand
remain on their surviving transports; an evacuation order alone is not a rescue.

## Checked Stars! reference and deliberate Suns! differences

The original [Stars! Player's Guide, chapter 22](https://www.bestoldgames.net/download/games/stars/stars-win-manual.pdf#page=213)
describes station-bound population, hull-based capacity and loss of the residents
when the base is destroyed. It also retains environmental suitability, allows
limited innate mining alongside mining ships, and ties production to Energy.
Thus radiation-only suitability and strictly ship-based extraction are our chosen
adaptations. Its other bonuses and penalties are not automatically adopted.

The [Technical Reference Guide, starbase hulls](https://www.bestoldgames.net/download/games/stars/stars-win-guide.pdf#page=25)
identifies Death Star as AR-exclusive; Ultra Station requires Improved Starbases.
A stronger native hull line is the Suns! direction, not a claim that every larger
Stars! station was exclusive to AR.

## First playable slice

### Population belongs to the inhabited hull

Native residents and capacity must be tied to the actual inhabited station.
Larger researched, paid hulls provide room for more residents and stronger
orbital fittings. The initial station is affordable and usable from the start;
ordinary late-game habitat technologies cannot be its hidden prerequisite.

This differs from current `Planet::habitat`: that is a separate permanent
installation which can survive station loss using emergency reserves. Ordinary
empires' existing habitats keep that behavior. Their residents must not acquire
native station-destruction casualties merely because both live in orbit.

Actual battle destruction triggers the native loss once, in physical resolution.
A paid hull upgrade/refit, temporary damage, exhausted supplies or loss of a
shipyard module is not a battle destruction. Upgrades preserve real residents;
refits to insufficient housing require an explicit capacity/evacuation rule.
Already evacuated colonists aboard surviving transports are not station residents.

There is no post-explosion survivor pool, escape-pod fleet, surface resettlement
or automatic rescue. Physical station loss removes its remaining native population
exactly once; colony-loss/ownership cleanup must not leave a productive empty
native colony. Battle evidence and population loss reach HQ through ordinary
delivered reports.
Hidden combat must not refresh population panels or forecasts early.

### Exclusively remote mining, including owned orbital colonies

No native surface mines, population-powered innate extraction or free mineral
income. Remote mining ships work both unowned planets and their owner's orbital
colonies. Hostile ownership remains a real restriction. Ordinary races retain
their existing mining rules; the new capability is not a global permission.

Start with an operable basic mining fit and the ability to manufacture replacements
from year one. This means hull, mining component and legal engine availability,
a real initial miner and fuel, enough initial I/B/G and a viable production
budget. Early access is scoped to this race; it does not grant high research
levels or all advanced mining equipment to every empire.

Deposits, concentration, extraction, stocks and cargo remain physical. Current
remote mining deposits output in the planet's mineral stock; define that stock's
availability to the native station explicitly, counting extraction exactly once.
A destroyed miner stops extraction even when HQ still holds its older telemetry.
Work-here orders and arrival waypoint tasks must share the same eligibility rule.
Race design/import must handle conflicting mining restrictions explicitly.

### Radiation, with no surface temperature/gravity penalty

The native population's suitability/growth and supported output must be
independent of surface temperature and gravity. Radiation remains relevant.
The existing independent-orbit model uses stellar orbital exposure and paid
radiation screens; reusing it is the proposed implementation baseline. Display
orbital exposure separately from surface radiation and never imply that a surface
magnetosphere shields the entire orbital station.

Exact native radiation/growth/output curves and early protection are still
prototype balance. Compatible race presets must be explicit; an imported
radiation-immune preset must not quietly erase the remaining environmental
constraint or be silently rewritten. Starter conditions and legal protection
must be budgeted so the capital can sustain itself.

### Orbital industry and founding

Proposed AR-style economic identity: orbital output depends on real supported
residents and Energy progress, with no native surface factory economy. The user
has not selected a resource formula, efficiency coefficient or factory policy;
these are implementation proposals rather than accepted numerical rules.

Keep the normal 25,000-person total as the proposed comparison baseline. A native
capital, initial mining ship, stocks, fuel, support and research/manufacture must
form one playable budget. A second settlement needs transported real colonists
and a paid orbital founding capability, including finite initial support.
An ordinary colony module must not create a free late-game Starbase.

Larger native hulls and orbital facilities require actual research and payment.
Their advantage should be capacity, fittings/protection and a distinct research
path, not free ships or an across-the-board technology grant. Costs, capacities,
slots, startup output, growth and upkeep are recorded together in the first
implementation PR. Existing habitat values are references, not automatic native
starting endowments.

### Cargo, landing and ownership

For this first slice, native colonists can occupy their own inhabited station or
their own transports, never `Planet::population` or an ordinary independent
habitat. Use explicit orbital population endpoints/policies for station cargo;
do not silently reinterpret the existing surface `Colonists` / `All` policies.
The following are implementation requirements, not changes enabled by this PR:

| Existing path | Required native behavior |
| --- | --- |
| `TransferCargoOrder` with colonists and a surface planet endpoint | Reject in either direction, including an owned planet; no surface-to-orbit fallback. Reject the whole exact manifest before moving minerals or people |
| `TransferCargoOrder` with an explicit orbital population endpoint | Use the actual native resident pool only at an owned, surviving inhabited station; require co-location, real available residents and hull capacity. Keep the endpoint population-only and the exact transfer atomic; reject an absent, destroyed, hostile or full receiver |
| `UnloadAll` with `Colonists`, or `All` while carrying colonists | Reject for a native fleet before moving any cargo. Use `OrbitalColonists` to unload residents; `All` without colonists and mineral-only transfers keep ordinary stock rules |
| `UnloadAll` with `OrbitalColonists` | Unload only into that owner's surviving native station, up to free hull capacity; excess remains aboard. An absent, destroyed or hostile station accepts nobody and creates no colony |
| `LoadAllAvailable` with `Colonists` / `OrbitalColonists` | Reject the surface policy for natives; the explicit orbital policy loads real native residents, subject to its reserve and transport capacity |
| Fleet-to-fleet transfers | Keep ordinary ownership, co-location, cargo capacity and physical conservation checks; they do not establish ownership of a planet |

Native fleets cannot conduct ground invasion in this first slice. Both explicit
hostile `TransferCargoOrder` and arrival unloading must reject before calling
`invade_colony`, consuming attackers or changing ownership. Add the same guard
inside invasion resolution so imported/delayed commands cannot bypass it. There
is no station boarding, conversion of a captured surface colony to native housing,
or free founding by unloading colonists. A native empire can settle a genuinely
unowned world only through its paid orbital founding operation with real settlers.

Conversely, a surviving hostile native station continues to block enemy landing.
Its actual battle destruction kills its remaining residents and clears the native
colony's `Planet::owner` to unowned, with ordinary colony-loss cleanup. It does
not award the attacker a station or a populated colony. A later settlement uses
the attacker's normal paid founding rules. Ordinary surface-versus-surface
invasion and ordinary independent habitats keep their existing behavior.
Authoring, physical execution, delayed commands and PBEM validation must agree;
rejection reports reach HQ through ordinary delivery, not hidden-state UI checks.

### Computer-player availability is part of the first slice

Support for `PlayerController::Computer` is included. Do not enable the trait for
either controller until both Human and Computer acceptance below passes; a shared
trait whitelist in `generate_campaign` alone is not sufficient. Apply that gate
consistently in setup, imported race validation, campaign creation and restart.

The computer planner must source settlers from received native station residents,
evaluate known orbital radiation/protection rather than surface Hab, and budget
remote miners, replacements, paid founding and support instead of surface mines.
It must issue ordinary design/build, cargo, movement, mining and founding orders
from its received `PlayerView`, with the same costs and communication delays.
No pre-scripted free colony, direct host mutation or omniscient target choice can
stand in for a working native computer economy.

## Implementation map and acceptance

| Area | Existing support / required addition |
| --- | --- |
| Identity, setup, race files | Enable the reserved identity only with a working start; preserve other traits and explicit compatibility validation |
| Housing and combat | Native station-resident identity/capacity; battle loss once; paid refit without casualties; ordinary habitats retain their rules |
| Mining | Extend task authoring, waypoint arrival and extraction beyond the current unowned-only guard for this race's own orbit; basic miner available from the start |
| Environment | Exclude surface temperature/gravity; explicit radiation and protection model in core and received UI |
| Economy/founding | A complete paid capital-to-second-colony loop; explicit orbital output, material stocks and founding payload |
| Ownership and cargo | Explicit native orbital endpoints, reject surface unloading/invasion, paid founding only; destruction clears native ownership without granting a free enemy colony |
| Computer availability | Enable only with the Human and actual Computer startup/expansion acceptance; native population, suitability and mining planning through ordinary received orders |
| Knowledge and persistence | Ordinary PlayerView/commands/reports, versioned native state, restart, independent race files, host/PBEM round trips and deterministic continuation |

Acceptance must exercise an actual new campaign through first replacement miner
and second colony, no-miner extraction stopping, owned-orbit mining, identical
results across temperature/gravity changes, radiation exposure/protection, and
station destruction versus ordinary refit. Assert total loss of the residents
still aboard, survival of already evacuated colonists on surviving transports,
and no rescue from an evacuation command still in transit. Delayed battle reports
must preserve old HQ knowledge until receipt; physical loss occurs in the battle
year exactly once. Paid hull replacement preserves the actual residents.
Compare starter output/stocks and native hull upgrades against generalist and
gate-specialist campaigns. Human and computer players pay the same costs and
use the same received evidence; no new omniscient planning is introduced.

Cargo/ownership acceptance must cover explicit `TransferCargoOrder` and arrival
`UnloadAll` with `Colonists`, `All` and `OrbitalColonists` at owned, unowned and
hostile worlds, including absent/destroyed/full stations, mixed minerals/colonists,
load-back/evacuation and delayed/PBEM packets. Assert exact conservation or an
unchanged rejected manifest, no native surface/ordinary-habitat population, no
unpaid station or ownership change, and no invasion casualties on rejection.
After destroying a native station, assert unowned status and require a separate
paid founding operation before either attacker or former owner can settle again.

Run a real new campaign with a Computer-controlled native empire through working
starting-miner extraction, construction of a replacement miner, station-to-ship
settler loading and paid founding of a second working native station. Drive every
turn through the normal computer planner and turn processor; a manually injected
order sequence is not this acceptance test. Assert real production/mineral/fuel/
support spending, zero surface population/mines, no extraction without a miner, and
expansion using received orbital suitability despite adverse surface temperature/
gravity. Include delayed/stale reports and deterministic save/reload continuation;
the planner may not spend missing physical stocks or act on hidden changes.

Current game formats remain save v114 / orders v49 / portable race v3. The eventual
implementation must decide the native-state extension/version explicitly.
This document changes no game rules. Mobile residential arks, transport-network
#53 and exotic-material #63 work remain outside this first stationary slice.
#50 remains open for implementation and later comparative balance/other traits.
