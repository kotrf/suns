# Galaxy map visual direction

The map should remain information-first: attractive enough to make exploration feel good, but never so decorative that strategic state becomes harder to read.

## Current visual layers

From back to front:

1. dark space background with a restrained deterministic star field;
2. sensor-range overlays;
3. active and pending movement routes;
4. stellar glow and star core;
5. colony / unknown indicators, occupied-orbit rings and selection brackets;
6. fleet markers;
7. text labels.

Star colour comes from a physical `StarClass` property in the core model. Survey state, colonies, selection, routes and overlays are presentation concerns in the Qt client, while the ranges themselves come from simulation data.

## Sensor-range overlay

Sensor circles are now a real game mechanic rather than decoration:

- colonies provide a powerful 150 ly stationary survey and communications range;
- the starting Scout provides a smaller 50 ly mobile survey range;
- stars keep a permanent system contact when they enter ordinary friendly coverage;
- a moving scout sweeps its detection circle continuously across the segment travelled during a turn, so close fly-bys record system contacts without revealing planetary parameters;
- a later-tech penetrating scanner has its own shorter field and can produce a rough planetary estimate during the same fly-by;
- the Qt map renders colony ranges in green and scout ranges in blue below routes, stars and fleets;
- `Show sensor ranges` toggles the overlay without changing simulation state;
- system contacts and surveyed planetary knowledge remain known after the sensor source moves away.

Future enemy fleet detection should be modeled separately as transient contacts. Permanent survey knowledge and current sensor contacts are different concepts and should not be collapsed into one flag.

## Ship design connection

Fleet scanner range comes from installed components on every ship design, alongside engines, fuel and special modules. A ship's strategic role emerges from its fit rather than from a permanently hard-coded class. The colony field remains stationary infrastructure: it exceeds the starting Long Range Scanner, while a heavy Electronics 2 Extended Range Scanner can narrowly exceed it from a mobile hull.

Later overlays such as weapon range, fuel range and territorial influence should follow the same layer pattern: an overlay must explain a strategic constraint, not merely add visual noise.

## Compact markers and occupied orbits

Stars, fleet markers, wormhole markers and their labels use screen-sized geometry.
Zoom changes the distance between locations rather than enlarging the symbols
and text. Normal stellar cores are about 6 pixels across; population mode can
still vary core size. The glow and selection brackets are restrained. Sensor
range radii remain in galaxy coordinates, with a constant-width screen pen.

All fleets at a system's last reported position share one orbit ring: white for
your fleets, red for other empires, purple for both. This follows the original
[Stars! player guide](https://www.bestoldgames.net/download/games/stars/stars-win-manual.pdf)
(17-2 and glossary GL-4). A left click on the core selects the system; a left
click on the annulus opens the fleet list. Right-clicking either routes the
current source fleet to that system. Map-target mode uses the ring's list to
choose an individual fleet while retaining the source fleet.

Deep-space fleets are drawn at their reported coordinates, without artificial
vertical offsets. Co-located fleets share a small triangle and a picker; a
right click on that group chooses the fleet to use as the quick-order target.
Fleet names appear for selected or hovered deep-space markers, with full names
and additional details in tooltips. Routes start at reported coordinates.

System labels contain names only. Unknown status, colony details and stellar
variability stay in tooltips and panels. Labels try four nearby positions and
are hidden when they would overlap another label or a marker. Selection has
highest placement priority, followed by your colonies and surveyed systems.
Label placement refreshes after zoom, scrolling, resizing, selection, fleet
hover and map mode changes. Travel annotations are limited to the selected fleet.
