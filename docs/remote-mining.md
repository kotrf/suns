# Remote mining

Remote mining lets an empire exploit an uncolonized planet without turning it into a colony. It is deliberately a logistics mechanic, not a fourth ordinary mineral economy.

## First slice

- `Construction 1` unlocks the **Remote Mining Module**.
- A friendly fleet carrying one or more modules mines automatically while it is positioned at an uncolonized planet.
- A module produces minerals according to that world's geological concentration. Multiple modules add their output.
- The minerals are added to the planet's existing **surface stockpile** (`Planet::minerals`); they do not enter the miner's cargo hold.
- Any friendly cargo fleet co-located with that uncolonized planet may use the normal Cargo Manifest to collect the surface stockpile. Foreign colonies are not valid transfer sources.

This creates the intended two-role loop: a miner stays in orbit and builds a stockpile, while transports collect it on their own schedule. The first slice intentionally has no autonomous hauler programme, remote base, depletion model, or mining on foreign-owned worlds.
