# Orbital Civilization (#50): Alternate Reality-inspired design

Status: selected direction, 7 November 2026. The user chose a station-dwelling
civilization inspired by Stars! Alternate Reality. This replaces the earlier
choice between Habitat Civilization and Remote Logistics Specialist. The reserved
`HabitatCivilization` identity is the implementation candidate; it is not yet
playable. The reviewed design was merged in
[PR #282](https://github.com/kotrf/suns/pull/282); implementation proceeds in
reviewable stages without enabling an incomplete race.

## Implementation stage 1: station residents and cargo (merged #283)

The first implementation supplies `NativeOrbitalPopulation`, separate from the
ordinary late-game habitat, with a physical station ID and real people. Battle
destruction clears native residents/ownership exactly once; already evacuated
transport cargo is unaffected. Station refits preserve the ID/residents and
reject a smaller hull that cannot house them. Prototype housing for the existing
Dock / Starbase / Fortress is 100,000 / 500,000 / 2,000,000; these are initial
kernel limits, not a completed native hull/technology balance.

Shared `UnloadAll` handles partial native station unloading and independent
mineral delivery. Explicit station transfers/evacuation conserve residents;
surface exact/final-manifest orders and ordinary surface founding/invasion cannot
bypass the native housing requirement. Received and pending owner snapshots carry
native population separately. Delivered destruction is retained by station ID
and observation time independently of any cached fit, so a later pre-battle
snapshot cannot resurrect a station HQ had never seen. Native colony rows in
History use the same received view as the planet panel. Fleet holds and missing
contacts also come from that view, with missing contacts counted once. These
received observations can still have different dates: a returning transport can
report its empty hold before the destination reports the unloaded residents.
History retains the observation-year range. If dates differ or are missing,
empire population and mineral-stock graphs show a gap and explain that no
synchronized total is available. The stored sum remains a received subtotal,
not an exact empire total; individual colony/fleet records remain available.
This check uses only received evidence, never pending packets or physical truth.
Native freight is credited from the delivered manifest
in its receiving year, including reports already due when a host save resumes.
Earlier observation years are not rewritten, and refreshing the boundary retains
its already measured freight.

Save v115 introduced bounded native population and observation records; v116
adds the delivered destruction ledger. Real v114 files migrate with no native
population, and real v115 files preserve population/queued evidence and migrate
already received native tombstones. Save v117 adds bounded History date metadata;
native rows from older versions have unavailable date provenance and are not
retroactively certified as synchronized. Genuine v116 migration is covered too.
Orders v49 and portable race v3 are
unchanged. Core/save/PBEM tests use deliberately constructed native states;
this does **not** satisfy the later new-campaign/Computer acceptance below.

Campaign setup and race files still reject this identity for both controllers.
Stage 3 below supplies a paid startup kernel and replacement manufacture.
Next stages must deliver paid founding, native growth/radiation and support,
advanced hull progression and actual Computer startup/expansion.
The full first-playable acceptance remains the gate for enabling the trait.

## Implementation stage 2: owned-orbit mining and received manifests (merged #284)

One site rule permits neutral worlds for ordinary miners and additionally an
owned world with a live native inhabited station for this civilization. Work-here,
waypoint authoring, delayed task delivery, arrival and actual extraction use the
same eligibility rule; authoring and rate estimates use received knowledge while
execution rechecks physical ownership. Hostile worlds, ordinary owned surface
colonies and independent habitats do not acquire this mining permission.

Output goes once into the world's existing mineral stock, not the miner's hold.
The native orbital cargo endpoint can collect or return these minerals under the
usual ownership, co-location and cargo-capacity checks. Ordinary independent
habitats retain resident-only orbital endpoints. Native surface/innate extraction
is zero, including malformed states containing surface population or mines.
No initial ship, technology level or free output is granted by this kernel.

Native output combines a miner's stacks into one `RemoteMiningOutput` manifest
per mining fleet, world and year through normal communications. Its source fleet
ID is retained: a transmitting bystander at a neutral world cannot release a
silent miner's outbox. A friendly colony still forwards a local upload. It is queued at the mining phase,
before movement/combat can remove its real source. Mining precedes combat: a
miner destroyed later that year has already worked, but cannot work next year.
History credits only delivered manifests in their receipt year; refreshing a
boundary does not count them again or rewrite the observation year. Fleet/planet
panels identify rate estimates as based on last reported tasks.
Mining report map navigation uses the recorded world/position rather than the
source fleet's later location. Local History site selectors, including comparison
and Follow map, use only recorded extraction rows; retaining a legacy host amount
does not expose its unconfirmed site.

Save v118 accepts the new report/event kinds without changing the base payload,
orders v49 or portable race v3. Pre-v118 native extraction amounts are retained
but marked unrecorded because they lack receipt provenance. PlayerView/PBEM omit
those unrecorded amounts and site details; the host retains its archived values.
A genuine v117 file,
written with the merged #283 writer before this format change, verifies migration
and continued received accounting. Constructed core/UI/host/PBEM fixtures do not
replace the full paid new-campaign/Computer acceptance below.

## Implementation stage 3: paid startup and replacement manufacture

`initialize_native_orbital_start` constructs an empty native empire's capital on
the authoritative first-turn host state. It takes an explicit endowment and
returns the actual asset bill. Insufficient work, minerals or population, an
occupied world, an already initialized empire or a player-view call rejects
without changing state. This is a construction kernel for the later campaign
integration; it does not bypass the still-closed setup/import/restart whitelist.
It neither provides paid founding nor demonstrates a playable Computer campaign.

The prototype grant covers 25,000 real residents in an Orbital Dock (100,000
capacity), Shipyard and Refueling Depot services, a full planetary receiver and
one working MiniMiner with Quick Jump 5 and one Robo-Mini-Miner. Its 210 mg tank
starts full, and its reported task is Remote Mining at the capital. Surface
population/factories/mines and ordinary habitats start empty. Its initial home
snapshot, fleet telemetry and History agree; other players' observations are not
refreshed by this construction operation.

An accepted custom startup must have nonzero actual orbital output at its current
Energy level. With Energy 0, 1–9 residents reject atomically; 10 can produce one
resource. Higher Energy uses the same output function rather than an unrelated
population minimum. Before population growth is enabled, a zero-output station
cannot recover through either manufacture or research.

| Asset | Work | Ironium / Boranium / Germanium (kt) |
| --- | --- | --- |
| Orbital Dock, including its two standard services | 24 | 12 / 6 / 8 |
| Full planetary receiver | 16 | 2 / 3 / 4 |
| MiniMiner hull + Quick Jump 5 + Robo-Mini-Miner | 153 | 58 / 0 / 14 |
| Total initial asset bill | 193 | 72 / 9 / 26 |
| Liquid stock after paying the bill | — | 100 / 100 / 100 |

The default mineral endowment is therefore 172 / 109 / 126 kt, with 193 work
reserved for initial assets. Costs are calculated from the ordinary station,
radar and ship price functions; the receipt records what was consumed. Setup
work is an initial construction allowance, not a recurring income or a stored
production bank. Population and a full initial fuel tank are explicit separate
setup grants, as in ordinary campaign starts. This is a larger declared asset
endowment than an ordinary scout start, not a claim of final comparative balance.

Native residents produce `floor(sqrt(min(residents, hull capacity) *
(min(Energy, 26) + 1) / 10))` work per year, using integer arithmetic. At 25,000
residents this gives 50 at Energy 0 and 100 at Energy 3. The `+1` prevents an
Energy-zero research/manufacturing deadlock. Only residents linked to an owned,
surviving station contribute; surface assets do not. Temperature and gravity do
not change this output. Radiation, protection, growth and support are still
pending and this formula is a reviewable prototype, not the completed native
environmental economy. Native population does not grow in this stage.

The existing generalist start still yields 29 resources (25 population + four
operating factories). Native early access grants only MiniMiner and
Robo-Mini-Miner through shared availability checks; Quick Jump 5 is already
legal at level zero. No Construction/Electronics level, Advanced Remote Mining
flag, advanced miner or unrelated hull is granted. Conflicting Basic Remote
Mining, a radiation-immune profile and a profile without environment rules reject
explicitly at startup instead of being silently changed.

Native surface factories/mines, their advanced forms, upgrades and reconstruction
are structurally inapplicable. Host single/batch orders and planning reject them;
templates containing such rows reject atomically, including defaults. Stale
partially built, fully paid or automated rows retire before spending or completion.
Production hides/disables these choices; ordinary empires keep their installations.

Ship Designer, Production choices, availability and queue resolution share the
received design projection. Pending choices retain their owner-scoped name,
including when a temporary projected ID collides with another empire's project.
Loading a native host preserves its actual primary trait in restart setup;
Restart explicitly refuses a reserved trait instead of rebuilding it as Generalist.

That refusal applies to reusing the current empire setup. File → New campaign
supplies explicit new empire settings and validates them independently of the
loaded civilization. Only successful generation commits those settings, detaches
the old file and permits menu viewport fitting; refusal preserves the save path,
title and explanatory status. The native trait itself is still unavailable.

The cheap queue estimate applies the same racial applicability test as the host.
An old native surface row is shown as skipped and reserves no work or minerals,
even when it was an annual rule. It does not obscure the following miner's fixed
estimate. Rows retain their original positions for cancel/reorder commands;
free skips never restore an already uncertain prefix or simulate future years.

With the default zero research allocation, a newly designed replacement miner
costs 153 work and 58 / 0 / 14 kt through ordinary design/production orders. It
completes after four years of 50 work while the original extracts 4 / 2 / 1 kt
per year at concentrations 100 / 50 / 25. The ending stock is 58 / 108 / 90 kt;
the new hull needs its own mining task. Removing the original stops its actual
extraction. Tests use real host turns, decoded PBEM orders, save/reload and
received-state output after an unseen station loss. Formats stay v118/v49/v3;
no new serialized startup allowance is introduced. These prepared-host fixtures
do not substitute for the actual Human/Computer campaign acceptance below.

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
remote mining deposits output in the planet's mineral stock, shared with the
native station and counted exactly once. Stage 2 supplies its orbital cargo access.
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
habitat. On 7 November the user chose to retain the shared `UnloadAll` command
with `Colonists` / `All`: the receiving population depends on the unloading
empire's race. Ordinary empires unload onto the surface; native orbital empires
unload into their own inhabited station. The UI should identify that receiver
without requiring a separate unloading command. This supersedes the earlier
proposal to reject those shared unloading policies for natives.
The following are implementation requirements, not changes enabled by this PR:

| Existing path | Required native behavior |
| --- | --- |
| `SetFleetColonistsOrder` (`Colonists aboard` final manifest) | Reject for natives before changing fleet cargo, surface population or freight records, for both loading and unloading targets. This order names a surface colony and must not be reinterpreted as a station transfer; use an explicit orbital endpoint instead |
| `TransferCargoOrder` with colonists and a surface planet endpoint | Reject in either direction, including an owned planet; no surface-to-orbit fallback. Reject the whole exact manifest before moving minerals or people |
| `TransferCargoOrder` with an explicit orbital population endpoint | Use the actual native resident pool only at an owned, surviving inhabited station; require co-location, real available residents and hull capacity. Native stations also access the world's shared mineral stock; ordinary independent habitats remain population-only. Keep the exact manifest atomic; reject an absent, destroyed, hostile or full receiver |
| `UnloadAll` with `Colonists` or `All` | Resolve native colonists to that owner's surviving inhabited station, up to free hull capacity; excess remains aboard. Ordinary empires retain surface unloading. `All` processes minerals by the ordinary stock/ownership rules, independently of how many residents fit |
| `UnloadAll` with `OrbitalColonists` | For natives, use the same station receiver and capacity checks as shared unloading; this policy is optional, not required for unloading. Ordinary empires retain their independent-habitat receiver |
| `LoadAllAvailable` with `Colonists` / `OrbitalColonists` | Reject the surface policy for natives; the explicit orbital policy loads real native residents, subject to its reserve and transport capacity |
| Fleet-to-fleet transfers | Keep ordinary ownership, co-location, cargo capacity and physical conservation checks; they do not establish ownership of a planet |

For native shared unloading, an absent, destroyed or hostile station accepts no
colonists; they stay aboard. A full station also leaves them aboard, and partial
capacity transfers only the number that fits. `All` may still unload minerals
where ordinary mineral unloading is legal; a blocked population leg does not
discard minerals or make a legal mineral transfer fail. Hostile ownership and
orbital-control restrictions continue to block transfers that are not legal.
Neither a successful population transfer nor a mineral-only delivery constructs
a station or changes ownership. Resolve the native receiver once in shared core
logic used by authoring/preview, arrival execution and decoded PBEM commands.
Physical execution rechecks ownership, station survival and capacity on arrival;
an old waypoint must not fall back to surface population after station loss.

Native fleets cannot conduct ground invasion in this first slice. Both explicit
hostile `TransferCargoOrder` and the population leg of arrival unloading must
reject before calling `invade_colony`, consuming attackers or changing ownership.
Add the same guard inside invasion resolution so imported/delayed commands cannot
bypass it. There
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
The native UI must not offer the surface-only `Colonists aboard` field as a way
to load or unload residents. Its ordinary cargo planning preview must enforce
the same rejection as `apply_cargo_order` and host execution. Receiving a validly
encoded old `SetFleetColonistsOrder` in a PBEM packet does not authorize it for
this race. Check every population-writing path against the native-housing rule;
the table is not permission for another route to write native surface residents.

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
| Ownership and cargo | Shared unloading resolves native residents to their own station; exact station transfers use orbital endpoints; no native surface population, invasion or unpaid founding |
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

Cargo/ownership acceptance must cover `SetFleetColonistsOrder`, explicit
`TransferCargoOrder` and arrival
`UnloadAll` with `Colonists`, `All` and `OrbitalColonists` at owned, unowned and
hostile worlds, including absent/destroyed/full stations, mixed minerals/colonists,
load-back/evacuation and delayed/PBEM packets. Assert exact conservation or an
unchanged rejected manifest, no native surface/ordinary-habitat population, no
unpaid station or ownership change, and no invasion casualties on rejection.
Exercise `SetFleetColonistsOrder` with final cargo targets both below and above
the actual aboard count at an owned live station. Assert rejection and unchanged
ship colonists/minerals, surface and station populations and freight records in
the current-year planning preview, local execution and a real decoded PBEM
packet. Verify that explicit orbital transfers still allow unloading and
evacuation; disabling a surface shortcut must not disable station cargo.
For the shared `UnloadAll` command, compare ordinary surface settlement with
native station unloading for both `Colonists` and `All`. Cover partial/full hull
capacity and a station destroyed or ownership changed after the waypoint was
authored. Assert exact population conservation, excess/blocked residents aboard,
zero native surface residents and no founding or invasion side effects. With
mixed cargo, check that legal mineral unloading still occurs when all or some
colonists remain aboard, while hostile/blocked mineral transfers do not occur.
Run these cases through preview, real arrival execution and decoded PBEM commands;
expose their actual results to HQ only through ordinary received evidence.
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

Current game formats are save v118 / orders v49 / portable race v3. The native
population/mining foundation does not enable a playable race. Mobile residential arks, transport-network
#53 and exotic-material #63 work remain outside this first stationary slice.
#50 remains open for implementation and later comparative balance/other traits.
