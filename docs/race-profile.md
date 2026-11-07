# Race profile core

Each player owns a serialized `RaceProfile`. Radiation tolerance and immunity,
which already affect colonists transported by hazardous drives, now belong to
that profile instead of being loose fields on `Player`.

The desktop app starts with a solo Terran campaign using physical environments
for racial habitability. **Game → New galaxy…** and **Restart current galaxy**
also use this model, with the current campaign's race setup or a solo Terran
default. **File → New campaign (races / multiplayer)** lets players choose races
and empire settings. Three presets are playable:

| Preset | Temperature | Gravity | Radiation | Tradeoff |
| --- | --- | --- | --- | --- |
| Terran | 25–75 | 30–70 | 0–40 | Temperate generalist |
| Cryophile | 0–40 | 15–65 | 0–40 | Cold and lower-gravity worlds |
| Radiotroph | 55–85 | 50–80 | immune | Narrow hot/heavy environments; safe hazardous-drive transport |

## Portable race files

**File → Race designer…** provides an independent form for every participant.
It shares its controls with **New campaign → Edit race…**. A `.sunsrace` saved
with **Save race…** can be sent to the host, loaded into one selected campaign
slot, and reused in another game. It carries all supported `EmpireSetup` race
settings, including factory/mine operation rates, while leaving control mode
and campaign identity to the host. Importing a player race sets its slot to Human.
Generation, save/load and restart retain the same racial rules.

The file is UTF-8 JSON with a separate schema version. A complete v1 example is:

```json
{
  "format": "suns-race",
  "version": 1,
  "name": "Explorers",
  "environment": "terran",
  "primary_trait": "generalist",
  "fuel_efficiency": false,
  "no_ram_scoops": false,
  "settler_engines": false,
  "hull_family": "standard",
  "remote_mining": "standard",
  "factories_per_10000": 10,
  "mines_per_10000": 10
}
```

Environment keys are `terran`, `cryophile`, `radiotroph`; traits are `generalist`
and `stargate-specialist`. Hull keys are `standard`, `inner-strength`,
`super-stealth`, `war-monger`, `space-demolition`, `hyper-expansion`. Mining is
`standard`, `advanced` or `basic`. The operation rates are whole numbers 5–25.
Names use 1–80 UTF-8 bytes, without control characters. Hyper Expansion includes
Settler engine access under the existing campaign rules.

The reader accepts at most 64 KiB and requires every v1 field with its exact
type and a supported value. Unknown versions/fields, missing fields and damaged
JSON fail without changing the editor or campaign row. Saves are atomic and
validate before opening the destination. No game-state, seed, technology or
turn-token payload can enter the host through this schema.

## Factory and mine operation

Each race has independent `factoriesPerTenThousand` and `minesPerTenThousand`
settings, each 5–25, with 10/10 as the default. New campaign exposes both settings
for human and computer empires. Surface residents operate each installation type
independently; workers are not allocated from a shared factory/mine pool:

`working = min(built, floor(surface population × racial rate / 10,000))`

At 10/10, 25,000 residents operate 25 factories and 25 mines simultaneously.
At 5/25 the same population operates 12 factories and 62 mines. Excess buildings
remain installed and idle; loading residents, casualties, growth and a new
owner's rates change the working count automatically. Fractional groups of
10,000 count proportionally before rounding down to whole installations.
Orbital habitat residents provide their established orbital output and do not
operate surface installations.

Only working factories contribute factory production. Population's existing
resource contribution and installation build costs retain their current rules.
Only working mines add extraction capacity, including colony xenocrystal mining.
Production, research, forecasts and computer orders use the same derived limits.
The UI shows active/built counts and the population limits, including accepted
local cargo intent. Manual construction beyond today's limit is allowed.

