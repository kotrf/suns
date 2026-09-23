# Empire turn history

Every player now keeps one compact `EmpireTurnStatistics` snapshot per planning
boundary. New games record Turn 1, and `TurnProcessor` appends exactly one sample
after each resolved year. Re-recording the same boundary replaces its sample
rather than creating a duplicate.

The initial metrics are:

- total population across owned colonies and fleets;
- colonies, factories, mines and current production output;
- owned planetary and fleet mineral stocks;
- fleets, ships and gross fleet mass;
- technology levels and invested RP by field.

Snapshots use only assets owned by their player. Neutral stockpiles, enemy
colonies, enemy fleets and unsurveyed authoritative truth are excluded, so the
model is safe to reuse in PBEM player views. History is persisted in save format
22; an older save receives one baseline snapshot at its loaded planning turn.

The Empire History dock plots the local player's saved snapshots. Choose
population, colony/infrastructure counts, yearly production output, I/B/G
stocks, fleet/ship counts, fleet mass, technology levels or invested RP. Hover
over a year to read the exact values, and select a first and last year to inspect
a shorter range. The dock can be detached, hidden and restored from View; it
never reads the host's other players' histories in a player turn. It uses
Qt Widgets painting and does not require Qt Charts.

This is the first UI slice for issue #48. Per-colony series, extraction and
freight counters, event markers and richer comparison tools remain later
extensions; snapshots from older saves start at the loaded turn.
