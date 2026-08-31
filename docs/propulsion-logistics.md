# Propulsion and logistics

Suns! uses a Stars!-inspired Warp model so propulsion creates route-planning and ship-design decisions rather than a single generic speed statistic.

## Warp and distance

Map distance is interpreted as light-years. A fleet travels at most:

`distance_per_turn = Warp * Warp`

Therefore Warp 6 covers 36 ly/turn, Warp 9 covers 81 ly/turn and Warp 10 covers 100 ly/turn.

Warp is a property of an active fleet course, not a permanent ship speed. A `MoveFleetOrder` may specify Warp explicitly; `warp = 0` means keep the fleet's current setting. The current Qt UI still uses the initial cruise Warp and will get an explicit Warp selector in a UI-focused follow-up.

The starting Scout cruises at Warp 8. The starting Colony Ship cruises at Warp 7. Warp 10 remains part of the global movement scale but is reserved for later high-technology engines.

## Engine fuel curves

Each engine component provides:

- maximum Warp;
- a signed fuel rate for Warp 1..10, measured as fuel units per 100 kt of gross ship mass per light-year;
- optional radiation hazard metadata.

Positive fuel rate consumes fuel. Negative fuel rate means the drive collects more fuel from interstellar space than it spends, so the tank fills while travelling. This is how ram-scoop drives can have effectively fuel-free or fuel-positive low-Warp regimes.

The current catalog contains four drives:

- **Fusion Drive** — straightforward starter engine, available through Warp 8 with steep fuel burn at its top speed;
- **Advanced Fusion Drive** — Propulsion 1, light and radiation-safe through Warp 9, but expensive and always consumes fuel;
- **Ram Scoop Drive** — fuel-positive at low Warp, economical at moderate Warp, maximum Warp 9;
- **Radiating Ram Scoop** — stronger scoop behaviour and Warp 9 capability, but carries a radiation hazard for transported colonists.

No current engine provides Warp 10. That step is deliberately reserved for future propulsion technology rather than being safe starter equipment.

The numeric curves are tuning placeholders. Their strategic shape is intentional.

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
