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
5. on arrival its task executes against the actual arrival-time `GameState`;
6. the next queued waypoint is promoted to the active leg;
7. **movement ends for that fleet for the current turn**; the promoted leg starts on the following turn.

The last rule deliberately makes arrival a clean phase boundary. Loading, unloading, refuelling, future combat and colony-side effects therefore have an unambiguous ordering before the next leg consumes fuel.

Most current tasks are one-shot. `Remote Mining` is persistent and terminal: it can appear only on the final waypoint, becomes active on arrival, starts extraction on the following turn and remains active until a replacement route or `No Task` command is delivered. Clearing a stationary fleet's route is the current `No Task` operation.

`Repeat Orders` is a flag on the complete program. After its final arrival, the fleet restores the original waypoint list and promotes its first leg; movement still waits until the following turn. A repeating program requires at least two distinct destinations. `Colonize` and `Remote Mining` are rejected in repeating programs because they consume the ship or intentionally remain active until cancelled.

## Dynamic logistics

Arrival actions remain policies rather than precomputed amounts. `LoadAllAvailable` and `UnloadAll` select one cargo type: Colonists, Ironium, Boranium or Germanium. They evaluate the real population or planetary surface stockpile and the shared free cargo space only when the ship actually arrives.

This means a program such as:

```text
Earth
  -> Alpha Centauri [Load all available Ironium]
  -> Vega           [Unload all Ironium]
  -> Deneb          [Refuel]
  -> repeat
```

can be issued in advance without pretending that future colony populations are already known.

## Replotting

Plotting a new direct course replaces both the active leg and all queued future legs. This prevents stale waypoint actions from surviving a change of intent.

## Future work

The first implementation keeps the active leg separate from the queued legs for compatibility. A later route editor can present the whole program as a single ordered list without requiring a simulation-model rewrite.

Possible later rules include:

- continuing onto the next leg with unused same-turn travel distance;
- conditional waypoints;
- projected versus exact fuel/load forecasts across future dynamic legs.
