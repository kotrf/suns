# Mini-4

Implements the five items in [issue #299](https://github.com/kotrf/suns/issues/299)
on main841c7bb after merged Mini-3/#298.

- Production's Item header is interactive, initially240px; refreshing the queue
  does not reset its width.
- Split names both singleton results with the project name and the first free
  positive decimal number: `Scout #1`, `Scout #12`. Own fleet names reserve
  numbers, including a multi-ship source which keeps its name; foreign names do
  not. Source ID, composition, fuel and cargo rules remain unchanged. UTF-8 names
  retain complete code points within the ordinary80-byte fleet-name limit.
- New waypoint starts one-shot map addition. A star, fleet or free-coordinate
  selection updates the draft controls and immediately appends one route leg.
  Right-clicking during that mode adds exactly once. Cancel/source changes clear
  the intent. The separate Pick target control retains manual draft confirmation.
  The ordinary Move order and PBEM format carry the resulting route.
- Fixed production batches reserve known minerals for earlier queue items.
  Tooltips show per-unit cost, the whole buildable batch, the remaining mineral
  budget after earlier rows/life support and that batch's shortfall. An uncertain
  prefix is labelled unknown rather than displaying a false zero. With two300-work
  Advanced mine×30 batches and333 production/year, fully funded estimates are
  +1/+2 years; an insufficient second-batch budget keeps its completion unknown.
  Rates, budgets and current limits remain fixed: no growth, future mining,
  arrivals, technology or simulated future turns are added to the preview.
- The fleet summary shows radiating-drive passenger losses only when its planned
  cargo actually contains colonists. Physical radiation/attrition rules are unchanged.

Save128 / orders55 / race3 remain unchanged. Tests exercise unique singleton names
across hidden foreign IDs, multi-ship names and UTF-8 limits, exact queue budgets,
actual Qt star/coordinate/right-click picking, cancellation/manual picking, queue
ETA/tooltips, zero/nonzero passenger summaries and UI Split through PBEM.
Complete GCC 14.2 / Qt 6.12 Debug build; focused16/16 in16.38s and full140/140 CTest in160.19s passed. All184 local documentation targets in seven related documents were checked.
