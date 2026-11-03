# Order editing and background forecast performance

**Historical checkpoint:** this describes the first #269 revision, 659fd9b.
The user subsequently chose minimal realtime estimates on 3 November 2026.
The revised #269 removes the detailed simulations and background workers below;
see [the current contract](minimal-ui-estimates.md). Earlier measurements remain
here as historical evidence, not the behavior of the latest head.

This continues the selection-only correction in #266. The user supplied a
288-system year-89 campaign where real Factory Add clicks still took about
8.2–8.5 seconds. The expensive path ran a disposable annual simulation, rebuilt
the map and refreshed unrelated/hidden tools; an active fleet route also caused
up to 96 years of necessary ETA simulation on the GUI thread.

## Dependency boundaries

| Input change | Immediate projections and map | Annual route estimates |
| --- | --- | --- |
| Star selection | Reuse physical state/map items | Reuse unchanged route; candidate warning tracks destination/action |
| Production/research/template queue edit | Reuse fleet/cargo/movement input; update panels | Invalidate: future supplies, stations and research may matter |
| Flight/waypoint edit | Reproject the programme/map; reuse unchanged logistics-only movement input | Invalidate |
| Cargo/refuel/split/merge/rename | Recompute the affected immediate projection | Invalidate |
| World replacement/load/year/player change | Invalidate, including same-year reused IDs | Stop old work and invalidate |

The same movement-order predicate filters the preview's actual input and decides
whether an appended order can reuse it. The real processor retains its original
fuel-generation, service and order semantics.
Unknown order types stay on the conservative rebuild path. Production queues,
received panels and annual route results retain their own broader invalidation.

## Background route presentation

Automatic route rows and waypoint warnings compute from owned immutable received
snapshots, not live host/world or QObject data. Work is bounded per window/kind,
new requests replace pending ones, and obsolete work checks cancellation between
years. Only the GUI thread publishes a result matching current player, revision,
fleet and complete route/action inputs. Pending estimates are labelled pending;
old estimates are not silently presented as current. Existing synchronous query
APIs remain available for deterministic callers and tests. Closing/replacing the
session cancels and joins work safely. Table/summary share a completed forecast.

## Other redundant work removed

- Automatic Research updates skip a hidden tool; show/reopen and explicit edits
  refresh it. Explicit hidden test/API calls remain immediate.
- Intel/Computer decisions use coalesced events rather than 500-ms polling;
  selection, filtering, Copy and session/player changes remain current.
- Supplied optional engineering support and first-turn mining no longer evaluate
  expensive unused fallbacks. Explicit false/zero values retain their meaning.
- A delay query directly inside a populated capital radar field uses its known
  zero propagation plus the same receiver jamming. Full physical route and
  transmitter/receiver identities are unchanged.
- CommunicationQueryScope amortizes graph queries only inside one pure immutable
  make_player_view snapshot. Exact state address/player/year keys, copied results,
  nested/thread-local scopes and destruction prevent reuse across mutation or
  other worlds. It never wraps a mutable turn processor.

## Reproducible diagnostic

The manual, EXCLUDE_FROM_ALL target suns_ui_performance_probe accepts --save PATH
or generated --systems/--turns cases. It loads a temporary copy, uses isolated
settings and measures foreground/event-loop latency separately from completion
of legitimate background estimates. CPU/heartbeat idle measurements run after
background work settles. Save/readback goes to temporary files; no turn is
advanced for a supplied campaign. Private saves are never committed.

Timing figures and validation are recorded in project-status/PR. There are no
machine-dependent performance pass thresholds in CTest. Gameplay, paid resources,
received knowledge, fair AI and save/orders formats remain unchanged.

## Verified year-89 measurements — 3 November 2026

GCC14.2, Qt6.12.0, Debug/offscreen, the same supplied 288-system/37-fleet save,
without concurrent compilers or tests. Baseline is merged main `79d22c2` (#266).
The same diagnostic source links either the preserved baseline libraries/header
or the current implementation; `SUNS_LEGACY_UI_PROBE` only disables the new
background-wait diagnostic when compiling against the baseline header.

The original selected fleet has an active route. Three real Factory Add clicks
in that unchanged selection scenario took **8477 / 8216 / 8470 ms before** and
**214 / 190 / 198 ms after**, about 40× faster. The broader probe waits for
background estimates to settle between actions and reports these foreground
times (including its explicit 20-ms event drain):

| Action | Before, ms | After, ms |
| --- | ---: | ---: |
| Load temporary campaign copy | 8566 | 685 |
| Factory Add, original active-route fleet | 8046 | 219 |
| Select next star | 703 | 308 |
| Select another fleet | 1813 | 453 |
| Factory Add after changing fleet | 809 | 184 |
| Open Research | 64 | 59 |
| Change research allocation | 808 | 207 |
| Save temporary file | 28 | 28 |
| Read temporary file back | 28 | 27 |

The legitimate active-route forecast still takes about **6.1 seconds** in the
background after Factory Add; this is not a claim that its simulation became
40× faster. During that wait the longest measured event-loop heartbeat gap was
**21.4 ms** (54 ms maximum across the other forecast waits). Settled idle CPU was
**76.8 → 7.2 ms per 2.1 seconds**, heartbeat **49.0 → 20.4 ms**. Final selected
fleet had no route, so the diagnostic correctly skipped its explicit route-read
timings rather than presenting them as a completed route benchmark. No supplied
turn was advanced; temporary save/readback preserved systems, turn and orders.

Full application/tests build succeeded; **111/111 CTest passed (37.95 s)**.
New regressions cover dependency invalidation/map item retention, hidden Research,
event-driven tools, scoped communication route/export equivalence and concurrent
snapshots, async/sync forecast equality with delayed sent commands, latest-only
rapid edits, obsolete cache-hit cancellation and load/stop/close/destruction.
Existing route editor checks await actual async table updates and retain exact
ETA/fuel/action/color expectations. These are author measurements on one private
campaign and executor, not an independently reproduced universal latency bound.
