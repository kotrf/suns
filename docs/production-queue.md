# Production queue

Each friendly colony exposes its ordered plan: position, item, remaining work
and approximate completion. The dock shows current stock/mining and the I/B/G
bill; selecting a row shows its cost and current shortfall. Under the
[minimal estimate contract](minimal-ui-estimates.md), UI dates use cumulative
remaining work divided by current production after research/upkeep, marked ~.
There is no future annual simulation, growth, newly built output or future
mining/delivery credit. An uncertain/blocked prefix makes later dates unknown.

Factory output is limited to the factories the current surface population can
operate under its owner's racial rate; extra installed factories remain idle.
Mining uses the independent mine-operation rate. Limits are recalculated in the
shared resolver as population grows or falls. UI estimates use only the current
operation limit and rate. The System panel shows active/built counts and population limits;
see [race operation settings](race-profile.md#factory-and-mine-operation).

Ordinary mining precedes cargo orders in the physical turn. A post-cargo plan
therefore uses received pre-cargo extraction for current output/upkeep facts,
while factories use post-cargo staffing. The displayed static mineral budget
does not grow in later estimated years. Real turns recalculate mining, staffing
and upkeep normally; the interface does not resolve them in advance. Mining labels and
the Minerals table show current-year extraction before cargo, independently of
the immediately updated active/built counts and cargo-adjusted stocks.

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

Mineral-short construction remains queued with zero production remaining. Its
UI completion date is unknown until the received mineral budget covers the
batch; future mining and deliveries are not predicted. Real turns continue to
fund and complete it under normal rules.

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

The host retains the full yearly queue resolver, research allocation, costs,
physical terraforming and growth. UI dates cover only simple finite batches at
the current rate and available mineral budget. Persistent rules show Annual
rule; changing-condition work, missing shipyards and uncertain prefixes show no
date. Templates list rules/costs without simulating an example future colony.

## Production templates

Enter a name and **Save** to store the current queue's auto rules for this empire.
One-off builds and unfinished work are excluded. **Apply** replaces the colony's
auto rules and appends the selected template after retained normal builds. The
new-colony checkbox takes effect when the template is saved. One template may be
the default; it is applied on colonization or conquest, including colonies founded
by fleet arrival actions. **Delete** removes the stored template without removing
rules already applied to colonies. Templates are player scoped, persist in saves
and player turn exports, and their changes travel in turn-order packets.

Saving a program also checks whether all its steps apply to the owner's
civilization. The editor disables inapplicable choices, blocks adding/updating
such a step and explains why an incompatible draft cannot be saved. The host
rejects the same edit before changing the library or its default flags. Pending
library edits use the real race profile. Existing incompatible programs remain
readable and deletable; their marked steps can be removed or replaced before
saving a repaired program. Normal programs of other civilizations retain their
existing choices.

Archive loading restores structurally valid programs even when their steps no
longer apply to the saved civilization. It validates the 16-entry limit, unique
names and at most one default directly, preserving the saved entries/flags.
Racial admission belongs to new editing/execution commands; those guards remain
active after loading. Host/PBEM files and legacy drafts can therefore be reopened
and repaired through the editor without accepting new incompatible saves.

Renaming uses the existing adjacent delete/save order pair. When the source
exists, host execution and both pending previews apply the pair atomically to
a bounded candidate library. If the replacement is rejected, the original
entries and default flags survive. Valid renames work at the 16-entry limit.
Standalone deletion remains available; a missing-source deletion does not block
the next create. Other orders keep their original ordering and indices. This
also protects loaded legacy drafts without changing the save/order formats.

Save v59 adds the bounded production extension and physical natural baselines;
turn-order v17 adds batch, template-edit and template-apply orders. Earlier
supported formats remain readable.

## Reconstruction

[Factory/mine reconstruction](planetary-reconstruction.md) consumes existing war ruins
through the ordinary yearly queue resolver. Its UI completion date remains
unknown because eligibility and surplus work can change. Once-only batches cost 4/3 resources
and ordinary factory/mine minerals. Annual rules and colony templates cannot create
planet-specific reconstruction. Queue reservations prevent overbooking ruins.

## Orbital habitat integration (#56)

Occupied orbital habitats pay yearly upkeep (and affordable emergency-reserve recharge) before research allocation and optional queue work. Construction retains the existing 80-work/year engineering limit, paid progress and single slot. A newly completed support station helps the next annual maintenance phase. See [orbital habitats](orbital-habitats.md) for the complete rules and save v94 / orders v38.

## Independent orbital shipyards

[Staffed orbital yards](independent-orbital-shipyards.md) use their own received
queue, workers and delivered orbital stocks, without a surface colony. Ship/refit
materials are paid cumulatively as work is done; cancellation addresses a stable
request ID and retains no refund. Ordinary colony production rules above are
unchanged.
