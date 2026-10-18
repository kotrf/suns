# Orbital climate engineering

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

An unarmed station may survive surface conquest under its former owner. Such a
station gives the captor no engineering support. Its orbit remains occupied
until ordinary combat destroys it; the new owner can then build a supporting
station and resume the preserved project.

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

Issue #56 remains open for atmospheric and magnetic engineering, oceans,
habitats, larger system projects and further balance/AI work. This slice does
not add autonomous AI project selection or imply manual gameplay acceptance.
