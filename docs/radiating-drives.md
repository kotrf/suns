# Radiating drives and colonist attrition

Suns! treats radiating propulsion as a real ship-design tradeoff rather than a flavor flag.

## Race tolerance

Each player has a normalized radiation tolerance in the range 0.0–1.0 plus an explicit immunity flag. The initial Terran placeholder tolerance is 0.50.

The current Radiating Ram Scoop rule uses a safe threshold of 0.85. A race at or above that threshold, or a radiation-immune race, can carry colonists behind a radiating drive without losses.

The 0.85 threshold deliberately echoes the classic Stars! behavior where a radiation optimum around 85 mR avoided the Radiating Hydro-Ram Scoop penalty. The exact Suns! attrition rate is a tuning rule, not an attempt to reproduce an undocumented original formula.

## Attrition rule

When a fleet carrying colonists actually moves during a turn with a radiating drive fitted and its owner is below the safe tolerance threshold, it loses 10% of the transported colonists, rounded up. At least one colonist is lost whenever an unsafe moving fleet carries population.

A stationary fleet does not take propulsion-radiation losses: the drive is considered inactive while parked.

The current constants are intentionally explicit:

- safe tolerance: 0.85;
- unsafe moving-fleet loss: 10% per turn.

Both are balance knobs. The important architectural rule is that the interaction belongs to race traits × fitted hardware × transported cargo.

## Future extensions

The same model can later support multiple radiation severities, shielding components, biological adaptation technology, crew-only effects, and race creation controls without changing the movement or cargo model.
