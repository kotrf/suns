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

The first expansion loop is now represented directly in the desktop app:

1. select **Sol / Earth** and queue a colony ship build;
2. end the turn to resolve production and construction;
3. select another system and move the colony ship there;
4. end the turn;
5. select the destination again and queue colonization;
6. end the turn to establish the second colony;
7. subsequent turns produce resources independently at both colonies.

The numbers are placeholders. At this stage the purpose is to validate the game-state/order/turn architecture and make each new rule usable through the UI immediately.
