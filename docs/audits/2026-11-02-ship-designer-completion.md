# Graphical Ship Designer #94 — completion check

The user selected #94 after the [backlog reconciliation](2026-11-02-backlog-reconciliation.md).
The remaining reproduced criterion was live access in an already open designer.
This change is based on documentation PR #263 (`73e487e`), after gameplay main
`67215fc` (#259/#262). #263 is still a separate open PR; its merge is not assumed.

## Live-access repair

`ShipDesignerDialog::refreshAvailability` updates existing hull/component rows
from received own-player knowledge using the core access functions. It updates
locked labels, icons, enabled/drag flags, selected details, Fit and Save. A
single-shot zero-delay callback coalesces `MainWindow::controlsChanged` signals;
there is no interval polling. Opening an existing window refreshes immediately.

Availability refresh does not change the selected hull, name, fitted components,
logical placements or selected fitting cell. It retains search/category/hull-fit
filters and still-visible catalog selection; unchanged access does not scroll.
Hull model changes block selection signals, so revoking hull access cannot
silently migrate banks or remove modules. A now-unavailable fitted part stays in
the draft and disables Save with a reason. MainWindow also rechecks current
received access on Save, before the normal CreateShipDesignOrder is queued.
The host's existing placement/access validation remains authoritative.

## Original acceptance criteria

| Requirement | Current implementation and evidence |
| --- | --- |
| Create legal designs graphically, without numeric counters | All 41 hull decks, stable banks and catalog drag/drop/Fit/remove controls; `propulsion_editor_tests.cpp`, `ship_fitting_layout_tests.cpp`, core hull/equipment/designer suites |
| Reject illegal placement and explain before End Turn | Core placement validation, live preview/Save state, incompatible drop feedback; designer and propulsion-editor regressions |
| New research unlocks a component without changing designs | New live-access suite resolves actual MainWindow End Turn from Propulsion 2 to 3, unlocks Efficient Ram Scoop in the same dialog, preserves the named fitted draft and existing design components, fits the engine explicitly and saves/resolves the ordinary design order |
| Restore saved logical placement | Start-from-design copy and saved logical IDs; template/window tests, fitting-layout archived placements and save-game round trips |
| Deterministically migrate old placements without losing equipment | Core autoplacement/normalization, homogeneous engine banks and versioned save readers; `ship_designer_tests.cpp` and `save_game_tests.cpp` |
| Reject malformed/unavailable orders independently in core | Existing core designer/research/hull/equipment and save/order regressions; UI adds a fresh Save access check rather than bypassing the host |
| Remain a separate window | Existing singleton modeless tool/menu/toolbar/window tests; new End Turn scenario keeps that same designer and draft alive |

The new suite also resolves a real Construction breakthrough to unlock a hull;
tests acquired equipment and a traded hull separately from research; checks
racial access/revocation, stale pre-refresh Save, stable filters/selection/scroll,
unrelated enemy changes and absence of orders from refresh alone. Revocation
preserves the fitted draft rather than repairing it silently. A synthetic enemy
blueprint/trait cannot unlock the local player's equipment/hull.

## Validation and boundary

Before the fix, the real End Turn regression failed at the newly available
scoop's UI flag after core research had advanced (assertion/exit 134). With the
repair, the new offscreen suite passes (0.47 s). The full GCC14.2/Qt6.12
Debug build and **106/106 offscreen CTest passed in 38.50 s**. Diff check and
relative links passed. Remote source CI/review remain separately tracked in
GitHub; these local results do not claim a merge.
This is automated Qt/core acceptance, not a claim that the user manually
playtested every hull or polished every possible presentation preference.

Production/core rules, unlock requirements, existing ships/designs and
**save v109 / orders v48** are unchanged. #94 should close with the reviewed
implementation's merge to main. Shared GameSession ownership remains #47;
frozen #53/#63 and other major features are not resumed by this change.
