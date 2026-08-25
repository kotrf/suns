# Player knowledge and Turn Messages

Suns! keeps simulation truth separate from what an empire has actually learned. The first player-knowledge slice applies that rule to system surveys.

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

## Turn Messages first slice

The desktop app presents delivered survey reports in a dedicated Turn Messages dock after End Turn. New items are unread, Next unread navigates through them, and activating a report centers the referenced system on the map.

This is the foundation for later event kinds from issue #45: arrivals, fuel stalls, production completions, colony founding, research, contacts and battle results.
