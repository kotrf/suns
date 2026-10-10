# Propulsion and logistics

Suns! uses a Stars!-inspired Warp model so propulsion creates route-planning and ship-design decisions rather than a single generic speed statistic.

## Warp and distance

Map distance is interpreted as light-years. A fleet travels at most:

`distance_per_turn = Warp * Warp`

Therefore Warp 6 covers 36 ly/turn, Warp 9 covers 81 ly/turn and Warp 10 covers 100 ly/turn.

Warp is a property of an active fleet course, not a permanent ship speed. A `MoveFleetOrder` may specify Warp explicitly; `warp = 0` means keep the fleet's current setting. The route dock uses a clickable progress bar, also operable with arrow keys, for Warp 1–10.

The starting Scout cruises at Warp 8. The starting Colony Ship cruises at Warp 7. Every fitted engine may be ordered through Warp 10, including emergency speeds above its safe rating. The selector turns red above the fleet's safe rating and displays the hull damage rate; the adjacent question-mark button explains overdrive.

## Engine fuel curves

Each engine component provides:

- safe Warp (the legacy API name remains `maxWarp`);
- a signed fuel rate for Warp 1..10, measured as fuel units per 100 kt of gross ship mass per light-year;
- optional radiation hazard metadata.

Reference engines store nonnegative consumption and separate collection per engine per ly. Their cargo allocation, rounding and Fuel efficiency modifier are described in the [reference fuel rules](stars-propulsion.md#reference-fuel-rules).

Legacy engines keep signed fuel curves: positive consumes fuel; negative means the drive collects more fuel from interstellar space than it spends, so the tank fills while travelling.

New games use the [Stars! reference engine catalog](stars-propulsion.md). The following prototype engines remain available only to empires that already own legacy designs:

- **Fusion Drive** — straightforward starter engine, available through Warp 8 with steep fuel burn at its top speed;
- **Advanced Fusion Drive** — Propulsion 1, light and radiation-safe through Warp 9, but expensive and always consumes fuel;
- **High Warp Drive** — Propulsion 2, safe through Warp 10, heavier, costly and fuel-hungry at high speed;
- **Ram Scoop Drive** — fuel-positive at low Warp, economical at moderate Warp, maximum Warp 9;
- **Radiating Ram Scoop** — stronger scoop behaviour and Warp 9 capability, but carries a radiation hazard for transported colonists.

Other engines can reach Warp 10 only using damaging overdrive. Existing designs
keep their fitted engines after the technology unlock; refitting is explicit.

## Overdrive damage

Initial balance values, in percentage points of hull damage per full turn of travel:

| Engine | Safe Warp | Warp 9 | Warp 10 |
| --- | --- | --- | --- |
| Fusion Drive | 8 | 12% | 35% |
| Advanced Fusion Drive | 9 | 0% | 10% |
| Ram Scoop Drive | 9 | 0% | 18% |
| Radiating Ram Scoop | 9 | 0% | 14% |

Damage is deterministic and proportional to distance travelled divided by Warp squared. Short legs and fuel-limited movement therefore incur only their actual exposure. Safe flight adds no damage. At 100% damage the fleet stops at the point where integrity runs out and its route is cleared; it remains on the map. At the end of each year, a fleet at a friendly colony with an orbital shipyard repairs 20 percentage points of damage, down to zero. A disabled fleet already at the dock regains the ability to fly after one repair year. Fleets arriving that year can repair; fleets that leave the dock cannot. Repair is automatic and currently consumes no minerals.

Construction 3 unlocks the Field Repair Bay, a general-slot module with an 18 kt mass and 4/2/4 I/B/G mineral cost. Each equipped ship contributes 8 percentage points of repair per year anywhere, including deep space and at critical damage. Mixed fleets receive the ship-count-weighted average, so one repair scout does not fully service a large convoy; a second bay on the same hull does not stack. At a friendly shipyard, the dock's 20-point rate replaces onboard repair. Field maintenance applies once after movement, so overdrive damage still accumulates if it exceeds the repair rate.

When a fleet recovers from 100% damage, the owner receives one Turn Message through the normal communications channel. The earlier route was cleared at critical damage, so a new order is needed to depart. Other repair years do not create messages.

The initial model stores one shared damage percentage per fleet. A mixed fleet uses the highest damage rate among its engines. Merge averages damage by ship count; split preserves the same percentage in both resulting fleets. A future per-ship hull model can replace this approximation. Damage is carried in confirmed and delayed telemetry and saved in format 30; older saves begin with zero damage. Restoring a save preserves unsafe Warp orders.

The numeric curves are tuning placeholders. Their strategic shape is intentional.

## Multi-engine hulls

Engine count is a hull requirement rather than a stacking speed bonus. Light
hulls require one engine; the current Medium Transport, Remote Miner and Utility
Hull require a bank of two identical engines. The selected engine model still
sets the design's maximum Warp and fuel curve. Each physical engine adds its own
mass, construction cost and mineral bill, so larger hulls pay for the machinery
needed to move their certified load without turning two engines into twice the
Warp speed.

## Fuel capacity and generation

A ship design has built-in hull fuel capacity. Components can add more capacity.

Current logistics components include:

- **Fuel Tank** — +300 fuel capacity;
- **Antimatter Generator** — Energy 1; +200 fuel capacity and +50 fuel per turn.

Fleets at a friendly colony are automatically refuelled at the start of turn for now. This stands in for explicit planetary fuel transfer until colony logistics are modelled in more detail.

If a legacy normal drive lacks enough fuel for the requested Warp distance, the fleet travels only the distance its remaining fuel can support and keeps its course. A legacy ram-scoop with a negative fuel rate can move even with an empty tank and collect fuel during that movement.

The reference Stars! engines use individual Warp fuel tables and separate collection per engine per ly. If fuel runs out at the ordered Warp, their fleets use the remaining travel time at the lowest free Warp among their engines. The ordered Warp is retained for the next year. Fuel efficiency reduces their consumption by 15%; legacy engines keep their previous behavior. See [reference fuel rules](stars-propulsion.md#reference-fuel-rules) for rounding, cargo allocation and collection details.

## Mass and cargo

Fuel consumption scales with gross ship mass:

`gross_mass = fitted_design_mass + cargo_mass`

Fuel itself is intentionally not counted as kt-scale ship mass because its game units represent a much smaller antimatter/reaction-mass quantity.

Colonists are real cargo. The current conversion is:

`10,000 colonists = 1 kt of cargo (100 kg per person)`

The starting Colony Ship has 5 cargo units of built-in capacity and currently launches with 250 colonists, so colonization transfers the colonists actually carried by that fleet rather than creating a fixed population from nowhere.

A **Cargo Pod** component adds 100 cargo units. Minerals and other cargo types can later share the same capacity system.

## Ship design interaction

Mass no longer directly reduces the Warp a ship is allowed to order. Instead, a heavier ship pays through fuel consumption. This creates the intended logistics tradeoff:

- a loaded transport can fly Warp 9, but burns much more fuel than an empty scout;
- additional tanks increase range but also add dry mass;
- cargo pods increase useful payload and therefore potential loaded mass;
- scoop engines reward slower economical travel;
- an antimatter generator can trade component mass/cost for endurance.

The earlier `engineThrust / mass` speed metric remains only as a temporary Qt presentation compatibility helper. Turn resolution is Warp-based.

## Completed follow-ups

The original follow-up list below is implemented. Current propulsion uses the
[reference engine progression](stars-propulsion.md); remaining systems are
tracked in [project status](project-status.md).

1. Explicit Warp selection and fuel forecasts in the Qt route UI.
2. Production of concrete `ShipDesign` orders.
3. Player-managed colonist/mineral cargo transfer and fuel logistics.
4. Radiating-engine hazards to transported colonists according to race tolerance.
5. Typed hull banks and a ship designer with competing logistics/combat equipment.

The guiding rule remains the same: every component should create a strategic decision, not merely add another statistic.


## Population units and migration (save v32 / order v3)

Population is a count of people. A homeworld starts with 1,000,000 inhabitants;
100% habitability supports 2,500,000. Population output uses one point per 500,000
people, and the mining workforce denominator is 750,000. Relative growth rates,
starting production/research output and extraction therefore retain their prior
scale. Every transported person is 100 kg; the 5 kt starting colony ship fits
50,000 people if no minerals occupy its hold.

When reading v12–31 saves, planetary population and population-history samples
are multiplied by 1,000. Existing fleet/telemetry colonists and numeric loading
or transfer orders are multiplied by 100: this preserves their previous cargo
mass and avoids silently overloading existing ships. Load All population reserves
scale by 1,000. This is an intentional distinction between surface population
rescaling and cargo-unit conversion, not a population-conservation simulation
step. Old order envelopes receive the same conversion. Re-saving as v32 prevents
any repeat conversion. The UI announces the conversion after loading.

MoveFleetOrder now has an explicit `clearRoute` flag, stored in v32 saves and v3
order envelopes. Older formats retain their coordinate-based clear convention.
All multiplayer participants should update to the same build before continuing.
