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

The eventual map presentation should therefore visually distinguish a confirmed position from an estimated one rather than silently exposing the simulation's real coordinates.

## Known first-slice limits

Route commands and fleet telemetry use the fleet communication packet model. Progressive basic/orbital/geological surveys and operational messages use the separate player-knowledge layer: a physical observation or fleet result creates a report, and player-visible knowledge changes only when that report reaches the empire. Higher survey levels can remain in flight while an earlier rough estimate is already usable. Arrival, route-completion and fuel-stall messages likewise cannot reveal remote truth before telemetry. Reports are never smuggled through fleet telemetry.

The galaxy map (fleet marker, sensor circle and route overlays), fleet dashboard, gauges and Route Program consume the owner player-view for remote fleets: confirmed telemetry plus deterministic prediction rather than authoritative coordinates/cargo/fuel. In-flight commands and telemetry are persisted in the current save format.

## Future extensions

The same model is intended to support:

- orbital relay stations and dedicated relay ships
- communication components in ship/station design
- autonomous fleet doctrines when commands cannot arrive in time
- additional delayed sensor/intelligence reports
- jamming, interception and communication warfare
- Turn Messages for command delivery and contact loss/restoration
- technology improvements to range, signal speed and resistance to interference
