# Stars! ship hulls in Suns!

The ship catalog contains all 32 original ship hulls: 31 normal/racial hulls
and the Mystery Trader's Mini Morph. Orbital starbase hulls are a separate
system and are not included here.

## Sources and transcription

- Original **Stars! Technical Reference Guide**, printed pp.16–23:
  https://www.bestoldgames.net/download/games/stars/stars-win-guide.pdf
- Original component-data export (category 15, original hull IDs 1–32):
  https://github.com/stars-4x/starsapi/blob/master/src/main/java/org/starsautohost/starsapi/items/UNEDITED.MOD
- Original **Stars! Player's Guide**, fuel transports (10-6), damage repair
  (23-7/23-8), and hull table (B-7):
  https://www.bestoldgames.net/download/games/stars/stars-win-manual.pdf

The data export supplies exact bank masks/capacities, costs, mass, cargo, fuel,
armor, initiative and Construction requirements. The printed guide provides
an independent check of the hull families and fitting restrictions. Large
Freighter uses **125 kt** from the component data; the printed Technical
Reference Guide instead lists 100 kt. B-17 is **69 kt** in the component data.
Names and numerical facts are transcribed; the implementation, schematic art
and grid arrangement are Suns!' own.

## Hull catalog

Values are bare hulls, before fitted engines/equipment. C is Construction.
Resources and I/B/G are base construction costs. Cargo/mass use kt, fuel mg
and armor damage points. IS/SS/WM/SD/HE identify the hull-access families.

| Hull | C | Mass | Resources | I/B/G | Cargo | Fuel | Armor | Initiative | Access |
| --- | ---: | ---: | ---: | --- | ---: | ---: | ---: | ---: | --- |
| Small Freighter | 0 | 25 | 20 | 12/0/17 | 70 | 130 | 25 | 0 | All |
| Medium Freighter | 3 | 60 | 40 | 20/0/19 | 210 | 450 | 50 | 0 | All |
| Large Freighter | 8 | 125 | 100 | 35/0/21 | 1200 | 2600 | 150 | 0 | All |
| Super Freighter | 13 | 175 | 125 | 45/0/21 | 3000 | 8000 | 400 | 0 | IS |
| Scout | 0 | 8 | 10 | 4/2/4 | 0 | 50 | 20 | 1 | All |
| Frigate | 6 | 8 | 12 | 4/2/4 | 0 | 125 | 45 | 4 | All |
| Destroyer | 3 | 30 | 35 | 15/3/5 | 0 | 280 | 200 | 3 | All |
| Cruiser | 9 | 90 | 85 | 40/5/8 | 0 | 600 | 700 | 5 | All |
| Battle Cruiser | 10 | 120 | 120 | 55/8/12 | 0 | 1400 | 1000 | 5 | WM |
| Battleship | 13 | 222 | 225 | 120/25/20 | 0 | 2800 | 2000 | 10 | All |
| Dreadnought | 16 | 250 | 275 | 140/30/25 | 0 | 4500 | 4500 | 10 | WM |
| Privateer | 4 | 65 | 50 | 50/3/2 | 250 | 650 | 150 | 3 | All |
| Rogue | 8 | 75 | 60 | 80/5/5 | 500 | 2250 | 450 | 4 | SS |
| Galleon | 11 | 125 | 105 | 70/5/5 | 1000 | 2500 | 900 | 4 | All |
| Mini-Colony Ship | 0 | 8 | 3 | 2/0/2 | 10 | 150 | 10 | 0 | HE |
| Colony Ship | 0 | 20 | 20 | 10/0/15 | 25 | 200 | 20 | 0 | All |
| Mini Bomber | 1 | 28 | 35 | 20/5/10 | 0 | 120 | 50 | 0 | All |
| B-17 Bomber | 6 | 69 | 150 | 55/10/10 | 0 | 400 | 175 | 0 | All |
| Stealth Bomber | 8 | 70 | 175 | 55/10/15 | 0 | 750 | 225 | 0 | SS |
| B-52 Bomber | 15 | 110 | 280 | 90/15/10 | 0 | 750 | 450 | 0 | All |
| Midget Miner | 0 | 10 | 20 | 10/0/3 | 0 | 210 | 100 | 0 | ARM; no BRM |
| Mini-Miner | 2 | 80 | 50 | 25/0/6 | 0 | 210 | 130 | 0 | no BRM |
| Miner | 6 | 110 | 110 | 32/0/6 | 0 | 500 | 475 | 0 | no BRM |
| Maxi-Miner | 11 | 110 | 140 | 32/0/6 | 0 | 850 | 1400 | 0 | no BRM |
| Ultra-Miner | 14 | 100 | 130 | 30/0/6 | 0 | 1300 | 1500 | 0 | ARM; no BRM |
| Fuel Transport | 4 | 12 | 50 | 10/0/5 | 0 | 750 | 5 | 0 | IS |
| Super-Fuel Xport | 7 | 111 | 70 | 20/0/8 | 0 | 2250 | 12 | 0 | All |
| Mini Mine Layer | 0 | 10 | 20 | 8/2/5 | 0 | 400 | 60 | 0 | SD |
| Super Mine Layer | 15 | 30 | 30 | 20/3/9 | 0 | 2200 | 1200 | 0 | SD |
| Nubian | 26 | 100 | 150 | 75/12/12 | 0 | 5000 | 5000 | 2 | All |
| Mini Morph | 8 | 70 | 100 | 30/8/8 | 150 | 400 | 250 | 2 | Mystery Trader |
| Meta Morph | 10 | 85 | 120 | 50/12/12 | 300 | 700 | 500 | 2 | HE |

