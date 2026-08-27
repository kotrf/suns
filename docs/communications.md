# Delayed communications

Suns! treats a distant fleet as an autonomous spacecraft rather than an object that can always be controlled instantly from the map.

## First playable model

Established friendly colonies are roots of an empire-wide instantaneous network. The network coverage is not a separate communications statistic: it is the union of connected ordinary-scanner fields.

- every colony projects its ordinary planetary scanner field;
- every friendly ship with an ordinary scanner automatically joins and extends the network when its field overlaps an already-connected field;
- chains of overlapping scanner fields relay instantaneously and can move with their ships;
- a ship without a scanner is still connected while its position lies inside the connected field;
- a detached scanner field is not part of the network;
- penetrating-only scanners never extend communications coverage.

Outside the connected mesh, a conventional signal expands in every direction until it reaches the nearest boundary of the network; the instantaneous backbone carries it from there.

```text
uncovered distance = distance to nearest connected scanner field
delay turns = ceil(uncovered distance / 150 ly per turn)
```

Only positions inside the connected scanner mesh have zero-turn communication. Any non-zero conventional hop arrives at a later annual planning boundary. The signal speed is deliberately provisional. Orbital stations will later provide durable scanner/relay fields without requiring a separate communications-range number.

Signals have the same propagation time regardless of message priority. Priority can matter only if a future relay has finite processing or transmission capacity; there is no artificial faster channel in the base model.

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

Route commands and fleet telemetry use the fleet communication packet model. Progressive ordinary-contact/penetrating/orbital/geological surveys and operational messages use the separate player-knowledge layer: a physical observation or fleet result creates a report, and player-visible knowledge changes only when that report reaches the empire. Higher survey levels can remain in flight while an earlier system contact or rough estimate is already usable. Arrival, route-completion and fuel-stall messages likewise cannot reveal remote truth before telemetry. Reports are never smuggled through fleet telemetry.

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

Jamming should not reduce the physical speed of the signal. The planned model is link-budget degradation: hostile interference reduces the maximum reliable range of the conventional hop and can make a remote fleet temporarily unreachable. Authentication and deliberately forged messages remain possible future ideas, not part of the base communications model.

When no new command can arrive, a fleet keeps executing its onboard waypoint queue and stops after the program is exhausted. Conditional contingency commands such as "if no contact for N turns, return to the nearest friendly relay" are a future autonomy layer rather than implicit fleet behaviour.
