# Science and technology #44 — completion audit

23 October 2026 (Europe/Moscow). Base: main
[`155f9e1`](https://github.com/kotrf/suns/commit/155f9e1e4f9fee7ce5e440b1fc57595d9dc0df48),
after [#229](https://github.com/kotrf/suns/pull/229).
The base uses **save v94 / orders v38**; the reviewed completion branch uses
**save v95 / orders v38** for persistent receiver evidence. The updated
[#44](https://github.com/kotrf/suns/issues/44) separates the implemented science
foundation from later capability families; its original requirements remain in
the issue for comparison.

## Requirements, implementation and evidence

The following are existing implementation and test coverage, not a claim that
every test was executed locally for this audit. Execution results follow below.

| Requirement / boundary | Implementation | Verification |
| --- | --- | --- |
| Six empire-level fields, independent players, persisted progress | `Player::technology`; `apply_research_points` in `turn_processor.cpp`; quadratic level costs | `research_tests.cpp`; independent research/turn collection in `campaign_tests.cpp`; technology round trip in `save_game_tests.cpp` |
| Ordered focus/queue, pause/resume, preserving invested RP and carrying overflow into the next field | Ordinary `SetResearchPlanOrder`; progress stays with its field; completed levels consume queued focuses | Research tests for overflow, reorder/clear, moving/removing/resuming the active field; pending-plan window lifecycle in `tool_windows_tests.cpp` |
| Science competes with local production; no free resource generation | `resolve_colony_production`: habitat upkeep first, per-colony allocation with integer rounding, then queue, then unused output to research | Allocation/leftovers in `research_tests.cpp`; queue forecasts in `production_queue_tests.cpp`; paid upkeep in `orbital_habitats_tests.cpp` |
| Research unlocks capabilities through multiple prerequisites and racial access | Shared `research_unlocks`, `research_unlock_available`, component/hull legality and race gates | `propulsion_tests.cpp`, `equipment_tests.cpp`, `ship_designer_tests.cpp`; secondary prerequisites, racial restrictions and queued-level accounting in `propulsion_editor_tests.cpp` |
| New designs require known technology; existing ships are not automatically refitted | Designer availability and host `CreateShipDesignOrder` validation; existing designs keep their components | `verify_component_unlock_is_enforced_for_new_designs` in `research_tests.cpp`; designer/core validation suites |
| Operational strategic uses, rather than research-only percentage bonuses | Engines, scanners, mining, weapons/defenses, stations/gates, paid terraforming, mobile bases, probes and planetary engineering | Thematic core suites, including propulsion, equipment, strategic operations, terraforming, Stargate Specialist, mobile base, survey probes and planetary engineering |
| Dedicated UI, next unlocks and completion announcements | Nonmodal Research window, allocation/plan/catalog; deterministic `ResearchLevelCompleted`; `main_window_turn_messages.cpp` names available unlocks | `tool_windows_tests.cpp`, `propulsion_editor_tests.cpp`; research completion/replayed event identity in `research_tests.cpp` |
| Physical science can supplement RP and add discovery prerequisites without granting free equipment | Once-only delivered science, retained artifacts, and Adaptive magnetosphere's delivered completed Compact remnant study | Completed [#59 audit](2026-10-22-field-science-completion.md); `science_samples_tests.cpp`; `magnetic_technology_and_delivered_science_gate_capability` in `planetary_engineering_tests.cpp` |
| Computer players use normal research orders and received information | `plan_research` consumes `PlayerView`, respects an existing queue, persists its target level, and considers received threats and specialist gate needs | Fairness, deterministic orders, retained settings and bounded replays in `computer_player_tests.cpp`; specialist tests |
| Headless, saves, PBEM and replay share the same model | Core turn resolution has no Qt dependency; save/order envelopes preserve technology and research orders | `campaign_tests.cpp`, `research_tests.cpp`, `computer_player_tests.cpp`, technology/order-envelope round trips in `save_game_tests.cpp` |

## Confirmed gaps corrected

1. **Research overpromised guaranteed RP during receiver bootstrap.** With six
   output, 100% science and a queued first Local Radar, the dialog promised six
   RP, while the host reserves one production point and grants five. The shared
   `colony_research_allocation` now supplies both the host's existing rule and the
   UI forecast, using received state plus admissible pending production orders
   in the UI. The core economy is unchanged. New core and Qt regressions compare
   the forecast with actual End Turn at 100% and 50%, without a queued receiver,
   and with research paused; the core also checks zero/one output.
2. **Capability guidance still described combat as future work.** The no-next-unlock
   fallback said “Weapons awaits combat rules” regardless of the active field.
   It now explains that the next level grants no new capability with the current
   prerequisites. `research.md` also stops describing Weapons as a future combat
   tree and states the mandatory-upkeep ordering.

3. **Remote receiver completion escaped through the forecast.** Review of
   `72663eb` reproduced a hidden completed receiver changing the guarantee from
   five to six RP while the received queue and observation year stayed unchanged.
   Owner-only operating evidence now captures the radar tier with that queue;
   PlayerView restores both from the same delivered packet and never falls back
   to live radar truth. Physical host resolution still uses the real receiver.
   Core and Qt regressions cover delayed completion and report arrival; core
   coverage also verifies that a subsequent hidden loss cannot rewrite evidence.

Save v95 adds one validated radar-tier byte to each present HABI observation,
including packets still in transit. Existing v94 records retain their queue and
other evidence; no receiver tier is confirmed until a fresh report arrives.
The migration does not invent past radar knowledge from current physical state.
Save/reload, player export, invalid enums and a reconstructed v94 payload are
covered in `orbital_habitats_editor_tests.cpp`.

No research costs, unlock levels, racial access, receipts, physical inventories
or order layouts change. Additional capabilities remain paid
construction or fitting decisions after research, rather than automatic gifts.

## Validation

- Seven focused core suites rebuilt and passed against the updated core:
  research, campaign, ship designer, production queue, science samples,
  planetary engineering and orbital habitats. The new receiver/allocation
  and remote receiver-delivery regressions are included in the research suite.
- The computer-player suite also passed on the unchanged audit base before the
  allocation helper was extracted; post-change AI validation belongs to CI.
- `git diff --check` passed.
- Qt/CMake are unavailable in this audit environment. The new UI regression,
  complete application build, save/PBEM suite and complete CTest run are left to
  the pushed commit's GitHub Actions check. Its actual result is reported on the
  PR separately; this document does not infer it from a previous PR's green run.

No new manual campaign/balance acceptance or cross-platform run is claimed.

## Completion decision and remaining scope

The original science foundation is implemented: independent empire science,
planned allocation/progression, enforced capability prerequisites, ordinary paid
application, player-facing planning/events, and common deterministic execution.
After review, passing CI and merge, **#44 is ready for closure**. This audit does
not close the issue or merge its branch, and the PR must not use an automatic
closing keyword before that decision.

The computer player has basic strategic heuristics, not a complete utility-based
technology evaluator. Catalog entries that explicitly describe unavailable
racial/special effects do not imply those effects are implemented. Further race
mechanics (#50), gate routing (#195), logistics/mobile yards (#53/#54/#61),
objects/visitors/probes (#55/#46/#60), planetary engineering (#56), and EW
(#58/#101/#205) retain their own scope. Balance tuning and deeper AI evaluation
are follow-ups, not a reason to keep this foundational issue indefinitely open.
**#63 remains frozen at the end of the queue.**
