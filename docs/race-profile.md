# Race profile core

Each player owns a serialized `RaceProfile`. Radiation tolerance and immunity,
which already affect colonists transported by hazardous drives, now belong to
that profile instead of being loose fields on `Player`.

New campaigns created through **File → New campaign (races / multiplayer)**
use physical environments for racial habitability. Three presets are playable:

| Preset | Temperature | Gravity | Radiation | Tradeoff |
| --- | --- | --- | --- | --- |
| Terran | 25–75 | 30–70 | 0–40 | Temperate generalist |
| Cryophile | 0–40 | 15–65 | 0–40 | Cold and lower-gravity worlds |
| Radiotroph | 55–85 | 50–80 | immune | Narrow hot/heavy environments; safe hazardous-drive transport |

Axes are normalized 0–100. A value outside any applicable range produces
negative habitability, scaled by its distance beyond the range. Within ranges,
suitability decreases from the center toward the edges; the mean of the three
axes determines positive habitability. Radiation immunity makes that axis fully
suitable. Capacity is 25,000 × positive current habitability. Positive values
grow population, zero holds it steady, and negative values kill a proportional
share each year. Hostile worlds may still be colonized, with an explicit UI
warning.

Biology 1/2/3 unlock Sealed/Adaptive/Extreme Habitats: tolerance extends by
5/10/15 points on each nonimmune axis, bounded by 0–100. This is automatic colony
adaptation rather than a planet-specific terraforming project; it can move a
borderline world from negative through zero into positive habitability. A future
terraforming project can use the same signed scale.
Natural-range gauges remain unchanged, so they show the race's original limits.
The map's habitability value includes unlocked adaptations for the viewer.

Save v35 stores signed habitability values. Save v31 adds
`RaceProfile::environmentBased`. Old campaigns and the legacy
quick-generate action keep this false, preserving their original scalar
habitability. Use the new-campaign dialog to opt into the new rules. Presets
are an initial balance pass; there is no point-buy editor yet.

The profile also stores one stable `PrimaryRaceTrait`:

- Generalist;
- Stargate Specialist;
- Habitat Civilization;
- Remote Logistics Specialist.

All three environmental presets currently use Generalist. The other identifiers reserve deterministic save/PBEM semantics;
they do not yet grant placeholder percentage bonuses. Their actual rule changes,
costs and exclusions must be designed before the race-creation UI enables them.

The Qt setup dialog calls `generate_campaign`; race mechanics and research
validation live in the core and apply identically to every player.
