# UI responsiveness: shared movement preview

## Change-driven workspace refresh — 1 November 2026, #250

The user selected Production Add reliability and late-campaign UI responsiveness
as the first two fresh playtest notes. Baseline main is `787f499`, save v108 /
orders v47. A real mouse press held across the 150 ms production timer loses its
release: `refreshProductionQueue()` transiently disables the enabled Add button,
which cancels Qt's pressed state. A new regression fails on that main code.

Production now refreshes from the existing planning invalidation path after
orders, selection, state replacement and load. Its revision/selected-star/display
key retains the displayed queue and forecast for unchanged input. Kind/template
controls use the already projected availability; changing quantity does not
rebuild the queue. A valid Add button stays enabled through a genuine refresh.

The main context (120 ms), colony (150 ms) and route (180 ms) polling loops also
become change-driven. Context/route notifications coalesce through single-shot
zero-delay callbacks; route draft controls explicitly schedule updates. Mineral
table geometry follows show/resize/font/style events, including the existing
11/14/18-point font acceptance cases, instead of relying on polling to fit rows.
The production forecast algorithm and received-information model are unchanged.

Validation: full GCC14.2/Qt6.12 Debug build; **100/100 CTest passed (35.74 s)**.
The real press/350-ms-hold/release produces exactly one Mines ×10 order both
while idle and when a research order changes the plan during the hold. Draft
edits/idle preserve the existing model index; accepted builds and research
changes update queue/ETA immediately. Loading an older same-turn save restores
its queue/ETA; deferred selection clears production on an unavailable world.
Existing cargo/workforce, PBEM, routes and layout suites also pass.

The manual probe now uses one passive human and seven computer empires, seed
20261027, and `--turns` resolves the ordinary campaign before opening its UI.
Run the identical commands on main and this branch, without concurrent builds
or tests:

```sh
cmake --build work/build --target suns_ui_performance_probe
QT_QPA_PLATFORM=offscreen work/build/tests/suns_ui_performance_probe --systems 288 --turns 0
QT_QPA_PLATFORM=offscreen work/build/tests/suns_ui_performance_probe --systems 288 --turns 69
```

Linux/offscreen, Debug, two-second requested idle window; elapsed wall time is
reported rather than assumed. Turn 1 has 8 fleets; turn 70 has 26.

| Turn | Measurement (ms) | Main before | Change-driven refresh |
| --- | --- | ---: | ---: |
| 1 | Idle CPU / wall | 188.51 / 2100.09 | 5.54 / 1946.57 |
| 1 | Longest heartbeat gap | 28.23 | 20.25 |
| 1 | Scene redraw | 51.94 | 51.90 |
| 1 | Save | 1.76 | 1.98 |
| 70 | Idle CPU / wall | 264.14 / 1968.12 | 8.27 / 1940.29 |
| 70 | Longest heartbeat gap | 44.16 | 21.66 |
| 70 | Scene redraw | 71.17 | 71.94 |
| 70 | Save | 5.43 | 5.35 |

All four saves read back successfully. The late campaign's idle CPU falls by
about 32×; full redraw/save cost is essentially unchanged in these samples.
These are controlled diagnostics, not measurements of the player's unprovided
save or proof that every late-game bottleneck is resolved. The historical
October measurements below used the earlier probe/setup and remain separate.

## Report and confirmed cause — 27 October 2026

The player reported slow map/panel interactions on a Medium map (about 300
systems) and a separate apparent freeze while saving. Main is `6d04c44`, merged
#233, save v98 / orders v40. Branch `codex/ui-preview-performance` is independent
of the pending orbital-retirement PR #234 and does not change save formats.

