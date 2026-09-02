# Orbital Stations

Orbital infrastructure is a player-owned object rather than an implicit colony
bonus. Each station has a stable id, an owner, an orbital planet, a hull type,
a name and an explicit module list. This keeps the first playable dock simple
while leaving room for station design, upgrades and combat damage later.

## Basic orbital dock

The initial `OrbitalDock` hull provides two modules:

- `Shipyard`: permits queued ships to be constructed at that colony;
- `RefuelingDepot`: permits immediate and waypoint-arrival refueling.

An orbital dock costs 24 production resources plus 12 Ironium, 6 Boranium and
8 Germanium. It is constructed in the planet's normal ordered production queue.
Only one station can currently occupy a planet's orbit.

The homeworld starts with a basic dock. Newly founded colonies do not. A ship
may still be placed in a colony queue before a shipyard exists, but it waits at
the front without consuming production resources. This allows a useful queue
such as `Orbital Dock -> transport` to be planned in one turn. A player-facing
warning is emitted once when production first becomes blocked.

Destroying or otherwise removing the station immediately removes its services:
ship production waits and refueling orders no longer work. Surface cargo
loading and unloading remain colony operations and do not require a station.

## Persistence

Save format 24 stores station objects, their modules, the next stable station
id and the shipyard-waiting state. Older supported saves are migrated by giving
each already established colony a basic dock, preserving the shipbuilding and
refueling behavior those campaigns had before stations became explicit.

Future slices can add larger hulls, specialized modules, upgrade queues,
defenses and station combat without replacing the identity or service model.
