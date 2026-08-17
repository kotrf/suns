# Galaxy map visual direction

The map should remain information-first: attractive enough to make exploration feel good, but never so decorative that strategic state becomes harder to read.

## Current visual layers

From back to front:

1. dark space background with a restrained deterministic star field;
2. pending movement routes;
3. stellar glow and star core;
4. colony / unknown / selection rings;
5. fleet markers;
6. text labels.

Star colour comes from a physical `StarClass` property in the core model. Survey state, colonies, selection and movement routes remain presentation concerns in the Qt client.

## Sensor-range overlay (planned)

Stars!-style sensor and radar circles should be implemented as a separate map-overlay layer rather than baked into star or fleet rendering.

Planned behaviour:

- circles render below routes, stars and fleets;
- multiple sensor sources may overlap without obscuring the map;
- a UI toggle controls whether sensor ranges are visible;
- ranges come from real simulation data once sensors/scanners become a game mechanic;
- no placeholder range values should be introduced merely for decoration;
- later overlays (weapon range, fuel range, ownership influence, etc.) should follow the same layer pattern.

The important rule is that an overlay must explain a strategic constraint, not merely add visual noise.
