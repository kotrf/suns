# Moving fleet targets

Route legs may target a stable `FleetId` as well as a fixed map position. The
stored position remains a snapshot for old saves and presentation, but the
simulation resolves the friendly target fleet again at every planning boundary.
The Route Program UI labels these legs as moving targets.

## Turn resolution

Orders remain annual, while fleet contact is resolved continuously inside the
turn:

1. all fleet routes are read from the same start-of-turn state;
2. each fleet's ordinary end-of-turn position is projected;
3. a pursuer aims at its target's projected end-of-turn position;
4. every fleet moves with its own Warp, fuel and mass limits;
5. relative motion along the two real movement segments is solved over the
   normalized interval from 0 to 1;
6. if separation reaches the 0.01 ly strategic encounter radius, both fleets
   stop at a shared encounter point and the arrival action resolves there;
7. otherwise both fleets keep their physical end positions and pursuit remains
   active for the next turn.

Reaching the coordinate where the target started the turn is not by itself a
meeting. Segment crossing counts only when both fleets occupy the crossing at
the same time. Paths that cross at different time fractions, and high-speed
fly-bys whose closest approach stays outside the encounter radius, remain real
misses.

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
visibility loss and encounter resolution beyond a friendly rendezvous depend on
the combat and contact model rather than being approximated here.
