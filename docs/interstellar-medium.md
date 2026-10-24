# Interstellar medium

Playable navigation foundation of #105: persistent broad density profiles,
physical fuel/sensor/hull effects, delayed surveys, an optional map overlay and
received-profile route alternatives for players and computer empires.
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
continued merged routes use the same segment helpers.

### Safe Warp and hull risk

Density shifts the effective safe Warp continuously by at most one tier: down
for conventional engines and up for scoops. This is engine/field tolerance,
not mechanical drag. It does not change the ordered speed or silently accelerate
ships. Let `R(w)` be the fitted engine's existing damage percentage per full
movement year at Warp `w`. At local density `d`:

- Conventional: `(1-d) * R(w) + d * R(w+1)`.
- Scoop: `(1-d) * R(w) + d * R(w-1)`.

Warp 0 has zero rate. For conventional Warp 10, the virtual next tier is
`min(100, R(10)+18)` percentage points/year. Zero density preserves the engine's
existing curve; maximum density shifts it one tier. Mixed fleets and multiple
engines in a hull use the maximum **local** rate of every fitted engine. That
maximum is integrated over the actual flown segment and multiplied by its time
at the requested Warp. Adaptive quadrature refines engine-curve crossings inside
each cloud-support interval, so straight-route subdivision preserves damage.

Paid movement and free-speed coasting both obey this hull budget. At 100% damage
the fleet stops at the computed point; fuel is billed only to that point. The
existing immobilization and repair rules remain. Uniform-space campaigns keep
the previous overdrive rules.

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
**Compare routes…** in Add waypoint compares a direct leg and up to two coarse
bypasses per received cloud (at most nine candidates). It starts at the final
static waypoint of the current programme, or the reported fleet position for a
new programme. Distance, gross fuel, movement time and potential hull damage are
coarse estimates for the appended portion: command delay, annual rounding,
refuelling and earlier legs are excluded. Use **Route forecast** afterwards for
the full programme. Choosing a bypass appends ordinary coordinate waypoints;
arrival work stays at the original final destination. Cancel leaves orders
unchanged. Moving fleet targets retain the existing rendezvous editor.

The Warp selector shows the base engine rating and **local received-profile**
hull risk; a clear starting point does not prove the whole route safe. Route
forecast applies the shared integrated damage rules and warns on immobilization.
All geometry and risk estimates remain independent of hidden cloud edits.

Computer empires evaluate the same bounded candidates while selecting mission
Warp, reject expected hull damage and reserve conservative worst-case gross
fuel across the route, without advance credit for future scooping. They prefer
short travel with a modest fuel-budget penalty; stable candidate order breaks
ties. Chosen bypasses become ordinary delayed MoveFleetOrder waypoints, preserve
survey/colonization/return actions and existing acknowledgement-based goals.
At the existing review boundary, fresh telemetry and received hazards can revise
an acknowledged programme with a bypass even when its Warp stays unchanged.
Warp selection and route materialization share the host's initial automatic
orbital-depot service: a received own working depot at the confirmed starting
position can fill the budgeting tank before orders, including an active loading
programme. Active programmes require fresh telemetry at that position; the
existing acknowledged-idle treatment remains. Hidden stations, modules without
refuelling, stale active positions and depots at later waypoints give no initial
credit. Physical host service still performs the real fill.

Route materialization carries paid cargo changes into its budgeting clone.
After an arrival **Load All**, it reserves a full shared hold for every remaining
leg: future stocks may grow, and a later unload may fail or be partial. This
conservative ballast never enters physical state or orders. No later leg is
priced as an empty transport merely because a previous waypoint loads cargo.
If the programme no longer fits its fuel reserve, the planner retries the whole
itinerary at lower Warp caps, preserving arrival actions. If no funded safe
programme exists, it emits an ordinary delayed stop and retains acknowledgement
handling instead of issuing an underfunded fallback. Regression scenarios execute
both orbital-passenger evacuation and mineral loading through real movement and
unloading, with no extra refuelling.
This is a bounded navigation heuristic, not a global route optimizer or a new supply programme.

MEDI was introduced in save **v80**, orders **v27**; this completion keeps current
**save v95 / orders v38** unchanged. The bounded MEDI section retains host profiles,
projection identity, received surveys and in-flight/message evidence. Real v79
layouts load with uniform space and remain uniform on resave; clouds are not
injected into existing routes. Earlier v78/v77 migration fixtures also remove
the actual new bounded section before exercising their historical layout.

The agreed navigation scope now includes density-dependent hull risk, explicit
coarse route choice and basic AI bypasses. Optional ionization/turbulence,
communications effects and field science/advanced characterization remain
separate future extensions. This work does not expand frozen #63 or introduce
new scientific rewards. Initial one-tier balance needs playtesting; automated
coverage verifies physical accounting, determinism and knowledge boundaries.
