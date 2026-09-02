# Suns!

**Suns!** is a turn-based space 4X strategy game inspired by the clarity and systemic depth of classic games such as *Stars!*.

The design goal is a relatively small set of deep, interacting systems rather than complexity through sheer feature count.

## Architecture

The simulation is deliberately separated from the desktop UI:

- `src/core` — deterministic, headless game model and turn processing.
- `src/app` — Qt 6 Widgets desktop application.
- `tests` — headless simulation tests.
- `docs` — design direction and project notes.

The central model is:

`GameState(N) + Orders(all players) -> TurnProcessor -> GameState(N+1)`

This keeps the core suitable for AI simulation, replays, multiplayer/PBEM and automated testing without requiring a GUI.

## Build

Requirements: C++20 compiler, CMake 3.24+, Qt 6.4+ (for the desktop app).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/app/suns
```

For a headless core/test build without Qt:

```sh
cmake -S . -B build -DSUNS_BUILD_APP=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

## Current playable loop

The desktop app now links exploration, planet quality, economy and expansion:

1. star positions are visible, but non-home planetary data starts unknown;
2. an ordinary scanner fly-by records a system contact but leaves planetary data unknown;
3. the later-tech penetrating scanner estimates habitability at range, while arriving confirms it exactly;
4. remain in orbit for a geological survey, then compare habitability, capacity and mineral value;
5. higher-habitability colonies grow faster and support larger populations;
6. population contributes to economic output while factories add infrastructure output;
7. queue a **Colony Ship** or invest first in a **Factory**;
8. local output advances persistent production queues over multiple turns;
9. move a completed colony ship to an orbital-surveyed system and colonize it.

Ending a turn also produces an actionable Turn Messages briefing. Survey discoveries, fleet arrivals, completed routes, fuel stalls, finished production, mineral shortages and new colonies are typed deterministic events. Remote fleet reports obey communication delay instead of exposing authoritative state through the UI.

Fleets may contain multiple ship designs and counts. Co-located idle fleets can be merged while preserving the chosen FleetId, or split by exact design stack into a new FleetId; movement, sensors, cargo, fuel, colonization and remote mining derive from the complete composition.

Route programs can target another friendly FleetId. Pursuers resolve the target's projected motion every turn and may use **Merge with fleet**. Their relative movement is checked continuously inside the annual turn, so a real crossing can merge the fleets at the intercept point while a closest-approach miss keeps the pursuit active. After merging, the surviving target FleetId continues its route for the unused part of the year at a Warp supported by every ship in the combined fleet. A lost target clears the route and produces a warning.

The galaxy map is the permanent workspace while Overview, Fleet, Fleet Route Program, Production, Research and Turn Messages are dockable panels. They may be tabbed, resized or detached into operating-system windows, and the chosen layout is restored on the next launch. Fleet logistics, organization, colonization and ship design live together in the Fleet area.

Colony production is shown as an ordered list with per-item remaining work and a forecast completion turn. Items can be moved earlier or later before End Turn. An empire-wide percentage funds global research before local production, and unused output after each colony's queue also becomes RP. Energy 1 unlocks onboard fuel generation, Propulsion 1 a light safe Warp-9 drive, and Electronics 2 a heavy 160 ly sensor.

Orbital services are explicit infrastructure. The homeworld begins with a basic Orbital Dock containing a shipyard and refueling depot; new colonies must build their own dock through the local production queue before they can construct ships or refuel fleets. Ships may be planned before the dock, but wait without consuming production until a shipyard is available.

A colony ship may enter an unknown or basically scanned system, but colonization requires a confirmed orbital survey. Planet quality therefore matters before the expansion decision instead of being a decorative statistic.
Successful colonization dismantles the entire fleet, deposits all carried minerals, and recovers 33% of every ship design's Ironium, Boranium and Germanium construction cost, rounded down per mineral. Split escorts or valuable ships away before founding the colony.

The numerical values are still placeholders. The current purpose is to grow a coherent playable loop while keeping the simulation deterministic and independent of the GUI.
