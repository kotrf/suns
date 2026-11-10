# Project status and development handoff

## #285 occupied-orbit startup review — 10 November 2026

Review 5834064968 on `241881e` reported a P3: a neutral surface with another
player's deployed independent shipyard accepted a native startup, creating two
orbital installations. Reproduced with the ordinary first-turn Deploy command;
the otherwise identical host starts successfully when the yard is removed.
Startup now checks `orbital_base_at` before any mutation, matching station
construction's occupancy rule. The regression preserves the existing yard,
residents, cargo, knowledge, designs and all identity counters on rejection.

Full GCC14.2/Qt6.12 Debug application build and **124/124 CTest in 125.38 s**
passed. Previous source CIs 42708044839 (#285, 124/124) and 42709755665
(#286, 125/125) succeeded. This parent fix also belongs in dependent #286;
its four new founding review findings are being handled in that branch.
Current publication/CI are in GitHub. No PR merge or thread resolution is
performed for this request. Main remains `0922375`; save v118 / orders v49 /
race v3 and minimal realtime estimates are unchanged. #50 stays open, native
Human/Computer setup stays gated, and #53/#63 remain frozen.

## #285 repeat review: explicit campaign intent and cheap queue skips — 10 November 2026 (MSK)

Reviews 5832950665 and 5833013783 on `9f32816` confirmed the previous four
fixes and reported a P2 and P3. Both were reproduced before implementation
changes. A supported File → New campaign now validates its proposed empires
independently of the loaded unavailable race, committing those settings only
after generation succeeds. Refused Restart preserves the game, orders, save
path, title and explanatory status; menu fit and file detachment require success.

The bounded production estimate shares the host's racial applicability rule.
Stale native surface industry, including Annual rules, is a free Currently
skipped row, preserving the following miner's four-turn estimate and every
input index for reorder/cancel. Unknown earlier work remains uncertain. No
future-year simulation was introduced.

Full GCC14.2/Qt6.12 Debug build and **124/124 CTest in 129.08 s** passed.
Regression coverage uses the actual File New campaign dialog, Restart QAction,
save-path/title retention, all eight incompatible industry kinds, actual Qt
queue reordering/cancellation and four host turns completing the paid miner.
Previous source CI 42676434534 (#285, 124/124) and 42677876109 (#286, 125/125)
are successful. New publication/CI and propagation to dependent #286 are tracked
in GitHub and subsequent checkpoints. Save v118 / orders v49 / race v3 remain
unchanged. Neither PR nor review threads are closed by this request. #50 remains
open, native Human/Computer setup is gated, and #53/#63 remain frozen.

## #285 review fixes: project identity, viable startup, industry and restart — 9 November 2026

Review 5827903909 on `b530098` reported one P2 and three P3s. All four were
reproduced against that source with real core turns and Qt controls before
implementation changes; the previous source CI 42631805031 was green (123/123).

Production list, button and selection now use the received design projection.
Owner-scoped names identify pending designs and survive a temporary ID matching
a foreign project. Saving and reopening Designer still recheck fresh access
before deferred refresh. Custom startup must actually produce work at its
current Energy, rejecting zero-output budgets atomically. Native basic/advanced
factories/mines, upgrades and reconstruction are inapplicable in single/batch
orders and planning; incompatible templates reject before changing the queue.
Stale partly built, paid and automated rows retire before spending/completion.
The UI omits these choices and keeps ordinary production available. Load retains
the primary trait; Restart refuses an unavailable civilization without changing
the game, pending orders or exchange identity.

The full GCC14.2/Qt6.12 Debug application build and **124/124 CTest in 125.41 s**
passed. Final startup/Qt/live-access tests passed 3/3 in 0.84 s, preserving live
research/acquisition checks. New tests use actual one/two-draft Designer saves,
foreign-ID collisions, decoded PBEM industry orders, template applicability,
Native→Generalist control restoration and real host load/restart. Formats remain
save v118 / orders v49 / race v3. These fixes also belong in dependent #286;
current propagation/source/CI are in GitHub. No PR merge or thread resolution
is performed for this review request. #50 stays open, native setup stays gated,
and #53/#63 stay frozen.

## Native startup and replacement economy (#50) — 9 November 2026

PR #284 merged as main `092237560a905a6601f38d0af34bfafa1f44fbef`, tree
`46a6c70281fb4f0ad151cee48a478c95bdf87f8d`, exactly the reviewed `62f31e4` tree.
Review 5825691221 was clean; all six confirmed-fixed threads were resolved at
the authorized merge under MEM-34. Source CI 42612184928 and main CI 42624373395
both succeeded (122/122). Work continues in the existing `/workspace/suns`,
branch `codex/native-orbital-start-50`.

Station residents previously generated zero production, preventing replacement
manufacture. The native prototype now derives output from residents and Energy
using deterministic integer square root: 50 resources/year at 25,000/Energy 0,
100 at Energy 3, capped by the live inhabited hull. Surface assets and T/G do
not contribute. Ordinary generalist startup still yields 29. Native access to
MiniMiner/Robo-Mini-Miner is narrow; no research level or ARM flag is granted.

An atomic first-turn host construction kernel consumes the ordinary initial
asset bill: 193 work and 72/9/26 kt for Dock/services, Full receiver and one
working Quick Jump 5/Robo-Mini-Miner. The declared default endowment leaves
100/100/100 kt in stock, with 25,000 station residents and a full 210 mg tank.
It rejects insufficient budgets, incompatible profiles, player views and repeat
starts. Ordinary design and production orders build a paid replacement in four
years while mining supplies real stocks. Removing the original stops extraction.

The full GCC14.2/Qt6.12 Debug application build and **123/123 CTest in 125.74 s**
passed. Coverage includes ordinary replacement orders, exact work/mineral costs,
narrow technology access, real mining/no-miner turns, atomic rejection,
host reload, decoded PBEM orders and received output after unseen station loss.
Save v118 / orders v49 / race v3 stay unchanged. See
[stage 3 budget and limits](asymmetric-empires.md).

This is a prepared-host startup kernel, not new-campaign integration or actual
Computer startup/expansion acceptance. Human/Computer setup/import/restart stay
closed. Next: paid founding, native radiation/protection/growth/support, hull
progression and actual Computer startup → replacement → paid second colony.
#50 stays open; #53/#63 remain frozen. Current source/PR/CI are in GitHub.

## #284 repeat review: evacuation link, History sites and map — 9 November 2026

Review 5825137484 on 011e9d4 confirmed all three prior fixes. Its source CI
42604101592 succeeded with 122/122 tests. The new P2 and two P3s were reproduced
on that source in core and actual Qt controls before changing production code.

Dockside cargo now shares one current-link admission rule between host execution
and command-boundary projection, rechecked after every preceding order. Fully
evacuating the radar capital can remove its network root while another living
colony remains without radar. The editor detects the broken first sequence and
selects ore unloading before evacuation; preview and real execution agree.
Legacy mineral-manifest orders use the same check. Existing mobile-base routing
and received-state planning boundaries are retained without annual simulation.

Remote-extraction scope, comparison and Follow map admit sites only from recorded
rows. Genuine v117 host archives stay intact and unknown sites match the player
export's empty choices; a newly received mining row makes the site available.
Mining Show on map uses its recorded system/position while retaining the source
fleet ID for transmission and identity. A moved miner cannot drag that historical
world report along; missing-star fallback and normal fleet-arrival focus are tested.

12/12 focused CTest passed in 2.36 s, including Cargo, communications, mobile bases,
native history/save, received population and real report navigation. The existing
remote-population fixture now checks host rejection outside communications and
retains its stale-report population assertions after restoring a known live link.
The full GCC14.2/Qt6.12 Debug application build and **122/122 CTest in 73.80 s**
passed. 139 local documentation links and diff checked.
Formats remain save v118 / orders v49 / race v3. PR #284 stays open for repeat
review and threads await an authorized merge. Native paid startup/founding,
economy/growth/radiation/hulls and actual Computer acceptance remain incomplete;
#50 stays open, the trait stays disabled, and #53/#63 remain frozen.

## #284 review: mining source, Cargo and legacy export — 9 November 2026

Review 5824506446 on b808664 reported one P2 and two P3s. Source CI 42530957705
succeeded with 122/122 tests. All three findings were reproduced on that source:
neutral-world extraction appeared in History via a transmitting bystander,
the native station was absent from the Cargo editor, and genuine-v117 physical
extraction remained in the exported player history despite its unrecorded flag.

Mining now combines stacks per actual miner and queues its manifest with that
fleet ID during the mining phase. Passive/Radio silence outboxes stay with their
source even alongside an unrelated transmitter; mixed fleets cannot combine
unreceived ore into received output. Distinct miners retain distinct report IDs.
Waking the real source releases its stored output; losing it does not hand its
unsent report to a bystander. Friendly-colony local exchange still works.

Cargo offers the live native station as an endpoint and default source, with its
real residents, shared world stock and hull housing capacity. Mixed people/ore
orders and same-turn reversal use the existing bounded order projection; native
surface population paths remain disabled. Independent habitat mineral fields
retain their original restrictions. PlayerView redacts only unrecorded native
extraction amounts/site details, retaining host archives and received history.
Genuine v117 and v118 host/player exports are covered, without a format bump.

The full GCC14.2/Qt6.12 Debug application build and **122/122 CTest in 75.03 s**
passed; 9/9 focused tests passed in 0.75 s. The styled Cargo screenshot was
inspected; 145 local documentation links and diff checked. PR #284 remains open for repeat review;
review threads await an authorized merge. Native startup/economy/founding and
actual Computer acceptance remain incomplete; #50 stays open and the trait is
disabled for both controllers. #53/#63 remain frozen. Current source/CI are on GitHub.

## #283 merged; #50 native owned-orbit mining — 8 November 2026

The user authorized merge and continuation. PR #283 merged as ef71c253153bc0228cbc377bc7c35c7a1f1740d0,
tree 9b5963ef8aed73a144e9eda2a0313c7da8b6f0b6, matching reviewed 88abe3a. The latest
review confirmed P3 and found no new issues; source CI 42519826411 passed 121/121.
All five confirmed-fixed threads were resolved under MEM-34. Separate main CI
42521693412 succeeded. Work continues in the existing checkout on
codex/native-orbital-mining-50.

The next #50 slice supplies one remote-site rule for neutral worlds and a native
empire's own live inhabited station. Work-here authoring, route authoring,
stationary command delivery, arrival, physical extraction and rate projection
share that rule. Known hostile colonies and ordinary owned surface worlds remain
ineligible. UI admission and rate estimates use received state; execution rechecks
physical ownership at delivery. Native surface/population-powered extraction is
explicitly zero.

Output is deposited once in Planet::minerals, never the miner's hold. The native
orbital cargo endpoint accesses that shared stock; ordinary independent habitats
retain their resident-only orbital endpoint. Native mining manifests use normal
communications, including passive/silent outboxes. They are emitted when mining
actually occurs, before later movement/combat, and History credits them by receipt
year rather than the hidden physical extraction year. Past rows remain unchanged.
Save v118 admits the new report/event enums; no base payload/order/race layout is
changed. Native pre-v118 extraction rows retain amounts but have no certified
receipt measurement. A genuine v117 fixture was generated with the merged old
core/save writer before introducing those manifests. Orders v49/race v3 remain.

The initial owned-work regression failed on merged #283. Core tests now cover
start/stop, arrival timing, normal/hostile ownership, unavailable equipment,
absence of innate output, real battle loss, delayed task revalidation, report
delay and collection of ore without duplicate accounting. UI checks cover owned
work-here and route editing, unchanged admission/rates after hidden ownership
change, and rejection after new hostile evidence. Host/PBEM tests cover pending
and delivered manifests, receipt-year boundaries, unchanged order v49 and genuine
v117 migration. The full GCC14.2/Qt6.12 Debug application build and **122/122 CTest
in 72.42 s** passed. Screenshot inspected; 139 local links and diff checked.
Starter mining access/ship/budget, paid founding,
native growth/radiation/output, stronger hulls and actual Computer expansion are
not completed by these constructed fixtures; the trait stays disabled for both
controllers and #50 remains open. #53/#63 remain frozen. Current PR/source/CI are
on GitHub.

## #283 review: event markers without measurements — 8 November 2026

Review of 757704c confirmed the asynchronous-totals P2 fix; source CI 42425919251
passed 121/121. One nonblocking P3 remained: the no-values paint return also hid
the year axis and confirmed event markers, including ordinary empires. The chart
now keeps those independently available records alongside its empty-data message,
while omitting the quantitative scale when there are no measured values.

A Qt rendering regression failed on 757704c: an out-of-contact ordinary fleet's
damage chart looked identical with/without Energy 2 completed. Both event and
year-label image comparisons now pass, together with the native PBEM gap/tooltip
checks and existing tool-window suite. GCC 14.2 / Qt 6.12 Debug application build
and 2/2 focused UI CTest passed in 1.10 s; the screenshot was inspected. Formats
remain v117/v49/race v3. PR #283 is for repeat review; #50 stays open and the race
remains disabled. Current source/tree/CI are on GitHub.

## #283 review: asynchronous observation dates — 8 November 2026

Review of 6e448e9 confirmed the earlier loss and simultaneous-unload fixes, then
reproduced one P2 with a real route: unload at 170 ly in year 2, return to instant
communications at 145 ly in year 3, receive the station update in year 4. In year
3 the received subtotal is 20,000 residents/200 kt Ironium while physical assets
remain 20,500/203. Reading both sources from PlayerView is insufficient to claim
that their combined values describe one observation date.

Native History now retains the oldest/newest received asset observation year and
a synchronization flag. Differing or missing dates produce gaps in whole-empire
population/mineral graphs, with an explanation and report-date tooltip. Colony
and fleet records remain available. The numeric fields are received subtotals;
consumers must honor the synchronization flag. No pending reports, hidden truth,
or realtime forecasting are used to certify totals. Freight remains credited by
receipt year, and older history boundaries are not rewritten after later receipt.

Save v117 appends bounded provenance metadata to NORB without altering the older
base History layout. Native pre-v117 rows retain their values with unknown date
provenance. A genuine v116 fixture was produced by the original 6e448e9 core/save
writer before layout changes; v114/v115 fixtures and the v107 golden prefix stay
covered. Orders v49 and portable race v3 remain unchanged. The regression opens
an exported PBEM turn and checks actual chart summary/hover behavior. Date bounds,
strict flags, host/PBEM round trips, migration/continuation, older-row retention
and counterfactual hidden-state changes are covered too.

The UI regression failed on original 6e448e9. Seven focused CTest suites passed
in 1.22 s; the full GCC 14.2 / Qt 6.12 Debug application/test build and 121/121
CTest passed in 70.44 s. The screenshot was inspected; all 139 local documentation
links and git diff checks passed. Previous source CI 42408566484 passed 120/120 in 134.26 s. Current
source/tree/CI are on GitHub. PR #283 remains for repeat review; #50 is open and
the race remains unavailable to Human/Computer pending its full playable slice.

## #283 repeat review: received fleet cargo and freight History — 7 November 2026

Review of 6fad4ca confirmed both earlier P2 fixes; source CI 42375151847 passed
120/120 tests. One new P2 was reproduced with ordinary UnloadAll, an actual
one-year communication delay and a reopened PBEM turn. The old station snapshot
was combined with the emptied physical hold, temporarily reducing totals from
10,500 to 10,000 residents and 103 to 100 kt Ironium. Freight was also credited
before its manifest arrived.

Native fleet assets now use the same received PlayerView as their colonies;
already included missing contacts are not appended again. Physical freight is
excluded from native History, which instead credits received FreightDelivered
manifests at their receiving boundary. Packets already due when a host resumes
are assigned to that boundary, not the newly resolved year. Ordinary empire
freight semantics are unchanged. No save/command/race schema changes: v116/v49/v3.

Regressions cover the actual waypoint unload, partial/full housing, independent
minerals, immediate receipt, host save/load, PBEM export/reopen, current-boundary
refresh, subsequent-year non-repetition, overdue-on-load manifests and missing
fleet contacts counted once. The native test fixture now captures its actual
initial transport manifest rather than retaining demo-scout telemetry.
Four focused CTest suites passed in 0.06 s. The full GCC 14.2 / Qt 6.12
Debug application/test build and 120/120 CTest passed in 72.04 s. All 135 local
documentation links and git diff checks passed. Current source/CI are on GitHub.
PR #283 remains for repeat review; #50 stays open and the trait stays disabled.

## #283 review: native History and first-snapshot destruction — 7 November 2026

Both P2 findings on 1c34a36 were reproduced with real combat and saved/exported
states. History exposed year-2 physical loss before the year-3 report, and a
pre-battle snapshot arriving after destruction could resurrect a station when HQ
had no prior fit. Native colony History rows now use canonical delivered state,
including residents, colony count, stock and output. Earlier graph rows stay
immutable when a later boundary receives loss.

A sorted owner-only destruction ledger retains station identity/time even before
its first snapshot. Receipt and snapshot merge reconcile by identity and evidence
time; duplicate/late reports cannot resurrect the old hull or erase a newer
replacement. Ordinary independent habitat residents survive station loss as
before. Save v116 persists this ledger, validates order/identity/time bounds and
migrates received v115 native tombstones without treating pending packets as
received facts. A genuine v115-loss fixture was created from the original
1c34a36 headers/libraries/writer before changing the layout. Orders v49/race v3
are unchanged.

Five focused core/history/save/PBEM/habitat tests passed after both regressions
failed on 1c34a36. GCC 14.2 / Qt 6.12 Debug application/test build and final full
CTest passed 120/120 in 72.93 s. Two local objects lacking header dependencies
were rebuilt before that final run. File links and git diff checks passed.
The preceding source CI 42367058289 succeeded; current source/CI are on GitHub.
PR #283 remains for repeat review; #50 stays open and the trait stays disabled.

## #282 merged; #50 native population/cargo foundation — 7 November 2026

Design #282 merged to main 52003e121e00cef66710abf16a799213f1870182 with the
reviewed d31344e tree d296aee5e07b6153341f32876b16f08b70fc14cb. Source CI
42360981612 succeeded (118/118 confirmed by review); all three fixed threads
were resolved under MEM-34. Separate main CI 42364397607 also succeeded. Work
continues in the existing checkout on codex/orbital-civilization-50.

The first implementation adds station-linked native residents, shared/explicit
orbital cargo, guarded surface shortcuts, battle-only total loss, refit capacity
checks and owner-only frozen/pending evidence. Save v115 adds a bounded extension;
orders v49/race v3 are unchanged. A real pre-change v114 writer/fixture verifies
migration rather than editing a current file's version header.

This is the population/cargo kernel, not a playable native start. Campaign/race
setup stays disabled for both Human and Computer until the paid start/founding,
mining/replacements, native economy/radiation/hull progression and actual Computer
expansion acceptance pass. Housing limits on existing hulls are provisional;
no free station or native campaign is introduced by this slice. #50 stays open,
and frozen work remains separate. GCC 14.2 / Qt 6.12 Debug application and test
build passed. Final full CTest passed 120/120 in 70.48 s, including native core,
save/PBEM, genuine v114 migration, ordinary habitats, station defense/invasion and
existing UI/performance suites. Local file links and git diff checks passed.
Current PR source/tree and separate GitHub CI are recorded in GitHub.

## #282 user decision: shared native unloading — 7 November 2026

The user chose to retain `UnloadAll` with `Colonists` / `All` for the orbital
race (MEM-36). The common command resolves residents to their own surviving
inhabited station; ordinary empires unload onto the surface. Partial capacity
leaves excess aboard, and absent/destroyed/hostile/full receivers accept nobody.
Legal mineral unloading under `All` remains independent of population capacity.

The [design](asymmetric-empires.md) now requires shared receiver validation in
preview/arrival/PBEM, including loss of the station after authoring, conservation,
no surface fallback and no free founding/invasion. This supersedes earlier
rejection of native shared unloading; separate surface-only final-manifest/exact
transfer commands still require their documented guards. The UI identifies the
station receiver without requiring a second unload command.

This is a design-only update; the playable trait is not yet enabled. Previous
source d2cfff8 CI 42359700772 succeeded. Local document links/diff are checked;
no runtime tests for this revision. Current source/CI are on GitHub, #282 awaits
repeat design review and #50 remains open for implementation.

## #282 repeat review: surface final-manifest bypass — 7 November 2026

Repeat review of 31b809a confirmed both prior P2 corrections and found the
separate `SetFleetColonistsOrder` path behind `Colonists aboard`. Its current
`apply_cargo_order` implementation writes surface population directly, and the
order is decoded from PBEM independently of the transfer/arrival paths.

The [design](asymmetric-empires.md) now rejects native use of that surface order
for both loading and unloading, including the planning preview and decoded PBEM.
Acceptance requires unchanged people/minerals/freight and working explicit orbital
unload/evacuation. Native UI authoring must not offer that surface shortcut.
The user confirmed that station residents do not need surface landing.

CI 42354006277 on 31b809a succeeded (118/118 reported in repeat review). This new
revision is documentation only; links/diff are checked, runtime tests not rerun
locally. Current source and CI are on GitHub. PR #282 awaits repeat design review;
the actual playable trait remains disabled pending the full first-slice acceptance.

## #282 review: explicit cargo/ownership and Computer acceptance — 7 November 2026

Both P2 findings on e6758e1 are addressed in the [design](asymmetric-empires.md).
Native surface cargo and invasion paths explicitly reject without partial cargo,
casualties or ownership changes; explicit own-station orbital transfers conserve
residents and enforce hull capacity. Station destruction clears native ownership;
neither invasion nor unloading supplies a free orbital founding operation.

Computer support is in the first playable slice, with trait availability for both
controllers gated on a real Computer campaign through extraction, replacement
miner construction and a second paid station via ordinary orders and received
knowledge. The design names the current surface-population/Hab/mine planner gaps
and adds delayed/PBEM and cargo/ownership acceptance. These are proposed first-slice
implementation rules; the user's agreed battle-loss/refit rule is unchanged.

Source e6758e1 CI 42300323444 succeeded (118/118 reported by review). This revision
changes documentation only; local links/diff are checked, no new runtime tests.
Current source/tree/CI are recorded in GitHub. PR #282 awaits another design
review; #50 remains open for gameplay implementation. Earlier readiness entries
below predate these findings.

## #50 selected: AR-inspired Orbital Civilization — 7 November 2026

The user selected the station-dwelling direction, resolving the prior trait
choice. Stations house the population and their actual destruction in battle
kills all native residents still aboard; stronger orbital technology/hulls provide the upside.
Mining is exclusively remote and available from the start. Surface temperature
and gravity do not affect the native orbital population; radiation does.

The Player's Guide chapter 22 and Technical Reference Guide were checked directly.
The [updated design](asymmetric-empires.md) separates the reference from Suns!
choices: AR also had innate extraction and retained environmental suitability;
Suns! uses the user's ship-only mining and radiation-only exposure direction.
The user subsequently explicitly agreed to total loss on 7 November: only
residents physically evacuated before destruction survive on their surviving
transports. Pending evacuation intent cannot rescue them. Paid hull replacement
preserves residents; no escape-pod/survivor mechanism is introduced. The casualty
question is resolved; hull/economy/radiation numbers remain prototype parameters.

The draft now requires native station-resident identity rather than treating it
as the existing separate late-game habitat. Actual battle loss is distinct from
paid refit, ordinary damage or upkeep failure. A complete start needs an operable
miner, replacement manufacture, paid founding, real stock/output/support and a
radiation-safe budget. Own-orbit remote mining requires changes to the current
unowned-only authoring/arrival/extraction guards. Existing bases supply a possible
orbital radiation/screen model; exact native curves and economy remain proposals.

The user asked for design review. PR #282's design is prepared for review with
the agreed casualty rule and acceptance cases. This remains documentation:
gameplay is not enabled and save/race/order layouts do not change. File links and
diff checks are the relevant validation; preceding design CI and main ea7749f CI
are successful. Gameplay implementation is the next separate slice.
#50 remains open for implementation; #53/#63 and visitor AI staging stay deferred.
Earlier unresolved trait-selection notes below are historical.

## #46 complete; next queued work is #50 design — 6 November 2026

The user requested continuing the ordered queue. #281 is merged into main
ea7749f7da50659f2255477fdd87ae03ede14a96, #46 closed completed, all five fixed
threads resolved under MEM-34. The merge tree equals reviewed 337982d's
a9eb479b141445d5881e2f8075bdf8fb79c03ecf. Source CI 42223101944 and separate main
CI 42228852797 both succeeded. Current formats remain v114/v49/race v3.

GitHub's open issues and the Notion queue agree: #50 precedes #54/#55; #53/#63
remain frozen. Its complete gate-specialist implementation is not restarted.
The remaining primary-trait choice is not specified by the backlog audit. An
async selection question is pending; no answer or balance approval is inferred.

[Next-trait design](asymmetric-empires.md) inventories the existing code, compares
Habitat Civilization and Remote Logistics Specialist, recommends a stationary
orbital first slice, and requires an entire starting/expansion budget before
enabling it. Current habitats need late technology and a supporting Starbase;
their residents do not operate surface mines. Changing only setup availability
would not produce a viable new civilization. The design identifies founding,
population/workforce, growth, paid support, capture, received knowledge, portable
races and fair computer play as a single acceptance boundary. No new game rules
or numerical balance are implemented. #50 remains open; further traits and
comparative balance are still future work.

Work stays in the existing /workspace/suns on codex/empire-asymmetry-50-design.
This slice changes documentation only; no new gameplay tests are required.

## #281 review: course measurement date and identity — 6 November 2026

[P3](https://github.com/kotrf/suns/pull/281#discussion_r4492655407) on c6385ba:
an active courier can measure a changed course at year 61, immediately fail
navigation, and return a mission result dated 62 that arrives in 63. The shared
event emitter incorrectly used the mission date for the course notice and its ID.
The emitter now takes an explicit date: x.observedTurn for a course notice,
r.observedTurn for the mission result. Delivery time remains the actual receipt.

The real TurnProcessor regression failed against unchanged c6385ba. It now checks
61/62/63, the frozen measurement position, distinct course/result IDs, and the same
course ID when the identical evidence arrives as a sighting instead of a failure.
GCC14.2/Qt6.12 Debug app/affected targets built; **4/4 focused CTest passed in
6.95 s** (visitor core/editor, save game and GameSession).

Review confirmed all prior four P3 fixes. Baseline [CI 42211929215](https://github.com/kotrf/suns/actions/runs/42211929215)
on c6385ba passed 118/118 in 140.33 s; that precedes this new date correction.
Current source/new CI will be recorded in GitHub. Save v114/orders v49/race v3
remain unchanged. PR is open for re-review and closes #46 on merge; frozen and
strategic AI boundaries stay unchanged.

## #281 repeat review: three course-change P3 fixes — 6 November 2026

The prior actual-dimensions P3 is confirmed fixed. All three new findings on
7ee9e38 were reproduced: the nine-shape test failed on a same-side exit; an ordinary
delayed EncounterVisitorOrder with a passive 49-kt G courier received the changed
track through a refusal but no notice; and a synthetic v114 with velocity 1e-20
loaded, then UBSan reported an out-of-range float-to-uint64 conversion.

The entrance is recovered by tracing the unchanged approach backwards. The two
signs of the seeded 20–45° turn are checked in stable order, requiring a different
exit side. Shared load/runtime geometry validates a finite positive whole-year
duration and departure <= 1,000,000,000 before conversion/addition. Bad schedules
are rejected on load; invalid in-memory manoeuvres retain their finite old leg.
Every first accepted changed measurement, including contact receipts, emits a
separate course event before transaction/gift handling; later evidence cannot
repeat it. Original transaction events, dates and deterministic IDs remain.

GCC14.2/Qt6.12 Debug app/affected targets built; **4/4 focused CTest passed in
7.43 s**. Expanded visitor core also passed UBSan + float-cast-overflow with
abort-on-error, covering tiny/subnormal speeds, the maximum date, valid finite
slow tracks, all nine geometries and real paid/refused contact receipts. Full
GCC14.2/Qt6.12 Debug build and **118/118 CTest passed in 134.37 s**. Baseline [CI 42194439143](https://github.com/kotrf/suns/actions/runs/42194439143)
succeeded; current source/new CI will be recorded in GitHub. Save v114/orders
v49/race v3 stay unchanged. PR remains open for re-review, #46 closes on merge;
frozen and strategic AI boundaries remain unchanged.

## #281 expanded: rare visitor course changes — 6 November 2026

The user explicitly chose to add rare turns to the same PR, superseding the
separate-follow-up proposal below. At generation, 1/10 of new visitors may schedule
one interior year-boundary turn, if the original track offers an eligible boundary
with two years remaining. The private schedule and 20–45° left/right choice use
only the independent physical RNG/visitor identity. After the old segment and its
encounters complete, a new ray reaches the expanded map edge at a recomputed
whole-year departure, without increasing speed. The one-active rule remains.

Received and dispatched tracks stay stale until an actual observation/report;
onboard refresh still requires that courier's active sensors. Delivered changed
sightings create a dated course-change message. A measured revision distinguishes
a post-turn boundary observation from an old-segment contact receipt with the
same year, preserving payment and preventing a stale rollback.

Save **v114**, orders v49/race v3. VISI persists the private schedule/completion
and measured revisions in all observation/mission copies; the new GameEvent keeps
its received profile. Pre-v114 visitors remain straight. A synthetic fixture was
written by unchanged b7ac910 before changing the writer, preserving v113 profiles,
pending commands and knowledge. Hidden schedules do not affect player exports.

Visitor core/editor **2/2 passed in 1.19 s** after the final one-active assertion. Acceptance checks cover deterministic
rare schedules over nine sizes, continuity and non-increasing speed, finite bounded
exit, no second turn, one-active flights, a real paid straight control versus a
blind changed-track miss, delayed/duplicate/old reports, local reacquisition,
same-year contact, byte-identical continuation before/after turning, legacy v113
and v112, received-event/profile persistence, parser rejection and UI notice text.
Full GCC14.2/Qt6.12 Debug build and **118/118 CTest passed in 129.55 s**. Prior source b7ac910 CI
[42191078963](https://github.com/kotrf/suns/actions/runs/42191078963) succeeded;
the new source/CI will be recorded in GitHub. #281 stays open for re-review and
still closes original #46 on merge. Frozen and strategic AI boundaries remain.

## #281 P3: actual galaxy dimensions; course-change idea — 6 November 2026

[Review P3](https://github.com/kotrf/suns/pull/281#discussion_r4489605667) on 9dee212
found that the inherited max(200, dimension) expanded supported 100-ly maps.
The crossing now consumes the actual validated wormhole-rule dimensions. The
independent geometry matrix has nine sizes, adding 100×100, 10000×100 and
100×10000 (100:1 aspect ratios). Its new boundary assertion failed against
unchanged 9dee212 before correction (work/review281-before.log).

GCC14.2/Qt6.12 Debug app/affected targets built; **4/4 focused CTest passed
in 6.87 s**: visitor core/editor, save game and GameSession. The original
[source CI 42129102015](https://github.com/kotrf/suns/actions/runs/42129102015)
passed 118/118 in 125.23 s; that is separate from this new source. Current
head/new CI will be recorded in GitHub. No save/order/race version changes.

The user also recalled rare visitor course changes in Stars!. A separate
follow-up is proposed in the visitor contract: rare deterministic heading changes
at year boundaries, bounded frequency, real exit/window recomputation, and old
received predictions until a genuine observation/report arrives. No route change
is implemented in this P3 fix. Unless the user chooses to expand #281, it remains
a next-slice candidate. The original straight-track scope of #281 and the
frozen/AI boundaries are unchanged.

## Visitor trajectories complete original #46 scope — 6 November 2026

The user continued the recorded queue. #280 is merged as 622189f266e1c7ec90c7e9ae1475b669f43790db;
its tree 44fb606553885e4b8a352fa1b6573fd8b0ffd723 matches reviewed 1591bd8.
Both source CI 42018032117 and main CI 42116108385 succeeded. New work remains in
the existing repository, branch codex/visitor-trajectories-46, on that main.

New visitors enter at a seeded point 150 ly beyond any of four edges. A point
inside the central half of the real galaxy defines a ray, clipped to its far exit
on another boundary of the enlarged rectangle. This supports oblique crossings
and extreme aspect ratios without corner grazes. Constant 2D velocity and the
existing ceil(length/121) year budget remain the physical model; rarity, one-active
limit, offers, detection, delays and continuous encounter geometry retain their
rules. Existing visitors/tracks/missions are never regenerated. Formats stay
save v113/orders v49/race v3. Prepared demos now exercise horizontal, vertical
and diagonal arrivals using the same ordinary encounter code.

Independent geometry checks replay years 60–9999 across six galaxy sizes (200²,
400², 1600×240, 240×1600, 10000×200 and 200×10000), require all four starts,
different bounded exits and central crossings, and verify finite speed/replay.
The acceptance test failed on unchanged 622189f's horizontal-only generation.
A real generated oblique visitor also survives save/load and byte-identical
TurnProcessor continuation past departure; a hidden velocity change does not
alter a player file. The original profile/paid AI/knowledge/reader tests exercise
the rotated demos. Visitor core/editor **2/2 passed in 0.99 s**. Full
GCC14.2/Qt6.12 Debug build and **118/118 CTest passed in 122.87 s**. The
diagonal demo and its map forecast were inspected offscreen at 1440×960. Ready
for review; no annual UI forecast or polling was introduced.

This completes the original issue's required rare encounters, received detection,
physical pursuit/payment, varied bounded gifts, concise flavor/portraits and
cancel/failure/departure UX. The PR will close #46 after review/merge. Broader AI
ship construction/staging is not an original #46 requirement and remains deferred,
with its future boundary retained in the visitor contract. Frozen #53/#63 and
transport evacuation remain unchanged. PR/source/CI will be recorded in GitHub.

## PR #280 review: received offer-card status — 5 November 2026

[P3](https://github.com/kotrf/suns/pull/280#discussion_r4479678276) on c1a55e7:
after the real Prospector exchange the table reported a withdrawn offer, but the
card still invited one exchange before year 66. A regression compiled against
unchanged c1a55e7 failed on that missing withdrawn status (work/review280-before.log).

The card now retains price/blueprint as last known terms and explicitly renders
available, withdrawn or observed departure from the received observation. The
announced year is separate; elapsed time means the announced window ended, not
that departure was observed. No hidden visitor lookup was introduced. Coverage
runs actual exchanges for all three profiles, retains historical terms, checks
withdrawn status after the deadline as well, and contrasts an expired announcement
with a delivered departure observation. Hidden physical mutations retain the card.

GCC14.2/Qt6.12 Debug app and affected test targets built; **3/3 focused CTest
passed in 6.18 s** (visitor core/editor and shared session). The prior source
[CI 42008381163](https://github.com/kotrf/suns/actions/runs/42008381163) passed
118/118 in 137.41 s; that is not a claim about the new source. Current head/new CI
are in GitHub. #280 stays open for re-review, #46 stays open for its remaining
scope, and gameplay/save v113/orders v49/race v3 are unchanged by this UI fix.

## #279 merged; visitor offer profiles (#46) — 5 November 2026

The user explicitly authorized resolving the seven confirmed fixed #279 threads,
merging and continuing the remaining backlog in its recorded order. All seven
threads were resolved, then #279 squash-merged as **f471c34bf2f74ef416bbf3feb873ce977de53b9c**;
#47 is closed as completed. Its tree **f1b580fc15c31328f1c3515edfc1f9f314d8bd01**
is identical to source f0b2ca0 (the final 30→60 s session-test timeout correction).
[Source CI 42001051662](https://github.com/kotrf/suns/actions/runs/42001051662)
succeeded. [Main CI 42004522148](https://github.com/kotrf/suns/actions/runs/42004522148)
also completed successfully on f471c34. The permission
for routine resolution of verified fixes is durable in MEM-34; it is not a reason
to resolve an unverified report or skip source checks.

Next in the last reconciled list is #46. Work uses the existing repository and
codex/visitor-offers-46, based on that merged main. The coherent slice adds three
fixed mineral-request/blueprint profiles, selected by the existing independent
visitor RNG and frozen in the physical visitor and every observation/mission/
receipt/event copy. Payment is atomic across I/B/G; one customer consumes the
single physical offer. Blueprint acquisition retains ordinary research/fitting/
manufacturing costs. Existing idle-courier AI reads the advertised profile and
loads the actual requested minerals; broader AI strategy is not resumed.

The panel/map/report text uses only delivered profiles. Three short greetings,
distinct procedural portraits and prepared demos make each offer inspectable.
No new real-time forecasts or polling. Save **v113 / orders v49 / race v3**; all
old visitor records default to Freight without cargo/unlock changes. The genuine
7722-byte v112 synthetic fixture was written with unchanged f471c34-era headers,
core and writer before implementing profiles; its provenance writer and hex data
are tracked. Invalid profiles, duplicate gifts, old migration, PBEM hidden-state
isolation and all nested mission/event profiles have coverage.

Two visitor core/editor targets passed in **0.62 s**. Full GCC14.2/Qt6.12 Debug
build and **118/118 CTest passed in 133.69 s**. The first full run exposed one
stale workforce-test header assertion (112); it now expects 113 while retaining
the unchanged genuine v107 SHA256 check. That focused test and the final complete
run passed. The new offer panel was inspected offscreen at 1440×960. Ready for
review; current PR/source/CI are recorded in GitHub. #46 remains open for
alternative trajectories and strategic AI staging. #53/#63/evacuation remain frozen.

## Follow-up review fixes for #279 / #47 — 5 November 2026

Re-review of source 702d1d4 confirmed the four earlier fixes and the bounded UI
waits, but found two remaining defects. Its [source CI](https://github.com/kotrf/suns/actions/runs/41992024641)
has now completed successfully; that does not cover the new changes below.

- [P2 queued Save](https://github.com/kotrf/suns/pull/279#discussion_r4478103388):
  peer designer closure is deferred, so an already queued Save could insert an
  old-world design after replacement. Each draft now captures a runtime document
  generation. Every completed load/restart/demo advances it before notification,
  even for the same saved campaign/year/token. Save validation and the actual
  accepted/order callback both reject a different generation; stale availability
  callbacks also stop. Normal orders/Save/selection/End Turn retain it. No file
  field or version is added.
- [P3 auxiliary Sensors](https://github.com/kotrf/suns/pull/279#discussion_r4478103394):
  Passive and emission ranges now uses the local redraw path, like active Sensors.
  Auditing visual toggles also found and fixed the same call in interstellar
  medium, outpost and transport-line overlays. All retain shared revision,
  received/accepted cache identity, movement build count and peer map items.

New regression cases were first built against unchanged 702d1d4. Queued Save
with another campaign, Save with the same file, a direct queued accepted event,
and all four affected overlays failed for the reviewed behavior (seven logs under
untracked work/review279-r2-before-*). After the fixes, the complete session target
passed in 5.40 s, also checking a fresh post-load draft and an existing draft saved
after a peer End Turn. The expanded session target passed five consecutive runs
(27.57 s total). Full GCC14.2/Qt6.12 Debug app/test build and **118/118 CTest
passed in 129.00 s**. Review fixes are ready; current source SHA/tree and new CI
are recorded in GitHub. PR #279 and issue #47 remain open for re-review/merge;
gameplay and v112/v49/v3 are unchanged.

## Review fixes for #279 / #47 — 5 November 2026

The review of source 3825b3d confirmed three P2 issues and one P3. Its source CI
41981060650 failed 117/118: the new linked-view test assumed a fixed 25 ms wait
was sufficient for two queued callbacks. The initial 118/118 record below is a
local result, not a claim that the original source CI passed.

Four dedicated regression cases were compiled against unchanged 3825b3d and each
failed for the reviewed behavior (logs under untracked work/):

- [Visitor demo](https://github.com/kotrf/suns/pull/279#discussion_r4477453260):
  exercise the real QAction and confirmation. Demo now uses the same document
  boundary as load/restart, closing old-world drafts in both views and resetting
  reports, context, history and identity together.
- [Unread reports](https://github.com/kotrf/suns/pull/279#discussion_r4477453268):
  hidden peer with a Fleet movement filter must not consume a new FleetArrived
  while the source reader retains an older non-fleet report. List rebuilding
  blocks selection signals, renders without read mutation and preserves explicit
  click/selection/unread-navigation/Show on map read actions.
- [Partial notifications](https://github.com/kotrf/suns/pull/279#discussion_r4477453271):
  a scoped session update suppresses Planning/Messages/Selection/Metadata while
  load/restart/demo install their complete fields. One completed Document boundary
  advances the epoch before publishing. Local route reset/history refresh also
  follow installation. The direct observer verifies state, received and planned
  populations, pending orders, selection and revision on **every** emitted signal.
- [Local Sensors](https://github.com/kotrf/suns/pull/279#discussion_r4477453277):
  use rebuildScene(false); retain shared revision, cache/build count and the peer's
  map items and overlay preference.

Linked-view checks now wait for observable results with a bounded deadline;
fixed 25 ms sleeps no longer decide completion. Targeted tests and five consecutive
runs of the session suite passed (12.95 s total). The full GCC14.2/Qt6.12 Debug
build and all 118/118 CTest targets passed in 126.19 s. Review fixes are ready;
consult GitHub for the current source SHA and its new CI run. Gameplay and v112/v49/v3 formats remain unchanged; #279 stays open
for re-review and #47 closes only after merging.

## Shared GameSession ownership (#47) — 5 November 2026

The user selected #47 next. The dock/preset/non-modal UI was already implemented;
this slice completes the retained session ownership requirement. Work uses the
existing repository, branch codex/shared-game-session-47, verified merged main
6ee0c6baf3bf7422450e4682d24d2f70e71aa88b, tree
a4c4c8951cb9f3b1c7c631561498819c858c7ec6. #277/#278/#267 are merged,
#275/#276 completed, all six Mini-2 items finished; main CI 41867803440 passed.
Earlier open/stacked statements below are historical.

GameSession owns canonical world/orders, shared selection/distance IDs, exchange
identity/tokens/inbox, reports/read IDs/save path and all received/planning caches.
MainWindow controllers alias that data and can attach to the same shared owner;
independent default constructors still create independent documents. Metadata and
identity no longer reset when another view opens. One controller performs actual
End Turn; notifications only refresh other views. Selection and panel-only edits
retain map items and physics caches. Peer changes coalesce outside Qt click
handling; file replacement resets peer tool context. Existing non-modal tools,
fog of war, delayed/sent intent, layouts and view-specific drafts remain intact.
Canonical save snapshots avoid the previous redundant full world copy in Save.
The [contract](shared-game-session.md) documents boundaries and acceptance.

Validation: full GCC14.2/Qt6.12 Debug app/test build and **118/118 CTest
passed in 118.21 s**. The new two-view suite exercises actual map/production/
report widgets, coalesced selection plus panel edits, exactly one paid End Turn,
shared PBEM identity, save/title propagation without clobbering local seed drafts,
same-year population/order replacement, direct document observers seeing the new
epoch before notification, live designer drafts across a peer turn, closure on
file replacement, read marks, vanished fleets and final owner release. Planning
cache addresses/build counts and map objects stay unchanged on selection/Add.
All 117 existing tests, including map/workspace/live-designer, logistics,
knowledge, PBEM, economics, old saves and layouts, also pass.

Ready for review; use GitHub #47 for the current PR/head and source CI. No gameplay
or wire-format changes: save v112 / orders v49 / race v3. #47 remains open until
review and merge. Frozen features and deferred AI are unchanged.

## Stars! starting population and growth follow-up — 4 November 2026

The user extended [#278](https://github.com/kotrf/suns/pull/278) with ordinary
Stars! surface demographics. New homeworlds start at **25,000**, with the existing
four basic factories producing **29 resources**. Ideal capacity is **1,000,000**;
racial maximum growth is configurable **1–20%**, new default **15%**. Full growth
through 25% capacity, then the quadratic factor `(16/9)*(1-density)^2`, red-world
mortality and capped overcrowding losses use a shared bounded integer function.
No minimum one-person birth is invented; natural mortality preserves 100-person
colonies. See [the contract](economy-rebalance.md) for rounding and all limits.

- Current UI growth and after-cargo next-year figures use the same scalar rule,
  with no turn simulation. Capacity labels use the same 5% habitability floor.
- Race designer, campaign slot, portable race, host/player save and reload→New
  galaxy retain the racial rate. That restart path also now retains the six
  research-cost choices introduced by this PR.
- Save **v112** appends validated POPR after v111 ECON; orders remain **v49**.
  Race files write **v3**, read v1/v2/v3. Older formats default to their implicit
  10% rate, retaining every stored population/cargo/ledger/RP/paid-work value.
  Subsequent years use the new density model; an already crowded old world may
  lose people on End Turn, with no load-time truncation.
- An actual original-v111 writer at source `109afca` generated the synthetic
  million-person fixture with advanced subsets, partial work, RP and ledger.
  Provenance/hash are in tests/fixtures. v110 fixture and v107 hash stay checked.
- Normal population is tested at/below/above 25%, 50%, 75%, capacity and 2x/4x/8x
  capacity, plus positive/zero/red hab, minimum, custom races, received views,
  real End Turn and extreme stored counts. Real race UI/file/campaign/restart and
  malformed POPR/race inputs are covered.

The original Player's Guide and public Craig Stars code are the references; the
original executable was not run. Advanced racial PRT effects, Accelerated BBS,
orbital habitat demographics and other ship/project recipes are outside this
population change. Whole-campaign balance and native Windows Qt remain manual
acceptance items. #278 is still stacked on open #277; neither is merged here.
The earlier source `109afca` passed CI 41839840687; the new published head needs
its own CI. Full GCC14.2 / Qt6.8.2 Debug app/test build and **117/117
CTest passed (132.58 s, -j4)** on the final code. The initial demographic changes
required explicit mature/funded fixtures for expensive ship/lab construction,
new-start cargo expectations, the 18th campaign column and a corruption test
addressing SMIN rather than the last byte of the whole file. All original paid,
knowledge, replay and transport assertions are retained. Race-editor growth and
Research/Production layouts were rendered and inspected. No private saves or
screenshots are committed.


## Population economy and Stars! research (#276) — 4 November 2026

The user approved implementing #276 and explicitly selected Stars! research
pricing in the same slice. Branch `codex/economy-research-276` is based on
**a1a5b87916dbdf9a8f0d6724b058d15c38afc17f**, the still-unmerged #277 source.
This is a separate stacked PR: merge #277 first and then retarget this branch
onto main. No merge is authorized by this implementation request.

- Population produces one resource per 1000 residents. Basic/advanced factories
  and mines share their type's workforce cap; advanced buildings operate first.
  Direct advanced construction and paid basic upgrades have identical total
  bills. Basic mines require no minerals. Growth remains factory-independent.
  [The contract](economy-rebalance.md) records all initial recipes and boundaries.
- Research uses the 1–26 Stars! base table, +10 per known level, racial field
  factors 50/100/175%, and optional doubled slow tech. Every overflow/ordinary,
  precursor and delivered-data path uses the shared current price. Legacy levels
  above 26 and per-field RP are retained; a capped focus falls through its queue
  and then the lowest unfinished field. Full research reserves no allocation.
- Race/campaign editors and race v2 persist each field's setting; v1 defaults to
  Normal. Research plan prices remain one bounded pass over projected **levels**;
  production estimates track known count/upgrade eligibility without annual UI
  simulation. Production, current extraction and selected-colony counts show both
  tiers through received owner observations.
- Richer income exposed mineral starvation in the civilian AI: endless factories
  consumed the intended colonizer's Germanium. Development now uses capped,
  affordable batches and population-scaled mining, saves that civilian bill,
  considers forward docks before unlimited factories, and upgrades working
  buildings. Military selections use ordinary paid queues, which may wait for
  minerals. Late engineering/probe decisions canonicalize the received view by
  stable IDs so input storage order cannot change orders or decision history.
- Save **v111**, orders **v49**, race **v2**. ECON follows #277's v110 SURF and
  preserves physical/received/pending advanced subsets plus racial costs. Old
  buildings load basic; four more remaining factory resources preserve old paid
  work at the new price 10. Templates/annual rules migrate consistently; old
  stocks, population, technology, RP and historical ledgers are retained.

Full GCC14.2 / Qt6.8.2 Debug build and **117/117 offscreen CTest passed (138.77 s)**.
Checks include exact reference prices/factors/overflow/cap, real resource debits
for repeated advanced/upgrading units, no-ore mines, common workforce/idle/capture
bounds, host/player/order round trips, hidden-upgrade byte invariance and corrupt
ECON rejection. A genuine writer from #277 produced the synthetic v110 migration
fixture with partial work, templates and received evidence; its source and hash
are recorded under tests/fixtures. The real v107 writer hash still matches.
No private game or screenshot is committed.

The fair AI's three fixed-seed 120-year scenarios retain development, fuel,
replay and shuffled-input assertions; each narrow race is now explicitly given
one nearby viable frontier in physical setup. The combat/landing fixture keeps
its small defender's population fixed. These isolate policy/simulation contracts;
they do not assert campaign balance or future conquests on arbitrary maps.
Production/Research/race layouts were rendered and inspected; real UI Add,
reservation, End Turn and scalar estimates pass. Native Windows Qt was not run.
The million-person start now yields 1004 resources with four basic factories;
new/continued campaign pacing still needs play acceptance. PR/source CI on GitHub
is authoritative; this branch is not merged. Independent #267 and frozen
project directions remain separate.

## Mini-2: received radar and mineral-stock bars — 4 November 2026

User authorized completing all remaining Mini-2 notes, including newly added
items 5–6. Items 1–4 are already merged in #274 and #273 is completed. Work uses
the existing repository, branch codex/mini-2-radar-minerals, verified main
4b4ec170775673183f27e1e9c3ee89ecf4c558d7 (tree
155c5617cc562638afc2b51bb387af3464dfeebe). Tracking
[#275](https://github.com/kotrf/suns/issues/275), [contract](mini-2.md).

System Details shows received None/Local/Full planetary radar and nominal range,
with unknown distinct from confirmed absence. Mineral rows share a clickable kt
scale and paint light stocks, dark current extraction and an independently scaled
concentration diamond. Exact tooltips and visible report age distinguish dated,
zero and unknown stocks. The Minerals table now includes known uncolonized stocks.
Both panels reuse player/planning caches; current mining is an algebraic pass over
received/accepted stacks, with the colony's pre-Cargo workforce phase preserved.

Ordinary surface evidence is independently dated in survey snapshots. Annual
post-mining/cargo observations refresh already orbitally surveyed worlds without
a scanner, preserve the original exploration rules, use the existing delayed
outbox and preserve Passive/Silence. Older packets cannot roll back final stock;
foreign supplies and exotic discoveries remain private. Save v110 appends bounded
SMIN evidence after the unchanged v109 bodies; older saves remain readable and
orders stay v48. Player packets retain received evidence and redact in-flight stock
reports. The old workforce migration fixture still validates its original v107
body hash; its tail lookup now tolerates later extensions.

Validation: full GCC14.2/Qt6.12 Debug build and **115/115 CTest in 83.14 s**
passed. Coverage includes radar Local/None/unknown and hidden receiver changes,
known-zero/unknown/dated uncolonized stocks in the real UI, scannerless mining
reports with unchanged exploration prerequisites, out-of-order evidence, Passive
hold/coalescing/transmission, stock-scale keyboard/button control, accepted Cargo,
host/player round trips, v109 migration and invalid/truncated evidence. Existing
workforce v107 migration hash and all prior regressions still pass. The actual
System Details window was inspected at 1366×900 and 1000×700.

Ready for review; use linked GitHub #275 for the current PR/head and CI. Leave the
new PR unmerged for review. The frozen gameplay issues and deferred AI work remain
outside this slice.

## Mini-2: four approved interface/platform improvements — 4 November 2026

The user explicitly approved implementing all four accumulated notes; the prior
collect-only instruction is superseded for this slice. Existing codex/mini-2 is
based on merged main 317a23607642e6c5698cfeb5141c8028d3c37f14, tree
48678d13201f28101420f838ec841b4fe15d8f72. #272 is merged and #270/#271 completed.
Tracking: [#273](https://github.com/kotrf/suns/issues/273), [contract](mini-2.md).

- Selected route geometry remains without map waypoint numbers; numbered table
  rows and the map-coordinate marker retain their editing roles.
- Station's icon-only 28×28 button shares the 84-pixel portrait header at its
  upper right, saving the previous separate row. Tooltip, click and received
  fitting/hidden-state semantics are preserved.
- Windows RC embeds a seven-size ICO; Qt application/window icons use embedded
  PNGs and explicit static-resource linkage. Linux gets hicolor SVG/PNG assets,
  suns.desktop, matching desktop-file identity and CMake install rules. QSettings
  identity is unchanged; custom Qt install runpaths remain usable.
- Fuel ≈ gives nominal per-leg mg at each Warp/current received or accepted
  manifest. The existing direct-distance formula is bounded by rows/stacks, not
  years. Later costs become unknown after load/position/composition changes;
  refuelling alone preserves burn but not completion dates. No future tank,
  shortage, scooping/regeneration or cloud forecast, annual simulation or workers.

Focused checks pass: actual UI icons at seven sizes; station geometry, live
click/fitting and delayed intel; map trajectory without numbers; current fuel
with Warp/cargo table refresh, real-turn billing/annual-rounding comparison,
conditional unknowns and hidden-manifest invariance. A synthetic station/route
render was inspected. Warm Debug row queries during a build measured 0.1027 ms
for five legs and 19.2521 ms for 1000; this is not a complete click benchmark.

Staged Linux install, installed-app smoke without SDK activation and
desktop-file-validate pass. MinGW windres 2.44 compiled the configured RC and ICO
into a resource-only Windows PE, with seven RT_ICON entries and RT_GROUP_ICON 1.
A full native Windows Qt build/runtime was not executed here. Assets are
committed, so normal builds require no artwork tools/SVG runtime plugin.
Core rules, command delivery, balance and save v109/orders v48 are unchanged.
Full GCC14.2/Qt6.12 Debug build and **114/114 offscreen CTest passed (79.22 s)**,
including the pre-existing mining/Stop packet, hidden-refuel and gate invariants.
Diff check is clean. Only existing Qt QDragEnterEvent deprecation/old nodiscard
warnings appeared; this slice adds no new warnings. New PR/source/CI are recorded
on GitHub; this implementation awaits review and is not merged.
Independent #267 remains outside this slice; frozen directions stay frozen.

## PR #272 review: keep Stop cancellation before mining — 3 November 2026

[One P2 on source 724ef9e](https://github.com/kotrf/suns/pull/272#discussion_r4460217935)
was reproduced with the real UI and TurnProcessor: deleting queued Stop lost
its cancellation of already sent navigation. A six-year Move packet issued at
T1 survives when a relay resumes at T2 and new commands become instantaneous;
native mining starts, but the old Move later cancels it and takes the ship away.

Start now deduplicates only unsent mining orders, preserving Stop before the
new native task. An already sent Stop is reissued before mining if it would
otherwise arrive later. A subsequent Stop removes unsent mining, including when
replacing an earlier Stop in-place. The summary resolves final queued task order
and describes cancellation followed by work rather than incorrectly promising
No Task. Cancellation remains an ordinary physical command; no core rules,
formats, emissions, communication delays or annual UI estimates changed.

The new regression failed against the original app library on the surviving
pendingCommands assertion. After the fix it passes real UI Stop/Start through
T9, past the old delivery boundary, with mining and position retained; a control
without Stop reproduces escape/No Task. Additional cases cover a Stop already
in flight, last-click Stop, no artificial waypoint and actual paired-order
serialization/descriptions. Full GCC14.2/Qt6.12 Debug build and **114/114
offscreen CTest passed (82.09 s)**, including existing route, communication,
strategic-work, recycling and scanner-coverage suites. Diff check is clean;
no new build warnings. Prior source 724ef9e CI is separate evidence, not a
claim for this revision; see GitHub for the updated source and new CI.

Start work here and Stop work / route remain explicit controls: changing the
action selector is a draft; Stop also cancels travel and older packets. Mini-2
currently contains four accumulated notes in Notion (remove map numbers, compact
station button, platform icons/desktop entry, cheap per-leg fuel estimates);
implementation was not requested and none is included in this review fix.
Current branch is codex/mining-map-coverage, PR #272 awaits repeat review/unmerged.

## Stationary mining and scanner coverage — 3 November 2026

Current base is main e2211cd48cf8a7075608b6f1b78c1719e645e44b after merged #269;
#268 is completed. Its separate main CI 41643240035 succeeded. The older open-PR
checkpoints below are historical; independent documentation PR #267 remains open.

Issues [#270](https://github.com/kotrf/suns/issues/270) and
[#271](https://github.com/kotrf/suns/issues/271) are implemented in the existing
codex/mining-map-coverage branch. Start mining here stages the existing native
SetRemoteMiningOrder instead of a fake arrival waypoint at the fleet's position.
It validates equipment and received world ownership, replaces repeated unsent
mining/Stop intent in the right order, and retains physical command delivery and
emission settings. Route summary distinguishes queued/in-flight work and explains
that extraction accumulates on the planet's surface, not aboard the miner.
Ordinary arrival mining still starts output in the annual phase after arrival.
The reported earlier lost arrival order was not separately reproduced: synthetic
UI Add-waypoint and edited-leg orders survive travel and communication delays.

Numbered route labels now have a fixed 10-pixel font, ignore map transforms and
pass mouse clicks. Default sensor coverage merges ship scanners and planetary
radar into one faint blue fill, with a stronger blue penetrating-scanner layer.
The map previously never drew penetrating coverage. Each type uses one winding
path/no pen: overlap does not accumulate opacity or cancel duplicate circles.
Optional passive/emission layers also merge; Sensors hides all layers. Geometry
uses received player data, reported interference and received emission settings,
without host jammer queries, boolean unions, annual forecasts or gameplay changes.
User chose this simpler map presentation explicitly (MEM-33).

Full GCC14.2/Qt6.12 Debug build and **114/114 offscreen CTest passed (82.39 s)**.
The new native-order assertion failed against the original app library before
the fix. Actual UI buttons/edited legs feed real TurnProcessor calls, including
local and delayed starts, arrival, persistent output, Stop/Start order precedence,
existing orders serialization, equipment/world validation, unknown ownership and
hidden-ownership invariance. Fixed route-label device size is checked at four
zooms. Coverage tests paint real graphics paths and compare pixels in single and
overlapping areas, including duplicate discs, reported interference, emissions,
missing penetration, hidden-state invariance, input transparency and visibility.
The existing map selection, route, mining and recycling suites also pass.
A local synthetic map render was visually inspected; no private screenshot or
save is committed. Diff check is clean; final build introduced no new warnings.
All new fixtures are synthetic; private save contents are not published.
Save v109/orders v48, core turn rules and minimal realtime estimates remain unchanged.
New source head, PR and CI are authoritative on GitHub; this branch awaits review.

## PR #269 review: skipped production rows — 3 November 2026

Review of 62f0e93 identified one P2: the minimal queue estimate charged known
skipped work to later rows. A legal template of 100 defenses followed by a Factory
at Energy 0, or at the defense cap, predicted Factory T86 instead of T2. Both
cases were independently reproduced using the original helper and one real
resolve_colony_production call before the fix.

The helper now receives the current received/planned colony and applies cheap
resolver-equivalent technology/cap/radar/station skip checks before budgeting.
Skipped rows have zero effective quantity/work and preserve prefix certainty.
Manual defenses clamp to the remaining cap, including partial first-unit work;
capped annual rules remain explicitly idle. Scalar defense/radar/station facts
advance only after a confidently estimated prefix. Missing minerals/shipyards
still block, and skipping a later row never clears an earlier unknown. The UI
shows effective bills/work and Currently skipped; skipped stations do not create
an estimated shipyard. No core, real-turn, save/orders or forecast policy change.

Full GCC14.2/Qt6.12 Debug build and **112/112 offscreen CTest passed (37.07 s)**.
New cases use exact mineral budgets and one real resolver call for the two legal
template repros, annual cap persistence, partial/repeated capped defense batches,
installed/duplicate radar and invalid stations. A real MainWindow production
template test checks the Factory row is ~Turn 2 (+1). Huge-batch/overflow and
unknown-prefix regressions still pass. Independent read-only review found no
additional confirmed defect; diff check clean. Complexity remains one queue pass,
without year/unit loops; previous private-save timings are not newly remeasured.

Work stays on codex/ui-order-performance, PR #269 open/unmerged. The prior
62f0e93 source CI 41599894742/job 125795896503 passed **112/112 (83.02 s)**,
Qt6.12.0/GCC15.2, checkout/log verified; it does not validate this subsequent fix.
Current source/CI are recorded on the PR. #267 remains independent; frozen
directions and the minimal-estimate decision are unchanged.

## Minimal realtime estimates — user decision, 3 November 2026

The user explicitly prefers minimal, imperceptibly cheap estimates over detailed
realtime forecasts, including background calculations. Rough population growth
and distance/speed ETA are sufficient; actual outcomes resolve on End Turn.
This supersedes the detailed background-forecast approach in the earlier #269
checkpoint below. Work remains in the existing codex/ui-order-performance branch;
PR #269/#268 stay open for review, not merged. Main is still 79d22c2; #267 is an
independent open documentation restoration.

Deleted the 96-year route simulation, QtCore worker manager, pending forecast
states and Fuel/Left columns. Route rows/summary use cumulative distance/Warp²,
marked ≈, received-derived positions and existing pending/sent route precedence.
Conditional actions, moving targets, gates and wormholes leave later ETA unknown.
The current accepted cargo/fleet projection replaces the disposable full-turn
movement preview. It stays in the current year and credits no future generation
or automatic service. Its dependencies include route edits/Stop, unlike the old
logistics-filtered preview; stale route-cache retags were removed and tested.

Production uses one pass over current work, net output and received mineral
budget. Auto/nonlinear/blocked prefixes show Annual rule or no date. Removed
future queue/service simulations from templates, engineering, resonance labs and
mobile bases. Current rates, eligibility, costs, order validation and received
knowledge remain. Bounded habitat-upkeep arithmetic derives current net rates
on one local planet copy; it does not run future world turns. Real simulation
and save v109/orders v48 are unchanged. [Current contract](minimal-ui-estimates.md)
and durable decision MEM-32 explain the new boundary.

Full GCC14.2/Qt6.12 Debug build and **112/112 offscreen CTest passed (39.76 s)**.
Updated UI assertions distinguish nominal estimates from actual outcomes; exact
EndTurn/logistics/knowledge/save assertions remain. New tests cover geometric
routes beyond 96 years, received track projections/hidden invariance, sent
command precedence, conditional unknowns, current-year manifests, static queue
rates/mineral budgets, large batches and overflow.

On the same private year-89/288-system/37-fleet save without concurrent builds:
route summary **0.050 ms** synchronous; detailed 659fd9b needed **6.594 seconds
background work** after Factory, now there is no such work. Paired foreground
Factory **209 → 215 ms** (including queued GUI events): no claim of additional
foreground improvement over that revision. Three real current clicks were
**184 / 260 / 245 ms**, versus original main **8477 / 8216 / 8470 ms** before
the broader #269 work. Settled idle CPU **5.4 ms / 2.1 s**, heartbeat **20.4 ms**;
both modes use the same 500-ms layout settling floor. Save/readback passed and
the supplied turn was never advanced/overwritten. Measurements are executor
Debug results; private files remain untracked. PR #269 carries exact source/CI.

#53/#63/auto-evacuation remain frozen; AI extensions postponed. Prior sections
describe their historical checkpoints, including superseded forecast behavior.

## Order-edit responsiveness #268 — 3 November 2026

The user confirms system selection improved after merged #266, but adding a
Factory still takes seconds, and requests broad UI optimization. Current work
is `codex/ui-order-performance`, based on verified main
`79d22c2ee18d140dcd404816e975c6a13990c483`. #265 is closed; main CI
41538166676 completed successfully for that exact SHA. Documentation-only #267
remains open and restores the older performance history; this task leaves its
`docs/ui-performance.md` file untouched.

Production/research-only edits now retain immediate physical projections and
map items while invalidating annual route estimates correctly. Automatic route
rows, candidate fuel warnings and the Forecast dialog use bounded cancellable
QtCore workers with owned received snapshots, complete request identities and
GUI-only publication. Superseded jobs, cache-hit reversions and invalid editor
candidates cancel unnecessary work; stop/load/new world/turn/close join workers.
Synchronous forecast APIs keep their exact existing simulation. Hidden Research
is lazy; Intel/Computer tools refresh on coalesced events. Exact delay shortcuts,
immutable snapshot-scoped communication graphs and lazy optional fallbacks
remove additional repeated work. No gameplay or save v109/orders v48 change.

Full GCC14.2/Qt6.12 Debug application/tests build succeeded;
**111/111 Linux/offscreen CTest passed (37.95 s)**. Tests cover cache dependencies,
fresh received knowledge, graph/export equivalence across scopes/threads, tool
visibility/events, exact async/sync route parity, rapid edits, delayed commands,
cache-hit cancellation and same-ID load/stop/close/destruction. Existing exact
route editor ETA/fuel/action/color assertions now wait for async completion.
Diff check passed. Uploaded saves/profiling files remain outside tracked files.

Controlled real Factory Add clicks with the original active-route fleet on the
user's year-89/288-system save: **8477 / 8216 / 8470 → 214 / 190 / 198 ms**, about
40× lower foreground latency. Paired broader probe: original-fleet Factory
**8046 → 219 ms**, fleet selection **1813 → 453 ms**, research allocation
**808 → 207 ms**, load **8566 → 685 ms**. Necessary ETA work still takes about
6.1 seconds in the background, with 21.4-ms longest heartbeat gap after Factory;
idle CPU **76.8 → 7.2 ms / 2.1 s** after all forecasts settle. Save/readback
passed without advancing or overwriting the original. Debug author measurements
on this executor; no universal timing guarantee. [Mechanism, complete table and
diagnostic](ui-order-performance.md). Source PR/CI tracked in
[#268](https://github.com/kotrf/suns/issues/268); code awaits review/merge.

#53/#63 and auto-evacuation stay frozen, AI extensions postponed. The active
priority is user verification of responsiveness; no new gameplay slice selected.
Older pending checkpoint sections below describe their original point in time.

## Late-campaign selection stalls #265 — 3 November 2026

User reports severe system-selection stalls in the ongoing year 80–90 campaign,
predating #264. The supplied year-89 save has 288 systems and reproduces the
problem. Work is on `codex/ui-selection-performance`, based on merged main
`b1fe4ad`; #263/#264 are merged and #94 is closed.

Pure selection used to increment planningRevision, discard cached logistics/
movement/ETA projections and run a disposable full TurnProcessor again. A star
selection now reuses projections and existing scene markers when the selected
fleet and rendered state revision match. Fleet selection still rebuilds geometry
without invalidating unchanged physical projections. Actual data/order/load/turn
boundaries retain invalidation. Related panel readers share a received-player
projection; explicit control updates refresh it. Command/Designer Save validation
still reads fresh knowledge. Core rules and save v109/orders v48 are unchanged.

The fleet-planning Qt regression verifies a real queued star selection, retained
planning projection and marker/highlight, then confirms a real rebuild invalidates
the projection. The uploaded campaign is kept outside the repository. Validation
and comparative measurements follow.

Full GCC14.2/Qt6.12 Debug build and **106/106 offscreen CTest passed (38.24 s)**.
The received-access regression also passed: action/Save checks retain fresh
knowledge despite shared rendering snapshots. Diff check passed.

With no concurrent compiler/tests, the same fully installed offscreen Qt window
loads the supplied year-89 save, settles, and selects three stars through the
normal queued selection path. Baseline main b1fe4ad: **8776 / 8854 / 8512 ms**;
fixed code: **701 / 707 / 763 ms**, roughly **12× faster**. These are Debug
measurements on this executor, not a promise of sub-millisecond interaction or
identical user-machine latency. Idle was quiet in both versions (four initial
scene notifications, no continuous recomputation). The earlier 18-second figure
was observed with concurrent compilation; use the controlled comparison above.
Remote head/source CI/review are tracked separately in GitHub #265's PR.

## Graphical designer #94: live availability — 2 November 2026

The user selected #94 after the backlog audit. Branch
`codex/ship-designer-live-access` is based on documentation-only #263
(`73e487e078f954edeebe78bafb12dc352f593a48`), still open, after gameplay main
`67215fc`. The parent is not silently merged. The independent main CI
41481245165/job 125399816988 passed **105/105 (88.56 s)**, checkout/log verified.

The real modeless designer now updates hull/component availability after
received own-player research/acquisition/racial changes, using shared core
access functions. MainWindow coalesces controlsChanged into one single-shot
queued refresh; no periodic polling. Reopening the same window refreshes
synchronously. Rows/details/icons, drag/Fit and Save state update without
replacing the selected hull, name, fitting placements/cell or filter settings.
Unchanged access does not scroll the catalog. A revoked fitted part remains in
the draft and prevents Save with a reason; Save also rechecks current received
access before emitting the normal CreateShipDesignOrder.

The new offscreen regression reproduced the old failure after an actual
MainWindow End Turn researched Propulsion 3 (assertion/exit 134). It now passes,
covering research/hull unlocks, fitting/saving/resolving the ordinary new design,
unchanged existing designs/draft, acquired equipment/traded hulls, racial and
blueprint restrictions/revocation, pre-refresh Save rejection, stable selection/
filters/scroll and irrelevant enemy changes. No order comes from refresh alone.
Full GCC14.2/Qt6.12 Debug build and **106/106 offscreen CTest passed (38.50 s)**.
The new target also passed separately (0.47 s); diff check and completion-matrix
relative links passed. Remote head/CI/review remain separately tracked in GitHub.

[Original #94 acceptance matrix](audits/2026-11-02-ship-designer-completion.md),
[current UI contract](ship-designer.md). Core simulation/unlock rules and save
v109/orders v48 are unchanged. #94 remains open pending review/merge to main;
this is automated Qt/core acceptance, not a claim of user gameplay acceptance.
The separate #47 ownership refactor and frozen/other major areas are unchanged.

## Merged mini-ideas and reconciled backlog — 2 November 2026

The user authorized merging #259/#262, then explicitly selected an open-issue
completion audit. #259 merged as `e42b804757e1b15464dd6cf7339cd395d7079afb`;
#262 merged as **`67215fc96933d8c6a427bce15f9d448fb2a6736a`**. Final main
and #262 source head `262e402f73f3a2209c54e9348a0bb46216b1a982` have the same
tree **`134e9b63d335670b122e751b1346fa7d402c438c`**. The latter is an
ancestry-only sync after retargeting from #259 to main, with no source changes
from reviewed `ef82069`. Re-review confirmed the legacy-hull price P2 fixed
and no new confirmed findings; the sole old inline thread is resolved/outdated.
Required source CI [41477603122](https://github.com/kotrf/suns/actions/runs/41477603122),
job 125387190869, passed **105/105 (88.94 s)**, Qt6.12.0, checkout/log verified.
#259 source CI passed 104/104 (90.54 s), and its merge's main CI passed 104/104
(89.64 s). Final-main CI is a separate run, not implied by these results.

Scrap Fleet, space recovery and the complete catalog are now on main; focused
#258/#260/#261 are closed. Save v109/orders v48 remain current.

The [backlog audit](audits/2026-11-02-backlog-reconciliation.md) compared twenty
older feature issues with original requirements, merged PRs, source and scoped
tests. **#45/#48/#105/#195 are closed as completed**, with evidence and preserved
original bodies. Advanced reconnaissance stays under #58/#101/#205; optional
medium phase 5 is future design. Exactly **16 feature issues remain open**,
including frozen #53/#63 and discussion-only #108. #50 now correctly records
that deferred/scheduled specialist gate programmes are delivered by #232.

#94 stays open with a reproduced acceptance gap: changing own technology while
ShipDesigner is open does not refresh component access in that reused dialog.
An offscreen own-state probe shows core access true but UI availability false;
this is not a full research-turn playtest. Catalog refresh does not fix Designer.
No repair or new major feature was implemented by this audit. #47 retains its
explicit shared GameSession-style ownership refactor. No next implementation
slice has been chosen; gameplay acceptance remains relevant.

Seven gate-related suites passed (4.97 s) and seven history/report/navigation
suites passed (1.28 s), 13 distinct targets because save-game overlaps. The
audit follow-up changes documentation only. The sections below are historical;
references there to unmerged #259/#262 or pending #195/#105 are superseded.

## Catalog mineral prices: review fix in #262 — 2 November 2026

Review of head `8b6370dacca9240b47b0d7735491ec710de4c4bf` against #259
found one P2: all seven legacy hulls displayed zero I/B/G because their
`ShipHullSpec::baseMineralCost` is empty, while production uses the legacy
prices in the common economic calculation. No published findings in #259.

`hullDescription` now calls `ship_design_mineral_cost` for a bare hull without
components. Core prices, production, equipment costs, Scrap Fleet distribution
and save/orders formats are unchanged. The catalog UI regression reads all
41 displayed hull prices and compares them with the economic calculation;
seven independent legacy fixtures include Scout 4/1/1 and Heavy Transport
18/6/6. It fails on the old UI at Scout's ironium assertion, then passes with
the fix. No price table is duplicated into product UI code.

Full GCC14.2/Qt6.12 Debug build and **105/105 Linux/offscreen CTest passed
(40.59 s)**; diff check clean. The prior source CI 41451422936 / job
125297282822 passed 105/105 (92.70 s), Qt6.12.0, checkout SHA/log verified;
that predates the new regression/fix and is not proof for the new head.
New head/tree/CI are tracked in GitHub #262. The PR remains based on open
#259 (`codex/scrap-fleet`); no merge is part of this review fix.

## Complete technology catalog #261 and space recovery #260 — 2 November 2026

The user explicitly chose the remaining separate full catalog mini-idea. Work
is on `codex/technology-catalog`, based on open #259 head
`aa6ffb6e5fdf2a3c786928d8bfd157a967389c09`. Its source CI 41446442935 /
job 125280603840 passed **104/104 (90.54 s)**, Qt6.12.0; SHA/log verified.
#259 is not merged. Main remains `12dc73e` (#257), verified main CI
102/102 (88.37 s). The new PR should initially target `codex/scrap-fleet`,
then be retargeted to main after the parent is reviewed and merged.

The separate Catalog window has 224 entries (41 ship hulls, 3 orbital hulls,
180 components), combined search/category/available filters and selectable
locked entries. Legacy hulls are labelled. Details cover hull costs/capacities/
banks, component effects and exact technology/racial/acquisition restrictions.
Station gates use dedicated access requirements and explicit unlimited limits.
The Ship Designer component formatter is shared; special kit/screen descriptions
retain basic mass/cost facts. Browsing adds no orders. Menu and toolbar actions
reuse one modeless window; visible control changes coalesce without polling.
Received own-empire availability refreshes while selection/unchanged text scroll
are preserved. Public specifications reveal no opponents' private state.

The user also requested a wider space Scrap Fleet density, reaching zero,
while leaving the orbital case unchanged. The chosen initial density is
triangular on 0–50%, weighted towards 50% (mean ~33.3%, standard deviation
~11.8 percentage points); discrete sqrt-transformed rolls include both endpoints.
The prior orbital 75% at own populated colonies / 50% at other planetary orbits,
with 20% loss-of-5–15pp, retains its exact previous seed outcomes.
Cargo minerals remain fully preserved. Zero structural material with an empty
hold consumes no wreck ID, creates no fake scientific wreck/observation and
still delivers completion. Save v109/orders v48 remain unchanged from #259.

Full GCC14.2/Qt6.12 Debug build and **105/105 Linux/offscreen CTest passed
(39.15 s)**. The new catalog suite checks all 224 unique selectable entries,
combined filters/no results, locked inspection, technology/race/trader changes,
station gate requirements/unlimited limits, complete kit descriptions, small
window Close, both real entry actions, singleton reuse/close/reopen, live state
refresh, stable text/scroll, no orders and no opponent-derived availability.
Native themed hull/gate catalog captures inspected. Expanded recycling tests
check broad space mean/variance/tails and deterministic replay over 1000 seeds,
true zero with no fake wreck/ID/intel, retained cargo at zero structural yield,
and fixed previous orbital outputs including loss rolls at owned, foreign and
uncolonized planets. Existing fitting,
save/PBEM/knowledge/layout/economy checks pass; diff check clean.
See [catalog](technology-catalog.md) and
[recycling](fleet-recycling.md). Frozen #53/auto-evacuation/further AI remain
outside this work. Next new phase has not been selected.

## Scrap Fleet #258 — 2 November 2026

Reviewed [#257](https://github.com/kotrf/suns/pull/257) was merged at the user's
instruction as `12dc73e80bb1ae0de8f7acb46a52de72be38a10e`; #256 is closed.
Merge tree `04a8c67936fdd85bb765b185435b725a979e6f5e` matches reviewed head
`b7e63f8aa2782ef274a9685f52cc459149af41f1`. Source CI 41438849185 passed
**102/102 (90.10 s)**; main CI 41442259081 / job 125266741968 passed
**102/102 (88.37 s)**, Qt6.12.0. Checkout SHA and final logs verified.

The next approved mini-idea, **2**, is implemented on `codex/scrap-fleet`,
[#258](https://github.com/kotrf/suns/issues/258). The user explicitly accepted
50% space / 75% owned populated colony recovery plus a probability of further
loss. Initial risk chosen for implementation: 20% chance of another 5–15
percentage points lost, deterministic per seed/fleet/owner/year.

Scrap Fleet is a terminal, one-shot route action with ordinary command delay;
Scrap here queues it at a stationary fleet's received position. Every fitted
ship stack contributes its mineral cost. Mineral cargo survives intact; surface
material is credited at an owned populated colony, otherwise a 20-year mineral
wreck remains. Passengers, samples and xenocrystals must be unloaded; fixed
installations retain decommission controls. A full surface stock blocks rather
than overflowing. Blocked final steps hold/retry and can be stopped. Stationary
scrapping, including immobilized ships, resolves before motion/battles; a flight
that reaches its scrap waypoint resolves there after its ordinary approach.

The final transmission dispatches the fleet's stored outbox using the existing
ledger logic. Passive/Silence without local exchange holds the step until it can
report. A remote old contact persists until completion delivery; the new wreck
observation travels in that packet. Player exports omit physical wreck stock.
Recover wreckage retains its scanner/cargo requirement and science behavior;
mineral collection is proportional to free shared hold space, preserves leftover
stock, and never gives science. Markers distinguish received mineral/scientific
wrecks and show observation/expiry dates.

Save **v109 / orders v48** permit the new action/status. A trailing SCRP section
stores stock by existing wreck ID, leaving STRA bytes intact. Old v108 wrecks
load without invented stock. The v107 workforce migration's real baseline hash
is retained; its offsets now account for the new trailing empty SCRP section.

Full GCC14.2/Qt6.12 Debug build and **104/104 Linux/offscreen CTest passed
(38.16 s)**. New core coverage checks 1,000 seed outcomes and replay, mixed ship
stacks and intact cargo, actual commands, no repeated reward, final/repeat
constraints, immobilized stationary fleets, partial/full/competing collection,
foreign/empty orbits, passengers/samples/xenocrystals, full surface stocks,
remote command/completion delivery and the final silent-fleet outbox. UI/save
coverage checks the actual Scrap here click and planned route row, physical
ships remaining during planning, save/turn-order round trips, v108 migration,
PBEM omission of physical stock, NaN and truncated SCRP rejection. Existing
workforce baseline hash and all earlier knowledge/economy/route suites pass.
Native themed screenshot inspected; diff check clean. Native capture command:
`QT_QPA_PLATFORM=offscreen work/build/tests/suns_fleet_recycling_editor_tests --capture`.
See [fleet recycling](fleet-recycling.md). PR/head/remote CI are tracked in
GitHub #258. No new catalogue/AI/frozen logistics scope is included.

## Station icon and received fitting #256 — 2 November 2026

At the user's instruction, reviewed [#255](https://github.com/kotrf/suns/pull/255)
was merged as `6ff8f81067eb4ce48f38a4960ee659a740bab8b9`; #254 is closed.
The merge tree `2b03b967ae7e44f35b243420b9f8e6bbb2984d5f` exactly matches
reviewed head `2c52da6a117fd0f1513909b2ac62679f9c5e2ea6`. Source CI
41412632143 / linux job 125168922060 passed **101/101 (86.06 s)** with Qt6.12.0;
checkout SHA and final log were independently checked. Main run 41437299011
is separate; its final result must be checked in GitHub rather than inferred.

The next approved mini-idea, **6**, is implemented on
`codex/station-details-fit`, [#256](https://github.com/kotrf/suns/issues/256):

- System Details contains a hull-specific station icon/button when the
  selected system has a station in `make_player_view`.
- Clicking opens a read-only snapshot with the hull at the center, numbered
  weapon cells above, armor/shield cells left and electronics right. Repeated
  fitted components, empty cells, gates (including unlimited limits), total
  armor/shields, damage and fitted services are visible. Small docks start in
  a shorter dialog; large/narrow fits scroll while Close stays reachable.
- The snapshot uses received operational equipment, independent of queued
  refits and current research availability. Hidden refit/loss/construction
  cannot refresh it before receipt. Delivered absence hides the icon. The
  existing context signal refreshes selection/load/turn changes, without a
  timer. Icons are cached. Foreign station intelligence and independently
  anchored fleet-base controls retain their current separate contracts.

Full GCC14.2 / Qt6.12 Debug build and **102/102 Linux/offscreen CTest passed
(38.73 s)**. New real-UI coverage opens/closes the dialog through the station
button, checks all three hull capacities, repeated/interleaved component groups,
empty cells, gate names/limits, HTML-safe station names, queued-vs-operational
fits, both services, small-window scrolling and Close accessibility. Host and
sanitized PBEM views retain the same old fit after hidden refit/destruction;
new receipt, explicit absence, unreported construction, legacy loss ledger,
foreign/empty systems, deselection and state reload are covered. Native themed
images of all three hulls and the System Details button were inspected.

No core rules or save/order format changes: **save v108 / orders v47**. The
slice is ready for review; actual PR/head/remote CI are tracked in GitHub #256.
Next are new actions; Scrap Fleet partial mineral recovery remains approved in
principle, with recovery percentages still unspecified. Full catalog window,
frozen #53/auto-evacuation and further AI are not included here.

## Ship Designer decks #254 — 2 November 2026

The user continued the approved mini-idea order with 3/4. Main `8f1c625` already
contains merged #253; #252 is closed. Its CI run 41398408781, linux job
125124681797, passed **100/100 (85.60 s)** with Qt6.12.0; checkout SHA/log
independently verified. Save v108/orders v47 unchanged.

Branch `codex/designer-hull-layouts`, [#254](https://github.com/kotrf/suns/issues/254),
adds explicit presentation decks for all 41 hulls. Stable cell IDs, categories,
equipment masks, bank identities/capacities and archived placements are retained.
Engines sit aft, forward/flank equipment to the right; each hull's bank anchors
and family outline replace the former vertical bank list. Expedition Base uses
radial service decks. A smaller reference portrait/orientation legend and wider
fitting area leave more room for the deck; large hulls scroll without leaving
stale grid dimensions after switching back to Scout.

Locked hulls remain visible, dim/italic with lock icons and exact requirements.
Empty cells are subdued/dashed yet selectable; fitted cells and focus/selection/
drag feedback remain distinct. Keyboard movement now uses displayed positions.
In-place refresh keeps buttons alive during drag/drop; core validation remains
on the unchanged logical hull. No core source or serialized data is changed.

Full GCC14.2/Qt6.12 Debug build and **101/101 Linux/offscreen CTest passed
(37.12 s)**. The new suite checks every hull's ID/bank coverage, bounded disjoint
geometry, representative real widgets, large-to-small resizing, locked popup
state, archived placements, spatial keyboard navigation, delete/refit with
stable widgets and Save visibility in a 1000×760 window. Existing actual mouse/
drag/template, core, save/PBEM and knowledge checks remain green. Themed Scout,
freighter, miner, battleship/Dreadnought/base and locked-popup captures inspected;
captions refined to avoid double elision, diff check clean.

This slice is ready for review; actual PR/head/CI are tracked in GitHub #254.
No new merge or remote CI success is claimed. The separate full catalogue
window is later work; next are station icon/fit access and new actions. Scrap
Fleet recovery fractions remain undecided. #53/#63 and evacuation stay frozen;
further AI development remains deferred.

## UI readability #252 — Research, Hab and selected routes, 2 November 2026

The user continued the agreed mini-idea order after merged #251. Baseline main
`20eed22` has confirmed CI run 41335589634, linux job 124934616373: **100/100
CTest, 84.55 s**, Qt6.12.0; checkout SHA and log independently checked. #250 is
closed; Add/change-driven refresh and the preceding four-PR integration are in
main. Save v108/orders v47 unchanged.

Branch `codex/research-hab-route-polish`, [#252](https://github.com/kotrf/suns/issues/252):

- Research displays six separate current levels with large bold numbers. Every
  Add choice names its current level and the appended step's target after the
  active/queued plan. For current Energy L3 with two Energy steps already in
  the plan, the choice targets L6; adding/removing advances/restores that target.
  Field IDs, current combo selection and normal order submission are retained;
  a field whose maximum level is reached/planned cannot append an invalid step.
- Hab shows the terraforming comparison only if potential Hab is strictly
  higher. Equal values at any percentage show current Hab alone; received
  evidence, engineering suffixes and technology tooltips retain their rules.
- The selected-route overlay uses a thin cosmetic pen, matching ordinary map
  routes; zoom no longer expands its thickness. Waypoints and movement orders
  retain their existing behavior.

Full GCC14.2/Qt6.12 Debug build and **100/100 Linux/offscreen CTest passed
(36.53 s)**. Both initial new Research/Hab regressions failed on the old UI.
Research coverage includes append/remove/reorder, a physical level completion,
refresh after restoring earlier state, and the maximum-level boundary without
blocking other fields. Hab covers 100%, actual below-100 improvement with
available technology, and below-100 equality without it. The prior positive
fixture now explicitly grants terraforming technology instead of asserting an
unchanged comparison. Existing knowledge/engineering, production, routes,
save/PBEM and layout checks pass. Themed Research and route images at 0.6×,
1.6×/4× zoom were captured; layout/line thickness inspected, diff check clean.

The slice is ready for review; new PR/head/CI are tracked in GitHub #252.
No new merge or remote CI success is claimed. Next are Ship Designer, station
icon and new actions; Scrap Fleet's partial-return rule is recorded, exact
recovery percentages undecided. #53/#63 and evacuation remain frozen; further
AI development is deferred.

## UI playtest #250 — Add reliability and change-driven refresh, 1 November 2026

The user selected mini-ideas 8/9 first, followed by Research/Hab/thin-route
polish, then designer and new-action work. Main `787f499` already contains
merged #247/#249/#245/#248; its CI run 41304477536 passed 100/100 (82.53 s).

Branch `codex/production-ui-refresh` reproduces the lost Add click with actual
mouse press/hold/release on main, then fixes it by removing periodic production
refresh and transient button disabling. Planning changes refresh the queue;
unchanged input and draft controls retain its display/forecast. Context, colony
and route updates follow notifications/coalesced one-shot callbacks instead of
120/150/180 ms polling. Mineral row fitting follows layout/font/style events.

Full GCC14.2/Qt6.12 Debug build and **100/100 CTest passed (35.74 s)**, including
held clicks across both idle and genuine plan updates, immediate research ETA,
same-turn save reload, draft/model stability, deferred selection and the
existing cargo/workforce/received-knowledge/layout suites. At turn 70 in the
same deterministic 288-system/7-computer campaign, idle CPU for ~2 seconds falls
**264.14 → 8.27 ms**; longest heartbeat gap **44.16 → 21.66 ms**. Redraw/save
cost is essentially unchanged. [Details and reproduction](ui-performance.md).
The user's particular save and all possible late-game bottlenecks remain
outside this controlled measurement. Save v108/orders v47 unchanged.

This slice is ready for review; publication/head/CI are tracked in GitHub
[#250](https://github.com/kotrf/suns/issues/250). No new merge/CI success is
claimed. Scrap Fleet's partial orbital recovery/collectible space debris is
recorded for a later slice; recovery percentages remain undecided. #53/#63 and
evacuation remain frozen; further AI development is deferred.

## Integration checkpoint — all four reviewed PRs

On 1 November 2026 the user explicitly authorised merging #245, #247, #248
and #249. Starting main was `23d6b0f`. Each original head has independently
verified successful CI: #245 `770d358` (96/96), #247 `a06f298` (94/94), #248
`ab1f5ee` (98/98), #249 `2241898` (96/96). SHA checkout, Qt6.12.0 and CTest
summaries were checked in the server logs.

The complete local integration passed a full GCC14.2 / Qt6.12 Debug build
and **100/100 Linux/offscreen CTest (35.84 s)** before any GitHub merge.
All C++ changes merged automatically. Conflicts were confined to documentation
and adjacent CMake registrations; both the race-file and fleet-planning suites
are present. The tested code includes current-year mining before cargo,
post-cargo factory operation, independent racial settings/race files, arbitrary
coordinate waypoints, pending fleet reorganisation/names, ordinary fuel exchange
and the commands-in-flight regression. Save v108 / orders v47.

Publication order is #247 -> retarget #249 to main -> #249 -> update #245 with
that main -> #245 -> retarget/update #248 with main -> #248. Dependencies must
be incorporated into main rather than merged into a feature base. The final
GitHub tree is compared with this local integration; actual merge SHAs and
latest remote CI remain authoritative. Subsequent edits to this checkpoint
are documentation only. Further #53, AI expansion, evacuation and #63 remain
frozen/deferred under the user's earlier decisions.

## Reviewed dependency — PR #245 mining before cargo

Checked 1 November 2026: the review of #245 head `fb0d5ce` identified one P2
([thread](https://github.com/kotrf/suns/pull/245#discussion_r4433394858)).
Post-cargo population was funding current-year mining, although ordinary
extraction physically runs before cargo orders. With 500 residents, 100 mines
and a 100,000-person unload, this promised a factory on Turn 2 rather than the
actual Turn 3.

The queue forecast now accepts received pre-order extraction for its first year
while retaining post-cargo production staffing and stocks. Later years derive
mining from evolving population and completed infrastructure. Production,
engineering and resonance forecasts use the same first-year input. Current-year
mining in the Minerals table and mining details uses pre-cargo residents;
active/built counts and marginal future mine benefit still reflect the accepted
cargo plan. Output, research and orbital-growth estimates use that same mineral
budget for upkeep; zero extraction is an explicit valid input. No physical turn
phase, save payload or order format changed: this branch stays v108/v47.

Validation: full GCC14.2 / Qt6.12 Debug build and **96/96 Linux/offscreen CTest
passed (36.08 s)**, plus the final expanded workforce suite. Regressions compare
forecast/UI with real TurnProcessor for loading and unloading across staffing
thresholds, one factory and a batch of ten, future reactivation, orbital upkeep
with zero/low starting extraction, and unchanged planning state. The reported
case shows 1.000667 I and Turn 3 in both preview and resolution. The regression
fails with the former forecast calculation; the themed UI capture was inspected
and diff check is clean. Local checks do not claim a new GitHub CI result or a
merge.

Dependency integration into #248: its complete race-file tree was rebuilt and
**98/98 Linux/offscreen CTest passed (35.53 s)**. The forecast implementation
and its regressions exactly match the reviewed #245 fix; both histories are
retained in the dependency merge. Race import/export and the real editor path
remain covered. The dependency PR still targets #245, pending its merge to main.

Main remains `23d6b0f` after #244/#246. #248 (race files) depends on #245 and
now includes this fix in its existing branch; merge #245 then retarget #248 to
main. Separate coordinate #247 and fleet/fuel #249 remain open; #249 depends on
#247. The user merges PRs. #53 and the previous deferred directions stay frozen.

## Earlier checkpoint — independently prepared player races

Checked 1 November 2026 (Europe/Moscow): main is `23d6b0f` after merged #246,
save v107 / orders v47. Population-operated industry #245 and coordinate
waypoints #247 remain separate open PRs. User selected pre-game race-file
exchange as the next step toward playing a human PBEM campaign.

Branch `codex/player-race-files` builds on #245 and retains save v108 / orders
v47. File → Race designer saves/loads a participant's `.sunsrace` without
changing the active campaign. New campaign imports into an explicit player
slot, switches that slot to Human, and offers Edit/Save with the same form.
Every supported race setting, including both workforce rates, survives the
file-to-campaign path and subsequent reload/restart/player-turn export.

The independent JSON schema v1 is bounded to 64 KiB and validates exact field
types, enums and ranges. It carries no controller, campaign/player identity,
world state, technologies or credentials. Failed import/cancel preserves the
current race and atomic writes preserve existing files on failure. See
[race files](race-profile.md#portable-race-files) and
[participant workflow](multiplayer-turn-exchange.md#prepare-each-players-race).

Validation: full GCC14.2 / Qt6.12 Debug build; **98/98 Linux/offscreen CTest
passed (78.11 s)**. New suites cover every supported race-file choice,
malformed/missing/oversized data, exact field types/rates, unchanged output on
failure, the actual File menu/editor/save/load/cancel path, two imported Human
slots, per-slot editing/export, unchanged active-game bytes, reload/restart and
the remote player's own race in a PlayerTurn. The two themed dialogs were
visually checked; diff check is clean. Prepared for review, not merged; no new
GitHub CI result is claimed. Merge the #245 dependency first.

## Earlier checkpoint — population-operated factories and mines

Checked 31 October 2026: #244 is merged as main `72009d4`, save v107 / orders
v47. Branch `codex/population-operated-industry` implements the accepted next
step: separate racial operation rates, each 5–25 per 10,000 surface residents,
defaulting to 10/10 and exposed for every human/computer empire in New campaign.
Working buildings are `min(installed, floor(population × rate / 10000))`.
At 25,000 residents and 10/10, 25 factories and 25 mines operate simultaneously;
a current-turn load of 2500 leaves 22 of each working. Excess buildings remain
installed and reactivate after growth or unloading. Capture uses the new owner's
rates. Orbital residents retain orbital output without staffing surface assets.

Factory output, research, ordinary and xenocrystal extraction, upkeep budgets,
forecasts and history use the shared workforce calculation. Existing computer
development/reconstruction avoids spending on buildings beyond received limits
and resumes useful construction after received population growth. This adapts
existing rules; further AI automation and #53 remain deferred/frozen.

System Status and selected-system details show active/built counts, with
population limits and rate tooltips. Mining details explain idle marginal mines.
The already merged cargo-plan projection updates these counts immediately.
Manual excess construction remains allowed; build prices and existing population
resource/baseline-mining contributions retain their rules.

Branch save **v108 / orders v47**: fixed-size, count/ID/range-validated WFRK
extension persists rates. Old saves migrate to 10/10 while retaining buildings
and past history. A v107 fixture is byte-checked against the former writer's
SHA256, then loaded and continued. PlayerTurn retains the owner's rates and
withholds foreign settings; normal PBEM orders remain unchanged.

Validation: full GCC14.2 / Qt6.12 Debug build; Linux/offscreen CTest **96/96
passed (38.70 s)**; diff check clean. Dedicated core/save suites cover independent
rates, proportional rounding, loss/reactivation/capture, shared mining/production,
current cargo plans, growing and declining production forecasts versus real
turns, AI limit/resumption, extreme integer bounds, valid host/PlayerTurn/PBEM
round trips, real v107 migration/continuation, foreign export invariance and
malformed/truncated rates/IDs/counts. Existing fixtures that relied on unstaffed
factories now provide workforce or already-completed work; installation costs,
queue order, science and communication assertions remain covered. Core/Qt tests
also check xenocrystal dormancy, New campaign settings, reload/restart retention
and live 22/100 to 25/100 counts when reversing cargo. UI captures were visually
checked. Prepared for review; no merge or new remote CI result is claimed.

## Reviewed dependency — PR #249 commands already in flight

Checked 1 November 2026: review of `0e8c251` against coordinate base `a06f298`
identified one P2 ([thread](https://github.com/kotrf/suns/pull/249#discussion_r4435207719)).
The received view cleared already sent commands, so an apparently idle remote
fleet could acquire a phantom Split in the UI while the physical host rejected
it. Both the real Split dialog and detachment-reference regressions reproduced
the problem before the fix.

The fleet and disposable movement previews now retain the owner's already sent
command intent before projecting reorganisation, matching the existing route
forecast. Split and both ends of idle Merge remain busy while that command is
in flight. The host's temporary-ID calculation applies the same condition, so
an invalid earlier Split cannot consume the reference intended for a later
valid detachment. Reports, physical manifests and telemetry dates are not
restored, and generic PlayerView/player-turn sanitisation stays unchanged.

Validation: complete GCC14.2 / Qt6.12 Debug build and **96/96 Linux/offscreen
CTest passed (35.30 s)**. Real Move -> year 2 -> command due in year 35 keeps
Split/Merge blocked without new orders or fleets, including after save/reload;
a busy source is excluded from merging into an idle destination. Loaded older
invalid Split plus a valid Split preserves composition/IDs/names in both
previews and in physical resolution with a hidden global allocator. Existing
fuel/rendezvous, route, communications and knowledge suites also pass; diff
check is clean. The focused suites failed on the pre-fix code and pass now.

The former head's [GitHub CI run](https://github.com/kotrf/suns/actions/runs/41295280523)
was independently checked: checkout `0e8c251`, Qt6.12.0, completed linux job,
96/96 CTest in 80.42 s. That result belongs to the former head; no new remote
CI result is claimed for this follow-up. Save/orders remain v107/v47.

#249 still depends on open #247: merge #247 first, then retarget #249 to main.
Separate #245 is updated at `770d358` (mining-before-cargo fix, 96/96); #248
includes it at `ab1f5ee` (98/98) and must target main after #245 merges. Main
remains `23d6b0f`. The user merges PRs; #53 and prior deferrals stay frozen.

## Earlier checkpoint — fleet planning and ordinary fuel exchange

Checked 1 November 2026: main `23d6b0f` contains merged #244/#246. Branch
`codex/fleet-planning-and-fuel` builds on open coordinate-waypoint #247, with
save v107 / orders v47 unchanged. It addresses the user's fleet gameplay report:

- Selected fleet, map markers/orbital picker, source selector and fleet summary
  share the valid command-boundary projection. Split exposes both detachments
  immediately; pending names are consistent everywhere. A new detachment can
  be renamed, loaded/refuelled and assigned a route before End Turn.
- Detachment references are derived from PlayerView. The host binds each
  successful projected split to its private allocation within that player's
  submission, including nested splits and later source/target references.
  Hidden fleet IDs and simultaneous foreign splits cannot redirect orders;
  failed ancestors invalidate subsequent references.
- Cargo adds reversible Fuel controls for two ordinary friendly fleets at the
  same received position, including deep space under a current live link.
  Both projected tanks update immediately. Donor stock and receiver capacity
  bound the amount; fuel remains separate from hold capacity. Physical exchange
  executes before movement, allowing same-year departure. Expedition bases keep
  their dedicated docking/service phase. A queued gate hop does not change the
  received orbit at which Cargo opens.
- Destination includes all received friendly fleets, with locations. Transfer
  fuel to fleet / outpost follows a friendly FleetId and transfers on contact,
  keeping a donor reserve and leaving both fleets intact. Merge remains a
  separate action. Fixed-coordinate outpost supply retains its previous rules.
- Destination's size policy keeps long names from widening the narrow Fleet
  panel. Scene teardown no longer refreshes panels after state members die.

Validation: full GCC14.2 / Qt6.12 Debug build; **96/96 Linux/offscreen CTest
passed (35.55 s)**. New core/Qt suites cover actual Split/Rename dialogs and
orbital clicks, detachment route/forecast, ordinary fuel conservation, capacity,
reserve, invalid amounts/ownership/location, deep-space exchange and funded
same-year departure, return via the receiving slider, Cancel, moving-target
fuel delivery without merge, hidden allocation changes, simultaneous players
in both submission orders, failed nested splits, save/reload and actual PBEM
order-file round trips. Existing gate/received-station invariance and compact
Fleet layout checks also pass. Fuel dialog and 1000 px Fleet captures inspected;
diff check clean. Local checks do not claim a new GitHub CI result or merge.

Separate open work remains #245 (population operation, v108/v47) and #248
(independent race files, based on #245; merge #245 then retarget #248 to main).
For this fleet change, merge #247 first and retarget its dependent PR to main
before merging. The user merges PRs. Further #53/AI/evacuation/#63 remain deferred.

## Earlier checkpoint — accumulated UI feedback in PR #244

Base main remains `bdaac4b` after merged #243; [PR #244](https://github.com/kotrf/suns/pull/244)
is open on `codex/map-ranges-and-mineral-layout`. Save v107 / orders v47 remain
unchanged. The original active-only sensor defaults and measured Minerals
height are retained. This follow-up implements the user's accumulated notes:

- Fleet emissions moves below the route editor inside its scroll area; its
  controls fit the narrow column. The fixed fleet header stays above the scroll.
- Hab 100% omits the redundant terraforming suffix. Lower habitability retains
  the potential, and engineering status remains visible.
- Turn Messages uses a dark, hover-highlighted splitter handle.
- Orders this turn is an independent scrollable reader opened through View and
  the Orders toolbar action. Live updates preserve reading position. The
  original full empire scope is retained and the dock summary is removed.
- Toolbars have recognisable scalable line icons with text and tooltips.
- Cargo's right-hand sliders can return previously planned colonists/minerals.
  Mixed directions are emitted as ordinary nonnegative transfers in a feasible
  order; final stocks and shared capacity are validated with the common command
  projection. Invalid partial swaps are disabled. Cancel preserves prior intent.
  Compact System Status/Minerals and colony/empire output use accepted cargo
  intent immediately, retaining received state and normal End Turn resolution.

Validation: full GCC14.2/Qt6.12 Debug build and **94/94 offscreen CTest passed
(39.62 s)**. Expanded Qt regressions cover loading 2500, full return, simultaneous
colonist return/mineral load, mineral return and Cancel; host state stays
unchanged during planning. The final expanded Qt check also compares the real
End Turn manifests, mineral conservation after mining and population after growth. They also cover Hab 100%/lower Hab, scrollable fleet
emissions, a 150-order reader with retained scroll position, menu access and
reopening one window. Normal/small workspace captures and the cargo/Orders
windows were visually checked. Diff check is clean. This records local checks;
new GitHub CI or merge is not claimed.

The user accepted **population-operated factories and mines as in Stars!**:
independent racial settings of 5–25 each per 10,000 residents; excess buildings
remain idle. At 10/10, 25,000 residents support 25 factories and 25 mines together.
They explicitly requested these UI fixes first. Workforce economics is the next
implementation stage, not included in #244. Station fitting was found and judged
clear; no extra station-fit screen is requested. Further #53 remains frozen
until gameplay experience; AI expansion, evacuation and #63 remain deferred.

## Earlier checkpoint — scanner overlays and Minerals layout

Checked 31 October 2026: #243 is merged as main `bdaac4b`, save v107 /
orders v47. The user froze all further #53 development until more gameplay
experience, superseding the earlier transport roadmap. Implemented lines stay
available. AI expansion is deferred; evacuation and #63 remain frozen. Remaining
#61/#56 scope is being discussed; no next implementation slice has been chosen.

Branch `codex/map-ranges-and-mineral-layout` addresses the player's screenshots:
Sensors defaults to one active ship scanner circle (blue) plus planetary radar
(green). A split-button menu optionally adds the passive hull baseline (gold)
and reception of other ships' emissions (violet); switching Sensors off hides
all ranges. Circle tooltips identify their meaning and radius. Detection rules,
received sensor measurements and save/order formats are unchanged.

The Minerals table now measures its header, three rows and frame using the live
font/style instead of a fixed 92 px height. The existing colony refresh updates
that height after font/style changes, keeping Germanium fully visible.

Validation: full GCC 14.2 / Qt 6.12.0 Debug build passed; Linux/offscreen CTest
**94/94 passed (40.00 s)**. The added row-visibility regression failed before the
fix and passes with 11/14/18 pt fonts plus item padding. Map coverage verifies
default/optional/master-off overlays and passive scanner mode. Existing small
and normal window layouts pass; a captured 1366×900 layout was visually checked.
`git diff --check` clean. Prepared for review; no merge or new remote CI result
is claimed.

## Historical pre-merge checkpoint — ordered multi-stop mineral lines (#53)

Starts from merged #242, main `1d17369`, save v106 / orders v46, green main CI.
Branch `codex/transport-line-multi-stop` adds up to 15 extra recipients after
the primary recipient, per-stop stock targets, ordered delivery visits with
physical surplus carried onward, and a return to the ordinary loading source.
Missing/foreign recipients block delivery; full installations accept what fits
and let the remainder continue. Two-stop demand-waiting semantics stay unchanged.
Received coordinates, ordinary delayed commands, bounded sparse TLMS catalog
extension and versioned delivery visits preserve fair knowledge and PBEM.
Branch save v107 / orders v47. Full GCC 14.2 / Qt 6.12.0 Debug application/test
build passed; offscreen Linux CTest **94/94 passed (39.75 s)**; diff check clean.
Expanded existing core/Qt/save suites cover autonomous distribution and conserved
surplus, full intermediate installations, foreign/neutral blockers and delayed
edits, received moving/waiting status, AI respect for extra-recipient intent,
real add/update/save controls and full map cycle, draft/physical/in-flight
payloads, save continuation, PBEM/forged old envelopes, malformed/truncated TLMS,
received-only remote exports and genuine v106 continuation plus older migrations.
Prepared for review, not merged; #53 stays open. No manual campaign balance,
cross-platform or sanitizer acceptance is claimed.


### Review follow-up — reset stop drafts on Open/load

The [P2 review finding](https://github.com/kotrf/suns/pull/243#discussion_r4431611639)
was reproduced through real Qt file loading: an unsaved 90 kt stop persisted
after loading the same line ID with a saved 40 kt target. Loading now invalidates
the optional draft selection and refreshes even a hidden Logistics dock from
new intent. New-line ID zero is covered; ordinary non-reset polling preserves
unsubmitted edits. The actual restored selector ID is used after rebuilding the
line list, including fallback to New line.

The expanded existing Qt suite failed before the fix, then passed after it.
It exercises visible/hidden docks, older same-campaign saves, a different
campaign, Host-to-PlayerTurn loading, renaming and actual roster dispatch after
load, and new-line draft reset/polling. Full GCC14.2/Qt6.12.0 Debug rebuild and
94/94 offscreen CTest passed (39.75 s); diff check clean. Formats remain v107/v47.
This fixes the reported P2; it does not claim a new independent review or merge.

## Merged implementation — stationary-installation supply (#242)

Checked 30 October 2026 (Europe/Moscow): #241 merged as `7292305`, main CI
94/94; main save v105 / orders v45. Branch `codex/transport-line-outpost-supply`
adds mineral delivery endpoints for exact owned stationary depots, relays and
independent orbital shipyards, with received-only authoring/status, capacity
limits, target stock, retained surplus and delayed receipts. Branch save v106 /
orders v46; ordinary planetary payload layouts and old migrations remain.
Further AI automation is deferred; network evacuation and #63 stay frozen.
#53 remains open. Detailed contract and boundaries: [transport lines](transport-lines.md).

Validation: full GCC 14.2 / Qt 6.12.0 Debug application/test build passed;
Linux/offscreen CTest **94/94 passed (40.05 s)**; `git diff --check` clean.
Expanded existing transport suites exercise real depot and independent-yard
delivery, finite/fractional shared capacity, target demand, conservation and
continued operation without HQ, missing/foreign/moved/disabled receivers,
received-only authoring/export, pause after endpoint loss, actual Qt endpoint
selection, held and in-flight programmes, delivered and pending delayed receipts,
PBEM roundtrip/forged old envelope rejection and v105 planetary migration.
Existing v104/v103/v102 and other suites pass. Merged as main `1d17369` on 31 October 2026; main CI is green.
#53 is not closed. No manual campaign balance, cross-platform
or sanitizer acceptance is claimed.


## Merged implementation — destination stock targets (#241, historical branch checkpoint)

Checked 29 October 2026 (Europe/Moscow): #240 merged as `0b67ec1`, CI green;
main save v104 / orders v44. Branch `codex/transport-line-stock-targets` starts
there and uses **save v105 / orders v45**. The user requested the next player
logistics slice. Further AI automation is deferred; network evacuation and
#63 remain frozen. #53 stays open.

An optional whole-kt destination target freezes into ordinary attributed
unload actions. Zero preserves full unloading. Positive targets unload only
the actual deficit at the exact owned destination, retain excess physical
cargo and retry annually without HQ orders. The programme returns to its
source once the selected cargo is empty. Capture/neutral/wrong destinations
block rather than dump that held cargo. Existing source floors/minimum loads,
shared holds, mass, fuel, combat and delayed Stop/edit/resume remain ordinary.
Only positive actual partial unloads create Freight/line receipts. Received
waiting status and a conditional forecast do not promise future consumption;
geometric prediction cannot assume the policy completed. UI edits are normal
orders. Versioned fields default to zero for older catalogues/programmes.
[Rules and remaining scope](transport-lines.md).

Validation: full GCC 14.2 / Qt 6.12.0 Debug application/test build succeeded;
Linux/offscreen CTest **94/94 passed (80.45 s)**; `git diff --check` clean.
Expanded core/Qt/save suites cover a multi-year autonomous target cycle with
actual paid factory consumption and conserved minerals, continued work without
the HQ catalog, competing/fractional unloads and other cargo, idle fuel and
positive-receipt-only behavior, capture/neutral ownership blockers, pause/resume retaining
cargo, a target edit arriving after 34 years, stale-geometry freezing, invalid
targets, actual Logistics editing and draft/received-stock forecasts, partial
waiting and in-flight target persistence, hidden-event player-export invariance,
PBEM fields/old-envelope/trailing-byte rejection and genuine v104 full-unload
migration (plus existing v103/v102 fixtures). Other cargo, route, controller and
save suites pass. No manual campaign balance, cross-platform or sanitizer
acceptance is claimed. Prepared for review; no automatic merge/issue closure.

## Merged implementation — transport-line statistics and interruptions (#240, historical branch checkpoint)

Checked 29 October 2026 (Europe/Moscow): #239 is merged as `db1204d`, CI green;
main save v103 / orders v43. Branch `codex/transport-line-statistics` starts
there and uses **save v104 / orders v44**. The user prioritized player-facing
logistics statistics/diagnostics, then destination targets and depot/station
service. Further AI automation is deferred; network evacuation stays frozen
pending actual play. #53 remains open, #63 remains frozen last.

Frozen line/revision/destination attribution in ordinary mineral unload actions
produces delayed immutable receipts only for actual successful owned-destination
unloads. The owner-private bounded received ledger feeds Logistics volume/rate
in the last 20 receipt years, bounded receipt history with both dates, calculated
flight-only round-trip lower bounds, dated hold occupancy and received causes
of interruptions. Geometry does not substitute for new telemetry. Ordinary
colony Freight/History remains the only economic delivery event. Existing
v103 programmes keep running without automatic remote upgrades; explicit
edit/reassignment enables attribution. [Rules and boundaries](transport-lines.md).

Validation: full GCC 14.2 / Qt 6.12.0 Debug application/test build succeeded;
Linux/offscreen CTest **94/94 passed (82.09 s)**; `git diff --check` clean.
Expanded core/Qt/save suites cover actual attributed unloads, neutral/enemy/
wrong-destination guards, conserved cargo after Stop/split and both merge
paths followed by reassignment, old-revision reports delayed 34 years,
deduplication across replay/save/load/window expiry, bounded history/replay
watermark, received-only status despite projected geometry, hidden-event byte
invariance of player exports, real Logistics metrics/receipt controls, pending
and delivered receipt persistence, new action PBEM roundtrip/forged old-envelope
rejection and genuine v103 programme migration. Ordinary route/fleet and all
other suites pass. No manual campaign balance, cross-platform or sanitizer
acceptance is claimed. Actual cycle-duration/time-utilization analytics remain
future work; estimates and snapshots are labelled. Prepared for review; no
automatic merge or issue closure.

## Merged implementation — computer-player transport lines (#239, historical branch checkpoint)

Checked 29 October 2026 (Europe/Moscow): #238 merged as `830bcd1`; main and
branch `codex/transport-lines-ai` use save v103 / orders v43. The user requested
continuing #53 and froze network evacuation automation until more actual play.
#53 remains open; #63 remains frozen last. No evacuation automation is added.

The AI can create one two-colony I/B/G supply line per turn with an existing
empty idle freighter. Received surplus/shortage, real fitted cargo and fuel,
received depots, source reserve and onboard departure minimum determine the
ordinary delayed orders. HQ catalog intent reserves a fleet during stale
telemetry and after save/load; other planners cannot requisition it silently.
No extra memory/format fields or free production are introduced. Automatic
ship construction, mining-cache service, demand retasking, interruption recovery
and statistics remain future #53 work. [Rules and limits](transport-lines.md).

Validation: full GCC 14.2 / Qt 6.12.0 Debug application/test build succeeded;
Linux/offscreen CTest **94/94 passed (69.71 s)**; `git diff --check` clean.
Expanded core/Qt/save suites cover actual AI dispatch and unattended physical
delivery/conservation/source reserve, fitted fuel use and fuel-free fallback,
deterministic reordered inputs, received-stock/depot/cargo/damage guards,
hidden-world planning invariance, one assignment through a 34-year command
delay, protection against military reassignment, paused intent, and real host
save/load/controller continuation through delayed delivery. Existing LINE,
route-policy, Qt controls, migrations and PBEM suites also pass. No manual
campaign balance, cross-platform or sanitizer acceptance is claimed. Prepared
for review; no automatic merge or issue closure.

## Merged implementation — transport-line load policies (#238, historical branch checkpoint)

The user authorized merging reviewed #237 and continuing #53. PR #237 merged as
`4fb67cf`; checked head `2fa89b7` passed CI 94/94 (81.23 s). Branch
`codex/transport-line-load-policies` starts there: main save v102 / orders v42,
branch v103 / v43. #53 stays open; #63 remains frozen.

Two zero-default whole-kt policies: preserve a source reserve and optionally
wait for a departure minimum of the selected I/B/G aboard. Physical loading
keeps partial cargo, respects shared capacity and source ownership, and retries
locally without annual HQ control. Stop/edit/resume retain real cargo and use
ordinary delivery. An impossible current hold does not silently lower the goal.
Received telemetry/status and conditional forecasts expose the waiting state;
geometric projection cannot assume that an unseen future loading attempt met
its threshold. Policy payloads travel in existing physical/received/in-flight
route structures, with versioned catalog/order fields and v102 migration.

Validation: full GCC 14.2 / Qt 6.12.0 Debug application/test build succeeded;
Linux/offscreen CTest **94/94** passed (65.65 s); `git diff --check` clean.
Expanded transport core/Qt/save suites cover source floors, gradual unattended
loading/departure, capacity blockage, capture, stop/resume, delayed payloads,
received waiting status and conditional forecast, stale geometry, hidden export
invariance, physical/received/in-flight save state, v102 migration and new PBEM
policy fields. No manual campaign balance, other-platform or sanitizer acceptance
is claimed. This follow-up is now merged as noted above.
[Rules and remaining #53](transport-lines.md).

## Merged implementation — transport lines (#237, historical branch checkpoint)

Branch `codex/transport-lines` starts from main `c957b12` (merged #234).
The user selected #53 on 28 October; the previous roadmap postponement is
superseded by this request. Main is save v101 / orders v41; branch v102 / v42.

The first slice adds an owner-private stable/revisioned catalog of named two-stop
I/B/G shuttle lines and multiple assigned paid fleets. Create/edit/pause/resume/
delete/release use ordinary orders and delayed repeating Load All / Unload / Stop
programmes. Once dispatched, movement runs without the catalog or further HQ
commands. Existing cargo, mass, fuel/refuelling, combat and ownership rules apply.
The Logistics dock separates roster/draft intent from received programme status,
evidence age, target, hold and stocks; map overlay shows registered definitions.
Draft line commands participate in route preview and manual overrides preserve
command order. Bounded LINE and new order tags support save/load and PBEM.

[Rules, constraints and remaining scope](transport-lines.md). No automatic AI
staffing, new throughput attribution, emergency evacuation, depot/fleet stops or
expanded exotic/engineering mechanics are included. #53 remains open.

Validation: full GCC 14.2 / Qt 6.12.0 Debug application/test build succeeded;
final Linux/offscreen CTest **94/94** passed (65.41 s), `git diff --check` clean.
Two new suites cover autonomous multi-fleet finite/conserved freight, delayed
edit/pause/resume, same-dispatch Stop/Resume, manual override and draft route
editing, received-only validation/export invariance, actual Qt controls/map
overlay, v101 migration, bounded/truncated/duplicate LINE data and v42 PBEM
orders (including rejection under forged old envelopes). No manual campaign
balance, native-platform or sanitizer validation is claimed. Prepared for review;
no PR merge or issue closure.

## Merged implementation — orbital base retirement and colony rebuilding (#234, historical branch checkpoint)

PR #234 / branch `codex/orbital-base-lifecycle` originally started at `6d04c44`
(#233), and now integrates fresh main `7e2e769` (merged #236). The #236
reviewed head `95c3bf8` passed GitHub CI. Main is save v100 / orders v40; this branch
uses save v101 / orders v41. The combined writer appends CCON then ORLC;
v100 retains colony conditions, development v99 retains retirement state.
#54/#61 remain open, #63 stays frozen.

The user selected base lifecycle as the next slice. Safe delayed retirement
waits for evacuation and real recovery transport capacity, conserves remaining
stocks/fuel, and recovers 25% of the current fitted hull/paid assembly mineral
bill reduced by damage. Colony rebuilding queues a full-price ordinary station
only after actual retirement; no services are granted while it builds. Stable
request/cancel receipts and delayed completion knowledge survive save/load and
PBEM. Station capture and automatic destructive AI policies remain future work.
[Detailed rules](independent-orbital-shipyards.md#retirement-and-colony-station-rebuilding-54--61).

Integration follow-up: v99/v100 migration fixtures exercise physical, received
and in-flight retirement state, delayed cancellation and exact CCON evidence.
A reproduced faster-recovery-manifest defect is corrected: confirmations
follow the transport and its merged successors, while old UI/export contacts
keep their received telemetry. The archived physical request is persisted by
existing ORLC; no extra command fields or snapshot exposure are introduced.

Review follow-up — 28 October, `bcc1155`: reproduced the P2 through an actual
paid rebuild, a departing onboard transport route and restored scanner mesh.
The T7 station survey overtakes the T8 retirement report; pre-fix player-turn
write succeeds but read rejects the conflicting orbital occupants. Delivered
newer station evidence now removes the old base loss contact durably, without
changing the separate recovery report or reading physical station truth. The
regression includes hidden-construction export invariance, host save/load,
player-turn roundtrip and a subsequent delivered station loss without revival.
Save v101 / orders v41 are unchanged.

Current integrated validation: GCC 14.2 / Qt 6.12.0 Debug full application/test
build succeeded; post-review offscreen CTest **92/92** passed (60.36 s),
and `git diff --check` is clean.
New scenarios cover same-year real evacuation/recovery, shared-hold/tank guards,
precious cargo and queued-job blockers, one-time damage-adjusted recovery,
delayed/overtaking cancellation, full normal station production/mineral payment,
34-year command delay, silent report handover, immutable player exports,
physical/received/in-transit retirement snapshots, retained dismantled contacts,
v100/v99/v98/v97/v96 migration, malformed/missing ORLC/CCON tails and PBEM
payloads. The station-cost
control includes ordinary population mining rather than assuming fixed stocks.
Validation is Linux/offscreen Qt; no manual balance, cross-platform or sanitizer
acceptance is claimed. Prepared for review; no merge or issue closure performed.

## Merged implementation — main audit corrections (#236, historical branch checkpoint)

Branch `codex/audit-corrections` starts from main `c161b0e` (merged #235).
The user authorized all 12 reproduced correctness fixes and three measured
performance corrections. [Original audit and correction matrix](audits/2026-10-27-main-code-audit.md).
Main remains save v98 / orders v40; this branch uses save v100 / orders v40.
CCON stores dated defense/war-damage evidence alongside immutable colony packets.
Development save v99 from separate PR #234 is explicitly rejected rather than
silently dropping its ORLC missions. That PR needs format reconciliation on rebase.
#63 remains frozen; empty-colony factory policy is outside these proven defects.

Validation: GCC14.2 / Qt6.12 Debug full application/test build succeeded;
post-review offscreen CTest **92/92** passed (59.73s), `git diff --check` clean.
Two new suites cover core/save and real Qt controls; existing sample/legacy-save
fixtures were updated for received merge contacts and trailing extensions.
A separate150-boundary Medium/288 campaign with5AI saves, loads, reserializes
and continues from loaded state every year: all150 cycles succeeded; bytes are
stable after the initial legacy telemetry-stack normalization.
Measured CPU: route warning W1 1379→32ms; reversed128-node communication queries
1106→45ms; probe1000/full notebook target selection430→0.8ms.
A later faster full manifest forwards/coalesces its merge confirmation so an
absorbed source cannot coexist with its newly received combined manifest.
Unknown source ships never create phantom missing contacts.

Review follow-up on `8c0ea71`: both P2 findings reproduced before correction.
Route forecasts now retain each PlayerView missing contact exactly once (the
real Qt mining/load forecast stays at 8 kt instead of 16 kt). A faster full manifest
forwards the entire pending merge lineage A→B→C, independent of report order
and through a final edge whose report needs no deadline adjustment. Core tests
perform two real rendezvous merges, host save/load and player-turn export;
the received result contains exactly three ships and 30 kt, with no old contacts.
Save v100 / orders v40 are unchanged.
Linux/offscreen validation only; no native chooser, user disk, other OS or
sanitizer acceptance is claimed. Ready for review, no automatic merge.

## Merged implementation — Medium-map UI preview performance (#235)

Branch `codex/ui-preview-performance` starts from main `6d04c44`, merged #233,
save v98 / orders v40. The independent retirement PR #234 remains separate;
no format, balance, simulation or knowledge-rule changes are introduced here.

The user reported map/panel slowdown at roughly 300 systems and a save freeze.
A confirmed UI defect repeatedly ran a full TurnProcessor movement/logistics
preview from polling panels even while idle. One per-window preview is now
shared until the planning revision/player/turn changes; redraw invalidation
covers edits, state updates, new games and same-year loads. Cargo regressions
exercise same-size replacement, repeated reads and actual same-year save/load.
The optional `suns_ui_performance_probe` target is outside CTest/default builds.
[Reproduction, measurements and save-investigation boundary](ui-performance.md).

Local GCC 14.2 / Qt 6.12 Debug/offscreen measurements: at 288 systems idle CPU
changes from ~100% to 4.4% of one core, heartbeat gaps 317 → 22 ms, full redraw
186 → 124 ms. At 1000 systems, gaps are 1170 → 53 ms; other refresh costs remain.
All four measured fresh saves loaded successfully. The player's save freeze
is not reproduced; OS/build provenance and chooser-versus-write stage are pending.

Validation: full GCC 14.2 / Qt 6.12.0 Debug application/test build succeeded;
final offscreen CTest **90/90** passed (59.45 s), and `git diff --check` is clean.
Four focused editor suites passed (1.93 s). The optional probe built and all
four isolated 288/1000-system control/fixed runs saved and loaded successfully.
Native file-dialog behavior, the player's exact campaign and other platforms
remain unverified; no fix for the reported save freeze is claimed. Ready for
review without automatic merge.


## Merged implementation — independent orbital founding (#233, historical branch checkpoint)

Branch `codex/independent-orbital-shipyards` starts from main `0d4ec03`, merged
#232. Its reviewed head `004da54` passed GitHub CI; main is save v96 / orders v39.
The user chose permanently orbital yards founded from physically transported
constructors, real workers/supplies and fitted passive radiation protection.
[Complete first-slice rules and prototype balance](independent-orbital-shipyards.md).
The branch uses save v98 / orders v40; #54/#61 remain open and #63 stays frozen.

The implementation reuses anchored FleetIds, actual hull/fittings/cargo/combat,
ordinary delayed commands and immutable telemetry. It adds six supplied annual
assembly stages, 40-work staffed manufacture, cumulative whole-kt mineral
payments, paid refits and finite life support/refueling. Orbital background is a
fixed public stellar-class proxy, independent of surface Hab/magnetospheres;
three mutually exclusive screen modules subtract 20/40/60. No natural growth,
surface mining, flying yard, new rare material or station capture is introduced.
A received base can survive subsequent friendly surface colonization; its queue
stays separate and it occupies that planet's one station slot. Converting or
decommissioning the fixed hull remains future work.

Validation: GCC 14.2 / Qt 6.12 Debug full application/test build succeeded;
final offscreen CTest **90/90** passed (56.88 s), and `git diff --check` is clean.
New core, Qt editor and persistence suites cover supplied assembly/pause/resume,
finite cumulative mineral payments and refits, durable request receipts,
delayed/overtaking cancellation, workforce/hold/fuel conservation and evacuation,
radiation/immune races and emergency attrition, permanent anchoring, occupied orbit,
hostile landing blocking, later friendly surface founding and combat destruction.
They also cover honest AI loading/deployment/ship orders, immutable received UI
and exports, hidden remote launches, malformed tails, v96 migration and PBEM.
The catalog count and historical v94 fixture were updated for the new components
and trailing extension. Full-field research/habitat/gate/navigation suites also
pass. No manual balance, cross-platform or sanitizer acceptance is claimed.
Prepared for review; no merge or issue closure is performed.

Review follow-up — 25 October: two P2 reproductions failed on `583d605` before
correction. Route source/target lists and explicit selection now withhold a
remote newborn ship until delivered telemetry; enemy target choices also use
received contacts. The AI keeps a per-base stable request ID in its durable
controller state and waits for received acceptance and completion/cancellation.
A newer empty queue alone cannot acknowledge an intention. Save v98 appends this
memory to ORBI; v97/v96 migration is supported, orders stay v40. The regressions
cover 39 years with a 34-year command delay and unavailable return telemetry,
in-progress/completed acknowledgements, repeat planning, save/load continuation,
and actual Qt source/target lists before and after delivered launch evidence.
Full Debug/Qt application/test build succeeded; final CTest **90/90** passed
(59.09 s, offscreen Qt), and `git diff --check` is clean. The new core/Qt
reproductions failed on the pre-fix head; the save/load continuation and v97/v96
migration checks now pass too. GitHub CI belongs to the newly pushed head.

## Historical branch — deferred/scheduled gates #195 (merged #232)

Branch `codex/scheduled-stargate-routes` starts from fresh main `4292aee`
(merged #231; exact merge CI green). It adds ordinary delayed gate commands and
an onboard gate arrival action with approach, separate hop years, physical
revalidation, hold/retry and subsequent route continuation. One hop per year
survives reorganization; receiving-system combat remains active. The UI authors
and forecasts received gate programmes; AI keeps normal received staging orders.
Save v96 / orders v39 extend the shared action codec; gate prices and #63 rules
stay unchanged. [Mechanics and boundaries](stargate-specialist.md#deferred-commands-and-scheduled-gate-routes-195).
Validation: GCC 14.2 / Qt 6.12 Debug full application/test build succeeded;
full CTest **87/87** passed (58.58 s, offscreen Qt), and `git diff --check` passed.
New checks cover multi-hop/repeating routes, real approach fuel, load → gate →
unload → flight, held/captured/out-of-limit transit and restoration, delayed
commands/stops, split-year limits, destination combat, stable competing pulse
payments, received-only AI/deferred staging, hidden-independent forecasts and
player exports, in-flight programme editing and save/PBEM action payloads.
Validation is Linux/offscreen Qt; no manual balance, cross-platform or sanitizer
run. GitHub checks are reported on the PR's exact pushed head. Prepared for
review; no merge or issue closure performed.

Review corrections — 25 October 2026: rendezvous merging now carries the latest
movement/service epochs to the surviving FleetId, preventing a just-gated fleet
from flying again that year. Gate forecasts retain the owner's sent command
journal with its original delivery dates while continuing to omit incoming
telemetry and hidden logistics. Reached receiving-system No action points are
accounted for alongside the hop, including trailing points and repeat restart.
Regressions cover the actual gate → merge → next-year flight, ETA after End Turn,
and intermediate/trailing no-action points in ordinary and repeating programmes.
The five focused suites pass (2.00 s). The full Debug application/test build and
CTest **87/87** pass (54.21 s, offscreen Qt); `git diff --check` is clean.
Formats remain v96/v39; no gameplay price or #63 extension is included.

Additional review P2 — 25 October: route editing now prioritizes the current
unsent draft, then the latest sent navigation command (including Stop), then the
received onboard programme. An old active-route telemetry snapshot can no longer
erase a gate step when a waypoint is appended. Non-navigation command packets
do not mask the sent route. The regression reproduces moving remote telemetry,
executes the extended programme through TurnProcessor without new commands,
compares the gate-only control and verifies a newer Stop does not revive an
older gate programme. Full Debug application/test build and CTest **87/87**
passed (55.89 s, offscreen Qt); `git diff --check` is clean. The reproduction
failed on the pre-fix editor, before the new TurnProcessor delivery checks.
No format or balance changes.

## Verified checkpoint — 24 October 2026: main after #231

Main `4292aee4c8e31aae592b389f206b6124dcae3e38` includes #231 and both
review corrections: full-hold arrival-load budgeting and confirmed initial depot
service for active programmes. The reviewed head `979b52a` and the merge both
passed GitHub CI; save v95 / orders v38. The agreed #105 navigation slice is
complete; optional medium extensions remain future work.

## Historical checkpoint — 23 October 2026: main after #230

Main is `bde65922e2c960a653f12b62157b8167dbe1317a`, merged #230. The
science completion audit and received radar-tier fixes were independently
reviewed and passed Debug CI **87/87** on the identical pre-merge tree. Formats
are **save v95 / orders v38**. #44 and #59 are closed; broader #56 stays open;
#63 stays frozen last. Old v94 packets await fresh receiver evidence rather than
reconstructing it from host truth. See the
[science completion matrix](audits/2026-10-23-science-progression-completion.md).

## Merged branch — medium navigation #105 (historical review checkpoints)

`codex/medium-navigation-completion` starts from that main. The user confirmed
that clouds may cause hull risk at a Warp safe in empty space. The first
continuous balance shifts effective engine tolerance by up to one Warp tier,
down for conventional engines and up for scoops. Full flown-segment integration
covers mixed fitted engines, hull-limited movement and free-speed coasting.
Player and computer route alternatives use received profiles only and ordinary
delayed coordinate waypoints. See [rules, estimates and limits](interstellar-medium.md).
No save/orders format changes. [PR #231](https://github.com/kotrf/suns/pull/231)
is for review; read its current exact-head CI before any merge. These notes do
not authorize merge or claim manual balance acceptance. Optional ionization,
radio propagation and science remain future extensions; #63 is untouched.

PR #231 P2 follow-up (24 October): arrival Load All now reserves full-hold mass
for later legs, without assuming a successful future unload. The materializer
retries the complete programme at lower Warp caps or sends a normal delayed stop
if no safe funded itinerary exists. Physical TurnProcessor regressions cover
70,000 orbital passengers, 700 kt minerals, actual bypass delivery, a tight-fuel
slower route and an unfunded hold. The prior `dd06cfd` CI result (87/87) belongs
to the pre-fix head; read the updated PR's exact-head checks for this correction.

Second P2 follow-up (24 October): initial received depot service is now shared by
Warp selection and route materialization, including acknowledged active loading
programmes with fresh position telemetry. Idle-only top-up no longer cancels a
funded shuttle. Physical controls cover the original onboard delivery and the
revised safe bypass starting with 20 mg, plus missing/hidden/non-refuelling,
stale-position and future-waypoint negatives. Future supplies are not prepaid;
the first P2 full-hold cargo reserve remains. `d9556ca` passed 87/87 before this
follow-up; the updated PR head needs its own exact-head CI.

## Historical checkpoint — main after #229

Main was `155f9e1e4f9fee7ce5e440b1fc57595d9dc0df48`, save v94 / orders v38,
with the orbital habitat slice and reviewed corrections from #229 (CI 87/87).
The #44 branch described above has since merged as #230 and closed #44.

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
