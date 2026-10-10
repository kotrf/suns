# Project status and development handoff

Snapshot: **10 October 2026, Dialog #06**. Main is now
`09f0123a3d9b0be1fe476f1c3d1aba87b45b4595` (documentation PR #171 merged).
The merged gameplay baseline is `0f138577f27e647ada45f37b601cbdfb324b6375`, after PRs
[#168](https://github.com/kotrf/suns/pull/168),
[#169](https://github.com/kotrf/suns/pull/169) and
[#170](https://github.com/kotrf/suns/pull/170) merged in dependency order.
The [main CI run](https://github.com/kotrf/suns/actions/runs/38078501535)
passed; the combined Qt Debug build also passed all **47 local CTest targets**.
The user is testing the merged game locally; results of that playtest are pending.

## Current authorized implementation

The user requested bombardment/minefields, stealth/EW and science from exploring
physical objects. Branch `feat/strategic-warfare-field-science` implements
[strategic fleet operations](strategic-operations.md): operational bombs, three
minefield types/sweeping, cloaking, jamming, emission modes/decoys and finite field
observations/novel wreck recovery. This work is **not yet merged**. Its focused
issue is [#172](https://github.com/kotrf/suns/issues/172); broader #58/#59/#101
remain partial. Do not repeat the work or close umbrella issues for this slice.

Main's old limitations below describe the merged baseline; the branch implements
bombardment, mines and initial stealth/EW. It does not add station combat, colony
defenses, planetary scars, autonomous doctrines, diplomatic treaties or physical
sample-return missions.

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

Reference equipment follows Stars! progression and ratings. Suns! remains its own
simulation: tactical combat rules and terraforming production price are documented
adaptations, not claims of an exact Stars! implementation.

## Current boundaries

- Combat treats all different owners as hostile; unarmed fleets do not fight
  each other. Stations, bombardment, minefields, stealth detection, retreat
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

The implementation branch uses save **v62** (readers v12–62) and orders **v19**
(readers v1–19); merged main still uses v61/v18 until the implementation merges. Host and players must use the same build. Version numbers in
older thematic documents describe when a feature was introduced unless explicitly
identified as current. Legacy designs and campaigns retain documented old rules.

GitHub Actions dispatches builds to our self-hosted Linux/X64 runner with label
`suns`. Current jobs use ephemeral VM runner names such as `suns-eph-*`; the old
`suns-eve` name is historical. The workflow runs on pushes into this repository,
including merges to main. Each merge can therefore create a new main build.
Public fork PRs do not execute through this push-only workflow. Do not switch to
GitHub-hosted runners without the user's decision.

## Remaining work and continuation

These are open directions, not authorization to start every item:

- [#44](https://github.com/kotrf/suns/issues/44): the research system and equipment
  catalogs are implemented; remaining capabilities include station/stargate,
  bombardment, minefield, stealth and full racial-trait systems.
- [#45](https://github.com/kotrf/suns/issues/45): operational, contact, wormhole,
  ground- and space-battle messages exist; continue from unmet briefing/intel
  requirements rather than rebuilding the message pipeline.
- [#47](https://github.com/kotrf/suns/issues/47): dockable panels, shared stable-ID
  selection, presets, permanent colony details and expanded map exist; the
  separate shared GameSession refactor remains pending.
- [#48](https://github.com/kotrf/suns/issues/48): empire/colony graphs, extraction,
  freight, fleet aggregates and battle markers exist. Separate military-loss
  time series and additional fleet scopes remain follow-ups.
- [#94](https://github.com/kotrf/suns/issues/94): logical slot migration, graphical
  fitting, drag/drop, keyboard controls and catalog filters exist; finish the
  live UI acceptance review and remaining polish.
- [#50](https://github.com/kotrf/suns/issues/50): full asymmetric racial rules.
  [#56](https://github.com/kotrf/suns/issues/56) retains system-scale engineering
  beyond the implemented colony terraforming slice. Transport networks
  [#53](https://github.com/kotrf/suns/issues/53) remain late in the roadmap.

Collect the user's main playtest results while finishing the authorized strategic
operations branch. After it merges, candidates include station fitting/combat,
colony defenses/scars or battle doctrines/diplomacy; choose the next slice explicitly. Do not restart existing partial implementations or close
umbrella issues solely because a first slice landed.

Implementation goes through a branch and reviewed PR; merge only on the user's
explicit instruction. GitHub [#123](https://github.com/kotrf/suns/issues/123) and
the [Notion handoff](https://app.notion.com/p/3e47451cd64a81aa9bc2dca95689ad86)
mirror the project context. Preserve dated older snapshots as history.
