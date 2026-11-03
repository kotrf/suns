# Minimal realtime estimates

On 3 November 2026 the user explicitly chose responsiveness over detailed live
forecasts: retain cheap population growth and rough distance/speed travel times;
resolve the actual world during turn generation. Even background annual
simulations are outside the normal interface contract.

## Budget and boundaries

An estimate makes a bounded pass over current received information, queue rows
or waypoints. Its cost must not grow with future years or units in a batch.
Ordinary clicks, selection, window refresh and route summaries never invoke
TurnProcessor, annual production forecasting or future fleet-service simulation. Exact costs,
requirements, ownership and immediate accepted Cargo/Split/Merge/Refuel orders
still use shared current-order validation. Opening a tool does not advance time.

- Population: current accepted population and current growth percentage.
- Research: current RP/output/allocation and known plan, without future years.
- Current output/growth can include the bounded habitat-upkeep calculation on
  one local planet copy. This derives today's net annual rate/reserve condition;
  it does not run a full world turn or a sequence of future years.
- Routes: cumulative ceil(distance / Warp²), marked ≈. Command delivery, fuel,
  hazards, refuelling and future actions are excluded. Moving targets use the
  position estimated from their received track, not hidden coordinates. After any conditional
  arrival, gate, wormhole or moving target, later ETA is unknown. A gate's own
  number is only the approach to its source; it does not promise a hop.
- Production: cumulative remaining work divided by current production after
  research/upkeep, marked approximate. Known mineral bills are reserved once.
  No future growth, factories, mining, research or supply deliveries are assumed.
  Annual rules, changing-condition projects and an uncertain/blocked prefix show
  no completion date. Their current progress, costs and requirements remain.
  Rows the current resolver would skip (unavailable/capped defenses, redundant
  radar, invalid station work) consume no work/mineral budget and do not make a
  known prefix uncertain. Defense batches count only currently buildable units.
  A skipped row cannot restore an already uncertain prefix; missing minerals or
  shipyards still block estimates. Idle capped annual defenses remain a rule.
- Current fuel, local overdrive risk and direct-leg algebraic estimates remain
  current-state facts/rough estimates; there is no table of simulated future
  fuel remaining or predicted shortage years.
- Engineering, resonance labs, templates and mobile bases show current work,
  stocks, rules and eligibility, without simulated future completion/service.

The old 96-year route calculation, worker manager, pending forecast states and
Fuel/Left columns are removed. Current manifests share the existing order
boundary projection; the discarded full-turn movement preview is removed too.
The real turn processor retains all fuel, cargo, population, production,
communication, risk and service rules. Save/orders formats do not change.

## Verification

Regressions distinguish approximate UI values from actual core turn outcomes.
They cover cumulative geometry beyond the former horizon, conditional unknowns,
received/sent command precedence, independence from hidden/future fuel and
economy, same-year world replacement, current-turn manifest validation, static
production rates, blocked prefixes, large batches and date overflow. Existing
real-turn physics, logistics, knowledge and serialization assertions remain.

The manual suns_ui_performance_probe loads a temporary campaign copy with
isolated settings. It reports synchronous/queued action latency and settled idle
CPU; no supplied turn is advanced or overwritten. SUNS_DETAILED_UI_PROBE exists
only to compare the probe against preserved pre-change libraries, not in the
application. Measurements and the current validation result are recorded in
project-status and PR #269. Private saves and profiling artifacts stay untracked.

On the supplied year-89 campaign (288 systems, 37 fleets), the revised route
summary takes **0.050 ms** synchronously. The previous detailed revision 659fd9b
needed **6.594 seconds of background calculation** after an original-fleet
Factory Add; the minimal version has no background calculation. These are
different estimates, deliberately trading prediction detail for response time.

Foreground Factory latency remains around **0.2 seconds** in Debug/offscreen:
paired diagnostic **209 → 215 ms**, including queued GUI events. This policy
change does not claim another foreground speedup over 659fd9b. Repeated real
clicks in the minimal version measured **184 / 260 / 245 ms**; original main
79d22c2 measured **8477 / 8216 / 8470 ms** before the broader #269 optimizations.
Settled idle CPU was 5.4 ms / 2.1 seconds, heartbeat 20.4 ms. Both comparison
modes get the same 500-ms layout settling floor; historical background waits
are reported separately. Temporary save/readback passed; the original was not
advanced or overwritten. These are author measurements on this executor.

Full GCC14.2/Qt6.12 Debug build and **112/112 offscreen CTest passed (39.76 s)**.
