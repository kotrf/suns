# Project status and development handoff

## Audit corrections — 17 October 2026

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

## Current implementation — 17 October 2026: deep-space infrastructure

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

## Current implementation

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

## Current boundaries

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

This slice uses save **v67** (existing readers v12–67) and orders **v21**
(readers v1–21). Earlier casualty history remains unknown; old pending reports
without exact summaries never invent measurements. Host and players must use
the same build. Version numbers in older thematic documents describe when a
feature was introduced unless explicitly identified as current. Legacy designs
and campaigns retain documented old rules.

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

## Remaining work and continuation

These are open directions, not authorization to start every item:

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
