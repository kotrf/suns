# Production queue

Each friendly colony exposes its complete production plan as an ordered four-column list: position, item, remaining production and forecast completion. The forecast mirrors turn order: colony mining occurs first, stored plus current output is spent, completed factories and mines affect later estimates, and population grows at the end of the turn.

**Move up** and **Move down** append a typed `ReorderProductionQueueOrder`. Indices apply to the queue as already modified by earlier pending queue and reorder orders, so the displayed plan is the same order the turn processor will resolve. Invalid or stale indices are rejected atomically.

Research is deliberately ongoing. When it reaches the head of the queue it consumes every available production point each turn, so later rows show **blocked by research** until they are moved ahead of it or research is stopped.

Mineral-short construction remains in the queue with zero production remaining. The completion forecast simulates future planetary mining as well as I/B/G construction bills; very long estimates are reported as beyond the forecast horizon rather than inventing a date.
