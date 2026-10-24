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

In ordinary transit, both endpoints impose their own range and mass limits. Infinity connected to
a Jump Gate still has the Jump Gate's 160 ly / 450 kt limits. Only two Infinity
gates provide unlimited range and mass. This is an explicit unbounded capability,
not a very large numeric limit. Mixed fleets use their total gross mass,
including ships and cargo, rather than checking each ship separately.

## Transit and logistics

Select a fleet, then **Fleet → Transit stargate…**. The dialog shows the source
limits, loaded mass, racial cargo rule and confirmed valid destinations. The
station designer shows gate limits, prerequisites and price.

An immediate explicit transit still needs an idle fleet at its source, with no
route, repeating programme, stationary work, moving target or commands in transit.
With a delayed link the command installs an ordinary delivered gate programme
instead; see the scheduled-route rules below. Both planets and surviving fitted stations must belong to the
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
unknown/ungated space. Allied networks, unsafe overgating and traffic statistics
remain future work.

## Deferred commands and scheduled gate routes (#195)

The route dock's **Add gate step…** selects two received owned gates. Each step
is an ordinary waypoint at the source with a `TransitStargate` arrival action,
containing stable source/destination planet IDs and an explicit resonance flag.
Append normal cargo, flight or further gate waypoints afterwards; reorder,
remove, edit approach Warp and Stop use the existing route controls. Gate steps
also work in repeating programmes. A single step has one action, so loading or
unloading needs its own waypoint before/after it.

Example: Load All at A → gate A to B → Unload All at B → flight to C. After the
programme's real command delivery, it runs without annual headquarters commands
or a continuing link. Existing generalist cargo restrictions still apply; a
specialist can transport a loaded freighter within both endpoints' limits.

Rules for this slice:

- Approach uses ordinary propulsion, fuel, mass, hull risk and combat. Arrival
  by flight and the gate hop require **separate years**. A hop consumes its
  movement year, with no further flight or second hop that year. Reorganization
  preserves the existing `lastMovedTurn` accounting, so split/merge cannot reset
  the annual limit.
- Gate steps already at the source run before ordinary movement and battle
  resolution. A successful arrival therefore participates in combat at the
  destination that same year. Gate travel itself burns no fuel or hull health;
  normal generation, depot service and combat remain active.
- Physical execution rechecks surviving owned endpoints, exact source position,
  loaded gross mass, range, racial cargo restrictions, hull condition and any
  requested resonance payment. It needs no headquarters link or acknowledgement
  once the programme is onboard; unrelated commands in flight do not disable it.
- **Unavailable transit holds and retries once per year.** It preserves the
  step, subsequent waypoints, cargo and fuel. There is no automatic skip or
  conventional fallback. Restored gates, changed cargo/mass or replenished
  source stocks can permit a later attempt. Captured/destroyed gates cannot
  grant transit. Failed attempts never spend crystals.
- Stop/replacement takes effect only on delivery, through the normal ordered
  command queue. Before then the old onboard programme keeps running. Manual
  routes pause probe/visitor autonomy through the existing mechanism.
- Standalone remote `GateFleetOrder` follows the same command-delivery path.
  The UI records a remote transit as a visible route draft so later waypoints
  can be appended before submission. Immediate standalone transit keeps its
  existing order-phase validation/payment and one-hop behavior.

Telemetry carries the current gate action and remaining programme in its dated
snapshot. Success/failure reports use ordinary immutable delivery; a failed
attempt reports no payment. A geometric extrapolation stops at an unconfirmed
gate step instead of inventing a successful hop. Headquarters sees only received
results. Waiting fleets remain physical combat targets.

The route table names both systems and labels the gate step. Its forecast runs
received station/cargo/economic evidence through the normal processor, labels gate
outcomes and dependent legs as projected, and reports a waiting reason when the
received conditions cannot support transit. Hidden station changes cannot update
an old forecast or player file. The forecast is conditional on arrival-time
support and supplies; it is neither a reservation nor delivered telemetry.
Conventional cloud-route comparisons after a gate start at its planned exit.

Save **v96 / orders v39** append the arrival-action ordinal and its conditional
source/destination/resonance payload. Fleet state, dated/in-flight telemetry,
repeating templates and pending route commands all reuse the same action codec.
Older actions keep their byte layout; v95 saves and v38 packets remain readable.
Old packet versions reject the new ordinal. No gate prices, equipment catalog,
rare-material rules or existing save migration policy are expanded.

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
telemetry before issuing the next frontier move. Remote staging now sends the
same ordinary delayed gate command and can execute it without a continuing HQ
link; it never grants fuel, fittings or speculative arrival acknowledgement.
It does not use hidden host
destinations, hidden enemy state or a speculative same-year arrival. Choices and
ties are deterministic. Other primary traits retain their existing policy.

## Optional xenocrystal resonance (#63)

A late **Energy 18 / Electronics 18 / Construction 16** pulse consumes eight
physically delivered source-colony xenocrystals for one successful transit at
twice the finite range of both endpoints. Select it explicitly in the transit
dialog. Mass, cargo, ownership, command-link and one-hop rules remain. Ordinary
transits keep their existing rules and prices. No crystals are charged on
failure or when ordinary range already suffices. Current received stock and
fleet telemetry are required for estimates; valid earlier pending transfers
and transits are included. The host rechecks physical state. See
[rare materials](rare-exotic-materials.md#spend-resonant-gate-pulse) for payment,
phase ordering, delayed knowledge, UI and computer policy. Save v90 / orders v36
add the optional gate-order flag; older packets mean ordinary transit.

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
