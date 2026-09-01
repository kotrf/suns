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

Level costs currently double from an 18 RP base: 18 RP for level 1, 36 RP for level 2 and 72 RP for level 3. These values are early balance parameters rather than permanent rules.

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

- Energy 1: Antimatter Generator, adding 200 fuel capacity and producing 50 fuel
  per turn. Existing designs that used the previously unrestricted component
  remain unchanged; new designs require the technology.
- Propulsion 1: Advanced Fusion Drive, a safe 16 kt Warp-9 engine. It is lighter
  than either ram scoop, but costs more to build and consumes fuel at every Warp.
- Electronics 0: the starting Long Range Scanner remains available.
- Electronics 1: Compact Long Range Scanner, 55 ly range, 5 kt, cost 2 and a smaller mineral bill.
- Electronics 2: Extended Range Scanner, a 160 ly ordinary sensor at 24 kt,
  cost 8 and a substantial Germanium bill. It does not penetrate planets.
- Electronics 3: Penetrating Scanner.
- Construction 1: dedicated Remote Miner hull plus heavy Remote Mining Module. Mining equipment fits only `Mining` slots; the persistent waypoint task deposits output into the planet's surface stockpile for cargo fleets to collect separately.

The scanner line offers three different engineering choices rather than automatic replacements: Compact saves mass at 55 ly, the starting scanner balances mass and a 90 ly field, and Extended reaches 160 ly at more than twice the starting scanner's mass and cost. The Advanced Fusion Drive likewise trades the ram scoops' fuel collection for lower mass, higher thrust and safe Warp 9. Existing ship designs remain unchanged. New designs are validated against the owner's technology both in the desktop Ship Designer and again in core order processing.

## Events and UI

The modal Research dialog opens from the compact dialog toolbar. It shows all
field levels, current RP progress, the next concrete unlock and an ordered plan
with per-level target and RP work. Every row has Move Up, Move Down and Remove
controls, including the active first row. Removing or moving it preserves its
accumulated RP. Returning the displayed rows to the committed plan removes the
pending research-plan order instead of leaving an accidental action in the turn
order list. The same dialog sets the empire-wide allocation percentage and shows
its guaranteed RP contribution; unused output after local queues is additional
research. Changes immediately update the current turn's pending orders, while
Close simply returns to the galaxy map.

Every completed level emits a deterministic `ResearchLevelCompleted` event. Turn Messages announces the new level and names implemented unlocks such as the Compact Long Range Scanner.

Future scientific expeditions and reverse engineering can add discovery or artifact requirements alongside field levels. The first slice does not implement those systems and does not assume that RP alone must unlock every late technology.
