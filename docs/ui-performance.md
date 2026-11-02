# UI planning and map selection performance

MainWindow separates selection redraws from state/order boundaries. Selecting a
star cannot invalidate the physical logistics/movement/ETA projections: their
inputs are game state, player and accepted pending orders, not the selected star.
A selected-fleet change still rebuilds geometry such as sensor/route overlays,
while reusing unchanged physical projections.

A queued selection refresh keeps existing map objects when the rendered planning
revision and selected fleet still match. It updates star highlights, label
placement and system/production/route panels. Scene selection signals are blocked
during highlight restoration, preventing recursive selection redraws. Load/new
campaign/turn/order changes use the normal invalidating rebuild; changing a fleet
forces a geometry rebuild. Coalesced callbacks always read current selection.

A received-player projection is shared by related panel/route/map readers. It is
keyed by planning revision, player and turn, and explicitly refreshed at control
updates. It is always built through make_player_view; the cache never contains
raw host truth. Readers that modify projected orders work on copies. Command
checks, including Designer reopen/refresh/Save, still request fresh knowledge
rather than relying on a previously rendered panel snapshot.

This removes redundant work; it does not replace the real disposable movement
simulation with an approximate model or alter game rules. The supplied private
year-89 campaign reproduces the issue but is not included in the repository.
The automated regression uses a synthetic fixture and checks retained map items,
selection and planning caches plus invalidation on a real boundary. Absolute
wall-clock limits are avoided in CI; comparative save-based measurements belong
to the project-status checkpoint and PR.
