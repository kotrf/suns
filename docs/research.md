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

The player selects one current focus and may preselect a one-time next field. When the current field gains a level, the focus switches automatically and the preselection clears. Excess RP from the completing turn immediately enters the new field; no research is lost.

Level costs currently double from an 18 RP base: 18 RP for level 1, 36 RP for level 2 and 72 RP for level 3. These values are early balance parameters rather than permanent rules.

## Colony production

An owned colony can add an ongoing `Research` item to its production queue. When it reaches the front, every point of stored and current production becomes empire RP each turn. The item remains active until explicitly stopped, so it never needs to be queued annually. Stopping it removes the repeating item and lets the colony store output or resume later queued work.

This makes science compete with factories, mines and ships through the existing physical economy instead of appearing as a free global income or a separate tax slider.

## First unlocks

- Electronics 0: the starting Long Range Scanner remains available.
- Electronics 1: Compact Long Range Scanner, 55 ly range, 5 kt, cost 2 and a smaller mineral bill.
- Electronics 2: a heavy extended-range sensor remains planned but is not part of this slice.
- Electronics 3: Penetrating Scanner.
- Construction 1: Remote Mining Module. A ship in orbit of an uncolonized planet mines into its surface stockpile; cargo fleets collect the minerals separately.

The compact scanner is not a universal upgrade: it halves scanner mass and lowers cost but also reduces field radius from 90 ly to 55 ly. Existing ship designs remain unchanged. New designs are validated against the owner's technology both in the desktop Ship Designer and again in core order processing.

## Events and UI

The Research dock shows all field levels, current RP progress, the current focus, the optional next focus and the next concrete unlock. It also starts or stops ongoing research at the selected friendly colony.

Every completed level emits a deterministic `ResearchLevelCompleted` event. Turn Messages announces the new level and names implemented unlocks such as the Compact Long Range Scanner.

Future scientific expeditions and reverse engineering can add discovery or artifact requirements alongside field levels. The first slice does not implement those systems and does not assume that RP alone must unlock every late technology.