## Fitting banks

Each semicolon separates a distinct original bank. A bank takes one equipment
model at a time, with quantity zero through its listed capacity. The engine bank
must be complete and homogeneous; engine count does not multiply fleet speed.
S/E/M means Scanner/Electrical/Mechanical; Sh/A means Shield/Armor; W means Weapon
(beam or torpedo); ML means Mine layer. General accepts Scanner, Shield, Armor,
Weapon, Mine layer, Electrical or Mechanical equipment, but not engines, bombs
or mining robots.

| Hull | Banks, in stable catalog order |
| --- | --- |
| Small Freighter | Engine ×1; S/E/M ×1; Sh/A ×1 |
| Medium Freighter | Engine ×1; S/E/M ×1; Sh/A ×1 |
| Large Freighter | Engine ×2; S/E/M ×2; Sh/A ×2 |
| Super Freighter | Engine ×3; S/E/M ×3; Sh/A ×5; Electrical ×2 |
| Scout | Engine ×1; Scanner ×1; General ×1 |
| Frigate | Engine ×1; Scanner ×2; General ×3; Sh/A ×2 |
| Destroyer | Engine ×1; Weapon ×1; Weapon ×1; General ×1; Armor ×2; Mechanical ×1; Electrical ×1 |
| Cruiser | Engine ×2; Sh/E/M ×1; Sh/E/M ×1; Weapon ×2; Weapon ×2; General ×2; Sh/A ×2 |
| Battle Cruiser | Engine ×2; Sh/E/M ×2; Sh/E/M ×2; Weapon ×3; Weapon ×3; General ×3; Sh/A ×4 |
| Battleship | Engine ×4; S/E/M ×1; Shield ×8; Weapon ×6; Weapon ×6; Weapon ×2; Weapon ×2; Weapon ×4; Armor ×6; Electrical ×3; Electrical ×3 |
| Dreadnought | Engine ×5; Sh/A ×4; Sh/A ×4; Weapon ×6; Weapon ×6; Electrical ×4; Electrical ×4; Weapon ×8; Weapon ×8; Armor ×8; W/Sh ×5; W/Sh ×5; General ×2 |
| Privateer | Engine ×1; Sh/A ×2; S/E/M ×1; General ×1; General ×1 |
| Rogue | Engine ×2; Sh/A ×3; ML/E/M ×2; Scanner ×1; General ×2; General ×2; ML/E/M ×2; Electrical ×1; Electrical ×1 |
| Galleon | Engine ×4; Sh/A ×2; Sh/A ×2; General ×3; General ×3; ML/E/M ×2; E/M ×2; Scanner ×2 |
| Mini-Colony Ship | Engine ×1; Mechanical ×1 |
| Colony Ship | Engine ×1; Mechanical ×1 |
| Mini Bomber | Engine ×1; Bomb ×2 |
| B-17 Bomber | Engine ×2; Bomb ×4; Bomb ×4; S/E/M ×1 |
| Stealth Bomber | Engine ×2; Bomb ×4; Bomb ×4; S/E/M ×1; Electrical ×3 |
| B-52 Bomber | Engine ×3; Bomb ×4; Bomb ×4; Bomb ×4; Bomb ×4; S/E/M ×2; Shield ×2 |
| Midget Miner | Engine ×1; Mining ×2 |
| Mini-Miner | Engine ×1; S/E/M ×1; Mining ×1; Mining ×1 |
| Miner | Engine ×2; A/S/E/M ×2; Mining ×2; Mining ×1; Mining ×2; Mining ×1 |
| Maxi-Miner | Engine ×3; A/S/E/M ×2; Mining ×4; Mining ×1; Mining ×4; Mining ×1 |
| Ultra-Miner | Engine ×2; A/S/E/M ×3; Mining ×4; Mining ×2; Mining ×4; Mining ×2 |
| Fuel Transport | Engine ×1; Shield ×1 |
| Super-Fuel Xport | Engine ×2; Shield ×2; Scanner ×1 |
| Mini Mine Layer | Engine ×1; Mine layer ×2; Mine layer ×2; S/E/M ×1 |
| Super Mine Layer | Engine ×3; Mine layer ×8; Mine layer ×8; Sh/A ×3; S/E/M ×3; ML/E/M ×3 |
| Nubian | Engine ×3; General ×3; General ×3; General ×3; General ×3; General ×3; General ×3; General ×3; General ×3; General ×3; General ×3; General ×3; General ×3 |
| Mini Morph | Engine ×2; General ×3; General ×1; General ×1; General ×1; General ×2; General ×2 |
| Meta Morph | Engine ×3; General ×8; General ×2; General ×2; General ×1; General ×2; General ×2 |

