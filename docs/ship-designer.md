# Ship Designer

Suns! treats a ship role as the result of a fitted design rather than a fixed class. The first player-facing designer introduces hulls and hard slot constraints so adding capability always creates an engineering choice.

## Hulls

Three hulls are currently available:

| Hull | Dry hull mass | Hull cost | Base fuel | Base cargo | Engine slots | General slots |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Scout Hull | 34.5 kt | 2 | 300 | 0 | 1 | 2 |
| Light Transport | 45 kt | 2 | 400 | 5 | 1 | 3 |
| Medium Transport | 70 kt | 5 | 500 | 50 | 1 | 5 |

The numbers are tuning placeholders. The structural rule is more important: a design must contain exactly one engine and may not exceed the hull's general slots.

## Fitting

The first designer exposes these components:

- Fusion Drive
- Ram Scoop Drive
- Radiating Ram Scoop
- Long Range Scanner
- Colony Module
- Fuel Tank
- Cargo Pod
- Antimatter Generator

The engine occupies the dedicated engine slot. Every other installed component consumes one general slot. Fuel tanks, cargo pods and generators may be fitted more than once when the hull has room.

The dialog previews derived mass, build cost, maximum Warp, fuel capacity/generation, cargo capacity, scanner range, colonization capability, radiation hazard and the engine's Warp-by-Warp fuel curve.

## Turn architecture

Saving a design does not mutate `GameState` directly from Qt. The UI queues a `CreateShipDesignOrder`. During turn resolution the core:

1. assigns the next stable `ShipDesignId`;
2. validates hull slots and the engine requirement again;
3. rejects duplicate names for the same player;
4. stores the design in `GameState`.

The design therefore becomes available for production after `End Turn`, using the same order-processing path intended for future PBEM/server play and AI players.

## Strategic intent

A design is not expected to maximize every stat. Examples of useful tensions already supported by the model:

- a Scout Hull can fit a scanner plus either extra fuel, cargo or an antimatter generator, but not all three;
- a Ram Scoop can make low-Warp exploration fuel-positive but gives up the Fusion Drive's Warp 10;
- a Light Transport can combine a Colony Module with limited extra logistics equipment;
- a Medium Transport has more slots and built-in cargo, but starts heavier and more expensive, increasing fuel demand.

Future technology should unlock new hulls and components rather than replacing this model with fixed ship classes.
