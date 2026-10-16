# Interstellar medium

First three linked slices of #105: persistent broad density profiles, physical
fuel/sensor effects, and delayed player surveys with an optional map overlay.
Free coordinate flight remains available; no lanes, grid, new orders or new
per-fleet controls are introduced.

## World and movement

New galaxies with at least 16 stars generate `min(4, 1 + stars / 160)` broad
elliptical clouds from a separate seed-derived generator. Stellar/planetary RNG
is unaffected. Clouds have stable IDs, centres, radii and peak density, saved as
physical host state. Density falls smoothly as `peak * max(0, 1 - r²)²`.
Overlapping profiles combine as `1 - product(1 - density)`, bounded to 0–1.

The complete travelled segment is integrated, splitting at ellipse crossings
and using deterministic Gauss-Legendre quadrature. Clear endpoints do not hide
a cloud between them. Reversing or subdividing a straight segment preserves its
integrated density within floating-point precision.

For mean density `d` along each movement leg:

| Fitted engine | Consumption distance | Fuel collection |
| --- | --- | --- |
| Conventional | × `(1 + 0.30d)` | Baseline |
| Ram scoop | × `(1 - 0.10d)` | × `(1 + 0.75d)` |

Costs remain mass/cargo/engine dependent. Reference engines round the integrated
effective consumption distance, then retain the existing per-stack whole-mg
billing. Collection is rounded after aggregation. Each year's movement and each
actual leg are billed separately. Mixed fleets apply the appropriate effect to
each fitted engine; future collection cannot finance the earlier consuming part
of a leg. New medium-enabled legacy mixed fleets also separate gross consumption
from collection for this affordability check. Existing uniform-space campaigns
retain the previous legacy accounting.

Actual movement, fuel-limited/free-speed coasting, rendezvous, interceptions and
continued merged routes use the same segment helpers. Engine safe Warp and
overdrive rules are unchanged. Density-dependent safe Warp is later scope.

## Sensors and surveys

Active scanner/radar range is multiplied by `1 - 0.20 * local density`.
Active ship/stellar sight lines additionally use `1 - 0.30 * segment density`:
a cloud can shelter a target as well as reduce its own scout's awareness.
Ordinary jamming/cloaking/tachyon rules still apply. Passive reception and
fast/slow emission interception/communications retain their existing rules;
ionization and channel-specific propagation need a later balance pass.

Active fleets and built colony radars detect broad cloud presence. Distant
reports give a centre quantized to a 20 ly cell, rounded radii, and peak density
0–1. A receiver inside the core (`normalized r² <= 0.5`) characterizes the exact
stationary profile. Reports use the actual receiver's normal communications
route. Dispatched geometry/density never changes when the source or world later
changes. Headquarters knowledge changes only on delivery. Classification cannot
be downgraded; routine observations refresh measurement dates without another
briefing. No science points are awarded merely for mapping a cloud.

**View → Interstellar medium** lists received profiles and observation/receipt
years, age, density bounds and footprint. **Show area** centres on the received
coordinates. **View → Interstellar medium overlay** toggles a subtle layer behind
stars/fleets, independently of the journal. It is remembered between sessions.
Turn Messages → Exploration reports first detection and characterization.

## Navigation knowledge and persistence

Player files contain the public medium-enabled rule and received profiles, never
raw clouds or pending survey reports. Their projection marker makes all density
queries use received evidence. Forecasts discard host cloud truth before any
simulation, so hidden cloud edits cannot change route rows or fuel warnings.

Route simulation uses received profile estimates with the real movement rules.
The table marks ETA/fuel remaining as approximate and shows a conservative
density 0–1 consumption envelope for the **forecast flown segments**, including
fitted engines, cargo changes and annual rounding. Unknown density can change
coasting, actual arrival, future refuelling and therefore the travelled schedule;
the range is not a guarantee for every possible completed itinerary. Direct-leg
fuel summaries also show this uncertainty; use Route forecast for annual costs.
Computer empires reserve the same bounded worst-case gross fuel without reading
host clouds or taking credit for future scooping. Choosing cloud detours is later
scope.

Save **v80**, orders **v27**. The bounded MEDI section retains host profiles,
projection identity, received surveys and in-flight/message evidence. Real v79
layouts load with uniform space and remain uniform on resave; clouds are not
injected into existing routes. Earlier v78/v77 migration fixtures also remove
the actual new bounded section before exercising their historical layout.

#105 stays open for medium-dependent safe Warp/engine balance, cloud detours and
AI route alternatives, ionization/turbulence, communications effects and field
science/advanced characterization. The current scalar model deliberately keeps
these distinct from the delivered fuel and active-sensor rules.
