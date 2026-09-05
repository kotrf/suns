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

Positive fuel rate consumes fuel. Negative fuel rate means the drive collects more fuel from interstellar space than it spends, so the tank fills while travelling. This is how ram-scoop drives can have effectively fuel-free or fuel-positive low-Warp regimes.

The current catalog contains four drives:

- **Fusion Drive** — straightforward starter engine, available through Warp 8 with steep fuel burn at its top speed;
- **Advanced Fusion Drive** — Propulsion 1, light and radiation-safe through Warp 9, but expensive and always consumes fuel;
- **Ram Scoop Drive** — fuel-positive at low Warp, economical at moderate Warp, maximum Warp 9;
- **Radiating Ram Scoop** — stronger scoop behaviour and Warp 9 capability, but carries a radiation hazard for transported colonists.

No current engine provides safe Warp 10; all can reach it using damaging overdrive.

## Overdrive damage

Initial balance values, in percentage points of hull damage per full turn of travel:

| Engine | Safe Warp | Warp 9 | Warp 10 |
| --- | --- | --- | --- |
| Fusion Drive | 8 | 12% | 35% |
| Advanced Fusion Drive | 9 | 0% | 10% |
| Ram Scoop Drive | 9 | 0% | 18% |
| Radiating Ram Scoop | 9 | 0% | 14% |

Damage is deterministic and proportional to distance travelled divided by Warp squared. Short legs and fuel-limited movement therefore incur only their actual exposure. Safe flight adds no damage. At 100% damage the fleet stops at the point where integrity runs out and its route is cleared; it remains on the map. Repair mechanics are a follow-up, not part of this slice.

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

If a normal drive lacks enough fuel for the requested Warp distance, the fleet travels only the distance its remaining fuel can support and keeps its course. A ram-scoop with a negative fuel rate can move even with an empty tank and collect fuel during that movement.

## Mass and cargo

Fuel consumption scales with gross ship mass:

`gross_mass = fitted_design_mass + cargo_mass`

Fuel itself is intentionally not counted as kt-scale ship mass because its game units represent a much smaller antimatter/reaction-mass quantity.

Colonists are real cargo. The current conversion is:

`100 colonists = 1 cargo unit`

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

## Planned follow-ups

1. Add an explicit Warp selector and fuel forecast to the Qt route UI.
2. Make production queue a concrete `ShipDesign` rather than a hard-coded Colony Ship production kind.
3. Add player-managed loading/unloading of colonists, minerals and fuel.
4. Apply radiating-engine hazard to transported colonists according to race radiation tolerance.
5. Add hull slots and a player-facing ship designer where tanks, cargo pods, engines, scanners and later combat equipment compete for space/mass/cost.

The guiding rule remains the same: every component should create a strategic decision, not merely add another statistic.
