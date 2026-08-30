# Continuous fleet interception

Suns! issues orders once per year, but a year is no longer treated as a single
teleport between fleet endpoints. For a fleet pursuing another friendly fleet,
the simulation projects both fuel- and Warp-limited movement segments and solves
their relative motion over the turn.

## Contact rule

For normalized turn time `t` from 0 to 1, the relative position is linear:

`r(t) = (pursuerStart - targetStart) + (pursuerVelocity - targetVelocity) * t`

The closest time is the projection of the origin onto that relative segment,
clamped to the turn. A rendezvous exists only if separation enters the strategic
encounter radius of 0.01 ly. The quadratic entry time supplies the earliest
contact, so a head-on pass stops at first contact rather than at the closest
point or the end of the year.

Both fleets first reach one shared point on the target's trajectory at that
time. Moving the pursuer across the final separation absorbs at most the 0.01 ly
encounter radius as tactical closure, while the target remains exactly on its
physical route. Movement fuel, sensor sweeps and radiation attrition use the
resulting travelled distance.

For `Merge with fleet`, the pursuing FleetId is then consumed while the target
FleetId, route and orders survive. The merged fleet continues toward the
target's destination for the unused fraction of the year. Its Warp is reduced
to the target's ordered Warp or the maximum supported by every ship in the new
composition, whichever is lower; fuel use is recalculated for the combined mass
and cargo. This prevents a fast target from towing a newly attached slow ship at
an invalid speed.

## Determinism and misses

All ordinary endpoints are computed from one start-of-turn snapshot before any
contact changes movement. Candidate contacts are resolved by time fraction,
then pursuing FleetId and target FleetId. A fleet can take part in at most one
contact during a turn.

Spatially crossing paths are not enough: both fleets must be close at the same
turn fraction. An impossible chase or fly-by outside the radius leaves both
ordinary endpoints intact, and the moving-target order remains active next year.

The Route Program simulates this same rule to show an intercept turn. If no
contact occurs within its preview horizon, it explicitly reports an uncertain
ETA instead of claiming arrival at the target's current coordinate.
