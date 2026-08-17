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

The desktop app now contains the first small strategic economy:

1. select **Sol / Earth**;
2. queue a **Colony Ship** or invest first in a **Factory**;
3. end turns while local industry advances the persistent production queue;
4. factories increase that colony's industry for future turns;
5. when a colony ship is completed, move it to another system;
6. colonize the destination world;
7. manage independent local production queues across the growing empire.

Multiple player orders can be prepared before ending a turn. Production is local to each colony, deliberately preserving geography as a future strategic constraint.

The numerical values are still placeholders. The current purpose is to grow a coherent playable loop while keeping the simulation deterministic and independent of the GUI.
