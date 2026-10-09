# Production queue

Each friendly colony exposes its complete production plan as an ordered four-column list: position, item, remaining production and forecast completion. Above it, the dock shows the colony stock, next-turn extraction and the total I/B/G bill for the visible queue. Selecting a row shows that item's bill and any shortfall against current stock. The forecast mirrors turn order: colony mining occurs first, the empire research percentage is deducted from current output, completed factories and mines affect later estimates, and population grows at the end of the turn.

**Move up** and **Move down** append a typed `ReorderProductionQueueOrder`. Indices apply to the queue as already modified by earlier pending queue and reorder orders, so the displayed plan is the same order the turn processor will resolve. Invalid or stale indices are rejected atomically.

**Remove**, or **Delete** while the queue has focus, cancels the selected item.
This works for newly planned orders and construction carried over from earlier
turns, including partially completed builds. The row disappears immediately and
completion forecasts update. `CancelProductionOrder` applies in sequence with
additions and reorders before production runs, with colony ownership and index
checks. Already spent production is lost; minerals are charged only at completion
and are not spent on the cancelled build. Cancelling an Orbital Dock also enables
planning its replacement immediately.

Save format v33 and turn-order format v4 persist cancellation orders, including
multiplayer submissions. Earlier supported saves and order files remain readable.

Save format v34 and turn-order format v5 add owner-scoped references to ship
designs created earlier in the same order list. A freshly saved design may be
queued immediately without predicting the globally assigned design ID.

Research is not a local queue item. Each colony first contributes the global
allocation percentage, resolves its own queue with the remainder, and sends any
unused output to the common research pool. Production points do not carry over
between turns.

Mineral-short construction remains in the queue with zero production remaining. The completion forecast simulates future planetary mining as well as I/B/G construction bills; very long estimates are reported as beyond the forecast horizon rather than inventing a date.

Ships can be added to a colony's queue only while an operational Orbital Dock
with a shipyard is present. The ship selector and queue button are disabled
without one, and the host rejects forged or stale ship orders. Ship items already
present in an older save still wait safely for a replacement shipyard.


## Batches and annual auto rules

The queue editor adds factories, mines or terraforming as either a one-off batch
or a persistent rule. One-off quantity is the number still to complete; an auto
quantity is the maximum completed units each year, not a total colony target.
Factories and mines support annual rules; terraforming offers Min and Max rules.
The same queue position sets priority for both normal and automatic work.

Auto rules stay in place and are skipped if resources, minerals or eligible work
are unavailable. They do not accumulate a fresh backlog every year. If production
starts a unit without finishing it, that unit becomes a normal partially built
row immediately before its rule. Completing that row consumes one unit of the
following matching rule's annual quota. A normal build, including such unfinished
work, can wait for minerals and block the queue as usual. Reordering or deleting
rows follows the existing pending-order sequence.

The forecast and host share the yearly queue resolver. It includes research
allocation, production, mineral costs and physical terraforming before population
growth. Normal batches show completion of the entire remaining batch; persistent
rules show their first future completion and remain active afterwards. Forecasts
with auto rules are limited to 256 years and assume current technology, without
predicting empire research discoveries or future cargo deliveries.

## Production templates

Enter a name and **Save** to store the current queue's auto rules for this empire.
One-off builds and unfinished work are excluded. **Apply** replaces the colony's
auto rules and appends the selected template after retained normal builds. The
new-colony checkbox takes effect when the template is saved. One template may be
the default; it is applied on colonization or conquest, including colonies founded
by fleet arrival actions. **Delete** removes the stored template without removing
rules already applied to colonies. Templates are player scoped, persist in saves
and player turn exports, and their changes travel in turn-order packets.

Save v59 adds the bounded production extension and physical natural baselines;
turn-order v17 adds batch, template-edit and template-apply orders. Earlier
supported formats remain readable.
