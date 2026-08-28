# Remote mining

Remote mining lets an empire exploit an uncolonized planet without turning it into a colony. It is deliberately a logistics mechanic, not a fourth ordinary mineral economy.

## First slice

- `Construction 1` unlocks the **Remote Mining Module**.
- The module is a heavy 80 kt orbital apparatus, so miners consume substantially more fuel in transit than light scouts using the same engine and Warp factor.
- A friendly fleet carrying one or more modules must be stationary at an uncolonized planet and receive the explicit **Remote Mining** task. Merely entering orbit does not start extraction.
- The task persists between turns until the player stops it. A movement programme cancels it when that command reaches the fleet, so a distant miner continues working during the communication delay.
- A module produces minerals according to that world's geological concentration. Multiple modules add their output.
- The minerals are added to the planet's existing **surface stockpile** (`Planet::minerals`); they do not enter the miner's cargo hold.
- Any friendly cargo fleet co-located with that uncolonized planet may use the normal Cargo Manifest to collect the surface stockpile. Foreign colonies are not valid transfer sources.

This creates the intended two-role loop: an assigned miner stays in orbit and builds a stockpile, while transports collect it on their own schedule. The first slice intentionally has no autonomous hauler programme, remote base, depletion model, or mining on foreign-owned worlds.
