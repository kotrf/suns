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

The desktop app now exposes the first interactive turn:

1. select a star on the galaxy map;
2. queue a destination for the player's scout fleet;
3. end the turn;
4. let the headless `TurnProcessor` resolve the order into the next `GameState`.

This is deliberately tiny. Planets, colonies, production and colonization are the next pieces of the first vertical slice.
