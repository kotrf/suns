# Project status and development handoff

Snapshot: **12 October 2026, Dialog #07**. The starting baseline is main
`6b90be08ee68c19c303c2d38fa57daa0c3b1e0aa`, after #179 (space-battle history)
and #180 (official Qt toolchain diagnostics) were merged. The user authorized
several planned tasks and merging after successful checks.

## Current implementation

Focused [#181](https://github.com/kotrf/suns/issues/181), continuing #48, adds
three planned categories: ground casualties, bombardment population/infrastructure
losses, and mine-strike destroyed ships/report counts. Reports carry immutable
recipient-relative summaries; History credits them to communication receipt
boundaries, retains unknown years as gaps and ignores duplicate packet IDs.
Save v64 stores pending/delivered summaries and compact per-category totals;
orders remain v19. See [Empire history](empire-history.md).

Validation: the full local Qt 6.4 Debug build and **51/51 CTest targets** pass.
Core coverage includes both ground outcomes, recipient isolation, multiple
bombarders, delayed mine destruction, overdue/duplicate packets and unknown
older categories. Save/PBEM round trips and malformed summaries are checked;
GUI coverage exercises all five metrics, gaps/scopes and exact 64-bit hover
values. The charts were visually inspected. CI is tracked by the implementation
PR linked from #181.

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
  each other. Station combat, colony defenses, planetary scars, retreat
  doctrines and diplomacy remain unimplemented. Between battles, damage still
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
  transport, signatures, an append-only resolution ledger and AI opponents remain
  future work. Detached delayed enemy-contact snapshots also remain pending.

## Formats and CI

This slice uses save **v64** (existing readers v12–64) and orders **v19**
(readers v1–19). Earlier casualty history remains unknown; old pending reports
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
`suns`. Current jobs use ephemeral VM runner names such as `suns-eph-*`; the old
`suns-eve` name is historical. The workflow runs on pushes into this repository,
including merges to main. Each merge can therefore create a new main build.
Public fork PRs do not execute through this push-only workflow. Do not switch to
GitHub-hosted runners without the user's decision.

## Remaining work and continuation

These are open directions, not authorization to start every item:

- [#44](https://github.com/kotrf/suns/issues/44): the research system and equipment
  catalogs are implemented, including initial bombardment/minefield/stealth;
  remaining capabilities include station/stargate and full racial-trait systems.
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

The #181 operational casualty categories are implemented. Next candidates include
station fitting/combat, colony defenses/scars or battle doctrines/diplomacy;
choose a focused slice from their unmet requirements and collect playtest results.
Transport networks #53 remain late; #108 still requires separate discussion.

Implementation goes through a branch and reviewed PR; merge only on the user's
explicit instruction. GitHub [#123](https://github.com/kotrf/suns/issues/123) and
the [Notion handoff](https://app.notion.com/p/3e47451cd64a81aa9bc2dca95689ad86)
mirror the project context. Preserve dated older snapshots as history.
