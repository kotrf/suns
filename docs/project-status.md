# Project status and development handoff

## Verified checkpoint — 23 October 2026 (Europe/Moscow): main after #229

Main is `155f9e1e4f9fee7ce5e440b1fc57595d9dc0df48`, merged #229,
including reviewed `ec135c4` (GitHub Actions 87/87). Formats are
**save v94 / orders v38**. Orbital habitats and both review corrections below
are on main. #59 is closed; broader #56 stays open; #63 stays frozen last.

## Completion audit branch — science and technology #44

`codex/science-progression-completion` starts from that main. The
[requirements matrix and completion decision](audits/2026-10-23-science-progression-completion.md)
cover the implemented science foundation. A shared allocation helper makes the
Research guarantee honor the existing one-point first-receiver reserve; core and
Qt regressions compare pending-order forecasts with actual resolution. Obsolete
combat guidance is corrected. Review follow-up freezes the receiver tier with
the received queue, including delayed completion/loss and persistent reports.
The branch uses **save v95 / orders v38**; old v94 packets await fresh receiver
evidence without host-state reconstruction. Balance and host allocation rules
are unchanged.
Seven focused core suites passed locally; Qt/save/full validation belongs to the
pushed commit's CI because this environment lacks Qt/CMake. #44 remains open
pending review, successful CI and merge, with optional capability/AI expansions
kept in their own issues. Historical local Qt results below belong to prior work.

## Historical checkpoint — 22 October 2026 (Europe/Moscow): main after #228

Main is `697bec011e0babddd0459a43dffc7c816d4c0144`, merged #228.
#59 is closed. Formats: **save v93 / orders v37**. Prior #59/#227 branch wording
below is historical; physical samples, working artifacts, delivered observation
unlocks and science delivery receipts are on main.

## Historical implementation branch — orbital habitats #56 (merged as #229)

`codex/orbital-habitats` starts from that main. The user approved the
[complete first habitat design](orbital-habitats.md): distinct orbital residents
under one colony owner/economy, competing for its single engineering slot, and
combined defense under existing invasion/casualty rules. Real construction,
colonist transport, ongoing upkeep, finite emergency reserves, attrition,
evacuation and inherited physical infrastructure form the first slice.

Formats **save v94 / orders v38** add HABI state/annual receipts/private received
operating evidence and explicit orbital bulk-cargo endpoints/route policies.
No rare-material extension or irreversible transformation is included; #63 is
frozen at the end of the queue and #56 remains open. PR merge is left to review.

Validation: GCC 14.2 / Qt 6.12 Debug full application/test build succeeded;
full CTest **87/87** passed (59.48 s, offscreen Qt). After the final System
Details received-owner selection adjustment, the application was rebuilt and
colony workspace, map selection and habitat editor suites passed again.
`git diff --check` passed. New core/editor tests cover paid construction,
mandatory upkeep before research, no extra mining workforce, finite emergency
reserves and attrition, cargo conservation/capacity and full evacuation, orbital
survivors after surface bombing, combined defense/capture, stale-transfer
rejection, same-year station battle/survey ordering, honest AI, annual receipts,
partial construction save/pause/reload/resume, v93 migration, corruption,
private player exports, deterministic replay and orders v38 PBEM.
Existing UI/AI fixtures now explicitly acknowledge station/economic facts instead
of reading live host changes. Balance has no manual gameplay acceptance;
validation is Linux/offscreen Qt, without a cross-platform or sanitizer run.
GitHub CI belongs to the pushed PR head and is reported separately.

### PR #229 review follow-up

Initial head `1bcfacc` passed GitHub Actions build/CTest **87/87** (65.39 s).
Its review reproduced two P2 gaps, now covered by regressions and corrected:

- Ship-building controls and `queueShipDesign` consumed host station services
  while PlayerView had frozen received fits. Both now use received state plus
  admissible pending designs; physical host execution retains its own checks.
  Qt tests exercise hidden shipyard removal/addition, delayed receipt and
  delivered battle loss, including direct order authoring.
- One-year upkeep/output/growth estimates omitted ordinary mining before
  maintenance. Both helpers now simulate mining and service on a copy; the
  engineering blocking explanation includes that known mining too. Core/Qt
  tests compare sufficient/insufficient extraction from empty stores with the
  real next turn (3 RP/+18,750 orbital residents in the supplied case), preserve
  the baseline, and verify a 3-year queue estimate without duplicate mining.

Review follow-up validation: GCC 14.2 / Qt 6.12 Debug full build passed;
full CTest **87/87** passed (62.19 s, offscreen Qt). The habitat editor suite
passed again after strengthening the independent combo/button assertions.
`git diff --check` passed. Original manual/cross-platform/sanitizer limits remain.
Save v94 / orders v38, balance and issue scope are unchanged.

## Historical checkpoint — 22 October 2026 (Europe/Moscow): main after #227

