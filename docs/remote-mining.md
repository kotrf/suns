# Remote mining

Remote mining lets an empire exploit an uncolonized planet without turning it into a colony. It is deliberately a logistics mechanic, not a fourth ordinary mineral economy.

## Equipment and research

New empires use the six [Stars! mining robots](stars-mining.md) and the reference
mining hulls. An ordinary empire's first combination is **Mini-Miner** plus
**Robo-Mini-Miner**, requiring Construction 2 and Electronics 1. Advanced Remote
Mining grants access to the Midget Miner hull and Robo-Midget Miner at level 0,
and later to the lighter Robo-Ultra-Miner. The existing Basic Remote Mining trait
blocks the reference mining hulls; its component restrictions are also enforced.

Mining equipment fits dedicated `Mining` cells. Scout, freighter and ordinary
general-purpose banks do not accept it. Every equipment bank contains one model,
while separate mining banks may carry different robots.

Reference robots weigh 80 or 240 kt. Their mass affects fuel and travel just like
other equipment, so a conventional-engine miner may need an accompanying tanker
or freighter for the journey. Research unlocks new equipment; existing ships keep
their fitted models and are never upgraded automatically.

Older designs retain the 120 kt prototype Remote Miner hull and its 80 kt Remote
Mining Module, producing 1.25 kt per mineral/year at concentration 100. That
prototype is shown only to empires with legacy hulls or an existing prototype
module design. It does not bypass the new research progression.

## Operations

- Already in orbit: select **Remote Mining** and press **Start mining here**. This queues the native mining task without a new waypoint. It is transmitted on End Turn, respects ordinary communication delay, and produces in the first annual mining phase after delivery. Emissions stay unchanged. The route summary distinguishes queued work, a command in flight and the received current task.
- Before travelling: assign **Remote Mining** to the final route waypoint, using Add waypoint or editing that leg's arrival action. The action selector alone is a draft. Merely entering orbit with `No Task` does not start extraction.
- Arrival orders may attempt an unexplored system, but reject a reported colony. The host checks actual world ownership when the order executes; stale or missing knowledge never exposes hidden ownership through UI admission. Starting work at the current position requires a reported unowned world there.
- It is a persistent terminal task: it must be the last item in a travel route, begins producing on the turn after arrival and continues until a replacement route or `No Task` command reaches the fleet. An arrival year does not also grant a full year of extraction.
- A robot produces its rated output multiplied by each mineral's concentration / 100. Modules add their individual rates; fleet stacks multiply output by ship count. Prediction, actual extraction, dashboards and history use the same calculation.
- The minerals are added to the planet's existing **surface stockpile** (`Planet::minerals`); they do not enter the miner's cargo hold.
- Any friendly cargo fleet co-located with that uncolonized planet may use Cargo Transfer to collect the surface stockpile. The planet and fleet dashboards show the stock and active I/B/G extraction per turn. Foreign colonies are not valid transfer sources.

This creates the intended two-role loop: an assigned miner stays in orbit and builds a surface stockpile, while transports can collect it with explicit dynamic cargo waypoints and a repeating route. Ordinary I/B/G ore still has no depletion model; conditional hauler programs and mining on foreign-owned worlds remain outside this slice. [Xenocrystal](rare-exotic-materials.md) adds separate finite deposits, DeepSurvey/technology gates and a bounded whole-unit extraction path using the same remote-mining task.
