# Player knowledge and Turn Messages

Suns! keeps simulation truth separate from what an empire has actually learned. The player-knowledge layer currently carries system surveys and operational reports from fleets and colonies.

## Survey reports

A scanner physically observing a system does not directly mutate `Player::surveyedStars`. It creates a `PendingSurveyReport` with stable subject IDs, the observation turn, its source fleet (or zero for a colony sensor) and a deterministic delivery turn.

Reports use the same relay coverage and signal-speed calculation as fleet communications, but live outside `FleetTelemetry`. This matters because intelligence can outlive a source fleet and later grow to include combat contacts, intercepted signals and reports shared by allies.

Repeated observations are coalesced while a report is in flight. A later observation replaces the pending report only when it produces an earlier delivery, with stable source-ID ordering as a deterministic tie-breaker.

## Delivery and events

At a planning boundary, due reports:

1. update the recipient's permanent surveyed-system knowledge;
2. emit a typed `GameEvent::SystemSurveyed` containing stable star, planet and source-fleet IDs;
3. preserve both observation and delivery turns, so the UI can state how stale the report is.

Event IDs are derived deterministically from the event payload. PBEM resolution, replays and headless tests therefore produce the same event identity without putting UI-only unread state into `GameState`.

`TurnProcessor::process_with_events()` returns a `TurnResult` containing the new authoritative state and the player-specific event list. The original `process()` remains as a state-only compatibility path for forecasts, AI simulations and tests that deliberately discard messages.

## Operational reports

Fleet arrival, route completion and insufficient-fuel facts become `PendingPlayerReport` packets at the physical source. Their delivery turn uses the source position and the same relay model as commands and telemetry. This prevents the message list from becoming a side channel that exposes remote simulation truth.

Fuel warnings are transition-based: a fleet emits one report when it first becomes unable to move, not another copy every turn while it remains stalled. A resumed fleet may produce a new warning if it later stalls again.

Production completion reports originate at the owning colony. Since colonies are the current relay nodes, factory, mine and ship completion is normally available at the next planning boundary with no extra communication delay. The payload preserves the colony, design and completed fleet IDs plus the resulting factory/mine count where applicable.

The save format persists both pending operational reports and the fleet's fuel-stall transition state.

## Turn Messages

The desktop app presents delivered survey and operational reports in a dedicated Turn Messages dock after End Turn. New items are unread, Next unread navigates through them, activating a report centers its star or recorded position, and warning severity is visually distinct.

Later issue #45 slices can add colony founding, production waiting for minerals, research, contacts and battle results without changing the separation between authoritative truth, delivered player knowledge and UI-only unread state.
