# UI responsiveness: shared movement preview

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
