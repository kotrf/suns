# Player knowledge and Turn Messages

Suns! keeps simulation truth separate from what an empire has actually learned. The player-knowledge layer currently carries system surveys and operational reports from fleets and colonies.

## Survey reports

A scanner physically observing a system does not directly mutate player knowledge. It creates a `PendingSurveyReport` with a survey level, stable subject IDs, the observation turn, its source fleet (or zero for a colony sensor) and a deterministic delivery turn.

The first staged model is deliberately compact:

- `BasicScan`: a fly-by or remote sensor footprint reveals a deterministic rough habitability estimate;
- `OrbitalSurvey`: arriving at the system confirms exact habitability and population suitability;
- `GeologicalSurvey`: remaining at the system for one additional turn reveals mineral concentrations and surface stocks.

Owned colonies have complete local knowledge. A new colony also promotes its system to geological knowledge. Colonization requires at least an orbital survey, so a rough fly-by estimate informs routing without being enough for an irreversible investment.

Reports use the same relay coverage and signal-speed calculation as fleet communications, but live outside `FleetTelemetry`. This matters because intelligence can outlive a source fleet and later grow to include combat contacts, intercepted signals and reports shared by allies.

Reports in flight are dominance-coalesced: a higher-quality report does not erase useful lower-quality information that would arrive earlier, while an equal-or-better report arriving no later suppresses the redundant packet. When several levels arrive together, the player receives one event for the best level.

## Delivery and events

At a planning boundary, due reports:

1. promote the recipient's permanent per-system survey level;
2. emit a typed `GameEvent::SystemSurveyed` containing the delivered level, stable star, planet and source-fleet IDs;
3. preserve both observation and delivery turns, so the UI can state how stale the report is.

Event IDs are derived deterministically from the event payload. PBEM resolution, replays and headless tests therefore produce the same event identity without putting UI-only unread state into `GameState`.

`TurnProcessor::process_with_events()` returns a `TurnResult` containing the new authoritative state and the player-specific event list. The original `process()` remains as a state-only compatibility path for forecasts, AI simulations and tests that deliberately discard messages.

## Operational reports

Fleet arrival, route completion and insufficient-fuel facts become `PendingPlayerReport` packets at the physical source. Their delivery turn uses the source position and the same relay model as commands and telemetry. This prevents the message list from becoming a side channel that exposes remote simulation truth.

Fuel warnings are transition-based: a fleet emits one report when it first becomes unable to move, not another copy every turn while it remains stalled. A resumed fleet may produce a new warning if it later stalls again.

Production completion and mineral-shortage reports originate at the owning colony. Since colonies are the current relay nodes, factory, mine and ship results are normally available at the next planning boundary with no extra communication delay. The payload preserves the colony, design and completed fleet IDs plus the resulting factory/mine count where applicable.

A blocked production item emits `ProductionWaitingForMinerals` only when it first enters the waiting state. The colony remembers that transition, suppresses annual repeats, and becomes eligible for another warning only after production resumes or the blocked item changes.

Successful colonization emits `ColonyFounded` from the new colony. Both direct colonization orders and programmed arrival actions pass through the same core operation, so they cannot diverge in event behavior.

The save format persists pending operational reports, fleet fuel-stall transitions and colony mineral-wait transitions.

## Turn Messages

The desktop app presents delivered survey and operational reports in a dedicated Turn Messages dock after End Turn. Survey text distinguishes basic, orbital and geological results. Planet panels, tooltips and the habitability map use only the delivered knowledge level: estimated values are marked and dimmed, exact habitability waits for orbit, and geology remains hidden until the deep survey. New items are unread, Next unread navigates through them, activating a report centers its star or recorded position, and warning severity is visually distinct.

Later issue #45 slices can add research, contacts and battle results without changing the separation between authoritative truth, delivered player knowledge and UI-only unread state.
