# Ship Designer

Suns! treats a ship role as the result of a fitted design rather than a fixed class. The first player-facing designer introduces hulls and hard slot constraints so adding capability always creates an engineering choice.

## Hulls

New campaigns use the 32-hull Stars! reference catalog, including racial and
Mystery Trader restrictions. See [Stars! hulls](stars-hulls.md) for all values,
bank layouts and research gates. New starter designs use Scout and Colony Ship.

The earlier prototype hulls remain available to empires which already own a
legacy design. Their values are preserved for save compatibility:

| Hull | Dry hull mass | Hull cost | Base fuel | Base cargo | Required engines | General | Mining |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Scout Hull | 34.5 kt | 2 | 300 | 0 | 1 | 2 | 0 |
| Light Transport | 45 kt | 2 | 400 | 5 | 1 | 3 | 0 |
| Medium Transport | 70 kt | 5 | 500 | 50 | 2 | 5 | 0 |
| Heavy Transport (Construction 2) | 140 kt | 12 | 600 | 250 | 3 | 6 | 0 |
| Remote Miner (Construction 1) | 120 kt | 8 | 500 | 0 | 2 | 1 | 2 |
| Utility Hull | 85 kt | 7 | 500 | 0 | 2 | 8 | 0 |
| Mini-Colony Ship (Settler engines) | 8 kt | 3 | 150 | 10 | 1 | 1 | 0 |

These legacy numbers are tuning placeholders. Reference hulls use original
values and bank capacities. Each hull has a fixed required engine count, and
every engine in that bank must be the same model. Multiple engines do not
multiply maximum Warp: together they are the propulsion plant required for that
hull to achieve the selected engine model's normal performance. Every installed
engine still contributes its own mass, build cost and mineral cost. Remote Mining
Modules use `Mining` slots on either legacy or reference miner hulls.

Transport hulls buy cargo efficiency through built-in hold capacity and have fewer configurable cells. The Heavy Transport moves five times the Medium Transport's base cargo, but requires a third engine, a larger mineral bill and more fuel per ship. The Utility Hull starts with no cargo capacity but has eight general cells. It may become a hauler by spending those cells on Cargo Pods, or instead become a survey vessel, colony expedition, relay or industrial support design. A fully cargo-fitted Utility Hull is intentionally more expensive and heavier than obtaining comparable capacity from a transport hull.

## Fitting

The designer exposes the [15 reference engines](stars-propulsion.md), the
following Suns! equipment, and legacy engines for existing legacy empires:

- Fusion Drive
- Advanced Fusion Drive (Propulsion 1; light, safe Warp 9, but no fuel scooping)
- Ram Scoop Drive
- Radiating Ram Scoop
- Long Range Scanner
- Compact Long Range Scanner (Electronics 1)
- Extended Range Scanner (Electronics 2; 160 ly ordinary field, heavy and expensive)
- Penetrating Scanner (Electronics 3)
- Remote Mining Module (Construction 1, 80 kt; miner hull `Mining` slots only)
- Field Repair Bay (Construction 3; repairs hull damage away from docks)
- Colony Module
- Fuel Tank
- Cargo Pod
- Antimatter Generator (Energy 1; +200 fuel capacity and +50 fuel/turn)

Engines occupy dedicated engine cells. Selecting or dropping an engine model
fills the complete required bank; removing one engine cell removes the bank.
Mixed or incomplete engine banks are never saved. Reference hulls have typed
equipment banks, each holding only one model. Fitting from the catalog fills
that bank; Delete reduces its quantity one cell at a time. Scanners use Scanner
slots, Relay Array uses Electrical, and tanks/cargo/generators/colony/repair
equipment use Mechanical. General banks accept these classes; shield/armor,
weapon, bomb and mine-layer banks enforce their own restrictions. Legacy hulls
retain their individual General/Mining cells.

Selecting any catalog row opens a persistent detail card below the catalog. It
shows the compatible slot category, mass, production cost, I/B/G bill and the
component's actual effect. Engines include thrust, safe Warp, fuel use or gain
at every Warp and overdrive damage; scanners explain both range and whether
they survey planets or extend communications. Locked components remain
selectable for inspection, but cannot be fitted. The complete design preview
shows derived mass, build cost and mineral bill, maximum Warp, fuel
capacity/generation, cargo capacity, scanner range, mission capability,
radiation hazard and the engine fuel curve.

## Logical fitting layout

