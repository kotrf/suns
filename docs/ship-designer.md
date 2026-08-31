# Ship Designer

Suns! treats a ship role as the result of a fitted design rather than a fixed class. The first player-facing designer introduces hulls and hard slot constraints so adding capability always creates an engineering choice.

## Hulls

Four hulls are currently available:

| Hull | Dry hull mass | Hull cost | Base fuel | Base cargo | Engine | General | Mining |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Scout Hull | 34.5 kt | 2 | 300 | 0 | 1 | 2 | 0 |
| Light Transport | 45 kt | 2 | 400 | 5 | 1 | 3 | 0 |
| Medium Transport | 70 kt | 5 | 500 | 50 | 1 | 5 | 0 |
| Remote Miner (Construction 1) | 120 kt | 8 | 500 | 0 | 1 | 1 | 2 |

The numbers are tuning placeholders. The structural rule is more important: a design must contain exactly one engine and may not exceed any dedicated slot category. Remote Mining Modules use `Mining`, not general, slots.

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

The engine occupies the dedicated engine slot. Remote Mining Modules occupy dedicated `Mining` slots. Every other installed component consumes one general slot. Fuel tanks, cargo pods and generators may be fitted more than once when the hull has room.

The dialog previews derived mass, build cost, maximum Warp, fuel capacity/generation, cargo capacity, scanner range, colonization capability, radiation hazard and the engine's Warp-by-Warp fuel curve.

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

Future technology should unlock new hulls and components rather than replacing this model with fixed ship classes.
