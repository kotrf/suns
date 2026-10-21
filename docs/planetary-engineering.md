# Orbital environmental engineering

The first planetary-engineering slice of #56 adds **orbital mirrors** and
**orbital shades**. They respectively raise or lower effective temperature by
an owner-selected **0–10 normalized axis points**, clamped to 0–100. Output zero
switches the installation off. This is a physical change to the environment used
for racial habitability, population capacity and growth, rather than a flat Hab
bonus. An inherited installation can help one race and harm another.

## Construction and support

Environment-based races unlock both projects at **Energy 14 and Construction 14**.
Each planet can retain one project or installation. The initial balance is:

| Requirement | Amount |
| --- | --- |
| Production work | 2,400 |
| Maximum assembly per year | 80 work; at least 30 years |
| Minerals per work | 0.10 I / 0.05 B / 0.10 G kt |
| Total minerals | 240 I / 120 B / 240 G kt |
| Local support | Populated colony, friendly Starbase or Fortress with a shipyard |

The project uses the ordinary colony production queue and its research
allocation. It pays production and minerals only for work actually completed.
Earlier queue items, low output or missing supplies can lengthen the schedule.
After the annual assembly allowance is used, surplus production can serve later
queue items. Before that allowance is used, missing support or materials retain
the project's manual queue priority. Mining and physical mineral deliveries
replenish the ordinary surface stocks; there is no abstract supply subsidy.

Work and paid materials are stored on the planet independently of the queue.
Removing its queue item or pressing Pause construction preserves both. Resume
adds the remaining work back to the queue. Duplicate requests cannot start a
second project or multiply the annual allowance. Climate projects are explicit
orders, not repeatable batches or colony templates.

## Operation, interruption and capture

Temperature output begins only after completion. Construction and operation
stop when their supporting station is lost, fully damaged, hostile, or refitted
without a shipyard. Returning support restores operation at the chosen output.
No separate recurring fuel/mineral upkeep is included in this first slice.

Capture or abandonment retains the paid physical project, its original builder
and dates. Capture clears ordinary production scheduling; the new owner must
explicitly resume construction with its own technology and support. A completed
installation only needs local support to operate. Dismantle removes the project
and its effect with **no production or mineral refund**; the UI confirms that
loss of investment. Changing between mirrors and shades requires dismantling.

Any hostile station must be destroyed before ground invasion, including an
unarmed dock. The captor must build its own supporting station before resuming
the preserved project. Older saves may retain a foreign station above a captured
colony; it gives no engineering support and must also be destroyed before the
new owner can build a replacement. See [orbital control](ground-invasions.md).

The natural and conventionally terraformed axes remain unchanged by orbital
output. [Ordinary terraforming](terraforming.md) keeps its own technology limits
relative to the natural baseline, while judging improvements against the current
effective environment. Disabling mirrors or shades therefore removes their
offset without accumulating numerical drift or erasing terraforming.

## Player information and persistence

The Planetary engineering panel shows project selection, progress, paid
materials and stocks, blocking conditions, output, queue completion estimate and
the expected temperature/Hab effect. Its production estimate assumes current
technology, support, production allocation and supplies. Environmental forecasts
assume current stellar conditions. Planning creates ordinary pending orders and
does not mutate the authoritative world. Production and engineering use the
same projected planet, queue and accepted pending research settings, including
allocation and activation changes, for their completion estimates.

Started, 25/50/75%, completed, interrupted, resumed, captured and dismantled
reports travel through ordinary delayed communications and appear in turn
messages and History on receipt. Foreign orbital surveys freeze visible project
type, progress, output, operational status and dates; private paid materials are
redacted. Basic scans expose no project detail. Hidden later construction or
station loss cannot rewrite a received survey or a player's exported turn.

Save **v85** adds a bounded ENGI extension for physical projects and separate
received/in-flight survey evidence. Orders **v31** add the project-control order.
The v84 body and prior readers remain intact; old saves start without projects.

After the first climate slice, #56 remained open for atmospheric engineering, oceans,
habitats, larger system projects and further balance/AI work. This slice does
not add autonomous AI project selection or imply manual gameplay acceptance.

## Artificial and adaptive magnetospheres (#56 / #59)

