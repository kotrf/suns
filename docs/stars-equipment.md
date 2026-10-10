# Stars! equipment fitting

Suns! now has the remaining 113 reference ship modules, alongside its 32
reference hulls, 15 engines, 16 scanners and six standard mining robots.
Beams, torpedoes, shields and armor now work in Suns!' first automatic
[space-combat model](space-combat.md). Other systems below remain pending.

## Sources

- Original component export, `UNEDITED.MOD`, blob
  `ed45032e17d36bcc7d292559caabd89fe42aa3c1`:
  https://github.com/stars-4x/starsapi/blob/master/src/main/java/org/starsautohost/starsapi/items/UNEDITED.MOD
- Original Stars! Technical Reference Guide, printed pp.2–9 and 12:
  https://www.bestoldgames.net/download/games/stars/stars-win-guide.pdf
- Original Player's Guide, ship design, electrical combat modifiers,
  cloaking and racial equipment:
  https://www.bestoldgames.net/download/games/stars/stars-win-manual.pdf

The export supplies exact mass, resources, I/B/G costs and all six technology
prerequisites. Values are numerical facts; the code, art and UI are Suns!' own.
Printed guide discrepancies are resolved in favor of the component export:
Super Fuel Tank is 8 kt, Maneuvering Jet and Overthruster 5 kt, Beam Deflector
1 kt and Fielded Kelarium 50 kt. The Player's Guide assigns Anti-matter Generator
to Interstellar Traveler; the Technical Guide's IS label is not used for it.

## Catalog and access

| Family | Modules | Fitting banks |
| --- | ---: | --- |
| Beam weapons | 24 | Weapon / General |
| Torpedoes and missiles | 12 | Weapon / General |
| Bombs | 15 | Bomb |
| Mine layers | 10 | Mine layer / General |
| Mechanical | 11 | Mechanical / General |
| Electrical | 17 | Electrical / General |
| Shields | 10 | Shield / compatible mixed / General |
| Armor | 12 | Armor / compatible mixed / General |
| Alien Miner and Orbital Adjuster | 2 | Mining robot |

A bank contains one model with a quantity up to its original capacity. Typed
banks reject incompatible models. Every fitted copy adds its own mass, resource
bill and mineral bill. Host orders independently enforce fitting and research;
dragging a locked item cannot bypass those checks. Unknown component/hull IDs
are rejected rather than becoming an empty or free component.

The existing hull-access families also gate supported racial equipment: IS
jammers/sharmor, SS cloaks and armor, WM beams, SD mine layers, HE capacitor.
War Monger cannot use mine layers; Inner Strength cannot use smart bombs.
Transport Cloaking currently accepts only the four unarmed reference freighter
hulls, and the project must not contain weapons or bombs.

Claim Adjuster, Alternate Reality and Interstellar Traveler are not selectable
race families yet. Their dedicated devices are listed with an explicit unmet
race requirement. Mystery Trader items require an already owned design with
that device; researching their listed levels alone never grants them. Acquisition,
trade, full PRT/LRT behavior, regenerating-shield traits and miniaturization are
separate work.

## Active capabilities and reference ratings

| Capability | Current behavior |
| --- | --- |
| Cargo and fuel pods | Capacity, mass, cost and fuel usage affect normal flight/cargo rules |
| Colonization Module | Enables ordinary colony founding |
| Anti-matter Generator | +200 mg tank and +50 mg/year per module; reference device awaits IT access |
| Alien Miner | 10 kt per mineral/year at concentration 100, if acquired; ordinary remote-mining rules |
| Armor and shields | Absorb battle damage; armor losses persist, shields recharge between battles |
| Guns, torpedoes, computers, jammers, capacitors, jets | Active in automatic combat using the documented Suns! tactical rules |
| Bombs / mine layers | Original fitting ratings only; no bombardment or minefield resolution |
| Cloaks / tachyon detection | Reference ratings only; no stealth detection modifier |
| Orbital Adjuster / AR construction / Jump Gate | Access and fitting data only; no operational special action |

Weapon cards expose power, battle-board range, initiative and accuracy.
Computers reduce torpedo **inaccuracy**; they do not add flat accuracy points.
Jets affect battle movement, not strategic Warp. Shield-only beams, gatling
beams, missiles, smart bombs and minefield types retain separate metadata.
Alien multifunction special effects beyond the explicitly exposed ratings
remain future work. Cards state which system is not yet operational.

## Designer and compatibility

The catalog has case-insensitive search, equipment-family filtering and a
"Fits this hull" filter. The latter uses accepted bank classes, independently
of research availability and occupancy. Changing hulls reapplies the filter.
The summary shows total armor, shields and fitted armament quantities. Existing
designs can be copied into a new named project and queued for production.

New campaigns use the reference Colonization Module. Their Colony Ship weighs
56 kt (20 hull + 4 Quick Jump 5 + 32 module) and costs 33 resources before
miniaturization. Reference Cargo Pod adds 50 kt cargo at Construction 3;
Super Cargo Pod adds 100 at Construction 9 / Energy 3. Reference Fuel Tank adds
250 mg at tech zero; Super Fuel Tank adds 500 at Construction 14 / Energy 6 /
Propulsion 4.

Prototype Colony Module, Fuel Tank, Cargo Pod and Antimatter Generator keep
their old IDs and all old properties. An empire with a legacy hull/design can
still select them, marked legacy. They do not appear in fresh reference-only
campaigns. Relay Array, Field Repair Bay and Anomaly Detector remain Suns!
support devices.

Save **v60** and turn-order **v18** append equipment IDs without changing record
layouts or existing component IDs. Earlier saves/orders remain readable and
their existing designs are not migrated to different equipment. Pre-v60 readers
cannot exchange new component IDs; all PBEM participants need the new build.

Tests cover costs and ratings, every technology prerequisite, racial/acquisition
gates, compatible and incompatible banks, invalid IDs, forged host orders,
actual ship construction, old module properties, all 113 save/order round trips,
v59/v17 compatibility, UI drag/drop, search, filters and multi-copy banks.
