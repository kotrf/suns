# Stars! propulsion reference in Suns!

The normal engine line uses the 15 models in the original **Stars! Technical
Reference Guide**, printed page 11 (PDF page 11). Source:
https://www.bestoldgames.net/download/games/stars/stars-win-guide.pdf

The reference supplies the engine names, count, field levels, access restrictions,
optimal/free/safe speeds, mass, base resource cost and I/B/G mineral costs. These
values live in one core catalog, `propulsion_technologies()`. The Research dock,
Ship Designer and host order validation use the same prerequisites. New campaigns
and quick galaxy generation start with Quick Jump 5.

Individual Warp 1–10 fuel percentages come from the original **Stars! Player's
Guide**, appendix B-6 (PDF page 246):
https://www.bestoldgames.net/download/games/stars/stars-win-manual.pdf
The tables replace the prototype's universal fuel curve. This includes Fuel
Mizer's distinctive high-Warp economy and Settler's Delight's 140% at Warp 7.

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

Starting fleets and newly built ships use the fitted reference engine's catalog
optimal Warp as their initial speed. Legacy ships retain their previous defaults
(Warp 8 for scouts/other ships, Warp 7 for colony ships). The starting reference
Scout weighs 22 kt (8 kt hull, 4 kt Quick Jump 5, 10 kt Suns! scanner) and holds
50 mg. A full turn at Warp 5 moves 25 ly and costs 3 mg; at Warp 8 it would move
64 ly and cost 57 mg, exceeding the entire tank. In deep space, Warp 5 sustains
16 full turns / 400 ly before needing a slower leg or refuelling. Existing saves
and explicit route speeds are preserved; set an existing Quick Jump 5 scout to
Warp 5 for normal exploration. Higher speeds remain available when needed.

## Access and fitting

New campaign setup offers three independent propulsion-access choices for every
empire: Fuel efficiency, No ram scoops, and Settler engines. Fuel efficiency
unlocks Fuel Mizer and Galaxy Scoop and reduces reference-engine consumption by
15%, rounding the adjusted table percentage upward. Legacy engines keep their
previous consumption. These choices do not implement the complete Stars!
IFE/NRSE/Hyper Expansion race traits, their point costs, starting-tech bonuses
or growth modifiers.

No ram scoops blocks the six researchable scoop engines, including the hazardous
Hydro-Ram Scoop. Fuel Mizer remains allowed and collects fuel through Warp 4.
Settler's Delight also remains allowed with
Settler engine access, but only in a Mini-Colony Ship.

The reference Mini-Colony Ship has one engine cell and one mechanical cell, 8 kt hull mass,
150 fuel capacity and 10 kt cargo capacity. Its base costs are 3 resources and
2/0/2 minerals. Its cargo still uses Suns!' 100 kg/person conversion.

Double-clicking a researchable engine in the technology catalog queues all missing
field levels, including Energy, and accounts for levels already queued. A
race-restricted row explains the requirement and does not add futile research.
The designer shows locked engines and prevents fitting unavailable components;
the host independently rejects designs that bypass those gates.

## Reference fuel rules

Travel remains Warp squared. For a table percentage `E`, unrounded consumption
is `loaded_mass_kt * E * distance_ly / 20000`: at 100%, one mg moves 200 kt by
one ly. Cargo is distributed between design stacks in proportion to their cargo
capacity. Consumption is billed separately for each stack: round distance up to
whole ly, truncate the computed bill to one decimal place, then round up to whole
mg. Fuel efficiency first changes `E` to `ceil(E * 0.85)`.

The rounding follows the empirical Stars! algorithm attributed to m.a@stars,
published in CraigStars (`getFuelCostForEngine`, pinned source):
https://github.com/sirgwain/craig-stars/blob/d6a71d0b981b2343498e31311ef17797ec501d2d/cs/fleet.go
Suns! implements these rules independently and retains its fractional ship/cargo
masses; it does not reproduce every integer-mass quirk of the original binary.

Collection is separate from consumption and depends on engine count and actual
distance, not cargo mass. Each engine collects 1/3/6/10 mg per ly at its free
Warp, one below, two below, or three-or-more below, respectively. Fuel Mizer is
included. Ordinary engines collect 1 mg per ly at Warp 1. A one-engine Fuel Mizer
travelling a full year at Warp 4 collects 16 mg; a three-engine ship collects
48 mg. The fleet's total collection is rounded down to whole mg and limited by
its fuel capacity.

When fuel cannot support the ordered Warp, reference-engine fleets spend their
remaining time at the lowest free Warp among their engines, retaining the
requested Warp for the next year. Collection during a move cannot finance the
initial consumption of another engine in the fleet. Pure legacy fleets retain
their previous fuel-limited movement and signed linear fuel curves.

The designer shows base consumption per 100 kt per ly separately from collection
for the fitted engine bank; Fuel efficiency is applied in flight. The reference
hull catalog now supplies original mass and fuel capacities (see
[Stars! hulls](stars-hulls.md)); legacy hulls, Suns! equipment and colonist mass
still affect range.

## Suns! simulation and balance

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

Save format is 56 and order format is 14 (propulsion originally introduced
55/13). Previous saves/orders remain readable; pre-55 races receive neutral
propulsion access settings. New engine/hull IDs and access settings round-trip
in host saves and player turn files. All PBEM
participants need the same build; older clients reject newer packet formats.

This completes the normal reference engine catalog, not every Stars! technology.
The Mystery Trader's Enigma Pulser is not a normal research unlock and awaits a
visitor/acquisition system. The complete ship hull catalog is documented in
[Stars! hulls](stars-hulls.md). Weapons, shields, terraforming and other
branches remain separate work under #44 and their respective mechanics.
