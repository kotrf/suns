# Ship Designer

Suns! treats a ship role as the result of a fitted design rather than a fixed class. The first player-facing designer introduces hulls and hard slot constraints so adding capability always creates an engineering choice.

## Hulls

Five hulls are currently available:

| Hull | Dry hull mass | Hull cost | Base fuel | Base cargo | Required engines | General | Mining |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Scout Hull | 34.5 kt | 2 | 300 | 0 | 1 | 2 | 0 |
| Light Transport | 45 kt | 2 | 400 | 5 | 1 | 3 | 0 |
| Medium Transport | 70 kt | 5 | 500 | 50 | 2 | 5 | 0 |
| Remote Miner (Construction 1) | 120 kt | 8 | 500 | 0 | 2 | 1 | 2 |
| Utility Hull | 85 kt | 7 | 500 | 0 | 2 | 8 | 0 |

The numbers are tuning placeholders. The structural rules are more important. Each hull has a fixed required engine count based on its structural mass class, and every engine in that bank must be the same model. Multiple engines do not multiply maximum Warp: together they are the propulsion plant required for that hull to achieve the selected engine model's normal performance. Every installed engine still contributes its own mass, build cost and mineral cost. Remote Mining Modules use `Mining`, not general, slots.

Transport hulls buy cargo efficiency through built-in hold capacity and have fewer configurable cells. The Utility Hull starts with no cargo capacity but has eight general cells. It may become a hauler by spending those cells on Cargo Pods, or instead become a survey vessel, colony expedition, relay or industrial support design. A fully cargo-fitted Utility Hull is intentionally more expensive and heavier than obtaining comparable capacity from a transport hull.

## Fitting

The first designer exposes these components:

- Fusion Drive
- Advanced Fusion Drive (Propulsion 1; light, safe Warp 9, but no fuel scooping)
- Ram Scoop Drive
- Radiating Ram Scoop
- Long Range Scanner
- Compact Long Range Scanner (Electronics 1)
- Extended Range Scanner (Electronics 2; 160 ly ordinary field, heavy and expensive)
- Penetrating Scanner (Electronics 3)
- Remote Mining Module (Construction 1, 80 kt; Remote Miner `Mining` slots only)
- Colony Module
- Fuel Tank
- Cargo Pod
- Antimatter Generator (Energy 1; +200 fuel capacity and +50 fuel/turn)

Engines occupy the dedicated engine cells. Selecting or dropping an engine model in the designer fills the complete required bank, and removing one engine cell removes the bank; mixed or incomplete banks are never saved. Remote Mining Modules occupy dedicated `Mining` slots. Every other installed component consumes one general slot. Fuel tanks, cargo pods and generators may be fitted more than once when the hull has room.

The dialog previews derived mass, build cost, maximum Warp, fuel capacity/generation, cargo capacity, scanner range, colonization capability, radiation hazard and the engine's Warp-by-Warp fuel curve.

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

Ship Designer is a non-modal top-level dialog. The galaxy map and other docks
remain usable while it is open, and attempting to open it again raises the
existing window instead of creating competing drafts.

## Turn architecture

Saving a design does not mutate `GameState` directly from Qt. The UI queues a `CreateShipDesignOrder`. During turn resolution the core:

1. assigns the next stable `ShipDesignId`;
2. validates hull slots, the engine requirement and component technology prerequisites again;
3. rejects duplicate names for the same player;
4. stores the design in `GameState`.

The design therefore becomes available for production after `End Turn`, using the same order-processing path intended for future PBEM/server play and AI players.

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
