# Race profile core

Each player owns a serialized `RaceProfile`. Radiation tolerance and immunity,
which already affect colonists transported by hazardous drives, now belong to
that profile instead of being loose fields on `Player`.

The profile also stores inclusive natural-habitat ranges for temperature,
gravity and planetary radiation. The initial Terran placeholder ranges are
25–75, 30–70 and 0–40 respectively. Planet environment gauges draw both range
boundaries, while hover text reports whether the selected world is inside the
range. These axes are intentionally separate from propulsion-radiation
tolerance. A later gameplay slice will make the physical ranges contribute to
habitability; until then the existing scalar habitability remains authoritative.

The profile also stores one stable `PrimaryRaceTrait`:

- Generalist;
- Stargate Specialist;
- Habitat Civilization;
- Remote Logistics Specialist.

Only Generalist is gameplay-neutral and usable by the generated Terran player
in this slice. The other identifiers reserve deterministic save/PBEM semantics;
they do not yet grant placeholder percentage bonuses. Their actual rule changes,
costs and exclusions must be designed before the race-creation UI enables them.

This deliberately separates core identity from the future lobby/editor. A host,
AI or file-based game creator will be able to submit the same profile without
depending on Qt widgets.