Main is [3326fb6](https://github.com/kotrf/suns/commit/3326fb61b444c8665e890e7ef2714d0b28849399),
merged [#227](https://github.com/kotrf/suns/pull/227), including reviewed follow-up
`b11fc45`. Its GitHub CI passed. Artificial/adaptive magnetospheres and the
received-study prerequisite are implemented, including received Hab displays.
Main formats are **save v92 / orders v37**.

## Historical completion branch — field science #59

`codex/field-science-completion` starts from this main. The
[requirements matrix and audit](audits/2026-10-22-field-science-completion.md)
cover the existing #173/#211/#220/#227 chain. Two confirmed gaps are corrected:
replayed generic scientific data could award RP twice, and owned remote surface
specimens/retention were copied from hidden physical state into PlayerView.
Awards now have stable identities and persistent sorted delivery receipts;
owner-only surface manifests, including confirmed emptiness, travel in ordinary
immutable survey packets. The sample panel labels their observation year.

Formats are **save v93 / orders v37**. The bounded SCIC tail follows EQIX;
existing SAMP physical cargo and order layouts are unchanged. v92 migration
preserves physical samples, assigns pending award IDs, and waits for a new owner
survey instead of synthesizing old surface knowledge from host state. It cannot
reconstruct receipts for already delivered pre-v93 generic science packets.
Local GCC 14.2 / Qt 6.12 Debug full build passed; full CTest **85/85** passed
(56.70 s, offscreen Qt), and `git diff --check` passed. Regression details and
validation limits are recorded in the audit. #59 is prepared for closure only
after review and merge; this branch neither closes it nor merges itself.
#63 is frozen at the end of the queue; remaining large engineering #56 is outside
this task. More artifact kinds/technology families/rewards are optional follow-ups.

## Historical checkpoint — 21 October 2026: main after #226

Main is [14c29ff](https://github.com/kotrf/suns/commit/14c29ff04f43c27f372b6d1bad2318a2b03b0125),
merged [#226](https://github.com/kotrf/suns/pull/226), including reviewed follow-up
`cacb012`. Its GitHub CI passed. Scaled xenocrystal clusters and finite colony
mining are on main; formats **save v91 / orders v36**. #63 remains open.

## Historical implementation — magnetic engineering and field-science unlock #56/#59

The user chose this linked slice ahead of the remaining moving phenomena #55.
`codex/magnetosphere-field-science` starts from that verified main. The basic
Artificial magnetosphere requires Energy/Construction 16, costs 2400 work and
I/B/G 240/120/240 kt, takes at least 30 years and reduces effective radiation by
0–10. Adaptive magnetosphere requires Energy/Construction 18 and the current
owner's delivered completed Compact remnant study, costs twice as much, takes
at least 60 years and reduces radiation by 0–20. Both use the existing single
planetary-engineering slot, ordinary paid production and Starbase/Fortress
shipyard support. Study data does not consume a unique specimen. Capture keeps
investment; adaptive operation still requires the new owner's knowledge/tech.

Owner project/queue evidence is frozen in normal survey reports, with bounded
EQIX scheduling persistence. UI shows requirements, evidence age and forecasts;
AI chooses useful, supported, fully supplied projects through PlayerView/orders.
Formats **save v92 / orders v37**. Local GCC 14.2 / Qt 6.12 Debug build
succeeded; full CTest **85/85** passed (25.44 s). Tests include paid 30/60-year
construction, pause/support/capture, real delayed study delivery, received AI
unlocks/costs, mixed laboratory/engineering queue positions, Qt authoring,
received/pending snapshot save/load, corruption, v91 migration, PBEM orders and
frozen project exports. `git diff --check` passed. Balance has no manual
playtest acceptance yet; CI is attached to the PR. #56 and
#59 stayed open at that implementation checkpoint; #59 completion is assessed above.

PR [#227](https://github.com/kotrf/suns/pull/227), initial head `586fec2`, passed
GitHub CI 85/85 (61.07 s). Review identified one P2: GUI consumers outside the
engineering panel still queried Hab from full host state and disclosed remote
completion/support loss. Follow-up routes System Details, map tooltip/Hab mode
(including terraforming potential), dashboard/colony summaries and portrait
capacity/growth through PlayerView. Physical core calculations remain unchanged.
Qt regressions reproduce the 91→100 completion and 100→91 support-loss cases,
compare texts/marker pixels/size/population displays while reports are pending,
then verify updated Hab and capacity after actual report delivery.
Follow-up GCC 14.2 / Qt 6.12 Debug full build succeeded; **85/85** CTest suites
passed (24.08 s), including map selection, colony workspace and engineering Qt
regressions. `git diff --check` passed. Save v92 / orders v37 are unchanged.

## Historical checkpoint — 21 October 2026: main after #224

Main is [d1a857b](https://github.com/kotrf/suns/commit/d1a857b00d75c0a0a98295d69b63e768674497d9),
merged [#224](https://github.com/kotrf/suns/pull/224), including reviewed head
`7b5c14c` and both forecast follow-ups. CI passed 85/85. Resonant gate pulses
are on main. Formats: **save v90 / orders v36**. Parent #63 remains open.

## Historical implementation — scaled clusters and colony extraction #225

`codex/xenocrystal-clusters-colony-mining` starts from that verified main.
[The next rare-material slice](rare-exotic-materials.md#colony-extraction-225)
scales centers with actual system count (two at Medium/Normal), preserves the
deposit budget and adds whole-unit colony extraction through ordinary paid
mines. Physical mining uses actual infrastructure and the owner's received
discovery; UI/AI use dated private operating snapshots. Save **v91 / orders v36**
append COLX without regenerating existing worlds or adding new orders.
Local validation on 21 October: GCC 14.2, Qt 6.12, Debug/assertions enabled;
full application/test build succeeded, full CTest **85/85** passed in 22.30 s.
New regressions cover all size/density presets, custom rounding boundaries,
spatial/ID determinism, finite colony extraction and ownership transitions,
phase ordering, delayed/reordered private reports, AI hauling, UI estimates,
save migration/corruption and frozen PBEM exports. CI is recorded on the PR.
#63 stays open; merge is left for review.

PR [#226](https://github.com/kotrf/suns/pull/226), initial head `217fd41`, passed
CI 85/85 (59.28 s). Review identified two P2 follow-ups: System Details compared
physical and received population when calculating cargo deltas; a loaded AI
hauler stalled when its last consumer's demand disappeared. Follow-up uses one
received baseline/date for population/output/growth and allows ordinary delivery
to a reachable own store, retaining the goal until received completion. Added
Qt cargo/growth and full AI unload/acknowledgement regressions.
Follow-up local full build and **85/85** CTest suites passed (24.38 s,
same GCC/Qt Debug environment); `git diff --check` passed. Save/orders unchanged.

## Historical checkpoint — 20 October 2026: main after #223

Main is [4a91bd3](https://github.com/kotrf/suns/commit/4a91bd3816a455a1edddca176909e2a6d9b4d106),
merged [#223](https://github.com/kotrf/suns/pull/223), including the visitor refusal
knowledge and malformed VISI reader fixes from reviewed head `0fb4af4`.
The first physical visitor encounter and its playable demo are on main.
Main formats: **save v89 / orders v35**. #46 remains open.

## Historical implementation — resonant gate pulse #63

The user chose another xenocrystal application rather than colony extraction
or additional deposit clusters. `codex/xenocrystal-gate-resonance` starts at
verified main `4a91bd3`. [Resonant transit](rare-exotic-materials.md#spend-resonant-gate-pulse)
uses Energy 18 / Electronics 18 / Construction 16 and eight source-surface
crystals per successful extended hop, doubling finite endpoint range while
preserving ordinary gates and mass/cargo/link rules. UI and basic specialist
AI use received information and ordinary orders, reserve earlier pulse payments,
and cannot spend outdated stock estimates. Save **v90 / orders v36** retain
an explicit optional gate-order flag; older orders default to ordinary transit.
PR #224 review follow-up removes host-state inputs from Cargo/System gate
forecasts and shares accepted command-phase handlers between projection and
resolution. Regressions cover undelivered station loss, move/clear/task ordering,
crystal reservations and fleet reorganization. The repeat review restores a
strict received-date gate check and preserves source/oldest dates through
projected splits/merges, preventing an unknown or stale manifest from becoming
current through a connection at a predicted position. Core/Qt regressions cover
missing telemetry, stale reorganization and genuinely current local splits.
Validation belongs to the PR. #63 remains open; merge is left for review.

## Historical checkpoint — 20 October 2026: main after #222

Main is [f10b5b9](https://github.com/kotrf/suns/commit/f10b5b9bc5aa2d2eb660723a629fdfa4800af409),
merged [#222](https://github.com/kotrf/suns/pull/222), including the review fix
allowing Stop before the first programme acknowledgement. CI for reviewed head
`d2ab870` passed. Autonomous survey probes are on main; #60 remains open for
self-replication and further applications. Main formats: **save v88 / orders v34**.

## Historical implementation — intergalactic visitors #46

The user selected #46 next, before the remaining general moving-anomaly work #55.
`codex/intergalactic-visitors` starts at verified main `f10b5b9`. The first
[visitor encounter](intergalactic-visitors.md) adds rare physical trajectories,
delayed immutable sightings/commands, continuous rendezvous, a 50 kt Germanium
exchange for the existing Multi Cargo Pod blueprint, ordinary paid manufacture,
a player interface and a prepared demo. Save **v89 / orders v35** keep host,
received and in-flight state separate. Validation belongs to the PR. #46 stays
open for richer visitor profiles and gifts; merge is left for review.

## Historical checkpoint — 19 October 2026: main after #221

Main is [5f01933](https://github.com/kotrf/suns/commit/5f01933b7dd65f95a663853752f7241c79424658)
after [#221](https://github.com/kotrf/suns/pull/221). Its tree matches reviewed
`5edd281`: finite xenocrystal logistics, resonance laboratories and both rounds
of review corrections. Main formats are **save v87 / orders v33**. #63 remains
open for further materials/applications.

## Historical implementation — autonomous survey probes #60

`codex/autonomous-survey-probes` starts from that verified main. The first
[autonomous survey slice](autonomous-survey-probes.md) adds a late paid probe
hull/autopilot, bounded deterministic survey programmes, separate onboard
notebooks and delivered HQ updates, normal movement/fuel/combat, dated UI and
fair basic computer construction/launch. Save **v88 / orders v34** retain physical,
confirmed and in-flight state separately. Validation belongs to the PR.
#60 stays open for self-replication, copies, beacons and further applications;
merge is left for review. Next major direction remains #55/#46.


## Historical checkpoint — 19 October 2026: main after #220

Main is [1155f53](https://github.com/kotrf/suns/commit/1155f531d62e9e57d6cb4faf4d3baffdf32b6965)
after [#220](https://github.com/kotrf/suns/pull/220): physical science samples and
observation-gated artifacts, including the review fixes for cargo conservation
and stale telemetry. Main formats are **save v86 / orders v32**. The rare-material
implementation below is a separate review branch; #63 remains open.

## Historical implementation — 19 October 2026: rare exotic materials

`codex/rare-exotic-materials` starts from verified main
[1155f53](https://github.com/kotrf/suns/commit/1155f531d62e9e57d6cb4faf4d3baffdf32b6965),
the merge of [#220](https://github.com/kotrf/suns/pull/220). That main has physical
science samples, the review fixes, and save v86 / orders v32.

The first [#63 slice](rare-exotic-materials.md) adds clustered finite xenocrystal
deposits revealed by delivered DeepSurvey, remote extraction into physical surface
caches, whole-unit cargo and an optional resonance laboratory. The laboratory
consumes eight delivered units once and enables retained artifacts to research
outside the main focus. It preserves paid work through pause/cancel/capture;
ordinary ships, gates and climate projects keep their existing recipes.
The branch adds a dedicated UI, fair basic computer use and save v87 / orders v33.
Validation belongs to the implementation PR. #63 stays open for further materials
and applications. Merge is left for review; next major direction remains #60,
then #55/#46, with broader #54/#61/#56 work still open.

PR #221 review follow-up prevents laboratory completion from creating a ship,
uses received owner snapshots for laboratory state and queue work, and routes
crystal exchanges through mobile-base docking and delayed service commands.
Service deliveries occur after production and cannot fund that year's forecast.
The save/load regression also corrected the pre-existing radar-contact hull
bound that rejected Expedition Base observations.
The repeat review also puts immediate fleet and surface exchanges in one stable
service order; previously due commands retain their saved order ahead of new
submissions. Remote commands still in transit reserve no capacity or material.

## Historical implementation — 19 October 2026: physical science samples

`codex/field-science-samples` is based on main
[2aaca80](https://github.com/kotrf/suns/commit/2aaca809f3829a7a43631d12825fda942a7ee0cd)
after orbital engineering / landing control #219. The [#59 sample path](science-samples.md)
adds finite indivisible specimens, shared cargo/mass, recovery after a delivered
study, physical return and a destructive-analysis versus retained-instrument
choice. Instruments require the owner's delivered observation and matching active
research. Computer empires use the same received knowledge and ordinary orders.
Save v86 / orders v32 preserve separate physical/confirmed/in-flight manifests.
Validation results belong to the implementation PR; #63 is separate work.

## Historical checkpoint — 19 October 2026: before #220

Main is `2aaca80` after [#219](https://github.com/kotrf/suns/pull/219), with orbital
mirrors/shades, projected research settings, legacy conquest-save recovery and
hostile-station landing/founding blockade. Main formats are save v85 / orders v31.
Further #56 engineering, #54/#61 infrastructure extensions and additional racial
traits remain open. Next: #59/#63, then #60 and #55/#46. #53 stays later;
#108 requires a separate discussion.

## Historical implementation — 18 October 2026: orbital climate projects

`codex/orbital-climate-engineering` is based on main
[5049da0](https://github.com/kotrf/suns/commit/5049da0c39cc14bc0d454892e5ccda673f705383).
The first [orbital #56 slice](planetary-engineering.md) adds mirrors and shades,
2,400 paid work capped at 80/year, minerals paid per completed work, and a
supported 0–10-point temperature offset. Physical progress survives queue
cancellation, station loss and capture. The panel uses ordinary pending orders;
foreign observations freeze public progress separately from private investment.
Dated milestones use delayed reports and History. Save v85 / orders v31 preserve
physical projects and received/in-flight evidence in a bounded extension.

[Review follow-up](audits/2026-10-18-orbital-engineering-review.md) addresses two
forecast regressions through a shared planning projection and separately tracks
the pre-existing loader rejection of a conquered planet with a surviving enemy
station. Legacy saves with separate owners remain readable. In a subsequent
explicit user decision, any hostile station (including an unarmed dock) now
blocks landing/founding until destroyed, with a delayed fleet-owner warning.
Space combat clears participant routes; landing requires a new order after
orbital clearance. Stations are not captured automatically.
Formats remain save v85 / orders v31. Validation results belong to PR #219.

This branch is prepared for PR review, not an assertion of merge or manual
acceptance. Broader atmospheric/magnetic engineering, habitats, system projects,
balance and autonomous AI selection remain later #56 work. Validation belongs
to the implementation PR.

## Verified checkpoint — 18 October 2026

Main is [5049da0](https://github.com/kotrf/suns/commit/5049da0c39cc14bc0d454892e5ccda673f705383)
after expedition support bases [#218](https://github.com/kotrf/suns/pull/218).
The implementation head passed local Debug **73/73** and GitHub CI before the
squash merge. Main formats are **save v84 / orders v30**. This records code and
validation evidence, not manual gameplay acceptance.

The first three computer-player phases #188/#189/#190 and colony plans
#207/#201 are complete. Stargate Specialist #213, the first paid depot/relay
slice of #54, audit fixes #216, context inventory #217 and the first
[mobile-base slice](mobile-expedition-bases.md) of #61 are merged. Do not restart
them from older next-step notes. Mobile shipyards and science modules remain
later #61 work; neither infrastructure issue is complete.

The next ordered major direction is #56, then #59/#63, #60 and #55/#46.
Transport #53 remains later; #108 needs discussion. For continuation and rationale
start with [PROJECT_CONTEXT](PROJECT_CONTEXT.md). Dated checkpoints below preserve
what each branch knew at the time; verify main and PRs again before new work.

## Historical checkpoint — 17 October 2026: audit branch before merge

Baseline: verified main `1165dce` after deep-space infrastructure #214/#215.
The [17 October audit](audits/2026-10-17-code-review.md) was imported from
`codex/audit-findings` / `5db0b49`. All six findings have fixes and regression
coverage in this branch: frozen foreign environment and fresh survey merging;
gate-hop inheritance across merge/split; shared movement effects with annual
radiation exposure tracking; usable-weapon combat distance; and name validation
before the nonmodal ship editor accepts its draft.

Save v83 adds a bounded survey-snapshot extension while keeping the v82 body
and orders v29. Older host evidence requires resurvey rather than reconstruction
from hidden physical state. Tests retain assertions in Release/RelWithDebInfo.
Validation and implementation commit IDs are recorded in the audit follow-up
and implementation PR. This section does not imply the branch has been merged.

## Historical checkpoint — 17 October 2026: deep-space infrastructure

Main baseline is PR #213 (`4cb245c`), the first playable primary trait. The
next ordered item #54 now has a first playable slice:
[paid constructors, fixed fuel depots and communications relays](deep-space-infrastructure.md).
Constructors physically travel to arbitrary coordinates and retain their paid
hull, fitted equipment and stable FleetId after permanent deployment. Finite
fuel deliveries, 1 mg/year upkeep and relay loss affect actual services; ordinary
combat and delayed telemetry apply. View → Deep-space infrastructure provides
construction coordinates, received site stocks, route refueling/supply and a map
layer. Save v82 / orders v29 add bounded deployment metadata and supply reserves.
Validation evidence belongs to this PR.

#54 remains partial for additional site types, capture, expanded construction,
maintenance/traffic statistics and autonomous infrastructure planning. The next
major ordered item is mobile bases #61, followed by planetary engineering #56,
field science/materials #59/#63, autonomous probes #60 and moving/transient
phenomena #55 / late visitors #46. #53 remains deferred; #108 requires discussion.

## Historical checkpoint — 17 October 2026: Stargate Specialist

Main baseline is PR #212 (`2b118d3`), surveyed interstellar clouds; branch and
post-merge main CI passed 66/66. The next ordered roadmap starts with #50:
[Stargate Specialist](stargate-specialist.md) is selectable in campaign setup,
carries cargo/colonists through owned gates and unlocks paid Network and
Infinity/Infinity station fittings. Both endpoints retain their individual
limits; an ungated frontier still needs conventional travel. Computer empires
research and construct networks through ordinary costs, stage civilian/military
fleets through received gates and wait for arrival telemetry.

Save v81 / orders v28 append component/report IDs with the v80 body layout
unchanged. Existing generalists retain ordinary finite unloaded gates. Validation
evidence belongs to this PR. #50 remains partial for additional primary traits,
mobile habitats and comparative balance.

The approved sequence continues with deep-space infrastructure #54, mobile bases
#61, planetary engineering #56, field science/materials #59/#63, autonomous probes
#60 and moving/transient phenomena #55 / late visitors #46. Habitat civilization
depends on infrastructure and mobile services; transport #53 remains late and
#108 still requires a separate discussion.

## Historical checkpoint — 17 October 2026: interstellar medium

Main baseline is PR #211 (`bdc0e11`), persistent unknown landmarks and science
expeditions; branch and post-merge main CI passed 64/64. This change implements
three linked first slices of #105: [broad persistent cloud profiles](interstellar-medium.md),
segment-integrated fuel/active-sensor effects, and delivered surveys with an
optional map layer and explicit navigation uncertainty. Forecasts and computer
empires use received evidence; raw host clouds never enter player exports.
Save v80 / orders v27. Existing v79 campaigns retain uniform space; new galaxies
generate clouds independently of stellar/planetary generation.

Density-dependent safe Warp, route alternatives/AI detours, ionization and radio
propagation remain further #105 work. #55/#59 retain the moving-object, sample
cargo and unique-find stages documented below. Validation belongs to this PR.

## Historical checkpoint — 16 October 2026: unknown objects and expeditions

Main baseline is PR #210 (`b80ee47`), contamination/cleanup and dated aftermath;
its branch and post-merge main CI passed 62/62. Earlier interception #208,
reconstruction #209 and colony plans #207/#201 are also complete.

This change adds two linked slices: [persistent unknown objects](unknown-objects.md)
with uncertain detection and delayed classification (#55), and explicit finite
Field research expeditions (#59). A received-object journal offers ordinary
Fly / Fly and study routes; computer empires use the same received targets,
fuel/command constraints and research rewards. Discovery and study History follow
the receipt year. Raw hidden objects and pending evidence never reach player files.
Save v79 / orders v27. Validation evidence belongs to the implementation PR.

Moving anomalies, physical sample cargo, unique artifacts, observation prerequisites,
biosphere/scorched-earth systems and advanced SIGINT remain later work. Older
checkpoints below are historical; do not restart completed functionality.

## Historical checkpoint — 16 October 2026: contamination and dated aftermath

Main baseline is #208 physical fast/slow interception and #209 persistent ruins /
paid reconstruction, both automatically merged. Their merged main `f7848b0`
passed full CI 60/60. Colony-development plans #207/#201 are also complete.

This change continues #106 with two bounded stages: [residual contamination and
paid cleanup](planetary-contamination.md), plus immutable observer-specific
aftermath estimates with normal report latency. Neutron weapons add a capped,
separate hazard; natural decay and ordinary paid cleanup remove it. Owners see
costs/forecasts, foreign observers see delivered dated ranges, and History records
receipt-year recovery/discovery milestones. Save v78 / orders v27 preserve the
new state while reading previous formats. Validation evidence belongs to the
implementation PR, not an inferred roadmap status.

Biosphere damage, scorched-earth choices, technology/project balance and advanced
SIGINT remain later work. The older checkpoints below are historical; do not
restart completed AI, stargate or colony-template work.

## Historical checkpoint — 16 October 2026 (before #204/#207)

PR #203 is merged into main `1c5ec0494292f2c74b4804a3a6ebafdbb90af479`.
Its branch CI and post-merge main CI passed 56/56 on Qt 6.12.0 / suns-vps.
Silent/passive fleets receive commands independently of TX; friendly orbital
colonies exchange full status locally and forward immutable packets. Radar
positions remain separate from onboard telemetry away from friendly orbit.

The next #204 branch implements terminal waypoint work: mine laying/sweeping,
bombardment, field research and wreck recovery alongside Remote Mining. The
single route editor replaces stationary Assign; Start work here uses that same
arrival selector at an idle fleet's reported location. Stop/new travel does not
change emissions. Computer bomber routes use ordinary bombardment-on-arrival.
Save v74 / orders v24 retain actions in all route/command/telemetry/PBEM paths.
Local focused core and UI/save CI evidence is recorded in its implementation PR;
this branch is not merged merely because #203 was authorized.

Founding with no chosen/default program already starts with no radar and an
empty queue in main. This branch fixes the remaining #201 conquest exception:
new-colony defaults are no longer applied to captured worlds. Conquest always
starts with no radar and an empty queue, including when a settlement default
is explicitly enabled. The dedicated template editor and per-Colonize frozen
snapshots remain #201. #205 remains a
design discussion, without new hardware or independent channel switches.

Fair computer empires' initial three phases are complete in #188/#189/#190 and
#184 is closed. The older snapshot below is history, including its historical
format numbers and next-stage statements; do not restart completed AI work.

## Historical checkpoint — 13 October 2026

Snapshot: **13 October 2026, Dialog #08 Human/Computer**. This branch starts
from main `14fa936d4e195a7904b47f71b9f29c3dc653c8a5`, after reconnaissance/EW
PR #187. The user requested the next controller/ordinary-orders slice of #184.

### Original controller integration — 13 October 2026

[#184](https://github.com/kotrf/suns/issues/184), step 1: campaign setup offers
Human/Computer control; controller choices and a bounded 32-decision journal
survive save/load. End Turn gathers computers' ordinary PlayerOrders from their
own PlayerView, then resolves all empires through the regular processor. PBEM
identities, files, imports and readiness apply only to humans. Restart preserves
controller choices and resets decision memory. Game → Computer decisions…
displays the host journal.

The initial controller sets a zero research budget to 20% once, resumes a paused
plan and preserves the existing focus/queue/nonzero budget. It then retains
standing programs through explicit passes. Independent expansion/economy and
military behavior remain subsequent stages; #184 stays open.
See [Computer empires](computer-players.md). Save **v67**, orders **v21**.
The full local Qt 6.4 Debug suite passed **55/55 CTest targets**. New checks
cover fixed-seed/reordered/hidden-state-independent decisions, equivalent ordinary
processing, bounded logs, mixed readiness, real human-only PBEM export, restart,
save/load and malformed controller data. The setup and decision windows were
visually inspected. CI results are recorded on the implementation PR.

## Completed preparation

[#186](https://github.com/kotrf/suns/issues/186), continuing #45/#58/#101,
adds immutable delayed enemy hull snapshots with confidence, uncertainty,
mass/hull evidence and last-seen dates. Intel survives lost sources; stale,
duplicate and out-of-order packets cannot overwrite newer knowledge. An
Intelligence window, map markers, briefings and enemy route selectors share
player-known evidence. Sanitized unknown fleets no longer invent a ship stack
or fail PBEM serialization.

Observers report sensor interference/clearance and effective ranges. Fleet and
station jammers affect sensing and source communication delay; anonymous
colony/ship emission reception and decoys retain one-year uncertainty areas.
Radio silence supports scheduled communication windows, optional active scans
and arrival wake-up, with an automatic finite failsafe.
See [Reconnaissance and emission programs](reconnaissance.md).

Save **v66**, orders **v21**. Validation includes detached/stale/lost contacts,
classification/hysteresis, reordering/duplicates, jamming, burst/arrival behavior,
source isolation, save/PBEM/malformed data and the Intel/emission controls.
The full local Qt 6.4 Debug suite passed **53/53 CTest targets**. The Intelligence window was visually inspected; CI results are recorded on the implementation PR.

Station fitting/combat and colony defenses #183 were completed in PR #185,
main `d44c4f5`, with local and branch/main CI **52/52**, Qt 6.12.0 on suns-vps.
See [Orbital stations and colony defense](orbital-stations.md).
PR #182 completed receipt-year ground, bombardment and mine casualties under
#181; #48 remains open for refinement. See [Empire history](empire-history.md).

Designer, Research and History remain separate non-modal windows. Workspace v6
retains main-window geometry and keeps these tools independent of dock presets.
Main already includes [strategic fleet operations](strategic-operations.md):
operational bombs, three minefield types/sweeping, cloaking, jamming, emission
modes/decoys and finite field observations/novel wreck recovery. Broader
#48/#58/#59/#101 remain partial; do not close umbrellas for this slice.

## Implemented since the previous handoff

| PR | Delivered behavior | Details |
| --- | --- | --- |
| [#158](https://github.com/kotrf/suns/pull/158) | Discovered, classified, transient wormholes with independently drifting mouths, explicit entry and delayed transit/loss reports; issue #57 closed | [Wormholes](wormholes.md) |
| [#159](https://github.com/kotrf/suns/pull/159) | 15 reference engines, research/access gates and original fuel curves with documented Suns! adaptations | [Propulsion](stars-propulsion.md) |
| [#160](https://github.com/kotrf/suns/pull/160) | 32 reference hulls, typed fitting banks and hull-access families | [Hulls](stars-hulls.md) |
| [#161](https://github.com/kotrf/suns/pull/161) | Galaxy size/density presets and fleet orders, fuel and cargo in one panel | [Density](galaxy-density.md) |
| [#162](https://github.com/kotrf/suns/pull/162) | Compact map markers/orbital fleet groups and selection of nearby systems | [Map](map-visuals.md) |
| [#163](https://github.com/kotrf/suns/pull/163) | Orbital fleet selector and a separate fleet summary | [Fleets](fleet-management.md) |
| [#164](https://github.com/kotrf/suns/pull/164), [#166](https://github.com/kotrf/suns/pull/166) | Persistent colony status/minerals/production and system details beside the map; expanded map; physical racial habitability by default | [Workspace](dockable-workspace.md), [Races](race-profile.md) |
| [#165](https://github.com/kotrf/suns/pull/165) | 16 reference scanners, 50 ly starting ship radar, independent ordinary/penetrating ranges | [Scanners](stars-scanners.md) |
| [#167](https://github.com/kotrf/suns/pull/167) | Six mining robots with technology/access gates and concentration-dependent rated output deposited on the surface | [Mining](stars-mining.md) |
| [#168](https://github.com/kotrf/suns/pull/168) | Paid physical terraforming, annual factory/mine/terraform rules and empire production templates | [Terraforming](terraforming.md), [Production](production-queue.md) |
| [#169](https://github.com/kotrf/suns/pull/169) | 113 additional reference modules, typed compatibility, research/access gates and catalog search/filters | [Equipment](stars-equipment.md) |
| [#170](https://github.com/kotrf/suns/pull/170) | Automatic simultaneous-motion fleet encounters, beams/torpedoes, shields/armor, ship losses and delayed immutable reports | [Space combat](space-combat.md) |
| [#173](https://github.com/kotrf/suns/pull/173) | Bombardment, physical minefields, cloaking/EW, emission modes and finite field science, available in fleet operations | [Strategic operations](strategic-operations.md) |
| [#175](https://github.com/kotrf/suns/pull/175) | Immediate Warp editing for a selected route leg, per-leg fuel/remaining fuel and a red first fuel-limited leg; issue #174 closed | [Route forecasts](route-forecast.md) |
| [#179](https://github.com/kotrf/suns/pull/179) | Receipt-year space-battle casualties/report counts, save v63; #178 closed | [History](empire-history.md) |
| [#180](https://github.com/kotrf/suns/pull/180) | Official and distro Qt qmake diagnostics for the self-hosted runner | CI workflow |
| [#177](https://github.com/kotrf/suns/pull/177) | Separate Designer, Research and History windows; larger knowledge-safe Hab stars; issue #176 closed | [Workspace](dockable-workspace.md), [History](empire-history.md), [Map](map-visuals.md) |

Reference equipment follows Stars! progression and ratings. Suns! remains its own
simulation: tactical combat rules and terraforming production price are documented
adaptations, not claims of an exact Stars! implementation.

### Historical boundaries — 13 October 2026

- Combat treats all different owners as hostile; unarmed fleets do not fight
  each other. Planetary scars, retreat
  doctrines and diplomacy remain unimplemented. Station hull prices and surface
  protection are initial Suns! balance for playtesting. Between battles, damage still
  uses a fleet-wide percentage rather than individual damaged-ship records.
- Battle reports obey communications delay. A destroyed remote fleet's last
  confirmed contact remains until its result arrives. Navigation forecasts omit
  enemy fleets and cannot resolve hidden battles or reveal their outcomes.
- Normal terraforming requires both technology and paid production work. Its
  natural baseline and axis limits persist; it does not silently widen the race's
  environmental ranges. Pre-v59 automatic adaptation remains a legacy rule.
- Reference hull/mining access families are not complete asymmetric Stars! race
  traits. Mystery Trader equipment is not acquired through ordinary research.
- Multiplayer is asynchronous file exchange with a trusted host. Network
  transport, signatures, an append-only resolution ledger and autonomous AI
  expansion/military policies remain future work. Computer controllers and
  ordinary automatic submissions are integrated. Detached delayed enemy-contact snapshots are implemented; advanced traffic,
  physical decoy probes and conditional doctrines remain follow-ups.

## Formats and CI

The verified main checkpoint above uses save **v83** and orders **v29**.
Reader compatibility depends on the current code. Host and players must use
the same build. Version numbers in older thematic documents describe when a
feature was introduced unless explicitly identified as current. Earlier casualty
history remains unknown; old reports must not invent measurements. Existing
legacy designs and campaigns retain their documented rules.

**Prototype save policy (11 October 2026):** backward compatibility of save files
is no longer a requirement. Future save-format changes may reject old campaigns
instead of adding migrations, with a clear unsupported-version message and a
new game. Existing readers can remain until the corresponding code is simplified;
this decision does not authorize a separate wholesale removal of legacy mechanics.

GitHub Actions dispatches builds to our self-hosted Linux/X64 runner with label
`suns`. The current verified runner is `suns-vps` with Qt 6.12.0. Older
`suns-eph-*` and `suns-eve` names are historical. The workflow runs on pushes into this repository,
including merges to main. Each merge can therefore create a new main build.
Public fork PRs do not execute through this push-only workflow. Do not switch to
GitHub-hosted runners without the user's decision.

## Historical remaining work — 13 October 2026

The list below is the original controller-stage handoff, not today's backlog.
For current priorities use the verified checkpoint above. It is not authorization
to start every item:

- [#44](https://github.com/kotrf/suns/issues/44): the research system and equipment
  catalogs are implemented, including initial bombardment/minefield/stealth;
  remaining capabilities include stargates and full racial-trait systems.
- [#45](https://github.com/kotrf/suns/issues/45): operational, contact, wormhole,
  ground- and space-battle messages exist; continue from unmet briefing/intel
  requirements rather than rebuilding the message pipeline.
- [#47](https://github.com/kotrf/suns/issues/47): dockable panels, shared stable-ID
  selection, presets, permanent colony details and expanded map exist; the
  separate shared GameSession refactor remains pending.
- [#48](https://github.com/kotrf/suns/issues/48): empire/colony graphs, extraction,
  freight, fleet scopes and battle markers exist. #178 implements receipt-year
  space-battle losses/report counts; #181 adds ground/bombardment/mine
  categories. Further history refinement remains a follow-up.
- [#94](https://github.com/kotrf/suns/issues/94): logical slot migration, graphical
  fitting, drag/drop, keyboard controls and catalog filters exist; finish the
  live UI acceptance review and remaining polish.
- [#50](https://github.com/kotrf/suns/issues/50): full asymmetric racial rules.
  [#56](https://github.com/kotrf/suns/issues/56) retains system-scale engineering
  beyond the implemented colony terraforming slice. Transport networks
  [#53](https://github.com/kotrf/suns/issues/53) remain late in the roadmap.

The agreed sequence is **stations/colony defense (#183, done) → reconnaissance/EW
(#186, done, PR #187) → computer empires (#184)**. Controller/ordinary orders
are implemented in this branch. Next are independent exploration/colonization,
economy and research, then military behavior. AI uses deterministic
C++ decisions on PlayerView, legal race profiles and ordinary costs, technology,
fuel and communication constraints; no hidden host knowledge or computer-only
economic/race advantages. Wider #45/#58/#101 stay open for remaining refinements;
they must not postpone AI by adding new prerequisites.

Scars (#106), stargates and full race traits (#50) remain separate backlog work,
not extra prerequisites before AI. Transport #53 remains late; #108 needs a
separate discussion. Preserve dated snapshots as history.

Implementation goes through a branch and reviewed PR; merge only on the user's
explicit instruction. GitHub [#123](https://github.com/kotrf/suns/issues/123) and
the [Notion handoff](https://app.notion.com/p/3e47451cd64a81aa9bc2dca95689ad86)
mirror the project context. Preserve dated older snapshots as history.
