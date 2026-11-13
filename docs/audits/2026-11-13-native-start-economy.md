# Native first-build liquidity — PR #293

[Review5888638178](https://github.com/kotrf/suns/pull/293#pullrequestreview-5888638178)
finds one P2 on source3c7fa83. An unmodified Medium / Sparse campaign, seed2,
Human1 and Terran Orbital Civilization Computer2 starts at home184 with27.0882%
Ironium. The first Miner Pair costs88 I and leaves2.41763 I in year6. No10-I
surveyor is built through year161. Human submissions are empty; resources,
environment and race are unchanged.

The new actual-campaign regression fails before the fix because no paid scout
has left the capital. The policy now deducts the current support quote, budgets
the current-rate mineral deficit over an arithmetic construction interval, and
protects a surveyor reserve when the proposed miner output cannot support a
full habitat. It falls back to the ordinary one-module Orbital Miner; richer,
self-sustaining starts keep the pair. The policy uses only received minerals,
concentrations, residents, fitted mining rates and station capacity. No future
TurnProcessor call, asset grant, format change or UI forecast is added.

On seed2 the fallback completes in year5 with33.3341 I remaining. The surveyor
is built in year6, leaving23.5012 I, and begins ordinary scouting. Its in-progress
host reload preserves controller decisions and encoded orders. This seed checks
the reported early exploration deadlock; seeds17/42/123 separately retain the
full replacement → paid second colony → independent mining supply acceptance.
Broader long-game comparative balance remains open under #50.

The targeted native campaign suite passed in2.29 s. A complete GCC14.2/Qt6.12
Debug build and the combined sensor/drifting stack's **134/134 CTest passed
in173.13 s**. Native policy/test sources are identical in standalone #293; its
source CI runs131 tests. The original regression was observed failing before
the correction.

The review discussion remains open until re-review and a user-authorized merge.
The stacked sensor-post and drifting-object slices include this correction.
