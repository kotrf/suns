# Route-program forecasts

The fleet route dock distinguishes navigation that is already determined from navigation that depends on a future dynamic logistics result.

For the selected fleet, the Qt client projects the programmed route by running a disposable copy of `GameState` through the real `TurnProcessor`:

- the current turn includes all orders already queued by the player;
- later projected turns assume no additional orders are issued;
- onboard fuel generation, ram-scoop behaviour, fuel-limited movement, population growth and arrival actions therefore use the same rules as the authoritative simulation;
- the preview horizon is 96 turns.

## Exact versus projected

A leg before any dynamic `Load All` result is labelled **exact navigation**. Its route geometry and departure mass are already known from the current/pending state.

`Load All` itself remains explicitly projected because the amount depends on the colony population that exists when the fleet actually arrives.

Every later leg is labelled **projected navigation** because its gross mass and fuel burn may depend on that dynamic load result.

The forecast is intentionally phrased as "if no further orders are issued". Future player actions, combat and other systems may invalidate a projection; the authoritative result always comes from `TurnProcessor` when the real turn is resolved.


## Per-waypoint ETA

The route table shows `ETA (years)` for each point: cumulative years from the
current planning boundary, including preceding legs, cargo/refuel actions and
command delivery. The same simulation powers the detailed forecast. `~N` denotes
an estimate with no further orders; `—` means arrival is not predicted inside
96 turns (for example, insufficient fuel or an unreachable moving target).
Repeating programs show the next traversal. The result is cached until the
planning state or selected fleet changes.

Committed routes can be reordered or have individual points removed exactly as
current-year drafts can. Removing the last point submits an explicit stop. It
cancels navigation, repeated waypoints and stationary tasks when delivered;
older command packets still in transit cannot restart that canceled program.
Stop semantics do not depend on an estimated fleet coordinate and also work
for an immobilized fleet.