`movementPhasePreviewState()` uses the full TurnProcessor on a disposable world
to preserve existing logistics/fuel-generation semantics. `updateControls()` and
`selectedFleetPlanningView()` independently recomputed that identical preview.
The dashboard, readability and base controls call the latter from refresh
callbacks, including the command-context timer every 120 ms. A turn costing
roughly 70–80 ms on a fresh 288-system campaign was therefore repeatedly run
even with no input or changes. This saturated the UI thread during idle and
also ran inside modal Qt event loops.

The fix shares one preview per MainWindow planning revision, player and turn.
`rebuildScene()` invalidates that revision for order edits, state/telemetry
changes, load and new campaigns. Merely refreshing a panel or changing an
unsubmitted control does not run the simulation again. The actual projection
algorithm, received-knowledge rules, fuel generation and order validation are
unchanged. The cache is per window and retains at most one world projection.

Regression checks exercise repeated panel reads, real same-size colonist-order
replacement, clearing orders and loading a different file with the same turn
and fleet IDs. They verify physical cargo stays unchanged and forecasts refresh
after edits/load rather than reusing the preceding session.

## Reproducible timing probe

With app/tests enabled, build the optional manual target:

```sh
cmake --build build --target suns_ui_performance_probe
QT_QPA_PLATFORM=offscreen build/tests/suns_ui_performance_probe --systems 288
QT_QPA_PLATFORM=offscreen build/tests/suns_ui_performance_probe --systems 1000
```

It installs the production workspace, creates a fresh seed-20261027 campaign
with eight human empires, redraws, pumps pending updates, measures a two-second
idle event loop with a 20 ms heartbeat, then saves to a temporary directory and
loads that file. `idle-ms` changes the requested measurement window. Wall time
may exceed the target when callbacks block; idle CPU time measures process CPU,
not total machine utilization. This target is excluded from the default build
and CTest: timings have no portable pass/fail threshold.

Measured locally with GCC 14.2 / Qt 6.12.0 Debug, Linux/offscreen, no concurrent
build or test workload. The control links main's original `main_window.cpp`
with the same remaining UI/core libraries; the changed version uses the shared
preview. These are fresh-campaign diagnostics, not a guarantee for every save.

| Systems | Measurement | Before | Shared preview |
| --- | --- | ---: | ---: |
| 288 | Scene/panel rebuild, ms | 186.3 | 124.4 |
| 288 | Idle CPU / wall, ms | 2308.9 / 2309.0 | 85.1 / 1945.1 |
| 288 | Longest heartbeat gap, ms | 316.7 | 21.5 |
| 288 | Save, ms | 1.46 | 1.48 |
| 1000 | Scene/panel rebuild, ms | 1431.1 | 1066.3 |
| 1000 | Idle CPU / wall, ms | 3571.3 / 3571.4 | 630.5 / 2111.2 |
| 1000 | Longest heartbeat gap, ms | 1169.8 | 52.7 |
| 1000 | Save, ms | 12.61 | 8.86 |

All four files loaded successfully. On Medium this changes idle duty from
roughly one full core to 4.4%; a full scene reconstruction still takes time.
At 1000 systems, other repeated panel/map work remains a measurable cost.
Future work can narrow refresh invalidation or introduce a dedicated movement
phase projection; it must preserve existing received evidence and order rules.

## Save-freeze investigation boundary

The actual reported save freeze has **not** been reproduced. In the measured
fresh campaigns, direct `saveGameToPath()` and read-back complete normally.
This does not validate a native file chooser, network filesystem or a specific
long-running user campaign. OS/build provenance and whether the freeze occurred
before the chooser appeared or after accepting a filename are still pending.
Do not describe the cache correction as a proven fix for that separate report.

## Full-audit follow-up

The next correction branch also fixes the independent route-warning hotspot:
all route types use received state, and chart-only systems without a received
planet are omitted from the disposable navigation world. On the same 288-system
W1 candidate, cold warning CPU changes from1379ms to32ms (W5:270ms to8ms).
Received planets/stations, sent commands and their timing remain in the model.
See the complete audit for the network/probe measurements and regression map.
