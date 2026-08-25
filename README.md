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
2. send **Scout 1** to a system and end the turn to survey it;
3. compare the revealed world's habitability and long-term population capacity;
4. higher-habitability colonies grow faster and support larger populations;
5. population contributes to economic output while factories add infrastructure output;
6. queue a **Colony Ship** or invest first in a **Factory**;
7. local output advances persistent production queues over multiple turns;
8. move a completed colony ship to a surveyed system and colonize it.

Ending a turn also produces an actionable Turn Messages briefing. Survey discoveries, fleet arrivals, completed routes, fuel stalls and finished production are typed deterministic events. Remote fleet reports obey communication delay instead of exposing authoritative state through the UI.

A colony ship may enter an unsurveyed system, but colonization requires survey intel. Planet quality therefore matters before the expansion decision instead of being a decorative statistic.

The numerical values are still placeholders. The current purpose is to grow a coherent playable loop while keeping the simulation deterministic and independent of the GUI.
