# Production queue

Each friendly colony exposes its complete production plan as an ordered four-column list: position, item, remaining production and forecast completion. The forecast mirrors turn order: colony mining occurs first, the empire research percentage is deducted from current output, completed factories and mines affect later estimates, and population grows at the end of the turn.

**Move up** and **Move down** append a typed `ReorderProductionQueueOrder`. Indices apply to the queue as already modified by earlier pending queue and reorder orders, so the displayed plan is the same order the turn processor will resolve. Invalid or stale indices are rejected atomically.

Research is not a local queue item. Each colony first contributes the global
allocation percentage, resolves its own queue with the remainder, and sends any
unused output to the common research pool. Production points do not carry over
between turns.

Mineral-short construction remains in the queue with zero production remaining. The completion forecast simulates future planetary mining as well as I/B/G construction bills; very long estimates are reported as beyond the forecast horizon rather than inventing a date.
