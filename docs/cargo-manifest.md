# Shared cargo manifest

Suns! uses one physical cargo hold for population and minerals. Cargo is deliberately not split into separate invisible capacities: fitting, loading and route planning should all compete for the same finite volume/mass budget.

## Cargo units

The current manifest contains:

- colonists: 100 colonists = 1 cargo unit;
- Ironium: 1 unit = 1 cargo unit;
- Boranium: 1 unit = 1 cargo unit;
- Germanium: 1 unit = 1 cargo unit.

A fleet's used cargo is therefore:

`colonist cargo + ironium + boranium + germanium`

This total may not exceed the fitted design's cargo capacity.

## Mass and fuel

All carried cargo contributes to fleet gross mass. The existing Warp fuel model therefore automatically makes a mineral-loaded freighter more expensive to move than the same empty freighter. Colonists and minerals use the same mass path; there is no special fuel exception for population.

## Dockside transfers

`SetFleetMineralCargoOrder` sets the desired mineral manifest for a fleet docked at a friendly colony. The turn processor transfers only the delta between colony stores and the fleet, rejects negative manifests, rejects unavailable minerals and rejects any target manifest that would overflow the shared hold.

Colonist loading uses the same hold. In particular, the dynamic `Load All` arrival action fills only cargo capacity left after any minerals already aboard.

## Colonization

When a colony-capable fleet founds a new colony, its transported colonists become the colony population and all transported Ironium, Boranium and Germanium are deposited into the new colony's mineral stores before the ship is consumed.

## Current scope

Earth begins with a small placeholder stock of all three minerals so the logistics model can be exercised immediately. Mining rates, mineral depletion/concentrations, component mineral costs and mineral-specific waypoint actions belong to later economy layers. The important foundation is already shared: population and minerals now compete for one hold and one gross-mass/fuel calculation.
