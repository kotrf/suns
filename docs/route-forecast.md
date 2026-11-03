# Quick route estimates

The 3 November 2026 [minimal estimate contract](minimal-ui-estimates.md) replaces
annual route forecasts. The interface performs one pass over known waypoints;
there is no disposable TurnProcessor, 96-year horizon or background worker.

The table shows position, destination, Warp and **ETA ≈**. Nominal travel time
is cumulative ceil(distance / Warp²) from the received fleet position. Numbers
are prefixed ≈ and exclude command delivery, fuel, coasting, generation, clouds,
refueling, encounters and future arrival actions. The Route estimate dialog is
a quick summary of the same geometry. It does not run a detailed calculation.
Current fuel and the local overdrive warning remain available separately.

A moving target uses a position estimate from its received track, without
claiming an interception outcome. A gate row estimates approach to the source only; the actual hop and
later ETA are unknown. Wormhole entry never predicts exit or survival. Any
conditional arrival action makes subsequent ETA unknown (—). The real turn
resolves every action normally; next year's received reports update the plan.

Selecting a row loads that exact leg's Warp and arrival action. Editing changes
only that leg; other speeds, targets and actions remain intact. New waypoint
clears row selection and restores the separate addition draft. Adding through
the button or map immediately selects the appended row; a failed append does
not select another leg. Choosing Colonize edits that selected leg and retains
the explicit colony-plan choice.

Committed routes can be reordered or have points removed like current drafts.
Removing the last point submits explicit Stop: navigation, repetition and
stationary tasks stop when delivered. Older packets cannot restart the canceled
program. Stop remains valid for an immobilized fleet and does not depend on an
estimated future coordinate.

Route intent precedence remains current pending edits, then the latest sent
navigation command (including Stop), then the received onboard route. Sent
commands retain original issue/delivery dates, even when telemetry still shows
an old voyage. Non-navigation settings packets do not replace this intent.
Neither hidden station loss nor undelivered fleet movement changes received
labels or confirms a gate outcome. See [scheduled gate orders](stargate-specialist.md#deferred-commands-and-scheduled-gate-routes-195).
