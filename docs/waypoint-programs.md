# Fleet waypoint programs

Suns! fleets can carry a persistent multi-leg program rather than requiring a new order every turn.

## State model

The currently active leg continues to use the existing `Fleet` fields:

- `destination`
- `warp`
- `arrivalAction`

`Fleet::waypointQueue` stores the future legs. Each `FleetWaypoint` contains its own destination, Warp and arrival action.

This keeps the already-tested movement and arrival-action semantics stable while adding automation incrementally.

## Resolution

A `MoveFleetOrder` represents the player's complete route intent:

1. the order replaces the active leg;
2. it replaces the entire future waypoint queue atomically;
3. all leg Warp values are validated against the fitted ship design before the order is accepted;
4. the active leg moves during the normal movement phase;
5. on arrival its one-shot arrival action executes against the actual arrival-time `GameState`;
6. the next queued waypoint is promoted to the active leg;
7. **movement ends for that fleet for the current turn**; the promoted leg starts on the following turn.

The last rule deliberately makes arrival a clean phase boundary. Loading, unloading, refuelling, future combat and colony-side effects therefore have an unambiguous ordering before the next leg consumes fuel.

## Dynamic logistics

Arrival actions remain policies rather than precomputed amounts. A queued `LoadColonistsToCapacity` waypoint still evaluates available population and cargo space only when the ship actually reaches that colony.

This means a program such as:

```text
Earth
  -> Alpha Centauri [Load to capacity, leave 1000]
  -> Vega           [Unload all]
  -> Deneb          [Refuel]
```

can be issued in advance without pretending that future colony populations are already known.

## Replotting

Plotting a new direct course replaces both the active leg and all queued future legs. This prevents stale waypoint actions from surviving a change of intent.

## Future work

The first implementation keeps the active leg separate from the queued legs for compatibility. A later route editor can present the whole program as a single ordered list without requiring a simulation-model rewrite.

Possible later rules include:

- continuing onto the next leg with unused same-turn travel distance;
- repeat/patrol routes;
- conditional waypoints;
- colonize and transfer-cargo arrival actions;
- projected versus exact fuel/load forecasts across future dynamic legs.
