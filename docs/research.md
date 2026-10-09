# Research and technology

Research is an empire-level strategic system resolved deterministically by `TurnProcessor`. Technology state belongs to each player and is persisted in save files for headless simulation, AI, PBEM and replays.

## Fields and planning

The first model uses six broad fields:

- Energy
- Propulsion
- Construction
- Electronics
- Biology
- Weapons

The current focus is the first row of an ordered research plan. Every row,
including the active one, may be reordered or removed before End Turn. RP already
invested in a field remains attached to that field and is available when the
field returns to the plan. An empty plan pauses empire research and leaves the
full yearly output available to local production. When the current field gains a level, the first queued field
becomes active and is removed from the queue. Excess RP from the completing turn
immediately enters the new field; no research is lost. If the future queue is
empty, research continues in the current field.

Level costs are `18 * (1 + level * (level - 1) / 2)` RP: 18, 36 and 72 for levels 1–3; 4572 for level 23. The quadratic curve makes late reference engines reachable and reprices existing higher-level projects without losing their RP. These values remain initial balance parameters.

## Empire allocation and colony production

The player sets one empire-wide research allocation from 0% to 100%. Every
colony contributes that percentage of its yearly resource output to the common
RP pool before resolving its local production queue. Integer rounding happens
per colony and the remainder stays available for production.

After a colony finishes everything it can in its local queue, all unused output
also enters the common RP pool. Research therefore competes with factories,
mines and ships without appearing as a local construction item. Production
points are yearly capacity and are never stored between turns; only physical
minerals accumulate. Legacy ongoing `Research` rows from older saves are removed
when the save is loaded.

## First unlocks

New campaigns use the [15-model Stars! propulsion line](stars-propulsion.md), including multiple field prerequisites and propulsion access restrictions. The early Propulsion 1/2/3 entries below describe legacy designs retained for older empires.

- Energy 1: Antimatter Generator, adding 200 fuel capacity and producing 50 fuel
  per turn. Existing designs that used the previously unrestricted component
  remain unchanged; new designs require the technology.
- Propulsion 1: Advanced Fusion Drive, a safe 16 kt Warp-9 engine. It is lighter
  than either ram scoop, but costs more to build and consumes fuel at every Warp.
- Propulsion 2: High Warp Drive, a safe Warp-10 engine at 25 kt and higher mineral
  cost. It burns considerably more fuel at Warp 9–10, so scoops and the lighter
  Advanced Fusion Drive remain useful for long routes and smaller ships.
- Ship scanners follow the [16-model Stars! catalog](stars-scanners.md), with
  independent ordinary and penetrating ranges and multiple field requirements.
  New empires start at Electronics 1 with a 50 ly Rhino Scanner; Bat has orbital
  survey capability at level 0. Prototype scanner unlocks remain for older empires.
- Electronics 6: Anomaly Detector provides dedicated 200 ly WH sensing, detects
  weak anomalies, classifies wormholes and reduces transit risk. It adds no
  ordinary ship radar or communications coverage. See [wormholes](wormholes.md).
- Mining follows the [six-model Stars! robot catalog](stars-mining.md), with Construction/Electronics prerequisites and Advanced/Basic Remote Mining access. Ordinary empires first fit Robo-Mini-Miner on a Mini-Miner hull at Construction 2 and Electronics 1; successive models change output and the mass/cost tradeoff. Prototype Construction-1 equipment remains for legacy empires. The persistent waypoint task deposits output into the planet's surface stockpile for cargo fleets to collect separately.
- Construction 2: Heavy Transport hull, with 250 kt built-in cargo capacity and three required engines. It carries bulk ore from mining sites, while smaller transports cost less to build and burn less fuel per ship.
- Construction 3: Field Repair Bay in a general slot. Equipped ships can repair 8 hull-damage points per year away from a dock; mixed fleets scale the rate by the equipped fraction. At a friendly shipyard, the dock's 20-point rate takes precedence.

Multiple scanners on one ship combine by the fourth root of the sum of their
fourth powers, independently for ordinary and penetrating ranges. Fleet coverage
uses the strongest ship. New designs are validated against the owner's technology
both in the desktop Ship Designer and again in core order processing.

## Events and UI

The Research dock opens from the toolbar or View menu. It can be tabbed, hidden,
detached and used beside the galaxy map; its layout is saved with the workspace. It shows all
field levels, current RP progress, the next concrete unlock and an ordered plan
with per-level target and RP work. Every row has Move Up, Move Down and Remove
controls, including the active first row. Removing or moving it preserves its
accumulated RP. Returning the displayed rows to the committed plan removes the
pending research-plan order instead of leaving an accidental action in the turn
order list. The same dialog sets the empire-wide allocation percentage and shows
its guaranteed RP contribution; unused output after local queues is additional
research. Changes immediately update the current turn's pending orders, while
closing the dock leaves pending orders intact.

Every completed level emits a deterministic `ResearchLevelCompleted` event. Turn Messages announces the new level and names implemented unlocks such as the Mole Scanner.

Future scientific expeditions and reverse engineering can add discovery or artifact requirements alongside field levels. The first slice does not implement those systems and does not assume that RP alone must unlock every late technology.


## Campaign technology catalog

The research window now lists the shared core unlock catalog with prerequisites,
availability and capability descriptions. Double-clicking a locked technology
appends all missing field levels to the plan, including secondary requirements, accounting for already queued
levels. Race-restricted technologies explain their access condition and cannot queue futile research. Component legality uses that same catalog on the host.

For new environment-based campaigns, Biology and a secondary science unlock
paid, planet-specific terraforming. The normal limits are 3/7/11/15 axis points;
Biology levels 1/2/3/4 pair with Energy, Propulsion or Weapons levels 1/5/10/16
for temperature, gravity or radiation respectively. Research alone does not
change racial tolerance or a planet. Each unit change costs 12 production and
must complete in the local queue; see [Terraforming](terraforming.md).

Saves made before v59 retain their existing automatic Biology tolerance bonus
through an explicit compatibility flag. Legacy scalar-habitability campaigns
retain their original rules and do not gain physical terraforming. Weapons now
supports radiation terraforming; this is still not a complete combat tree.
Each empire researches independently, including remote
players whose settings arrive through turn-order envelopes.
