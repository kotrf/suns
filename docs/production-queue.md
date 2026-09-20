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