Save v108 adds a bounded WFRK racial-settings extension. Earlier saves receive
10/10 defaults while retaining all installed buildings; host and player-turn
saves preserve the local race's settings. Ordinary turn orders remain v47.
Reference for the racial settings: [Stars! race guide](https://gamefaqs.gamespot.com/pc/198797-stars/faqs/41043).

## Environment and population

Axes are normalized 0–100. A value outside any applicable range produces
negative habitability, scaled by its distance beyond the range. Within ranges,
suitability decreases from the center toward the edges; the mean of the three
axes determines positive habitability. Radiation immunity makes that axis fully
suitable. Capacity is 25,000 × positive current habitability. Positive values
grow population, zero holds it steady, and negative values kill a proportional
share each year. Hostile worlds may still be colonized, with an explicit UI
warning. Here "hostile" means environmentally hostile: the world must still be
unowned. An enemy colony cannot be colonized and must first be conquered.

Ground invasion is separate from colonization. Unloading colonists onto an
enemy colony starts population combat; see `ground-invasions.md`.

In new campaigns Biology unlocks paid physical terraforming rather than freely
expanding racial ranges. Research prerequisites and production tasks are described
in [Terraforming](terraforming.md). Natural-range gauges still show the race's
original limits; their tooltips include each planet's natural, current and
reachable values. The environment panel shows current and reachable habitability.
Saves predating v59 preserve the previous Biology tolerance extension of 5/10/15
points, capped at Biology 3, via `legacyBiologyAdaptation`.

Save v35 stores signed habitability values. Save v31 adds
`RaceProfile::environmentBased`. Loading old campaigns keeps this false,
preserving their original scalar habitability and population rules. Creating
or restarting a galaxy uses the environment model; it does not convert an
existing save in place. The core `generate_game` function remains a legacy
generator for compatibility. Presets are an initial balance pass; there is no
point-buy editor yet.

The profile also stores one stable `PrimaryRaceTrait`:

- Generalist;
- Stargate Specialist;
- Habitat Civilization;
- Remote Logistics Specialist.

Environmental presets default to Generalist. Campaign creation can independently
select **Stargate Specialist**, whose rules allow physical cargo/colonist gate
transit and unlock paid Network and Infinity/Infinity orbital fittings. See
[Stargate Specialist](stargate-specialist.md) for prerequisites, prices, endpoint
constraints and Computer strategy. Habitat Civilization and Remote Logistics
Specialist remain reserved, unavailable choices rather than placeholder bonuses.

The selected next-trait [design and acceptance plan](asymmetric-empires.md) is an
Alternate Reality-inspired orbital civilization: station-bound population,
catastrophic battle loss, stronger orbital hulls, remote mining from the start,
and radiation sensitivity without surface temperature/gravity penalties. Battle
destruction kills all native residents remaining aboard; prior physical
evacuation and paid hull replacement preserve their actual residents. The shared
`UnloadAll` command delivers native colonists to their own inhabited station,
subject to actual housing capacity; absent/full stations leave residents aboard.
Economics remain prototype work; this is not yet a playable trait.
The native population/cargo kernel is separate from ordinary habitats; save v116
also retains delivered station destruction before the first received fit. It
does not enable this reserved identity in campaign
setup or portable race files; the complete start and Human/Computer expansion
acceptance in the design still applies.

The Qt setup dialog calls `generate_campaign`; race mechanics and research
validation live in the core and apply identically to every player.

## Stars! surface growth (4 November 2026 follow-up)

[Economy/population contract](economy-rebalance.md#surface-population-follow-up-4-november-2026)
supersedes the earlier population-rule descriptions above: normal new homeworlds
start at 25,000, ideal capacity is 1,000,000 and racial maximum growth is editable
1–20% (default 15%). Full growth lasts through 25% capacity; thereafter quadratic
crowding applies, with red-world and overcrowding mortality. Surface factories
do not set birth rate. Hull access still does not grant full Stars! PRT effects;
orbital habitat support/growth stays separate. Save v112 stores racial rates,
orders stay v49; race files write v3 and read v1/v2 as implicit 10% maximum.
Old counts, cargo and ledgers are retained; subsequent turns use the new curve.
