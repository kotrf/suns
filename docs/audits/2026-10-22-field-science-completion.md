# Field science #59 — completion audit

22 October 2026 (Europe/Moscow). Base: fresh main
[`3326fb6`](https://github.com/kotrf/suns/commit/3326fb61b444c8665e890e7ef2714d0b28849399),
merged [#227](https://github.com/kotrf/suns/pull/227), including reviewed `b11fc45`.
Main used save v92 / orders v37; this completion branch uses **save v93 / orders v37**.
The issue's #211 checkpoint describes physical samples and observation unlocks as
future work; #220 and #227 have already delivered them.

## Requirements, implementation and evidence

| Original #59 requirement / boundary | Implemented chain | Existing verification and completion additions |
| --- | --- | --- |
| Explicit deterministic field objectives, real expeditions and delayed usable data | Finite star/planet/wormhole objectives (#173), Ancient probe / Compact remnant studies (#211); scanners, real routes/fuel and ordinary reports | `strategic_operations_tests::field_science_is_finite_delayed_and_real`; `unknown_objects_tests::{field_research_is_explicit_finite_and_usable_only_after_delivery,route_actions_and_receipt_year_history_use_ordinary_processing}` |
| Selected capability requires observation as well as research | Adaptive magnetosphere (#227): Energy/Construction 18 plus current empire's **delivered completed** Compact remnant study; normal paid supported construction. Detection, classification and pending study are insufficient. Unique sample is neither required nor consumed | `planetary_engineering_tests::{magnetic_technology_and_delivered_science_gate_capability,magnetic_projects_are_paid_finite_and_reversible}` exercises actual study/report delivery with no physical specimen, plus captured-owner gates; engineering editor/catalog tests |
| Salvage rewards quality/novelty, no guaranteed battle science | Recoverable finite 20-year wrecks record destroyed equipment requirements; scanner/capacity required; only positive technology gaps reward the most novel field, capped at 96 RP; obsolete wrecks give zero | `strategic_operations_tests::field_science_is_finite_delayed_and_real`; completion editor test replays a real novel wreck award across pending/delivered save/load |
| Physical return and dismantle versus usable artifact | One global indivisible 5 kt specimen per landmark (#220), shared hold/gross mass; 64 Electronics / 96 Energy RP after destructive analysis **and data delivery**, or working +4 RP/year matching-field artifact under own delivered study/active research/allocation gates | `science_samples_tests::{study_recover_return_and_consume_once,retained_artifact_requires_observation_and_correct_research}`; completion tests guard duplicate report delivery and two distinct equal-value samples |
| Unique cargo survives logistics and ownership correctly | Atomic capacity-checked transfers, ordered previews, whole-item split, both manual/on-arrival merges, unloading/colonization, loss and conquest; loss/analysis never refill source; capture/transfer deactivate retained operation pending new owner's own study | `science_samples_tests::{hold_capacity_atomic_transfers_and_ordered_previews,split_and_merge_conserve_whole_specimens_and_cargo,split_colonists_without_minerals_keeps_both_holds_valid,route_merges_preserve_samples_and_delayed_manifests,colonization_and_conquest_preserve_stored_specimens,combat_cargo_loss_does_not_refill_the_landmark}`; editor reload tests for split and both merges |
| Repeats/multiple fleets/re-delivery cannot farm finite rewards | Stable objective reservations at collection; global depletion and stable FleetId recovery order; new durable award identities and delivered receipts at headquarters | Existing object/science finite/replay tests and sample repeat-order tests; added `completion_audit_generic_science_delivery_is_idempotent`, `completion_audit_equal_rewards_for_distinct_specimens_are_not_deduplicated`, `completion_receipts_and_surface_evidence_survive_save_load` |
| UI/AI/forecast/PBEM use received evidence, no retrospective hidden updates | Received object journal/messages/History; immutable fleet manifests and owner surface survey manifests; panel shows evidence year; forecasts replay accepted pending orders; physical execution validates real inventory | Existing unknown-object editor/export tests, sample stale-transfer/hidden-merge tests, sample GUI authoring and PBEM tests; added `completion_audit_surface_inventory_waits_for_delivery` and `remote_surface_changes_wait_for_receipts_in_ui_ai_and_exports` compare UI/research forecast/AI orders and byte-identical exports before receipt |
| Persistence and fair computer integration | Save/load of objectives, depletion, physical/received/in-flight cargo; ordinary sample/route/process orders; AI works through PlayerView and ordinary paid expedition/return/analysis/project orders | `science_samples_editor_tests::{persistence_and_migration,exports_and_order_exchange,merged_historical_manifests_stay_reloadable}`; `science_samples_tests::computer_uses_received_study_and_returns_physical_cargo`; `planetary_engineering_tests::computer_magnetic_projects_use_received_science_and_ordinary_costs`; new SCIC roundtrip/migration/corruption tests and `competing_recovery_and_destroyed_cargo_stay_finite_after_reload` |

Test sources: [core science](../../tests/strategic_operations_tests.cpp),
[objects](../../tests/unknown_objects_tests.cpp),
[samples](../../tests/science_samples_tests.cpp),
[UI/save/PBEM](../../tests/science_samples_editor_tests.cpp),
[engineering](../../tests/planetary_engineering_tests.cpp).
These extend existing suites rather than duplicating the lifecycle coverage.

## Confirmed gaps and minimal completion fixes

### Re-delivered scientific data awarded RP again

Reproduction: analyze an Ancient archive at a populated colony, retain a copy of
its queued ScientificData report, deliver it, then reinsert/deliver that packet
(or one with a later delivery date). Electronics advanced by 64 RP again. The
same generic delivery path served finite wreck rewards. Object studies had a
separate received-Studied guard, but generic science did not have a receipt.
The new core regression failed on the double award before the fix.

Fix: [`queue_player_report` / `deliver_due_player_reports`](../../src/core/src/player_knowledge.cpp)
assign deterministic frozen-payload/objective IDs and record sorted delivered
receipts before applying scientific data. Finite objectives use their ledger key;
[sample analysis](../../src/core/src/science_samples.cpp) uses the specimen source,
so simultaneous equal-value analyses remain distinct awards and messages. IDs
exclude delivery routing/date and use no hidden global allocation counter.
SCIC persists both pending IDs and receipts, including after save/load.

### Remote owned surface inventory leaked host state

Reproduction: capture an empty owned remote colony report, then unload or activate
a specimen there while the new report is in flight. Previously
[`make_player_view`](../../src/core/src/campaign.cpp) copied live owned
`Planet::scienceSamples`: the panel/export exposed cargo or retention early, and
retained-artifact research changed before receipt. Clearing physical stock could
also erase items from an old report retrospectively.

Fix: [`capture_planet_survey`](../../src/core/src/game_state.cpp) freezes owner-only
surface inventory, including explicit emptiness, into normal orbital/deeper
reports. PlayerView uses that received evidence; foreign inventories remain
omitted. The UI labels its observation year. Existing physical transfer/process
checks remain authoritative; planning from a historical manifest cannot create
cargo or research on the host. Tests cover hidden arrival, activation and loss,
actual report delivery, research forecasts, AI orders, GUI and byte-identical PBEM.

## Formats and migration

Save v93 appends bounded **SCIC** after EQIX. SAMP, engineering layouts and orders
v37 are unchanged. The reader rejects malformed/truncated/oversized extensions,
invalid receipts, mismatched packet keys and private surface evidence attributed
to another owner. Historical evidence may legitimately overlap physical items.

v92 retains physical samples and pending science. Migration assigns deterministic
pending award IDs; equal-payload historical packets remain separate because old
files cannot distinguish legitimate equal analyses from duplicate packets.
Missing historical surface inventory is **not** reconstructed from live host stock:
a normal fresh owner report makes it known. Already delivered pre-v93 generic
awards have no reconstructible receipt; the durable replay guarantee starts with
the new identity/receipt state. Existing object-Studied protection remains.

## Validation

Local GCC 14.2.0 / Qt 6.12.0, Debug with assertions enabled:

- Full application/UI/test build: `cmake --build cmake-build-qt612 --parallel 4` — passed.
- Full `QT_QPA_PLATFORM=offscreen ctest --test-dir cmake-build-qt612 --output-on-failure` — **85/85 passed**, 56.70 s, including all new regressions and existing science/engineering/PBEM suites.
- `git diff --check` — passed.
- The duplicate-analysis regression was reproduced failing before the receipt fix.

Qt checks use the offscreen platform; no new interactive campaign/balance
playtest or sanitizer run is claimed. GitHub CI is a separate push check; its
actual status is attached to the PR, not inferred from the local result.

## Completion decision and scope

The mandatory original chain is implemented: paid physical expedition → delivered
finite study → optional unique physical recovery/transport → consume or operate
artifact, plus a concrete research-and-observation-gated capability. Novelty-based
wreck science provides reverse engineering without a guaranteed battle reward.
The completion fixes address replay and information-delay gaps, not new rewards.

With validation passing, #59 is ready for a closure decision **after review and
merge**. The PR references #59 without an automatic closing keyword; neither the
issue nor the PR is closed/merged by this work. New artifact varieties, additional
observation-gated technology families, alternative rewards and mineral cargo from
wrecks are optional follow-ups. #63 is frozen at the end of the queue and its
mechanics are not expanded; remaining large engineering #56 is outside this task.
Prototype balance has no new manual playtest acceptance claim.
