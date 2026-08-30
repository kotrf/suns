# Moving fleet targets

Route legs may target a stable `FleetId` as well as a fixed map position. The
stored position remains a snapshot for old saves and presentation, but the
simulation resolves the friendly target fleet again at every planning boundary.
The Route Program UI labels these legs as moving targets.

## Turn resolution

Movement remains annual and discrete:

1. all fleet routes are read from the same start-of-turn state;
2. each fleet's ordinary end-of-turn position is projected;
3. a pursuer aims at its target's projected end-of-turn position;
4. every fleet moves with its own Warp, fuel and mass limits;
5. a rendezvous succeeds only if the real end positions coincide.

Reaching the coordinate where the target started the turn is not a meeting. A
segment crossing inside the annual movement interval is also not yet a meeting;
continuous interception belongs to the later combat/interception system.

FleetIds are processed in a stable order and projections use one shared
snapshot. Reordering the in-memory fleet vector therefore cannot change a
pursuit result or replay.

## Merge on arrival

`Merge with fleet` is a terminal arrival action available only for a friendly
fleet target. When the fleets actually meet, the target FleetId, name and route
survive. The pursuing FleetId is removed; all its ship stacks, fuel, colonists
and mineral cargo are added to the target, duplicate design stacks are
consolidated, and aggregate limits are recalculated.

If both fleets already share a position at a planning boundary, merging is
resolved before movement. The surviving fleet may then continue its own route
with the newly combined composition.

If the target no longer exists when the route is resolved, pursuit stops, the
route is cleared, and a delayed `FleetTargetLost` report is produced. The fleet
does not continue toward a stale coordinate.

The first slice deliberately accepts friendly targets only. Hostile contacts,
visibility loss and continuous trajectory interception depend on the combat and
contact model rather than being approximated here.
