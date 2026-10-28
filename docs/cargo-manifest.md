# Shared cargo manifest

Suns! uses one physical cargo hold for population, minerals and special cargo. Cargo is deliberately not split into separate invisible capacities: fitting, loading and route planning should all compete for the same finite volume/mass budget.

## Cargo units

The current manifest contains:

- colonists: 100 colonists = 1 kt of cargo;
- Ironium: 1 unit = 1 cargo unit;
- Boranium: 1 unit = 1 cargo unit;
- Germanium: 1 unit = 1 cargo unit.
- scientific specimens: each indivisible sample occupies 5 kt;
- xenocrystal: each whole consumable unit occupies 1 kt.

A fleet's used cargo is therefore:

`colonist cargo + ironium + boranium + germanium + scientific samples + xenocrystal`

This total may not exceed the fitted design's cargo capacity.

## Mass and fuel

All carried cargo contributes to fleet gross mass. The existing Warp fuel model therefore automatically makes a mineral-loaded freighter more expensive to move than the same empty freighter. Colonists and minerals use the same mass path; there is no special fuel exception for population.

## Dockside transfers

Select a fleet and press **Transfer cargo** in its panel, the Fleet menu or
the toolbar. The editor determines the system from the fleet's confirmed
current orbit; selecting a star separately is unnecessary, and an unrelated
map selection stays unchanged. The local surface and connected friendly fleets
there are available as endpoints. Unavailable cargo controls explain why in
their tooltip; direct calls show a status message instead of a modal prompt.
An old or predicted orbit does not permit immediate dockside transfers or
reveal remote surface stock. Use on-arrival cargo orders for remote fleets.

`SetFleetMineralCargoOrder` sets the desired mineral manifest for a fleet docked at a friendly colony. The turn processor transfers only the delta between colony stores and the fleet, rejects negative manifests, rejects unavailable minerals and rejects any target manifest that would overflow the shared hold.

The player-facing **Transfer cargo** dialog builds on those rules with explicit source and destination endpoints. At a selected system it lists the planetary surface and every co-located friendly fleet. An enemy colony is destination-only and is highlighted as such. The source is excluded from the destination list. A single atomic order may move Colonists, Ironium, Boranium and Germanium planet → fleet, fleet → planet or fleet → fleet. Sliders and exact numeric fields show the source remainder, destination result, shared hold usage and gross mass live. Each slider's scale is based on the destination's free hold after other selected transfers, independently of source stock. The handle stops at the available source amount even when the track extends farther; exact fields enforce that same limit. For unloading to a surface, the scale uses the source fleet's hold capacity and the handle is limited by its actual cargo. These limits update together. Transfer cargo replaces the separate Load to capacity and Unload colonists buttons. Peaceful population transfer requires a friendly colony, and loading from one always leaves at least one colonist behind. Unloading population onto an enemy colony starts a ground invasion; minerals cannot be included in that atomic invasion order. Minerals may be deposited on or collected from an uncolonized surface stockpile.

Transfers remain orders resolved at End Turn. Opening the dialog again previews earlier queued local cargo orders, so several deliberate transfers at the same system form a predictable sequence without mutating simulation state from the UI.

Waypoint programs add dynamic `LoadAllAvailable` and `UnloadAll` policies for each individual cargo type. Mineral policies transfer to or from the planet's surface stockpile, including an uncolonized world being worked by remote miners. Loading colonists requires a friendly colony and retains the configured population reserve. Unloading colonists at an enemy colony commits everyone selected by the policy to ground combat. `UnloadAll` with all cargo unloads minerals only if that invasion captures the colony; after a defeat they remain aboard. The amount is resolved on every actual arrival, so the same policy is safe in a repeating transport loop.

Colonist loading uses the same hold. In particular, the dynamic `Load All` arrival action fills only cargo capacity left after any minerals already aboard.

## Colonization

When a colony-capable fleet founds a new colony, its transported colonists become the colony population and all transported Ironium, Boranium and Germanium are deposited into the new colony's mineral stores. The complete heterogeneous fleet is dismantled, removing its FleetId, and 33% of every ship's construction minerals is also recovered, rounded down separately for I/B/G.

## Current scope

[Science samples / artifacts](science-samples.md) lists received specimens and
authors local specimen transfers separately from bulk cargo sliders. Ordinary
cargo accounts for their occupied hold space. On-arrival **Unload all cargo**
deposits specimens at a friendly colony; colonization also preserves them.

Earth begins with a small placeholder stock of all three minerals so the logistics model can be exercised immediately. The important foundation is shared: population and minerals compete for one hold and one gross-mass/fuel calculation, while mined minerals remain on the planetary surface until a transport explicitly loads them.

[Rare exotic materials](rare-exotic-materials.md) has a separate received-stock
panel and whole-unit transfers using the same endpoints/hold/mass. Arrival cargo
policies can load/unload xenocrystal; Unload all cargo includes it. Surface stock
and finite deposits use delivered, dated evidence.

## Orbital habitat integration (#56)

Orbital habitats add a separate population-only endpoint and an Orbital colonists arrival policy. Existing Colonists/All policies retain their surface meaning. Exact transfers conserve real people and respect shared hold/mass, orbital capacity and current received evidence. Emergency evacuation can remove the last orbital resident after station loss; local surface/orbit shuttles need support. See [orbital habitats](orbital-habitats.md) for the complete rules and save v94 / orders v38.

## Shared transport lines (#53)

[Transport lines](transport-lines.md) dispatch ordinary repeating mineral
Load All / Unload programmes to several paid freighters. They reuse the same
physical hold, surface stockpiles, cargo mass and arrival validation. Definitions
and roster edits are HQ intent; old telemetry is not a transfer permission or
a promise of stock. Passenger evacuation remains an ordinary physical cargo
operation; automatic emergency diversion is a later network policy.
