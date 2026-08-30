# Remote mining

Remote mining lets an empire exploit an uncolonized planet without turning it into a colony. It is deliberately a logistics mechanic, not a fourth ordinary mineral economy.

## First slice

- `Construction 1` unlocks both the **Remote Miner** hull and the **Remote Mining Module**.
- Mining equipment fits only the hull's two dedicated `Mining` slots. Scout and transport hulls have no such slots, so they cannot be turned into miners by adding a general-purpose module.
- The hull weighs 120 kt and each module another 80 kt. Even a minimal fitted miner is therefore far heavier and more fuel-hungry than a scout using the same engine and Warp factor.
- **Remote Mining** is assigned to a route waypoint as its arrival task. Merely entering orbit with `No Task` does not start extraction.
- It is a persistent terminal task: it must be the last item in the route, begins producing on the turn after arrival and continues until a replacement route or `No Task` command reaches the fleet.
- A module produces minerals according to that world's geological concentration. Multiple modules add their output.
- The minerals are added to the planet's existing **surface stockpile** (`Planet::minerals`); they do not enter the miner's cargo hold.
- Any friendly cargo fleet co-located with that uncolonized planet may use Cargo Transfer to collect the surface stockpile. The planet and fleet dashboards show the stock and active I/B/G extraction per turn. Foreign colonies are not valid transfer sources.

This creates the intended two-role loop: an assigned miner stays in orbit and builds a surface stockpile, while transports can collect it with explicit dynamic cargo waypoints and a repeating route. The current slice still has no conditional hauler programme, remote base, depletion model, or mining on foreign-owned worlds.
