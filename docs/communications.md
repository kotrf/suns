# Delayed communications

Suns! treats a distant fleet as an autonomous spacecraft rather than an object that can always be controlled instantly from the map.

## First playable model

Established friendly colonies temporarily act as communication relay nodes. A fleet within 120 ly of its nearest friendly relay has an effectively real-time link. Outside that coverage, a signal delay is calculated as:

```text
delay turns = ceil((distance to nearest relay - 120 ly) / 150 ly per turn)
```

The constants are deliberately provisional balance values. Orbital stations and dedicated communication components will later replace the temporary colony-relay assumption while keeping the same packet/delivery architecture.

## Commands

A local fleet receives a new route program immediately, preserving the familiar early-game interaction around the homeworld.

A remote fleet does not. A movement/route order becomes a `PendingFleetCommand` containing the replacement onboard route program and its delivery turn. Until that turn the fleet continues to execute its previous destination, arrival action and waypoint queue.

Delivery happens at a turn boundary. A command that arrives after the fleet has already moved for that year changes the program for subsequent movement; it never rewrites history or retroactively changes the just-completed trajectory.

Multiple commands may be in flight. They are delivered deterministically by delivery turn and issue turn.

## Telemetry and player knowledge

The simulation keeps the authoritative `Fleet` state, but player-facing information is represented by a confirmed `FleetTelemetry` snapshot and packets that are still in transit.

A telemetry snapshot includes:

- observation turn
- position and route program
- Warp
- fuel
- colonists and mineral cargo

When the fleet is connected the newest telemetry is confirmed immediately. When it is remote, the snapshot is delivered later using the same communication-delay model.

The UI can therefore distinguish:

```text
CONNECTED
Position confirmed on the current turn

DELAYED
Last confirmed telemetry: Turn 42
Estimated current position: ...
Command in transit: delivery Turn 46
```

## Prediction is not truth

`projected_fleet_position()` advances the last confirmed route program using its known Warp and waypoints. It exists to keep the strategic map usable when a fleet is out of contact.

The estimate is intentionally not authoritative. It may be wrong once future systems introduce autonomous doctrines, interception, combat, failures or other events that cause a remote fleet to depart from its last reported plan.

The map and fleet readouts must therefore distinguish confirmed state from estimates and must not silently expose simulation coordinates for a stale fleet.

## Persistence

Communications traffic is part of the campaign state. Save format v3 persists confirmed telemetry, commands in transit and telemetry packets in transit. The reader remains compatible with v1 and v2 saves; older fleets begin with no packets/commands and acquire a fresh communications history naturally as turns resolve.

## Known first-slice limit

The current delay applies to route commands and fleet telemetry. Existing survey discovery still updates the player's surveyed-star set immediately. Routing sensor discoveries and other intelligence through the communication network belongs in the player-specific knowledge / Turn Messages layer (#45).

## Future extensions

The same model is intended to support:

- orbital relay stations and dedicated relay ships
- communication components in ship/station design
- autonomous fleet doctrines when commands cannot arrive in time
- delayed sensor/intelligence reports
- jamming, interception and communication warfare
- Turn Messages for command delivery, contact loss/restoration and delayed discoveries
- technology improvements to range, signal speed and resistance to interference