Merged [PR #227](https://github.com/kotrf/suns/pull/227) links a paid engineering project to a concrete field-science
capability. In Planetary engineering, choose a magnetosphere and its output,
build through the normal production queue, then keep a populated colony and a
friendly Starbase/Fortress shipyard to operate it.

| Project | Requirements | Radiation reduction | Work / minimum years | Total I/B/G kt |
| --- | --- | --- | --- | --- |
| Artificial magnetosphere | Energy 16 + Construction 16 | 0–10 points | 2,400 / 30 | 240 / 120 / 240 |
| Adaptive magnetosphere | Energy 18 + Construction 18 + delivered completed Compact remnant study | 0–20 points | 4,800 / 60 | 480 / 240 / 480 |

Both retain the shared 80-work/year assembly limit, normal research allocation,
queue priority and per-work mineral payments. They occupy the existing **single
engineering slot per planet**, competing with mirrors/shades. Changing project
type requires dismantling without refund and a full new build; there is no free
upgrade or second hidden installation. Ordinary terraforming remains available.

Supported output subtracts whole normalized points from **effective radiation**,
clamped at zero, without changing temperature, gravity, natural axes or the
conventionally terraformed baseline. Turning it off, losing population/support
or dismantling removes the offset without numerical drift. Racial habitability,
capacity and growth use this environment normally. Lower radiation can help one
race and hurt another; radiation-immune races receive no radiation-axis benefit.
This does not grant bombardment defense or remove contamination.

The basic installation needs no expedition or specimen. For the adaptive variant,
use the existing Unknown objects / expeditions **Field research** mission with
real scanners, travel and fuel. A completed Compact remnant study must reach that
empire's headquarters through ordinary communications before it unlocks the
capability. Detection, classification, RP alone, physical hidden object state,
another empire's study and pending reports do not qualify. The Research catalog
and engineering panel show the explicit requirement. A retained/consumed specimen
is a separate choice under #59 and is not required or spent here. Each empire can
study the landmark; a rival recovering its one unique specimen does not block
this scientific discovery. If a world has no suitable landmark, basic protection
and ordinary terraforming remain available.

Capture and interruption preserve work, builder, dates and paid resources. Resume
requires the new owner's technology, population and support. As with existing
climate works, a completed basic magnetosphere is usable with local support;
**adaptive operation additionally requires the current owner's own delivered
study and Energy/Construction 18**, including after capture. Unsupported or
unlicensed adaptive equipment remains physically present, inactive.

Owner project state and scheduling now come from the same dated survey packet;
queue progress and paid totals do not advance merely because host construction
advanced. Paid totals are derived from observed work and the fixed recipe.
The panel labels the evidence year, while its completion/effect calculations
are forecasts using received state and accepted pending orders. Foreign evidence
continues to omit paid resources and private scheduling. Milestone/interruption
messages name the correct project, required time and output range.

System Details, map tooltips, Hab colours/sizes (including terraforming potential),
dashboard/colony summaries and the portrait's population capacity/growth also
use PlayerView. Undelivered completion or support loss cannot change those
displays through host habitability; delivery refreshes them from the new packet.
The host's physical environment, habitability and annual simulation are unchanged.

The basic AI chooses a supported, idle, sufficiently productive colony only when
received racial habitability improves by at least five points and received stocks
cover the full build. It considers both versions through the same received
unlock and emits a normal project order. Existing expedition policy already
studies compact objects. No technology, minerals, work or research are granted
directly to computer empires.

The #227 **save v92 / orders v37** formats appended the project/production enum values without
renumbering earlier values. The bounded **EQIX** extension stores owner-only
received/in-flight engineering queue positions; ENGI keeps its existing layout.
Old saves retain physical works and resource investments; missing queue evidence
waits for a normal fresh owner report rather than being rebuilt from hidden work.

#56 remains open for atmospheric/ocean engineering, habitats and system projects;
#59's required observation unlock and sample/salvage choice are implemented.
Further capabilities are optional extensions, not blockers for completion;
[the completion audit](audits/2026-10-22-field-science-completion.md) prepares
#59 for closure after review/merge. The completion branch uses save v93 / orders
v37; engineering costs, effects and EQIX layouts are unchanged.
The radical irreversible transformation is deferred, including evacuation and
capture rules. The costs above are prototype balance, not manual playtest results.
