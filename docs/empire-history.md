# Empire turn history

Every player now keeps one compact `EmpireTurnStatistics` snapshot per planning
boundary. New games record Turn 1, and `TurnProcessor` appends exactly one sample
after each resolved year. Re-recording the same boundary replaces its sample
rather than creating a duplicate.

The empire-wide metrics are:

- total population across owned colonies and fleets;
- colonies, factories, mines and current production output;
- owned planetary and fleet mineral stocks;
- I/B/G minerals actually extracted during the preceding year;
- fleets, ships and gross fleet mass;
- technology levels and invested RP by field.

Snapshots use only assets owned by their player. Neutral stockpiles, enemy
colonies, enemy fleets and unsurveyed authoritative truth are excluded, so the
model is safe to reuse in PBEM player views. History is persisted in save format
22; an older save receives one baseline snapshot at its loaded planning turn.

Each boundary also records population, factories, mines, output and surface
I/B/G stocks for every colony owned at that boundary. A former colony keeps
its previously recorded years, but has a gap while it is not owned. These
per-colony samples are saved in format 37; loading an older campaign can add
the breakdown for its current boundary, but cannot invent earlier colony
histories from empire-wide totals. Other players' assets are never included.
Format 38 adds actual yearly extraction per empire and per owned colony. The
year of a pre-38 save has no extraction record; those years show a gap and
"No extraction record" on hover. Extraction belongs to the empire that owned a mine when it
worked, even if the colony changes hands before the next planning boundary.
Remote mining of neutral worlds is not included in this colony mining series.

Save format 42 adds a separate remote-extraction series for neutral worlds.
The output is measured when an owned mining fleet deposits ore on the surface,
credited to the fleet's owner even if that world is settled later in the year.
It is not cargo delivered to a colony: the freight series records that only
after a transport unloads. Multiple miners at one world are aggregated per
player and per site, never across players. A recorded year with no mining is
zero; initial and pre-42 years are unknown gaps. The Remote extraction chart
supports empire totals, individual previously mined worlds, Follow map, and
comparison between these scopes without revealing other players' operations.

Save format 40 stores compact milestones for a player's delivered colony-founded,
colony-lost and research-level reports. The chart marks the delivery year with
colored vertical lines; hover to see the event and, if different, its observation
year. In colony scope only milestones for that colony appear. Events known only
to other players never enter this player's history. Older saves have no earlier
milestones; loading them does not invent past discoveries or losses.

Save format 41 records cargo actually unloaded from fleets onto owned colonies
in the preceding year: I/B/G minerals and colonists (shown in cargo kt). It
counts exact transfer orders, legacy cargo adjustments and automatic waypoint
unloads; it does not count loading, fleet-to-fleet transfers, neutral surface
deposits, invasions or minerals gained when founding a colony. Deliveries from
neutral remote-mining sites count when they reach a colony. Each delivery is
credited to the owner at unloading, even if the colony changes hands later.
An initial year or an older save has no freight measurement and shows a gap.

The Empire History dock plots the local player's saved snapshots. Choose
population, colony/infrastructure counts, yearly production output, I/B/G
stocks, fleet/ship counts, fleet mass, technology levels, invested RP or
mineral extraction, delivered freight or remote extraction. Extraction shows the three resources actually mined each
year for the empire or a selected colony. Hover over a year to read exact
values, and select a first and last year to inspect a shorter range. For
population, infrastructure, output, stocks, extraction and freight,
choose Whole empire, a recorded colony or Follow map. Follow map tracks the
currently selected owned colony; with no owned colony selected it prompts for
one instead of showing empire totals under a colony label. Missing observations show a gap and
"No owned-colony record" on hover. The dock can be detached, hidden and restored from View; it
never reads the host's other players' histories in a player turn. It uses
Qt Widgets painting and does not require Qt Charts.

The Compare selector overlays a second recorded colony or the whole empire for
the same scoped metric. Both series sets share the same year range and axis;
the comparison uses dashed lines and named legend/hover values. Missing years
remain gaps for a colony that was not owned. Selecting the current primary
scope twice leaves a single plot. Fleet counts, mass and research levels remain
empire-only metrics.

Further event categories remain extensions of issue #48; snapshots from older
saves start at the loaded turn.
