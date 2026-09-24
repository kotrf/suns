# Empire turn history

Every player now keeps one compact `EmpireTurnStatistics` snapshot per planning
boundary. New games record Turn 1, and `TurnProcessor` appends exactly one sample
after each resolved year. Re-recording the same boundary replaces its sample
rather than creating a duplicate.

The empire-wide metrics are:

- total population across owned colonies and fleets;
- colonies, factories, mines and current production output;
- owned planetary and fleet mineral stocks;
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

The Empire History dock plots the local player's saved snapshots. Choose
population, colony/infrastructure counts, yearly production output, I/B/G
stocks, fleet/ship counts, fleet mass, technology levels or invested RP. Hover
over a year to read the exact values, and select a first and last year to inspect
a shorter range. For population, infrastructure, output and mineral stocks,
choose Whole empire or a recorded colony. Missing observations show a gap and
"No owned-colony record" on hover. The dock can be detached, hidden and restored from View; it
never reads the host's other players' histories in a player turn. It uses
Qt Widgets painting and does not require Qt Charts.

Extraction and freight counters, event markers and richer comparison tools
remain later extensions of issue #48; snapshots from older saves start at the
loaded turn.
