# Scrap Fleet and mineral wreckage

Choose **Scrap Fleet (dismantle all ships)** in the route's On arrival selector.
It is a one-shot final waypoint action, incompatible with Repeat Orders or later
waypoints. **Scrap here** queues that same ordinary route action at a stationary
fleet's received position. Planning does not remove physical ships; commands
must reach the fleet and the waypoint must actually execute.

Every ship stack contributes its fitted hull/component mineral build cost.
The user refined space recovery to a broad distribution reaching zero, while
keeping orbital recovery unchanged. Space material recovery now ranges from
**0–50%**, using a triangular density increasing towards 50% (mean about
33.3 percentage points, standard deviation about 11.8). A discrete uniform
roll 0–10000 is transformed as `0.5 * sqrt(roll / 10000)`, including actual
zero and 50% endpoints. This is the selected initial shape for playtesting.
Above an owned populated surface colony, recovery remains **75% with a 20%
chance to lose another 5–15 percentage points**, including identical previous
seed outcomes. The same yield applies to I/B/G; it is deterministic from galaxy seed, fleet ID,
owner and execution year. Damage does not add a second recovery modifier.
Existing cargo minerals are preserved at 100%, independently of that roll.
Production resources and fuel are not refunded.

At an owned populated colony, recovered material goes to surface stocks. A
shipyard is not required. Elsewhere (including foreign or uncolonized orbits),
one mineral wreck remains at the fleet position if any material or cargo
minerals survived. A zero return with an empty hold creates no wreck, consumes
no wreck ID and publishes no false wreck observation; completion still reports
zero material. Wreckage uses the existing **20-year
wreck lifetime**, with no science/technology reward. Before scrapping, unload
colonists, scientific specimens and xenocrystals; a blocked final step holds
and retries, and Stop can cancel it. Fixed installations use their existing
decommission controls. A full colony mineral stock also blocks removal rather
than overflowing or losing material.

## Collection and received information

The existing **Recover wreckage** work requires a scanner and cargo capacity.
Each year it handles the nearest wreck within 10 ly, with ID breaking ties.
Mineral collection uses only free shared hold space and takes I/B/G in the same
proportion. Uncollected stock stays in that physical wreck until another pass
or expiry; a full hold consumes nothing. Scientific wreck recovery keeps its
existing research behavior. Mineral wrecks never award science.

Observed wreck markers distinguish Mineral/Scientific and show the observation
and expiry years. These are delivered sensor records, not live physical stocks.
The scrapping fleet sends its own wreck observation with the final completion
packet. Remote destruction and wreck creation do not update the owner's UI or
PBEM view before delivery: the old fleet contact remains until then. Collected
minerals arrive in ordinary fleet telemetry. Player exports omit physical wreck
inventories and pending reports.

Scrapping must be able to transmit its final packet under the normal emission
or local colony exchange rules. Passive/Radio Silence away from a friendly
colony therefore holds the step until transmission becomes possible; it does
not silently change emissions or destroy the only transmitter for its report.
Stored observations leave in the same final packet. The existing outbox
dispatcher accepts a source/time filter for this operation and retains its
merge/battle/base loss-ledger handling.

## Persistence

**Save v109 / orders v48** add the action and Scrap Fleet result reports.
A trailing **SCRP** section stores remaining mineral stocks keyed by existing
physical wreck IDs. Historical STRA records are unchanged; v108 wrecks load
with no invented mineral stock. Counts/IDs, nonnegative finite amounts and
truncation are checked. All PBEM players should use the same build.
