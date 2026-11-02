# Open-issue reconciliation — 2 November 2026

The user chose to compare open GitHub issues with the delivered game and close
completed work. GitHub commits/PRs, original issue requirements and current
source/tests take precedence over stale roadmap notes. This is a backlog audit,
not authorization to implement another large feature or resume a frozen one.

## Verified baseline

Main is `67215fc96933d8c6a427bce15f9d448fb2a6736a`, after merged #259
(`e42b804`) and #262. Its tree `134e9b63d335670b122e751b1346fa7d402c438c`
is identical to tested #262 head `262e402f73f3a2209c54e9348a0bb46216b1a982`.
The source-identical merge from main into the branch only satisfied strict
up-to-date protection after retargeting #262 from #259 to main.
[Source CI 41477603122](https://github.com/kotrf/suns/actions/runs/41477603122),
job 125387190869, passed **105/105 (88.94 s)** with Qt6.12.0; checkout SHA
and decoded test log were verified. Re-review of the preceding identical source
`ef82069` confirmed the legacy-hull price P2 fixed with no new confirmed findings.
#259 source CI passed 104/104 (90.54 s); its merge's main CI passed 104/104
(89.64 s). These are distinct runs; final-main CI is checked separately.

#258 closed with #259; #260/#261 closed with #262. The four older closures below
were applied directly to GitHub as completed, with evidence prepended and their
original requirements/history preserved. #94 and #50 also received current
remaining-scope notes. API recheck found exactly the sixteen retained issues.

## Disposition

Twenty older feature issues were open before this audit. Four are complete in
their agreed scope: #45, #48, #105 and #195. The other sixteen remain open.
Focused mini-idea issues #258/#260/#261 are accounted for separately by their
implementing PRs #259/#262; they are not additional old backlog closures.

| Issue | Decision | Delivered evidence and remaining boundary |
| --- | --- | --- |
| [#45](https://github.com/kotrf/suns/issues/45) Turn Messages | Close completed | Typed, deterministic, recipient-specific delayed reports; briefing reader, filters, unread/map navigation and warning levels. Initial families and later research/contact/combat/logistics events exist. Advanced reconnaissance remains in #58/#101/#205. |
| [#48](https://github.com/kotrf/suns/issues/48) Empire history | Close completed | Compact saved planning-boundary samples, empire/colony scopes, compatible series, exact hover/ranges/comparison, research, freight, remote extraction and received casualty/event history. Optional per-fleet charts and unspecified future categories do not block the original scope. |
| [#105](https://github.com/kotrf/suns/issues/105) Interstellar medium | Close completed | #212 plus merged #231 deliver density, full-segment propulsion/risk integration, received sensor/medium knowledge, overlay/estimates, explicit route alternatives and fair computer bypasses. Optional phase 5 remains future design. |
| [#195](https://github.com/kotrf/suns/issues/195) Stargates | Close completed | #196/#213/#232 deliver paid finite/specialist gates, endpoint/mass/trait limits, delayed orders, autonomous gate steps, forecasts and persistence. The old deferred-route TODO is obsolete. |
| [#94](https://github.com/kotrf/suns/issues/94) Graphical designer | Keep open; reproduced gap | Graphical fitting, all 41 hull decks, component catalog and core validation exist. An already open designer keeps its old component-access snapshot after own technology changes; showing that same dialog again does not refresh it. |
| [#47](https://github.com/kotrf/suns/issues/47) Workspace | Keep open | Docks, presets, shared stable-ID selection and separate tools exist. The explicitly retained shared GameSession-style ownership refactor is not delivered; MainWindow still owns state/orders. |
| [#46](https://github.com/kotrf/suns/issues/46) Visitors | Keep open | First physical timed encounter, payment/gift and fair interception exist. More request/gift profiles, dialogue/portraits, trajectories and strategic computer preparation remain. |
| [#50](https://github.com/kotrf/suns/issues/50) Asymmetric traits | Keep open | Stargate Specialist and its gate routes are complete. Other primary traits/mobile-habitat civilization and comparative balance remain. |
| [#54](https://github.com/kotrf/suns/issues/54) Deep-space infrastructure | Keep open | Paid fixed depots/relays and independent orbital shipyards exist. Listening/sensor posts, cargo caches, fortified types, capture and broader construction/management remain. |
| [#55](https://github.com/kotrf/suns/issues/55) Unknown objects | Keep open | Persistent ancient probes/compact remnants, received contacts and field research exist. Moving/transient/rogue objects, hazards/resources and learned unique finds remain; a public equipment catalog does not complete these. |
| [#56](https://github.com/kotrf/suns/issues/56) Engineering | Keep open | Paid mirrors/shades, magnetosphere/shielding and orbital habitats exist. Climate/ocean projects and bounded system engineering remain; radical transformations need their own design discussion. |
| [#61](https://github.com/kotrf/suns/issues/61) Mobile bases | Keep open | Finite-stock expedition repair/refuelling/docking services exist. Mobile manufacturing/refineries, scientific facilities and gate support remain. |
| [#60](https://github.com/kotrf/suns/issues/60) Autonomous probes | Keep open | Autonomous received-knowledge survey programmes exist. Beacon deployment, further science, supply searches and material-limited replication with explicit limits remain. |
| [#58](https://github.com/kotrf/suns/issues/58) Electronic warfare | Keep open | Received uncertain hull evidence, cloak/jamming/tachyon/decoy signatures and interference reports exist. Physical decoys, specialized/channel jamming and richer intelligence/doctrines remain. |
| [#101](https://github.com/kotrf/suns/issues/101) Emissions/posture | Keep open | Standard/Passive/Silence, receiving without transmitting, friendly-orbit exchange, scheduled windows and physical interception exist. Conditional contact/combat/scan-and-withdraw doctrines remain. |
| [#205](https://github.com/kotrf/suns/issues/205) Emission interception | Keep open | #208 delivers actual pulses, automatic fast/slow channels and separate interception/report journeys. Advanced traffic classification, justified SIGINT, channel jamming and balance remain; extra radio panels/hardware are not approved. |
| [#106](https://github.com/kotrf/suns/issues/106) Planetary scars | Keep open | Persistent ruins, contamination, paid cleanup and delayed aftermath observations exist. Biosphere/ecological weapons, scorched-earth choices and further recovery capabilities remain. |
| [#108](https://github.com/kotrf/suns/issues/108) Loyalty/acclimation | Keep open; design only | Tentative candidate requiring another gameplay discussion, not a population-simulation implementation commitment. |
| [#53](https://github.com/kotrf/suns/issues/53) Transport networks | Keep frozen | Existing #237–243 lines/statistics/stock targets/fixed recipients stay. All further development, including evacuation and analytics, waits for more gameplay experience. |
| [#63](https://github.com/kotrf/suns/issues/63) Exotic materials | Keep frozen | Existing finite xenocrystal extraction/cargo/resonance chain stays. Further exotic uses remain last in the queue until explicitly resumed. |

## Closure checks

### #195 — gates and deferred programmes

Merged implementations are #196 (`bb5d048`), #213 (`4cb245c`) and #232
(`0d4ec03`). [Stargate rules](../stargate-specialist.md) and
`src/core/src/stations.cpp` cover paid fittings, both endpoints, loaded gross
mass/range, cargo permissions, one hop per year and no fuel charge. Standalone
remote gate orders use normal delayed command delivery; delivered programmes can
load, gate, unload and continue flying without another headquarters link.
Missing/destroyed gates recheck/wait instead of inventing a transit. Player-view
authoring/forecasts and saves/orders cover these routes.

`stargate_specialist_tests.cpp` exercises invalid and loaded transfers, annual
limits, split/rendezvous, remote command/stop delivery, failed-gate waits, full
load→gate→unload→flight programmes and received-only computer planning.
The editor suite covers fitting/transit controls, programme editing and forecasts.
Seven related CTest targets passed: stargate core/editor, orbital station,
exotic-material core/editor, audit regression and save game (4.97 s).

### #105 — agreed phases 1–4

#231 is merged (`4292aee`); the issue's pending-review/merge checkpoint is stale.
[Medium rules](../interstellar-medium.md), core and editor suites cover
generation, bounded full-segment integration, mixed engines, safe-Warp/hull risk,
fuel affordability, immutable delayed surveys, hidden-independent forecasts,
choose/cancel route alternatives and ordinary computer bypass orders.
Load-all/future-leg budgets and confirmed initial depot service have regression
coverage. Optional ionization/turbulence, radio interaction and further cloud
science were explicitly phase 5/future extensions, not unfulfilled phases 1–4.
This closure does not resume #53/#63 or approve a new physical model.

### #48 — original history scope

[Empire history](../empire-history.md), `history_tests.cpp`,
`operational_loss_history_tests.cpp` and `tool_windows_tests.cpp` cover saved
player-owned compact samples, replacement rather than duplicate same-turn
boundaries, colony ownership gaps, actual extraction/deliveries, technology,
receipt-year event/casualty records and private PBEM exports. The modeless History
tool provides metric/scope selectors, compatible I/B/G series, range controls,
exact hover and comparison. Fleet counts/mass are empire aggregates; per-fleet
scope and map-following were optional in the original issue. Later event types
can be separate improvements; no concrete outstanding required item was found.

### #45 — original strategic briefing scope

`GameEvent`/`TurnResult`, `player_knowledge_tests.cpp` and the current turn
processor implement typed stable-ID recipient events and immutable delayed
delivery, with deterministic reports and transition-based shortage/fuel warnings.
Research, contacts, battles and cargo reports are implemented by their mechanics.
`src/app/main_window_turn_messages.cpp` renders the received evidence, automatically
shows new reports, filters by turn/subject/planet, marks unread state in UI,
supports previous/next unread and map focus, and distinguishes warning severity.
`tool_windows_tests.cpp` verifies grouped hostile/contact/combat filters cannot
show another player's report. The three old issue comments only awaited event
families now delivered. Advanced traffic/decoy/conditional-doctrine extensions
are preserved under open #58/#101/#205; closing the briefing issue does not
claim those extensions complete.

Seven history/navigation/report CTest targets passed in 1.28 s: history,
operational loss history, player knowledge, tool windows, medium core/editor
and save game. These and the seven gate targets overlap at save game, giving
13 distinct focused suites. These are checks of existing code; production and test sources and the
save/orders formats are unchanged by this audit.

## Confirmed remaining designer acceptance item

An offscreen probe on the catalog source tree creates the real modeless
ShipDesigner with own technology levels 0, then advances own levels to 26,
rebuilds controls/scene and invokes Open Designer again. Efficient Ram Scoop
is available according to the core, but the existing catalog item remains
unavailable: `before=false, core_after=true, ui_after=false, same_dialog=true`.
The probe is synthetic own-state evidence, not a claim of a full research-turn
playtest. Source inspection confirms End Turn reuses that window, while its
availability labels/flags are initialized from its construction-time snapshot.
The separate read-only Technology Catalog does refresh; that does not repair
ShipDesigner. Keep #94 open with this precise acceptance gap and remaining
player UI acceptance. A focused repair is a reasonable next candidate, but is
not implemented as part of this audit.
