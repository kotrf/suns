# Heterogeneous fleets

A fleet is now a shared strategic unit containing one or more ship stacks. Each stack stores a stable `ShipDesignId` and a ship count, so scouts, transports, colonizers and remote miners can travel and work together without being converted into a rigid fleet role.

The legacy single `Fleet::design` field remains as a representative design for old saves and presentation code. `Fleet::ships` is authoritative for new fleets. An empty stack list is interpreted as one ship of the legacy design, and save version 12 is migrated to that representation when loaded. Save version 13 persists fleet and telemetry composition as well as pending merge/split orders; version 14 adds pending production-queue reorder orders.

## Derived capabilities

- fuel capacity, cargo capacity, dry mass, onboard fuel generation and remote-mining output are summed across every ship and count;
- maximum legal Warp and strategic speed are limited by the slowest or least-capable design;
- ordinary and penetrating sensor ranges use the best fitted scanner in the fleet;
- colonization and remote mining are available when at least one suitable ship is present;
- a hazardous drive makes transported colonists vulnerable even when it is fitted to a non-representative stack;
- fuel use applies a dry-mass-weighted engine rate to the gross fleet mass, including shared cargo.

Colonization dismantles the complete fleet and removes its `FleetId`. Transported colonists become the new population and cargo minerals are deposited in full. The colony also receives 33% of the complete fleet's ship-construction Ironium, Boranium and Germanium, rounded down independently; fuel and production points are lost. A confirmation dialog previews composition, cargo, salvage and the final mineral delivery, so escorts or valuable ships should be split off first.

## Merge order

`MergeFleetsOrder{destination, source}` combines two co-located friendly fleets. The destination `FleetId` and name survive, every ship stack and carried resource is retained, duplicate design stacks are consolidated, and the source fleet is removed.

Both fleets must be stationary and idle, with no waypoint program, persistent task or command packet in flight. Moving-fleet rendezvous and merge-on-arrival remain a separate feature.

## Split order

`SplitFleetOrder{source, ships}` moves exact counts from selected design stacks into a new fleet. The source keeps its `FleetId`; the detachment receives `GameState::nextFleetId`. At least one ship must remain in the source.

Fuel is divided in proportion to fuel capacity. Colonists and minerals are divided in proportion to cargo capacity. This deterministic rule keeps both resulting fleets within their physical capacities whenever the original fleet was valid.

The Qt **Fleet** menu and unified dockable Fleet area expose logistics, colonization, ship design, **Merge fleets…** and **Split fleet…**. Its Overview & Logistics and Route Program tabs share one dock zone by default, but can be detached like the other workspace panels. The fleet dashboard lists the complete composition and reports aggregate capacities, sensors and maximum Warp.
