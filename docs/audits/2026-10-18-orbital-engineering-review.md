# Orbital engineering review follow-up — 18 October 2026

Source: [Codex review of PR #219](https://github.com/kotrf/suns/pull/219#pullrequestreview-5564690680),
reviewed head `0d345ae5f1346a9810b2b16eaf027b65f9c12c03`,
main `5049da0c39cc14bc0d454892e5ccda673f705383`.

## Two forecast regressions introduced by #219

1. **P2, newly planned project.** Production supplied a projected queue and
   state with an unprojected planet. The resolver discarded the engineering
   token because that planet had no physical project yet. At turn 10, a fully
   supplied start showed beyond forecast in Production but year 40 in the
   engineering panel. Production now forecasts the matching projected planet.
2. **P2, pending research settings.** The engineering panel omitted pending
   research allocation and plan orders. Shared `planned_production_state` now
   replays valid allocation, active/focus and queued-focus settings in order,
   with the same acceptance rules as End Turn. Both panels use that projection;
   the Production window's separate research override is removed.

Dedicated Qt regression scenarios exercise Start and both allocation directions
(0% to 100%, 100% to 0%), research activation/deactivation, invalid research
orders and agreement with actual next-year work. Planning must leave physical
state unchanged.

## Separate pre-existing P1: save after conquest with a surviving station

An unarmed transport can defeat the surface population while an unarmed enemy
station survives. The existing ground-invasion contract retains the station's
owner. The loader incorrectly required station ownership to equal surface
ownership, rejecting a state produced by the ordinary turn processor.

The fix preserves the documented gameplay rule. It accepts separate owners
while requiring a registered station owner, an existing planet, nonzero IDs,
and unique station/orbit identities. Capture does not grant the surviving
station's services, engineering support or ownership to the invader. Station
capture remains a separate future mechanic; ordinary combat can destroy it.

The save regression performs both manual cargo-transfer invasion and waypoint
UnloadAll, verifies that the enemy station survives without granting a shipyard
to the captor, and round-trips host and both player turns. Unknown owners,
missing planets and duplicate/zero station identities remain rejected.

This P1 is recorded separately from the two new forecast regressions; it was
already present on the reviewed main. No save or order layout change is needed.

All three reproductions failed on the original implementation before applying
the fixes. The four focused suites then passed, including old v84 conquest
saves and malformed station references. Full-suite and CI results, together
with the separate fix commits, are recorded in PR #219.