Hull specifications expose explicit fitting cells with stable numeric IDs, a
slot category (`Engine`, `General` or `Mining`) and small grid coordinates for
the visual designer. IDs describe logical cells and do not depend on widget
pixels or replaceable hull artwork.

Every persisted design may store an exact component-to-slot placement in
addition to its component list. Core validation independently checks that every
component occurs exactly once, no cell is occupied twice, and the component
category matches the target cell. It also returns a useful validation message
for the UI rather than only a boolean rejection.

Designs and pending design orders created before save format 19 contain only a
component list. Loading them uses deterministic first-compatible-cell
autoplacement, so no component is lost and repeated loads produce the same
layout. A legacy design that cannot fit reports the conflict instead of being
silently modified.

Save format 20 introduced mandatory multi-engine banks. Designs and pending
design orders from earlier supported formats retain their engine model and are
automatically expanded to the hull's current required engine count before slot
validation.

The designer presents the technology-filtered component catalog beside the
current hull's fitting grid. Components may be dragged from the catalog into a
compatible cell, dragged between cells, or fitted using the keyboard-
accessible selection buttons. Dropping onto another compatible fitted cell
replaces a catalog fit or swaps two fitted components. Double-click,
Delete/Backspace and an explicit Remove
button all remove equipment. Locked technology remains visible with its exact
research requirement, while core validation still protects the order path if a
malformed layout bypasses the UI.
With focus on a fitting cell, arrow keys move to the nearest cell in that
direction, including across gaps in the hull grid. Tab moves between the
catalog, fitting cells and action buttons. The selected cell stays highlighted
for keyboard fitting and removal.

Ship Designer is a non-modal top-level dialog. The galaxy map and other docks
remain usable while it is open, and attempting to open it again raises the
existing window instead of creating competing drafts.
Each hull now has a distinct schematic silhouette above its fitting grid, and
catalog/fitted components have compact category icons. Fitting cells are square,
show their category and stable ID, and retain the complete component name for
tooltips and accessibility when the visible caption is shortened. These graphics are drawn
in Qt and have no role in slot identity or simulation; replacing them later
does not change saved placements.
The Start from selector loads one of the current player's saved or planned
designs as a new draft, preserving each component's logical cell. The proposed
name gets a Copy suffix; creating the draft never edits the original design or
ships already built from it. Locked components are dimmed, compatible drag
targets turn green and incompatible ones red; dropping on a red cell gives a
persistent reason. The detached window remembers its geometry across launches.

## Turn architecture

Saving a design does not mutate authoritative `GameState` directly from Qt. The
UI queues a `CreateShipDesignOrder`, builds a deterministic planning view, and
immediately offers the pending design in the production selector. A production
order for such a design carries its owner-scoped name instead of guessing the
future global `ShipDesignId`. This matters in multiplayer because other players
may create designs in the same turn and consume IDs in host submission order.

During turn resolution the core:

1. assigns the next stable `ShipDesignId`;
2. validates hull slots, the engine requirement and component technology prerequisites again;
3. rejects duplicate names for the same player;
4. stores the design in `GameState`;
5. resolves later production orders from that player by the newly stored name.

The design can therefore enter a colony queue in the same planning turn while
still using the ordinary PBEM/server order path. Invalid or unavailable designs
never resolve to a build order. Save format 34 and turn-order format 5 persist
the pending-design reference; older supported files remain readable.

## Strategic intent

A design is not expected to maximize every stat. Examples of useful tensions already supported by the model:

- a Scout Hull can fit a scanner plus either extra fuel, cargo or an antimatter generator, but not all three;
- an Advanced Fusion Drive makes a light, safe Warp-9 courier after Propulsion 1, but cannot collect fuel like either ram scoop;
- a Ram Scoop can make low-Warp exploration fuel-positive and reach Warp 9, but costs more mass and build resources than the starter Warp-8 Fusion Drive;
- an Extended Range Scanner more than doubles the compact scanner's field, but its 24 kt mass makes it expensive to accelerate and leaves no planetary penetration;
- a Light Transport can combine a Colony Module with limited extra logistics equipment;
- a Medium Transport has more slots and built-in cargo, but starts heavier and more expensive, increasing fuel demand.
- a Remote Miner can carry one or two mining modules and at most one general module; its 120 kt hull plus 80 kt apparatus makes relocation a deliberate fuel-logistics decision.
- a Utility Hull has no built-in hold but eight general cells, so every Cargo Pod competes directly with scanners, fuel equipment, colony hardware and future support modules.

Future technology should unlock new hulls and components rather than replacing this model with fixed ship classes.
