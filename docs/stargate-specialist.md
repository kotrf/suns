# Stargate Specialist

**File → New campaign (races / multiplayer)** offers Generalist and Stargate
Specialist as primary traits, independently of environmental presets. Human and
Computer empires use identical rules, access checks, resources and orders.
Restarting a loaded campaign preserves its playable primary trait. Existing
campaigns retain their saved trait; no gate or technology is awarded on load.
Habitat Civilization and Remote Logistics Specialist remain reserved IDs and
cannot be selected as playable rulesets yet.

## Physical gate progression

These prices and thresholds are initial Suns! balance, not copied Stars! values.
Every gate is a paid station fitting occupying one electronics slot. A station
may carry one gate; different tiers cannot be stacked. Research unlocks the
fitting, and an existing station needs a normal paid refit to change its tier.

| Module | Range | Loaded gross fleet mass | Energy / Electronics / Construction | Minimum hull | Module production | Module I / B / G |
| --- | --- | --- | --- | --- | --- | --- |
| Jump Gate | 160 ly | 450 kt | 8 / 8 / 9 | Starbase | 40 | 0 / 0 / 50 |
| Network Gate | 750 ly | 1200 kt | 14 / 14 / 9 | Starbase | 160 | 40 / 20 / 160 |
| Infinity / Infinity | Unlimited | Unlimited | 24 / 24 / 16 | Fortress | 600 | 150 / 100 / 400 |

Network and Infinity fittings require Stargate Specialist. The ordinary Jump
Gate remains available to all empires with its existing limits and price.
Station hull, other fittings and services add their normal costs. Refit pays
the complete fitted station cost, with ordinary production and surface minerals;
there is no free conversion of already built gates.

Both endpoints impose their own range and mass limits. Infinity connected to
a Jump Gate still has the Jump Gate's 160 ly / 450 kt limits. Only two Infinity
gates provide unlimited range and mass. This is an explicit unbounded capability,
not a very large numeric limit. Mixed fleets use their total gross mass,
including ships and cargo, rather than checking each ship separately.

## Transit and logistics

Select a fleet, then **Fleet → Transit stargate…**. The dialog shows the source
limits, loaded mass, racial cargo rule and confirmed valid destinations. The
station designer shows gate limits, prerequisites and price.

The fleet must be idle at its source, with no route, repeating programme,
stationary work, moving target or commands in transit. It needs an immediate
command link. Both planets and surviving fitted stations must belong to the
fleet's empire. Destroyed, foreign, ungated and unexplored frontier systems are
not destinations. The host rechecks physical conditions when resolving the
ordinary `GateFleetOrder`. One fleet can make one gate hop per year.

Generalist gates do not carry minerals or colonists. Stargate Specialists may
carry both, even through their ordinary Jump Gates. Cargo remains aboard and
counts toward mass limits; unloading or colonizing requires a subsequent normal
operation. A successful transit consumes no fuel and preserves ship stacks,
cargo and hull damage. There is no unsafe overgating: out-of-limit orders fail
without gate movement or fuel charge.

Turn Messages → Fleet Movement reports completed and rejected transfers through
normal communications. The report contains an immutable transit cargo snapshot;
remote arrival is not revealed before the packet arrives. Telemetry remains a
separate received fact. Fleet fuel may change through the ordinary refueling
phase when a station also supplies that service.

Frontier ships must still reach a system conventionally and establish a paid
destination gate. Gate technology does not move stations or provide access to
unknown/ungated space. Deferred gate orders, scheduled route-programme gate hops,
allied networks, unsafe overgating and traffic statistics remain separate work.

## Computer empires

With at least two colonies, specialists first seek viable frontier propulsion,
then favour Construction, Energy and Electronics for their first network.
Further gate tiers become research priorities after two confirmed physical hubs
exist. Threat responses retain priority. Established colonies can build or refit
affordable gates through ordinary production, preserving fittings and paid work;
the AI receives no free station, resources or technology.

Military deployments and civilian frontier staging can choose a received gate
that saves more than one current movement-year's distance. Civilian staging
requires a refueling service at the receiving hub, so exploration can still
reserve a return route to a depot. The planner emits a
normal gate order and remembers the destination, then waits for newer arrival
telemetry before issuing the next frontier move. It does not use hidden host
destinations, hidden enemy state or a speculative same-year arrival. Choices and
ties are deterministic. Other primary traits retain their existing policy.

## Persistence and validation

Save **v81** and orders **v28** append module and report IDs without changing the
v80 body layout or the gate-order structure. Old generalist saves retain finite
gates. Old packet versions reject new component IDs. Player exports carry only
received own station data, race, orders and delivered messages; pending transit
reports remain host-only.

Core tests cover technology/trait access, paid refits, mixed endpoint tiers,
loaded mixed fleets, unlimited gates, invalid ownership/tasks/geometry,
communication delay and immutable reports, deterministic AI staging, research
and affordable construction. Qt checks cover trait setup/restart, fitting/transit
dialogs, save/order round trips, old v80 campaigns and rejected malformed fits.

This completes the first playable major asymmetry under #50. It does not close
the broader roster: mobile habitats depend on infrastructure/mobile bases, and
additional primary traits and comparative balance remain later stages.
