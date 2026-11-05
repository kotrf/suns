# Shared game session (#47)

`GameSession` is the single owner of a desktop campaign's canonical `GameState`,
pending orders/descriptions, stable-ID selection, PBEM mode/identity/tokens/inbox,
campaign reports/read IDs and current save path. It also owns the received and
accepted planning caches. No core object pointers are retained by the selection;
the single planet at a selected star is resolved by ID whenever needed.

`MainWindow()` creates a session. `MainWindow(GameSessionPtr)` attaches another
view to that same document. Creating a view does not regenerate the campaign,
reset its identity, clear orders or advance the planning revision. Closing one
view does not destroy a session retained by another; the final shared owner
releases it. Independent default windows still create independent campaigns.
The UI-thread session has no QWidget parent, and its lifetime is held through
`std::shared_ptr` rather than Qt parent ownership.

The existing MainWindow controller methods retain private reference aliases into
session storage. Those references are not a second model: they let the existing
stable-ID order tools migrate without rewriting game rules or hundreds of view
queries. Public `state()`/`snapshot()` expose the authoritative document for
controller/persistence use. Information/reference views use `receivedState()` or
`plannedState()`, which preserve the normal fog of war, delayed telemetry and
already-sent owner intent. The latter reflects current cargo/reorganization/
refuelling; production and research plans still use the pending order list. They must not render the raw host state. The existing
non-modal designer/catalog receive disposable received snapshots and validate
Save against current conditions. They do not own a second canonical campaign.

## Notifications and refresh

The session emits typed Planning, Panels, Selection, Messages, Metadata and
Document notifications, identifying the authoring controller. Peer windows
coalesce them through one zero-delay callback and never resolve a turn in
response to a notification. A source controller retains its existing immediate
refresh path. This avoids feedback loops and keeps scene destruction outside
Qt's current click/selection delivery.

- A star/fleet choice updates the shared IDs and distance reference, without
  invalidating the received, cargo or movement caches. Peer star-only selection
  reuses map objects, just as the existing main view does. Unknown fleet IDs are
  rejected against the received/accepted fleet list and missing owner contacts.
- Production/research-only orders advance the shared revision while retaining
  physical projections and map items. Peers use the same panel-only fast path.
- Other accepted orders and a real turn boundary refresh the shared planning
  data and peer displays once. The ordinary controller still invokes the real
  TurnProcessor only for the player's actual End Turn.
- File path, campaign identity, player/mode and turn-token/readiness changes
  update titles and controls in every attached view. Saving serializes one
  canonical session snapshot; opening a second view cannot rotate PBEM tokens.
- Loading/restarting a document replaces session data, not the storage object.
  Peers close their old-world ship drafts, cancel map target picks and reset
  view context before repainting. The session advances its epoch before the
  Document notification, so direct observers already see the replacement.
  Same-year file replacement cannot retain the old received/planning cache. Reports and read marks are shared; filters and
  transient selections inside the report reader stay local to each view.

Dock placement, tool visibility, map zoom/mode, sensor-overlay visibility,
filters and unsubmitted editor drafts remain view preferences. Existing presets,
Custom layouts, fullscreen map, Reset panel layout and persisted dock geometry
keep their existing implementation. No new workspace layout or mandatory second
monitor is introduced. No game rules, annual UI forecasts, polling, worker jobs,
core save/order/race payloads or file versions change: v112/v49/v3 remain current.

## Acceptance evidence

`game_session_tests` uses two real MainWindows attached to one session. It checks
that the campaign/orders/selection/PBEM/report storage and planning caches have
one identity; a real map choice appears in the other scene without a cache build;
a factory order appears in both production lists without rebuilding either map;
and a single End Turn increments the year once and completes one paid factory.
It exercises save/title propagation, loading an older same-year planning boundary,
peer draft closure, report/read-state propagation, invalid fleet IDs, vanished
fleets, independent sessions, closing the authoring view and final owner release.
Existing workspace, map, designer, received-data, logistics, PBEM and layout tests
remain part of the full suite. Native multi-monitor window-manager behavior is a
manual acceptance surface; automated tests run Qt offscreen.
