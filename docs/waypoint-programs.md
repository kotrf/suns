# Fleet waypoint programs

Suns! fleets can carry a persistent multi-leg program rather than requiring a new order every turn.

## State model

The currently active leg continues to use the existing `Fleet` fields:

- `destination`
- `warp`
- `arrivalAction`

`Fleet::waypointQueue` stores the future legs. Each `FleetWaypoint` contains its own destination, optional moving `targetFleet`, Warp and arrival action. A moving target keeps a positional snapshot for presentation and backward compatibility, but navigation resolves its current fleet state every turn.

This keeps the already-tested movement and arrival-action semantics stable while adding automation incrementally.

## Resolution

A `MoveFleetOrder` represents the player's complete route intent:

1. the order replaces the active leg;
2. it replaces the entire future waypoint queue atomically;
3. all leg Warp values must be within 1–10 and the fleet must have working engines; speeds above its safe rating incur overdrive damage;
4. the active leg moves during the normal movement phase;
5. on arrival its task executes against the actual arrival-time `GameState`;
6. the next queued waypoint is promoted to the active leg;
7. **movement ends for that fleet for the current turn**; the promoted leg starts on the following turn.

The last rule deliberately makes arrival a clean phase boundary. Loading, unloading, refuelling, future combat and colony-side effects therefore have an unambiguous ordering before the next leg consumes fuel.

Most current tasks are one-shot. `Remote Mining` is persistent and terminal: it can appear only on the final waypoint, becomes active on arrival, starts extraction on the following turn and remains active until a replacement route or `No Task` command is delivered. `Merge with fleet` is also terminal because the pursuing FleetId is consumed when the rendezvous succeeds. Clearing a stationary fleet's route is the current `No Task` operation.

`Repeat Orders` is a flag on the complete program. After its final arrival, the fleet restores the original waypoint list and promotes its first leg; movement still waits until the following turn. A repeating program requires at least two distinct destinations. `Colonize`, `Remote Mining` and `Merge with fleet` are rejected in repeating programs because colonization or merging dismantles the fleet while remote mining intentionally remains active until cancelled.

Moving-target turn resolution and rendezvous semantics are specified in [Moving fleet targets](moving-fleet-targets.md).

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

## Route editor

The route dock presents the active leg and future waypoints in a single queue with destination, Warp and arrival-action columns. Move up, Move down and Remove replace the selected fleet's complete pending program. The first leg can be moved or removed. Colonization, remote mining and merge must remain terminal. Removing points disables repetition if fewer than two distinct destinations remain. Remote edits still obey command-delivery delay. A separate Route forecast dialog contains detailed fuel, damage and arrival predictions.

## Future work

The simulation keeps the active leg separate from the queued legs for compatibility; the editor combines them for display.

Possible later rules include:

- continuing onto the next leg with unused same-turn travel distance;
- conditional waypoints;
- projected versus exact fuel/load forecasts across future dynamic legs.

## Waypoint editor isolation

The Add waypoint controls are a draft for one source fleet. Arrival action,
cargo, colony reserve, Warp and target are remembered independently for each
FleetId while the game is open. A fleet without a draft starts with No action.
Switching fleets through the source selector or map restores that fleet's draft
immediately. The draft becomes an order only when Add is clicked; existing
route rows retain their own actions. Starting or loading a game clears drafts,
so reused fleet IDs cannot inherit settings from the previous game.

## Choosing a moving fleet target

The source fleet and map target have separate roles while plotting a route.
After selecting the source fleet, **Pick target on map** temporarily arms the
galaxy map. Clicking a star captures a fixed destination; clicking another
friendly fleet captures its stable FleetId as a moving destination and selects
the Merge action by default. An enemy fleet is marked red and uses its currently
observed position as a fixed destination with No action rather than the friendly
Merge action. The player may change a friendly fleet's action to No action to
follow/rendezvous without merging. The source fleet remains selected throughout.
Pressing Esc, closing the Route Program dock, changing the source fleet, or
ending/loading a game cancels target-picking mode.

Selecting a system also fills one **Destination** combo with the system itself
and every other visible fleet currently in its orbit. Enemy entries are labeled
and drawn in red. This is the precise path when several markers overlap: choose
the intended system or fleet, adjust Warp and the arrival action, then press Add.

Right-clicking a star or another fleet is the fast path. It immediately
appends that object to the selected source fleet's route using the editor's
current Warp, cargo and arrival-action settings. A fleet target defaults to
Merge; an enemy fleet uses No action and its observed position. Use the
Destination combo and choose No action before Add when the intent is a friendly
rendezvous/pursuit without merging.
