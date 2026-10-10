# Strategic warfare and field science

Fleet missions and emission controls live in the Fleet operations panel. Assign
an idle fleet a task, or issue a new emission program. These are core orders;
commands travel through the same delayed channel as routes. Travel cancels
stationary work. Different empires remain hostile in the current combat rules.

## Bombardment

`Bombard enemy colony` requires fitted bombs and physical orbit above a foreign
colony. Space battles resolve before bombing, so destroyed ships cannot bomb.
Bombing runs before production and population growth. Bombs do not fire
automatically simply because a ship carries them.

For each bomb rating, combine population survival multiplicatively across fitted
copies and ship counts. Kills are the greater of that result and the combined
minimum-kill rating, capped by the actual population. Ordinary/LBU bombs also
destroy their installation rating, split between factories and mines. Smart bombs
preserve industry; their population percentage is scaled by population coverage
`clamp(population / 2,500,000, 0.1, 1)`. Retro bombs move altered environmental axes
towards the stored natural environment, up to one point per fitted copy each year.

The attacker and defender receive immutable losses, after communication delivery.
Third parties get no bombing result. At zero population the colony becomes
neutral, its queue is cleared and its station is removed. No permanent planetary
scars or defense installations are added in this slice (#106 remains separate).

## Minefields

`Lay minefields` creates/replenishes stationary physical fields using the module's
Stars! rate and type. Friendly fields of the same type and center combine; radius
is `sqrt(mine count)` ly. Fields lose 5% of their mines at the beginning of each
year and disappear below one mine. New fields affect subsequent movement years.
Counts are capped at 100 million mines per field.

| Type | Safe Warp | Dangerous crossing |
| --- | --- | --- |
| Standard | 4 | Stops; consumes up to 50 mines; armor damage 4 per consumed mine |
| Heavy | 6 | Stops; consumes up to 100 mines; armor damage 12 per consumed mine |
| Speed trap | 5 | Stops; consumes up to 50 mines; no armor damage |

Owner fleets pass safely. Crossings test the traveled segment, including a
fly-through with both endpoints outside the field. A dangerous passage stops at
the first field boundary and clears the route. Armor damage is averaged over the
fleet like existing overdrive damage; complete destruction removes the fleet.
A delayed owner loss retains its last confirmed fleet contact until the report
arrives. All simultaneous movements use the year's initial field boundaries;
mine consumption resolves by stable FleetId, and field IDs break crossing ties.

`Sweep hostile mines` requires beam weapons and a stationary fleet inside or within
10 ly of the field boundary. Sweep rate is twice summed beam power per year for
standard/speed-trap mines, half beam power for heavy mines. Decay and sweeping can
remove a field. Detonation, mine-specific racial advantages and more detailed
damage allocation are future work.

## Stealth, emissions and electronic warfare

Cloaking combines fitted copies with diminishing returns. Detection of a mixed
fleet is governed by its least cloaked stack, capped at 95% cloak. Tachyon
equipment combines similarly and reduces effective opposing cloak. Active hull
detection range is `scanner range × (1 − effective cloak)`.

| Fleet mode | Active scans/jamming | Telemetry/commands |
| --- | --- | --- |
| Standard | Enabled | Normal communication delay |
| Passive | Disabled; passive receiver remains | Transmission at years divisible by 3 |
| Radio silence | Disabled; passive receiver remains | Automatic resume year is mandatory (1–100 years ahead) |

Passive localization starts with 25% of the fitted scanner range, then uses a
stationary signature factor of 0.5, or a moving factor
`min(2, 0.5 + Warp/5)`, and effective cloak. Radio silence does not erase commands
already aboard. New commands wait for a communication window; in-flight packets
remain deliverable. A fleet's operation reports follow its emission program;
generic reports use the earliest co-located transmitter, with colony reports
independent of silent orbiting fleets. Combat snapshots retain these delivery
dates even if the reporting fleet is destroyed. Owner positions continue to be projected from confirmed
telemetry. Silent fleets do not act as communication relays or publish new active
surveys. A burst/single-scan route program is not implemented yet (#101).

Active jammers reduce opposing fleet scanners inside an aura of at least 30 ly
(or the jammer fleet's ordinary scanner range). Component strengths combine with
diminishing returns; the strongest local hostile fleet determines the effect,
capped at 75% sensor reduction. Jamming adds 1–2 years to communication at the
source. It retains its separate torpedo-combat effect.

An active/transmitting fleet can produce a passive emission contact at up to twice
the receiver's fitted scanner range, even when its hull is cloaked. The contact is
a 20 ly grid cell center with 15 ly uncertainty, never a fleet ID, owner,
composition or exact position. Received signal markers live for one planning year;
the report remains in Turn Messages. Detached receivers report with delay.

`Broadcast decoy signatures` requires a jammer and Standard mode. It adds a false
ambiguous signal area. A receiver whose combined tachyon strength is at least the
jammer strength rejects it. The player's display does not label an unverified
signal as a fake. This is a first deception model, not full contact confidence,
traffic analysis or autonomous fleet doctrine (#58/#101 remain partial).

## Science from the world

`Field research` requires a scanner and a stationary fleet in Standard mode.
Objectives and rewards are explicit in the mission panel:

| Observation | Requirement | Reward |
| --- | --- | --- |
| Stellar study | Orbit at a star | 12 Energy points |
| Deep planetary study | Orbit and received geological knowledge | 12 Biology points; 32 at an artifact-bearing site |
| Wormhole study | Within 10 ly of an endpoint | 24 Propulsion points |

Each empire can study each objective once. A host ledger reserves the observation
when collected; multiple ships cannot duplicate a reward. Data is queued at the
physical observer. Technology progress changes only when its packet reaches the
empire. Research can be paused; scientific findings still advance their specified
field, independently of the current research focus. A finding can complete normal
technology levels. Undelivered reservations are stripped from player exports.

Space battles with destroyed ships leave physical wreckage for 20 years. Wreckage
records the highest technology requirements of destroyed fitted equipment.
`Recover wreckage` requires a scanner and cargo capacity, within 10 ly. Recovery
consumes one nearest specimen; stable ID breaks distance ties. Only technology
above the recovering empire's current levels earns science: 12 points per level
of novelty in the most novel field, capped at 96. Obsolete debris earns no science.
Competing recoveries resolve by stable FleetId. Physical mineral recovery,
returning cargo samples, unique artifacts retained instead of dismantled, and
observation prerequisites on particular technologies remain future #59 slices.

## Persistence, evidence and forecasts

Save **v62** reads v12–62; orders **v19** read v1–19. Emission programs, task
commands, fields, wrecks, observations and pending intel persist. A bounded
trailing extension preserves all earlier record layouts and migrations. Old
campaigns begin with empty fields/wrecks, no field-science reservations and
Standard fleet emissions.

Player exports contain delivered object evidence and their own confirmed fleet
state, never physical hidden fields/wrecks or undelivered scientific data. The map
uses received snapshots with their observation date. Rescanning an empty location
removes obsolete object evidence. Route previews use only received minefield
snapshots; hidden hazards and future combat outcomes cannot alter a forecast.

These are Suns! deterministic strategic rules built on Stars! equipment ratings;
the bombing, mines, sensing and science formulas are initial balance policy.
