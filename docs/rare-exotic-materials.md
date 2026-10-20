# Rare exotic materials — #63 slices

Xenocrystal enables optional late scientific installations and exceptional gate transits.
Ironium/Boranium/Germanium remain the ordinary industrial economy. Scientific
samples from [#59](science-samples.md) remain unique objects; xenocrystal is a
fungible, consumable integer resource, not another artifact or empire currency.

## Discover, extract, carry

New galaxies deterministically place one deposit per 32 planets (rounded up),
in a single geographic cluster around a seed-selected unowned world. Deposits
hold **24–48 whole units** each, never regenerate, and exclude starting capitals.
Habitability does not filter them: a hostile, uninhabitable planet can be the
valuable mining site. Generation uses its own integer hash and does not perturb
ordinary galaxy generation. Older saves receive no retroactive deposits.

The existing **DeepSurvey** reveals a deposit, including a surveyed absence.
System detection, penetrating scans, geology, ownership and cargo delivery do
not reveal underground resources. Scanner fleets progress through the ordinary
orbital/geological/deep observations while remaining in orbit. Discovery becomes
actionable after its frozen survey report arrives at headquarters.

Extraction uses existing **Remote Mining** on an uncolonized world. It requires
an available mining hull/robot design and **Construction 8 + Electronics 8**.
Each mining ship extracts **1 unit/year**, regardless of its robot count; the
shared planetary limit is **4 units/year** across fleets and empires. Splitting
ships does not increase that cap. Existing robot and racial access restrictions
apply. Arrival work starts in the following mining phase, as for ordinary ore.
The last unit exhausts the deposit. No mining occurs without the owner's
delivered deep observation, on foreign colonies, or after depletion.

Crystals accumulate in the planet's physical surface cache, alongside ordinary
remote-mined ore. A hauler must collect them with a local transfer or a waypoint
**Load all available → Xenocrystal**, then physically deliver them. **Unload all
cargo** includes crystals; a dedicated xenocrystal unload is also available.
Uncolonized caches are shared physical caches: another empire with the necessary
received knowledge and transport can collect them. Colonization and ground
capture preserve surviving stocks. Colonizing a deposit ends remote extraction
under the existing uncolonized-world mining rule.

One unit is **1 kt**. Quantities are unsigned integers throughout mining, cargo,
orders and persistence; the implementation bounds each store at 1,000,000 units.
Crystals share hold capacity and gross mass with population, minerals and whole
scientific samples. Fuel and gate mass use that same total. Generalist gates
keep their no-cargo rule; Stargate Specialists may carry crystals within both
endpoints' existing limits. No existing ship, gate or climate-project recipe
acquires a crystal requirement.

Local transfers use owned fleet endpoints and friendly/unowned surfaces,
co-location, live contact and the destination's shared free hold. The planning
path also requires current received telemetry and surface evidence. A dated
remote manifest does not authorize a fictitious dockside exchange. Pending valid
transfers replay in order without advancing the year. Remote arrival policies
act on the actual surface stock and remaining hold at arrival, so estimates
based on old evidence remain estimates.

Transfers touching an Expedition Base use its ordinary post-movement service
phase: Expedition Dock, enabled external services, a stationary base without an
active route and actual co-location are required. Fleet-to-fleet commands travel
to a remote base with the normal communication delay; nothing is reserved in
transit. At the planning boundary, already-due commands enter the service queue
in their saved order before new submissions. Commands with zero delay and
surface exchanges join that same queue in submission order; endpoint type does
not change priority. Commands still in transit reserve no hold or stock.
Bulk cargo and crystals share service ordering and hold limits. These
exchanges occur **after colony production**, so their deliveries become usable
for construction in the following year. Construction forecasts do not spend
these future deliveries. Local transfer controls reject pending departures or
disabled services rather than showing a fictitious dockside transfer.

Splitting allocates whole samples first, whole crystal units second, integer
population third and divisible minerals last. Both final holds must fit before
the operation commits. Explicit and on-arrival merges share the physical and
confirmed special-cargo handling. Historical manifests remain separate from
truth. Partial combat losses preserve only whole special cargo fitting surviving
holds, then reduce ordinary cargo; total destruction loses its cargo. Lost and
consumed crystals do not regenerate.

## Spend: resonance laboratory

A colony may build one **resonance laboratory** through its ordinary production
queue. It enables retained scientific artifacts to produce their **native-field
research outside the empire's current focus**, in parallel with that focus.
The output is ordinary scientific data: **32 RP/artifact/year** in its native
field, credited after report delivery. Artifacts already matching the focus keep
their existing 4 RP contribution; the laboratory does not count them twice.
Each artifact still requires its owner's delivered completed study, populated
housing, active research and a nonzero research allocation.

| Requirement | Initial balance |
| --- | --- |
| Technology | Energy 16, Electronics 16, Construction 14 |
| Local support | Populated colony and operational friendly Starbase/Fortress with shipyard |
| Production | 600 work, at most 60/year; minimum 10 years |
| Ordinary minerals | 0.20 I / 0.10 B / 0.20 G per work; 120/60/120 total |
| Xenocrystal | 8 units physically present in the local surface cache |

Starting the project schedules work; it does not spend crystals. The **first
positive, affordable work** consumes all eight units once. Zero production,
missing station/technology, ordinary-mineral shortage or insufficient crystals
cannot charge them prematurely. Further work pays only its proportional
ordinary minerals and production. Duplicate queue entries cannot multiply the
annual allowance. A stopped manual project keeps queue priority until supplied;
after using its annual allowance, surplus production can serve later items.

Pause and ordinary queue cancellation preserve the physical work, original
builder, paid crystals and paid minerals. Resume requeues only remaining work
and cannot charge eight crystals again. Capture also preserves the project and
stocks, clears normal scheduling, and requires the new owner's explicit resume,
technology and support. Station loss stops construction and operation; restored
support restores operation when otherwise eligible. Completed operation can be
toggled. Dismantling destroys the installation and investment **without refunds**;
a replacement must be paid for again.

## Spend: resonant gate pulse

The second #63 slice adds an optional consumable use in **Fleet → Transit
stargate…**. After **Energy 18 / Electronics 18 / Construction 16**, eight
xenocrystals physically delivered to the **source colony surface cache** enable
one otherwise valid transit at **twice each endpoint's finite range**. A Jump
Gate reaches 320 ly instead of 160; a Network Gate reaches 1500 instead of 750.
The smaller endpoint still binds. Infinity stays unbounded and gives no extra
benefit when both endpoints already accept the ordinary distance.

This is a paid pulse through already constructed gates, with no permanent
upgrade or new free fitting. Both stations retain their existing construction,
ownership, hull, racial access and damage requirements. Fleet mass limits,
Generalist cargo restrictions, an idle fleet, immediate command link and the
one-hop-per-fleet-per-year rule remain. Fuel and permitted cargo stay aboard.
Eight units buy a substantial range extension, at the same material price as a
laboratory: a 24–48 unit deposit funds only 3–6 such pulses. These are initial
balance values for review.

Ordinary transits neither require the new technology nor consume crystals.
A pulse within ordinary range is rejected without payment. Successful extended
transit consumes all eight units atomically from the source cache; failures
consume none. Cargo aboard the fleet and destination stock cannot pay. Earlier
valid local unload orders can supply a pulse in the same year; later unloads,
remote arrivals, mining and mobile-base service deliveries cannot. Multiple
fleets compete for the same physical stock in order, and repeated orders cannot
charge the same fleet twice in a year. Transit precedes colony production, so a
laboratory waits if the pulse spent its last eight crystals.

The dialog explicitly selects resonance, shows received source stock and its
telemetry date/age, and labels the result as a planning estimate. Planning uses
PlayerView plus valid earlier pending cargo/transit orders. Resonant estimates
require current boundary stock and fleet telemetry, rather than spending stale
reports. The host independently rechecks real supplies and both gates. Hidden
stock changes cannot rewrite the old observation or player export; an optimistic
estimate may consequently fail on the host. Stock updates and successful/failed
transit messages follow the existing report delivery. Success identifies the
eight consumed crystals; failure charges none.

The basic Stargate Specialist AI can select the same ordinary paid pulse for
exploration or military staging from received information. It reserves prior
planned pulses, requires current evidence and a flight saving of at least
max(160 ly, two current-warp yearly legs), and awaits normal arrival telemetry.
It builds gates and transports rare cargo through the existing policies; this
slice does not add a dedicated gate-fuel supply-chain optimizer.

Save **v90 / orders v36** append the explicit resonance flag to gate orders,
including pending local orders and PBEM packets. v89/v35 and earlier gate orders
default to ordinary mode. Malformed flag values are rejected. The physical,
received and in-flight crystal stores keep the existing EXOT representation;
there is no new balance or empire currency.

### PR #224 review follow-up

Cargo and System Details now enter the common projection through PlayerView,
including endpoint link checks, so an undelivered station loss cannot change
container lists or population forecasts. The command-boundary projection shares
route, task, electronics, mission and fleet-reorganization handlers with the
host. Accepted earlier commands affect gate eligibility in order; invalid
commands do not. Explicit route/task clearing can permit a later pulse, while
an active route or task leaves its crystals available to another fleet or a
laboratory. No annual movement or work runs during projection. A resonance
estimate requires an explicit current received manifest date; a connection at a
predicted position cannot substitute for reception. Projected local split
results inherit the source evidence date; a merge inherits the older input
date, with zero remaining unknown. Derived manifest fields never upgrade that
date through the connection-based snapshot fallback. Thus current local
reorganization remains usable while absent/stale evidence, including repeated
split/merge chains, cannot reserve a pulse. Physical host reorganization and
telemetry delivery retain their existing rules.

## Knowledge, interface and computer empires

The **Rare materials — xenocrystal** dock shows known deposits, remaining units,
received surface/fleet inventories, observed extraction, evidence dates and
laboratory progress/payment/blocking reason. Deposit and surface observations
retain their own dates when delayed reports arrive out of order. A newer shallow
scan cannot erase or refresh an older deep measurement. Private project evidence
is frozen for its owner; foreign surveys do not disclose paid investment.
Ownership alone does not expose current laboratory progress, payment or
operation. The received snapshot also retains its queue position and remaining
work, including while physical completion is still in transit. Without a
received snapshot the project is unknown; there is no physical-state fallback.
Hidden extraction, stock movement and depletion cannot rewrite a received
snapshot or exported player turn.
Foreign colony warehouses remain private even under DeepSurvey; the received
geological deposit and shared uncolonized surface caches are separate evidence.

Controls issue ordinary orders: fly and survey, fly and remote mine, collect
and deliver through a two-leg route, local transfer, build/resume, pause,
operation toggle and confirmed destructive dismantling. Production estimates
use received state plus valid pending cargo/research/project orders. They do not
simulate a year on opening the panel. Research shows parallel scientific reports
separately from guaranteed focused RP. Discovery, acquisition, exhaustion,
construction transitions and crystal shortages appear in delayed Turn Messages.

Computer empires use **PlayerView** and ordinary orders. With extraction
technology, survey missions seek DeepSurvey. They assign suitable miners to
received deposits, use haulers to collect received caches and return cargo to
colonies, and can order ordinary miner/transport construction when needed and
affordable. Late empires retain observed specimens and schedule supported
laboratories once the required local crystals are known. They wait for dated
telemetry and cannot inspect hidden host depletion or stocks. This is a basic
policy, not an optimized supply-chain planner.
Received ownership that blocks extraction retires the corresponding mission;
hidden ownership changes do not. Paid laboratories can trigger ordinary station
restoration without requiring another eight crystals.

Save **v87** appends a bounded **EXOT** extension after v86, retaining deposits,
surface stores, laboratories, received/in-flight observations and each physical,
confirmed/in-flight/loss/wormhole cargo record separately, plus frozen laboratory
scheduling and pending mobile-base crystal commands. Orders **v33** adds
integer transfers and project control, and the xenocrystal arrival cargo type.
The readers reject invalid bounds, inconsistent paid work and overfilled physical
crystal holds. v86 and earlier saves keep their previous world, with empty rare
inventories and no laboratory.

## Remaining #63 scope

The original discovery-to-use chain and resonant gate pulse are complete slices. Additional materials, multiple
separated clusters, colony extraction, transport conditional programs, market
trade, other exceptional modules, further wormhole/gate/communications applications and
further AI/balance work remain future extensions. #63 stays open.
