# Population, industry and Stars! research (#276)

The user approved 1 resource per 1000 inhabitants, basic/advanced industry with
paid upgrades and shared workforce limits, and subsequently requested Stars!
research costs in the same implementation. The numerical industry recipes below
are initial implementation balance parameters, not a claim of campaign acceptance.

## Industry

| Installation | Resources | I / B / G (kt) | Output per working installation |
| --- | ---: | --- | --- |
| Basic Factory | 10 | 0 / 0 / 4 | 1 resource/year |
| Advanced Factory | 20 | 4 / 2 / 8 | 2 resources/year |
| Upgrade Factory | 10 | 4 / 2 / 4 | Converts one basic factory |
| Basic Mine | 5 | 0 / 0 / 0 | 1 kt of each mineral at 100% concentration |
| Advanced Mine | 10 | 1 / 2 / 1 | 2 kt of each mineral at 100% concentration |
| Upgrade Mine | 5 | 1 / 2 / 1 | Converts one basic mine |

Direct advanced construction and basic construction followed by an upgrade have
identical total resource and mineral bills. Both tiers are available initially;
no unapproved technology prerequisites or free automatic upgrades are introduced.
Existing Factory/Mine enum IDs mean Basic; four new IDs are appended.

`industry`/`mines` count **all** installations; advanced counts are subsets.
Each tier uses one slot in its type's existing racial workforce cap, independently
for factories and mines. Advanced installations operate first; excess remains idle.
An upgrade never adds a building or a workforce cap. It reserves an existing
basic building; unavailable/surplus upgrade work retires without payment after
losses or earlier upgrades. Construction and persistent annual rules respect the
32-bit installed-count ceiling. Templates can contain one-time upgrades and
ordinary/annual advanced construction.

Minerals are charged atomically on completion, work follows normal queue rules,
and newly built/upgraded mines first affect the following year's extraction.
The modest population baseline `1 + surface population / 750000` is retained.
Basic mines require no seed mineral stock. Concentration and remote ship-miner
rules are unchanged. War damage reduces installed counts and clamps advanced
subsets; ordinary ruin reconstruction restores a basic installation.

Population production is `floor((surface + normally serviced orbital residents)
/ 1000)`, plus working basic factories and twice working advanced factories.
Only surface residents operate buildings or supply baseline extraction. Output
saturates at uint32 maximum; empire RP aggregation uses uint64. Growth rules do
not reference factory counts. Habitat upkeep is paid before research/production.

The existing fair AI sizes ordinary affordable development batches up to ten,
uses population-scaled mining targets and upgrades working basic installations.
Its choices use received PlayerView data and normal paid production orders.
The view is canonicalized by stable object IDs once per plan, including late-game
engineering/probe policies, preserving determinism after export/storage reordering.
When a civilian ship needs minerals, free basic mining can continue but factory
spending waits for that ship. Selected military ships reserve their bill through
an ordinary queue, which can wait for materials; they receive no free work or
minerals. Forward docks are considered before open-ended factory expansion.

## Research

| Target level | Base resources | Target level | Base resources |
| ---: | ---: | ---: | ---: |
| 1 | 50 | 14 | 18040 |
| 2 | 80 | 15 | 22440 |
| 3 | 130 | 16 | 27050 |
| 4 | 210 | 17 | 31870 |
| 5 | 340 | 18 | 36900 |
| 6 | 550 | 19 | 42140 |
| 7 | 890 | 20 | 47590 |
| 8 | 1440 | 21 | 53250 |
| 9 | 2330 | 22 | 59120 |
| 10 | 3770 | 23 | 65200 |
| 11 | 6100 | 24 | 71490 |
| 12 | 9870 | 25 | 77990 |
| 13 | 13850 | 26 | 84700 |

The **single next level** costs
`floor((base + 10 × sum of all currently known levels) × field factor)`.
Normal costs 100%, Cheap 50%, Expensive 175%, separately for the six racial
fields. Slow tech advances doubles the result. Defaults are Normal and no slow
tech; new racial choices are explicit setup values, not opponent-visible data.
The shared race editor, campaign slots and portable race files retain all six.

Ordinary resources, precursor breakthroughs and delivered scientific data use
the same state-dependent price. A breakthrough reprices subsequent overflow
against the new total. Existing per-field RP is retained when switching/reordering
the plan. The ordered plan still controls field changes; at level 26 queued fields
take precedence, then the lowest unfinished field. An entirely completed empire
reserves no percentage allocation. Legacy levels above 26 are retained, without
allowing new research past the reference cap.

The Research window prices queued rows from projected completed **levels** only:
one bounded pass over rows, no annual economy simulation or background workers.
Production estimates similarly track known installation counts/upgrade eligibility
over their prefix and retain the minimal-estimate uncertainty policy.

Sources: [original Player's Guide, Research 8-5 and Research Costs 20-17](https://www.bestoldgames.net/download/games/stars/stars-win-manual.pdf),
and the StarsFAQ table reproduced in
[Craig Stars' research implementation](https://github.com/sirgwain/craig-stars/blob/develop/cs/research.go).
The table was cross-checked as reference data; the original Stars! executable was
not tested. This is not a full port of racial advantage points, technology trade
or Stars!' other research rules.

## Compatibility and balance boundary

Save v111 appends an ECON extension after v110 SURF. It stores public slow-tech,
physical advanced subsets, private racial research settings and the corresponding
advanced subsets in every delivered/pending owner habitat observation. Player
exports use the received snapshot, never the hidden physical upgrades. IDs,
counts, enum ranges and subset bounds are validated. Orders v49 accepts appended
production kinds; older order versions remain readable. All PBEM clients need
this build to use the new economy. Race file v2 adds six cost settings; v1 loads
with Normal research. Portable race files still cannot grant technology or IDs.

Old host/local saves retain installed buildings as basic, all known levels,
per-field RP, stocks, population and historical statistics. Current output and
mining adopt the new rules. For unfinished old factories the remaining work is
increased by four, preserving already paid resources against the new price 10;
annual rules/templates are repriced consistently. Old pending production orders
receive the current recipe. Construction minerals were never prepaid.

Suns!' million-person homeworld now starts at 1004 resources with four basic
factories, versus the old six. This deliberately keeps the existing population
setup while implementing the requested resource ratio and exact reference prices.
It is not a claim of matching Stars!' whole campaign pacing: starting population,
growth, ship recipes and existing special-project annual limits remain Suns! rules.
New/continued campaigns and AI development need gameplay acceptance beyond the
deterministic checks in this PR.
