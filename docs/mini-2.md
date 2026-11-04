# Mini-2

The user approved implementation of all four accumulated notes on 4 November
2026. Tracking: [#273](https://github.com/kotrf/suns/issues/273). This slice changes
presentation and platform assets, not game balance, turn rules or save/orders.
They were merged in [#274](https://github.com/kotrf/suns/pull/274). The user then
authorized completing the added notes 5–6; tracking [#275](https://github.com/kotrf/suns/issues/275).

## Map and station details

The map draws the selected route without numbered waypoint labels. Its thin
mouse-transparent trajectory and coordinate-picking marker remain. The route
table still numbers its rows for editing.

Selected system uses a shared header for the planet portrait and a 28×28
icon-only station button (24×24 icon), aligned at the upper right. It needs no
extra station row. The tooltip retains the station/hull names, and clicking
opens the same received fitting. Hidden refits, losses and queued construction
cannot replace the received station snapshot.

## Approximate fuel per leg

`Fuel ≈` reports the nominal cost in mg for each straight-line leg at that leg's
Warp, using current received/planned ships and cargo and the existing algebraic
engine curve. Accepted current cargo transfers update it immediately. It does
not forecast fuel remaining, shortages, future fuel regeneration or scooping,
future cargo, refuelling, or clouds. Real annual rounding can differ.

The calculation makes a bounded pass over route rows and fitted stacks. It has
no turn/year loop, workers or horizon. ETA remains cumulative distance/Warp²;
empty tanks or an expensive leg do not turn that geometric estimate into a
promise of physically achievable arrival.

A received moving target supplies only an approximate first approach. After a
moving target or an arrival action that can change load, ships or position,
later fuel is unknown. Refuel alone preserves nominal burn estimates for later
known geometry, while their completion dates remain unknown. Gates/wormholes
show the approach burn only; their uncertain exit cannot produce later costs.

## Application icon and installation

The original [SVG](../src/app/resources/icons/suns.svg) depicts a gold sun and
blue orbit on a dark background. Committed PNGs cover 16, 24, 32, 48, 64, 128 and
256 pixels; the same seven sizes form `suns.ico`. No image tools or SVG plugin
are required to build or run the app: Qt uses embedded PNG resources, including
when the UI is linked as a static library. Application/window icons are set
explicitly without changing QSettings identity.

Windows enables RC and compiles the ICO into the executable through a configured
resource file. Qt also supplies the window/taskbar icon. The RC object depends
on the ICO, so edits rebuild it. This environment validates the resource with
MinGW windres and a resource-only Windows PE link; it does not run a complete
Windows Qt application.

On Linux, CMake installs the executable, `suns.desktop`, scalable SVG and seven
hicolor PNG sizes. `Icon=suns`, `StartupWMClass=suns` and Qt's desktop-file name
agree. Custom Qt library locations are retained in the install runpath; Qt
runtime deployment itself remains the platform packager's responsibility.

```sh
cmake --build build
cmake --install build --prefix "$HOME/.local"
```

Icon regeneration is a developer-only operation using an SVG rasterizer such as
`rsvg-convert`, followed by ImageMagick for the ICO container. Rasterize the SVG
at each listed size, then combine the PNGs in increasing size order:

```sh
magick suns-16.png suns-24.png suns-32.png suns-48.png suns-64.png suns-128.png suns-256.png suns.ico
```

## Received planetary radar (item 5)

System Details now shows the received planetary radar tier (None/Local/Full)
and nominal 0/30/150 ly range. The evidence comes from the owner-only received
infrastructure snapshot, including its received construction queue; a newer
hidden host installation cannot replace it. A foreign or unreported receiver is
unknown rather than a confirmed absent radar. The intelligence age above the
summary applies to the infrastructure report. These are nominal receiver ranges,
not a claim that interference-adjusted sensor coverage is currently unobstructed.

## Surface mineral bars (item 6)

The three rows share a clickable kt scale (10, 50, 100, 500, 1000, 5000, 10000,
50000; initially 1000). Clicking a row/the scale button or pressing Space/Return
on a focused row cycles the same scale for all three. Light fill represents
surface stock, followed by a darker extension for current mining per turn. A
white diamond independently maps concentration onto 0–100%; changing the kt
scale never changes it. Stocks beyond the selected scale show an overflow mark.
Exact stock/mining/concentration values and stock observation age are available
in each tooltip; the age also remains visible below the rows. A missing stock
report is unknown; a received zero is displayed as a known empty stock. Stock
and concentration can be known independently.

The Minerals table uses the same knowledge-aware data. Uncolonized worlds show
the latest received surface stock instead of unconditional dashes. Stock evidence
is frozen in an optional independently dated survey field, captured on site and
published again after mining/cargo at the annual boundary on previously orbitally
surveyed worlds (or alongside a pending real orbital survey), including by fleets
without a scanner. Stock measurement does not grant a planetary survey to a
scannerless colonizer. A same-year final observation supersedes an arrival scan;
late older or higher-quality packets cannot roll stocks back. Foreign colony
stocks are never captured. Passive/silent observers retain their reports in the
existing outbox until transmission, and repeated held observations coalesce.
Ordinary mineral observations do not discover xenocrystal deposits.

Displayed known stock includes accepted current Cargo changes. First-year colony extraction
uses the received pre-Cargo workforce, matching actual mining phase order. For
uncolonized worlds, the estimate sums the scalar extraction of the player's
received/accepted remote-mining stacks currently at that orbit; unknown geology
has no extraction estimate. Hidden mining fleets, future travel/commands, cargo,
production and population are not simulated. The two affected panels reuse
existing event-invalidated planning/player-view caches, with no polling, workers,
TurnProcessor calls or year loops in their refresh paths.

Save v110 appends a bounded SMIN evidence extension; readers retain compatibility
with v12–109, with absent legacy uncolonized stock evidence remaining unknown
until a new report arrives. Existing legacy owner habitat evidence still provides
its recorded supplies. Player packets carry received stock evidence but no
in-flight reports. Orders remain v48; real mining/balance and communication rules
are unchanged.

## Validation

Existing route/mining tests require geometry without map numbers. Station tests
check the shared header's height and non-overlapping icon geometry, and retain
real click/fitting and delayed-knowledge coverage. Fuel tests compare a simple
leg with actual turn fuel billing, distinguish whole-leg approximation from
annual rounding, exercise Warp and accepted cargo changes through the visible
table, and verify conditional unknowns and hidden-manifest invariance.

Startup smoke checks the embedded application and window icons at all seven
sizes. Linux staged installation, installed-binary smoke without SDK activation,
desktop-file validation and Windows resource compilation are checked separately.
Local synthetic measurements: five route legs take **0.1027 ms** per row query,
1000 legs **19.2521 ms**, in Debug during a build. These are warm current-state
query timings, not complete click/painting benchmarks; the estimator scales with
waypoints rather than future years. Full build/CTest and current PR/CI evidence
are recorded in [project status](project-status.md) and GitHub.

Item 5–6 regression coverage includes received Local/None versus unknown radar,
hidden installation changes, post-mining publication without a scanner, frozen
and out-of-order stock reports, Passive hold/coalescing/transmission, known zero
versus unknown, current mining stack estimates, shared scale and keyboard control,
rendering, host/player serialization, v109 migration and corrupt/truncated evidence.
