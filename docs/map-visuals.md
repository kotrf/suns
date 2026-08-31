# Galaxy map visual direction

The map should remain information-first: attractive enough to make exploration feel good, but never so decorative that strategic state becomes harder to read.

## Current visual layers

From back to front:

1. dark space background with a restrained deterministic star field;
2. sensor-range overlays;
3. active and pending movement routes;
4. stellar glow and star core;
5. colony / unknown / selection rings;
6. fleet markers;
7. text labels.

Star colour comes from a physical `StarClass` property in the core model. Survey state, colonies, selection, routes and overlays are presentation concerns in the Qt client, while the ranges themselves come from simulation data.

## Sensor-range overlay

Sensor circles are now a real game mechanic rather than decoration:

- colonies provide a powerful 150 ly stationary survey and communications range;
- the starting Scout provides a smaller 90 ly mobile survey range;
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
