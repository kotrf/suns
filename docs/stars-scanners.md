# Stars! ship scanners

The ship scanner catalog is based on the original Stars! Technical Reference
Guide, printed page 10, and the Player's Guide, printed section 9-7 (PDF page
112). Sources: [technical reference](https://www.bestoldgames.net/download/games/stars/stars-win-guide.pdf)
and [player manual](https://www.bestoldgames.net/download/games/stars/stars-win-manual.pdf).
The shared catalog supplies ranges, technology prerequisites, mass, resource
cost and mineral cost to the simulation, research window and ship designer.

## Catalog

Ranges are in light years; mass and mineral costs are in kt. Requirements use
Electronics (El), Energy (En), Biology (Bio) and Propulsion (P).

| Scanner | Ordinary | Penetrating | Mass | Requirements | Resources | I / B / G |
| --- | ---: | ---: | ---: | --- | ---: | --- |
| Bat | 0 | 0 | 2 | None | 1 | 1 / 0 / 1 |
| Rhino | 50 | 0 | 5 | El 1 | 3 | 3 / 0 / 2 |
| Mole | 100 | 0 | 2 | El 4 | 9 | 2 / 0 / 2 |
| DNA | 125 | 0 | 2 | Bio 6, P 3 | 5 | 1 / 1 / 1 |
| Possum | 150 | 0 | 3 | El 5 | 18 | 3 / 0 / 3 |
| Pick Pocket | 80 | 0 | 15 | El 4, En 4, Bio 4; Super Stealth | 35 | 8 / 10 / 6 |
| Chameleon | 160 | 45 | 6 | El 6, En 3; Super Stealth | 25 | 4 / 6 / 4 |
| Ferret | 185 | 50 | 2 | El 7, En 3, Bio 2 | 36 | 2 / 0 / 8 |
| Dolphin | 220 | 100 | 4 | El 10, En 5, Bio 4 | 40 | 5 / 5 / 10 |
| Gazelle | 225 | 0 | 5 | El 8, En 4 | 24 | 4 / 0 / 5 |
| RNA | 230 | 0 | 2 | Bio 10, P 5 | 20 | 1 / 1 / 2 |
| Cheetah | 275 | 0 | 4 | El 11, En 5 | 50 | 3 / 1 / 13 |
| Elephant | 300 | 200 | 6 | El 16, En 6, Bio 7 | 70 | 8 / 5 / 14 |
| Eagle Eye | 335 | 0 | 3 | El 14, En 6 | 64 | 3 / 2 / 21 |
| Robber Baron | 220 | 120 | 20 | El 15, En 10, Bio 10; Super Stealth | 90 | 10 / 10 / 10 |
| Peerless | 500 | 0 | 4 | El 24, En 7 | 90 | 3 / 2 / 30 |

Super Stealth scanner access uses the existing empire access family. Cargo theft
(Pick Pocket / Robber Baron), Chameleon's cloaking and the No Advanced Scanners
race trait are not implemented. Their special abilities are not advertised as
available in the designer. Suns! retains its own survey tiers and fleet-contact
rules; this change imports the equipment progression and range calculation.

## Range and starting ships

Within one ship, each channel is calculated independently:

`range = (sum(module_range^4))^(1/4)`

Two Rhino scanners give 59.4604 ly, four give 70.7107 ly. Combining ordinary and
penetrating modules does not add their radii together. A fleet uses its strongest
ship for each channel; more ships never multiply coverage. Ordinary coverage
extends communications; penetrating coverage alone does not. Relay Array still
adds its dedicated communications range after combining ordinary scanners.

Bat has no remote coverage, but enables orbital surveys and the existing
multi-turn geology/deep survey sequence. An unequipped ship does not gain that
survey ability. The designer shows ordinary and penetrating ranges separately;
the map's fleet coverage circle uses the greater of the two.

All new empires start at Electronics 1 with a Rhino-equipped Scout, for a 50 ly
starting radar. The reference Scout now weighs 17 kt (8 hull, 4 Quick Jump 5,
5 Rhino); fuel capacity remains 50 mg. A full 25 ly Warp-5 turn still costs 3 mg
with the existing whole-mg billing, while a 64 ly Warp-8 turn costs 44 mg.
Colony sensors remain 150 ly; signal speed remains 150 ly/year.

## Anomalies and compatibility

Anomaly Detector remains at Electronics 6, 20 kt, 12 resources and 4/3/9 minerals.
It provides a separate 200 ly anomaly footprint (90 ly for faint mouths, 70 ly
for weak mouths), without extending ordinary radar or communications. A stronger
fitted scanner can still increase this anomaly footprint. Existing stability
classification, transit risk reduction and report latency are retained.

Old component IDs are unchanged. Old Long Range Scanner modules now have 50 ly
instead of 90; other prototype scanners retain their own specifications and are
shown as legacy only for empires already using them. Previously delivered intel
remains historical knowledge; shrinking a scanner does not erase surveys.
Old route speeds and fuel are not rewritten. Save format 57 and order format 15
append the new scanner IDs and retain readers for older formats. Clients must be
updated together before exchanging new scanner designs.