The designer expands each bank into individual cells with stable IDs, shows the
accepted equipment classes and bank capacity in tooltips, and scrolls larger
layouts. Fitting from the catalog replaces/fills the entire selected bank;
Delete/double-click removes one equipment cell. Removing an engine removes the
complete engine bank. Moving a fitted cell cannot mix models in a destination
bank. Automatic placement prefers dedicated/narrow banks to general banks.

Current Suns! scanners, including Anomaly Detector, use Scanner slots. Relay
Array and Antimatter Generator use Electrical slots. Fuel Tank, Cargo Pod,
Colony Module and Field Repair Bay use Mechanical slots. Remote Mining Module
uses Mining slots. For example, Colony Ship accepts its Colony Module but
does not accept a scanner; Small Freighter's Sh/A bank cannot hold a Cargo Pod.

## Access and research

New campaign setup exposes a hull-access family (Standard, Inner Strength,
Super Stealth, War Monger, Space Demolition, Hyper Expansion) and one remote-mining
choice (Standard, Advanced/ARM, Basic/BRM). These are access choices only;
they do not implement full Stars! racial traits, points, economy, growth,
cloaking, combat movement or starting-tech advantages.

Hyper Expansion also enables Settler engines. The independent Settler engines
option from the previous release continues to grant the reference Mini-Colony
Ship; it does not grant Meta Morph. ARM adds Midget Miner and Ultra-Miner.
BRM blocks all five reference remote-miner hulls. Mining robots still have the
existing Suns! Construction 1 gate.

The Research catalog displays every reference hull and its prerequisites.
Double-clicking a researchable hull queues missing Construction levels, taking
already queued levels into account. Racially blocked hulls do not queue futile
research. Mini Morph requires acquisition of an owned design; researching
Construction 8 alone never grants it. Mystery Trader visits/acquisition remain
future work. Host design-order validation independently enforces these gates.

New galaxies/campaigns start with the original Scout (8 kt, 50 mg base fuel)
and Colony Ship (20 kt, 200 mg base fuel). Existing empires retain their original
designs and can use the reference catalog at the appropriate tech levels.

## Operational effects and remaining systems

All hulls participate in existing mass, cargo, fuel, mineral/resource cost,
production and flight rules. All five miner hulls support Suns!' existing
remote mining: robots extract minerals onto an uncolonized planet's surface;
a separate freighter collects the output. Miners need no cargo hold.

Fuel Transport and Super-Fuel Xport each generate 200 mg/year per ship, limited
by the fleet's total tank capacity. One stationary tanker adds 5 or 10 damage
percentage points/year to Suns!' existing fleet repair rate. Tanker counts do
not stack; the stronger bonus wins. A fleet which travelled that year does not
get the tanker bonus, including its arrival year. This integrates the original
support bonuses with Suns!' aggregate damage model, rather than implementing
all original per-ship repair-location rules.

Armor and initiative are reference attributes for the future battle system.
Weapon, Shield, Armor, Bomb and Mine layer slots already enforce fitting
classes but their equipment catalogs and combat/bombing/minefield effects
remain separate work. The mine-layer hulls retain their doubled-mine-laying
metadata; it has no operational effect until minefields exist. Racial cloaking,
stealing cargo, battle movement, research cost differences/miniaturization
and other PRT/LRT effects are not implied by choosing a hull family.

## Compatibility

Save format **56** and turn-order format **14** append the 32 hull IDs after
the seven existing hull IDs and persist hull-access/ARM/BRM choices. Older
saves/orders remain readable. Pre-56 races default to Standard hull access
and neutral mining access; pre-55 propulsion access migration is unchanged.
Old hull stats, fitting IDs and designs remain unchanged, including the
deterministic demo fixture. The designer shows legacy hulls only to an empire
which already owns a legacy design. All PBEM participants need the new build
to exchange reference-hull designs.

Tests cover the original key statistics, all access/tech gates, typed and
homogeneous banks, host-side rejection, tanker generation/support, remote
mining, UI fitting/scrolling/research queueing, all 32 hull IDs in save/order
round trips, player export, malformed access flags and actual v55/v13
compatibility fixtures.
