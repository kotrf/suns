# Race profile core

Each player owns a serialized `RaceProfile`. Radiation tolerance and immunity,
which already affect colonists transported by hazardous drives, now belong to
that profile instead of being loose fields on `Player`.

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
