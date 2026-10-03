# Stars! propulsion reference in Suns!

The normal engine line uses the 15 models in the original **Stars! Technical
Reference Guide**, printed page 11 (PDF page 11). Source:
https://www.bestoldgames.net/download/games/stars/stars-win-guide.pdf

The reference supplies the engine names, count, field levels, access restrictions,
optimal/free/safe speeds, mass, base resource cost and I/B/G mineral costs. These
values live in one core catalog, `propulsion_technologies()`. The Research dock,
Ship Designer and host order validation use the same prerequisites. New campaigns
and quick galaxy generation start with Quick Jump 5.

| Engine | Propulsion | Energy | Optimal Warp | Free Warp | Safe Warp | Mass kt | Access |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Settler's Delight | 0 | 0 | 6 | 6 | 9 | 2 | Settler engines; Mini-Colony Ship only |
| Quick Jump 5 | 0 | 0 | 5 | 1 | 9 | 4 | All |
| Long Hump 6 | 3 | 0 | 6 | 1 | 9 | 9 | All |
| Daddy Long Legs 7 | 5 | 0 | 7 | 1 | 9 | 13 | All |
| Alpha Drive 8 | 7 | 0 | 8 | 1 | 9 | 17 | All |
| Trans-Galactic Drive | 9 | 0 | 9 | 1 | 9 | 25 | All |
| Interspace-10 | 11 | 0 | 10 | 1 | 10 | 25 | No Ram Scoop Engines |
| Trans-Star 10 | 23 | 0 | 10 | 1 | 10 | 5 | All |
| Fuel Mizer | 2 | 0 | 6 | 4 | 9 | 6 | Fuel efficiency |
| Radiating Hydro-Ram Scoop | 6 | 2 | 6 | 6 | 9 | 10 | Ram scoops enabled; radiation hazard |
| Sub-Galactic Fuel Scoop | 8 | 2 | 7 | 5 | 9 | 20 | Ram scoops enabled |
| Trans-Galactic Fuel Scoop | 9 | 3 | 8 | 6 | 9 | 19 | Ram scoops enabled |
| Trans-Galactic Super Scoop | 12 | 4 | 9 | 7 | 9 | 18 | Ram scoops enabled |
| Trans-Galactic Mizer Scoop | 16 | 4 | 10 | 8 | 10 | 11 | Ram scoops enabled |
| Galaxy Scoop | 20 | 5 | 10 | 9 | 10 | 8 | Fuel efficiency; ram scoops enabled |

Optimal speed is an efficiency reference, not the safe-speed ceiling. Quick Jump
5 may fly safely at Warp 9, but burns much more fuel than a later drive. Safe Warp
10 first becomes possible with Interspace-10 at P11 for a no-scoop empire,
Trans-Galactic Mizer Scoop at P16/E4, or Trans-Star 10 at P23 for any empire.

## Access and fitting

New campaign setup offers three independent propulsion-access choices for every
empire: Fuel efficiency, No ram scoops, and Settler engines. They control access
only. They do not claim to implement the complete Stars! IFE/NRSE/Hyper Expansion
race traits, their point costs, starting-tech bonuses or growth modifiers.
The fuel-efficiency choice unlocks Fuel Mizer and Galaxy Scoop; it does not apply
a global 15% fuel discount.

No ram scoops blocks the six researchable scoop engines, including the hazardous
Hydro-Ram Scoop. Fuel Mizer remains allowed because it burns no fuel through
Warp 4 without collecting any. Settler's Delight also remains allowed with
Settler engine access, but only in a Mini-Colony Ship.

The Mini-Colony Ship has one engine cell and one general cell, 8 kt hull mass,
150 fuel capacity and 10 kt cargo capacity. Its base costs are 3 resources and
2/0/2 minerals. Its cargo still uses Suns!' 100 kg/person conversion.

Double-clicking a researchable engine in the technology catalog queues all missing
field levels, including Energy, and accounts for levels already queued. A
race-restricted row explains the requirement and does not add futile research.
The designer shows locked engines and prevents fitting unavailable components;
the host independently rejects designs that bypass those gates.

## Suns! simulation and balance

Travel remains Warp squared. For Warp above the free-speed band, the current
fuel rate is `0.15 * (Warp / optimalWarp)^6` fuel units per 100 kt per ly.
Real scoops collect `0.02 * (freeWarp - Warp + 1)` in their free band; ordinary
engines and Fuel Mizer merely consume zero. Rates scale with loaded mass, and
multi-engine hulls retain Suns!' identical-engine-bank rule. These curves are
Suns! balance values, not a claim to reproduce the original fuel-use tables.

Each safe-Warp-9 model accumulates 18 hull-damage points per full year at Warp 10.
Safe-Warp-10 models add no overdrive damage. Damage is deterministic rather than
Stars!' probability of losing individual ships. Radiation uses the existing
Suns! transported-colonist hazard rules. Combat movement, miniaturization and
minefield susceptibility remain future systems.

Research now costs `18 * (1 + level * (level - 1) / 2)` RP per level: 18/36/72 RP
for levels 1/2/3, 126 for level 4, and 4572 for level 23. Quadratic growth replaces
the prototype's indefinite doubling so original high-level engine unlocks are
reachable. Existing progress is preserved; higher-level projects are repriced.
This cost curve applies to all six research fields and needs campaign balancing.

## Compatibility and scope

The six earlier Suns! engines keep their IDs, specifications, fuel curves and
technology gates. An empire that already owns legacy designs can continue to
build and copy them; the designer marks their engines as legacy. New empires do
not see or unlock that prototype line. Existing ships are never silently refitted.
The deterministic demo fixture retains its legacy ships.

Save format is 55 and order format is 13. Previous saves/orders remain readable;
pre-55 races receive neutral propulsion access settings. New engine IDs, the new
hull and access settings round-trip in host saves and player turn files. All PBEM
participants need the same build; older clients reject newer packet formats.

This completes the normal reference engine catalog, not every Stars! technology.
The Mystery Trader's Enigma Pulser is not a normal research unlock and awaits a
visitor/acquisition system. Weapons, shields, most hulls, terraforming and other
branches remain separate work under #44 and their respective mechanics.
