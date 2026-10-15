# Delayed subspace communications

Suns! treats a distant fleet as an autonomous spacecraft rather than an object that can always be controlled instantly from the map.

## First playable model

Established friendly colonies are roots of an empire-wide instantaneous network. The network coverage is not a separate communications statistic: it is the union of connected ordinary-scanner fields.

- every colony projects a 150 ly ordinary planetary scanner field, stronger
  than the starting 50 ly ship scanner;
- every friendly ship with an ordinary scanner automatically joins and extends the network when its field overlaps an already-connected field;
- chains of overlapping scanner fields relay instantaneously and can move with their ships;
- a ship without a scanner is still connected while its position lies inside the connected field;
- a detached scanner field is not part of the network;
- penetrating-only scanners never extend communications coverage; reference scanners
  with both channels extend it using their ordinary radius. Ship scanner radii
  combine by the fourth-power rule, and ship counts do not increase fleet coverage.

Outside the connected mesh, a finite-speed subspace signal travels between physical transceivers. A scanner field has no receiver distributed over its imaginary boundary, so its radius is never subtracted from the slow path.

A detached group of overlapping scanner fields remains an instantaneous local island. Traffic can cross that island without delay to its physical ship nearest the empire mesh; the slow hop is then measured between that ship and the nearest physical transceiver in the colony-rooted component.

```text
slow distance = shortest physical transceiver-to-transceiver gap
                between the source island and the colony-rooted mesh
delay turns = ceil(slow distance / 150 ly per turn)
```

Only positions inside the colony-rooted scanner mesh have a zero-turn subspace link to headquarters. Any non-zero subspace hop arrives at a later annual planning boundary. The signal speed is deliberately provisional. Orbital stations will later provide durable physical transceivers and scanner fields without requiring a second field-radius number.

Signals have the same propagation time regardless of message priority. Priority can matter only if a future relay has finite processing or transmission capacity; there is no artificial faster channel in the base model.

## Commands

A local fleet receives a new route program immediately, preserving the familiar early-game interaction around the homeworld.

A remote fleet does not. A movement/route order becomes a `PendingFleetCommand` containing the replacement onboard route program and its delivery turn. Moving-target legs preserve the stable target `FleetId`; they are not reduced to the coordinate observed when the command was issued. Until that turn the fleet continues to execute its previous destination, arrival action and waypoint queue.

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

The estimate is intentionally not authoritative. Interception, combat, fuel
stalls or other events can already cause a remote fleet to depart from its last
reported plan. Autonomous contingency doctrines remain future work.

The eventual map presentation should therefore visually distinguish a confirmed position from an estimated one rather than silently exposing the simulation's real coordinates.

## Known first-slice limits

Route commands and fleet telemetry use the fleet communication packet model. Progressive ordinary-contact/penetrating/orbital/geological surveys and operational messages use the separate player-knowledge layer: a physical observation or fleet result creates a report, and player-visible knowledge changes only when that report reaches the empire. Higher survey levels can remain in flight while an earlier system contact or rough estimate is already usable. Arrival, route-completion and fuel-stall messages likewise cannot reveal remote truth before telemetry. Reports are never smuggled through fleet telemetry.

The galaxy map (fleet marker, sensor circle and route overlays), fleet dashboard, gauges and Route Program consume the owner player-view for remote fleets: received radar positions or confirmed telemetry plus deterministic prediction. Hidden physical coordinates/cargo/fuel are never a display source. In-flight commands, telemetry and radar reports are persisted in the current save format.

[Space-battle results](space-combat.md) also use delayed player reports. Packets
are dispatched through the pre-casualty network. A destroyed remote fleet's last
confirmed contact remains until the result arrives, without remaining a physical
ship or relay. Delivered contact-loss/restoration and command-delivery messages
are already available in Turn Messages.

## Fleet location freshness

Friendly planetary radars and other scanner fleets detect owned hulls by the
same range, cloaking, interference and active/passive rules as opposing hulls.
The target does not need to transmit. A fleet cannot provide an independent
radar report about itself. Reports travel from the observing colony or fleet;
an isolated observer's transmission schedule and propagation delay still apply.

A received radar position newer than onboard telemetry takes precedence for
the map and fleet selector. It is labelled **Radar** with its own age and
uncertainty. This updates position only, without refreshing cargo, fuel, mission,
emissions or the command link. Outside detection range, that last measurement
ages; it never follows the hidden physical fleet. Newer onboard telemetry
supersedes older radar evidence. Player turn exports retain received evidence
and strip radar reports still in flight.

Locations derived only from old telemetry are marked **Estimated**. The Comms
panel distinguishes the position source from telemetry age. Cargo still needs
current onboard telemetry and a live link; a radar hull detection cannot supply
a current manifest. Its disabled control explains this distinction.

Idle changes a stationary mission only. Emissions are a separate order: choose
the mode and press Set, then End Turn. Radio silence admits commands at the next
scheduled window or finite wake-up. The mission panel shows the last reported
emission mode separately from draft selections and queued emission orders.

## Future extensions

The same model is intended to support:

- orbital relay stations (dedicated Relay Array ship modules already exist)
- communication components in ship/station design
- autonomous fleet doctrines when commands cannot arrive in time
- additional delayed sensor/intelligence reports
- jamming, interception and communication warfare
- technology improvements to range, signal speed and resistance to interference

Jamming should not reduce the physical speed of the signal. The planned model is link-budget degradation: hostile interference reduces the maximum reliable range of the subspace hop and can make a remote fleet temporarily unreachable. Authentication and deliberately forged messages remain possible future ideas, not part of the base communications model.

When no new command can arrive, a fleet keeps executing its onboard waypoint queue and stops after the program is exhausted. Conditional contingency commands such as "if no contact for N turns, return to the nearest friendly relay" are a future autonomy layer rather than implicit fleet behaviour.
