# Automatic space combat

This first combat model uses the Stars! equipment catalog's weapon power,
range, initiative, accuracy, armor and shields. The tactical resolver and the
formulas below are Suns! rules; this is not an exact Stars! battle simulator.

## Encounters and turn order

All different empire owners are hostile. There are no alliances, battle plans
or selective attack policies in this slice. Two unarmed fleets do not fight.

An armed hostile encounter is found along the same simultaneous motion
segments used for friendly interception. Both fleets stop at first contact,
spend fuel for the distance actually travelled, and fight there. Paths crossing
at different times do not cause a battle. Co-located fleets of several owners
fight together, including idle fleets. At most one motion encounter is selected
per fleet per year, by encounter time and stable FleetId.

Combat resolves after movement and before movement-arrival actions. A destroyed
fleet cannot subsequently unload, colonize or enter a wormhole. Surviving
participants interrupt their onboard route, repetition and mining task; new
commands already in flight can still arrive normally. Manual transfers and
ground invasions submitted at the starting planning boundary retain their
existing pre-movement order. Manual colony founding is blocked when an armed
hostile encounter is already present. Orbital stations do not yet participate in battle.

## Tactical resolution

- Each design stack is a combat group. Owner groups start on a circle of radius
  four tactical squares. These squares are independent of strategic light years.
- An armed group approaches the most dangerous enemy until its longest-range
  weapon is in range. Unarmed groups remain stationary. Target priority is
  fitted weapon power times surviving ships, then distance, then stable IDs.
- Movement is simultaneous: one square per round, plus fitted jets and
  overthrusters, minus the strongest Energy Dampener in the battle. Movement is
  bounded to 0.25–2.5 squares; strategic Warp is unaffected.
- A weapon fires once per round when in range. Initiative is hull initiative
  plus fitted computer bonuses plus weapon initiative. Equal initiative volleys
  fire simultaneously; casualties from an earlier initiative cannot fire later.
- Beams always hit. Damage falls by 10% of nominal power per square, with a
  floor of 10%. Capacitor multipliers are capped at 2.5; deflector multipliers
  cannot reduce damage below 10%. Gatlings attack every enemy group in range.
- Ordinary beams remove shields before armor. Sappers only remove shields and
  do not fire at unshielded groups.
- Torpedo probability is `(1 - (1 - baseAccuracy) × computerFailureFactor) ×
  jammerFactor`, with multiplicative fitted computer/jammer factors. Half the
  damage bypasses shields; unused shield damage also reaches armor. Missiles
  double damage when their target is already unshielded. Misses cause no damage.
- Hit counts use deterministic, seed-based stochastic rounding of expected
  hits. A single projectile is a Bernoulli trial; large volleys remain cheap to
  resolve. This is not a per-projectile binomial simulation.
- Battles stop when at most one owner remains or after 30 rounds. A round-limit
  result preserves all survivors; it never arbitrarily chooses a winner.

## Persistent losses and logistics

Initial armor includes existing fleet damage. Damage is resolved separately for
each design stack during a battle, with whole-ship losses and partial remaining
armor. Destroyed stacks are removed; an empty fleet is erased without triggering
the legacy one-ship fallback. Existing prototype hulls with no armor rating have
one armor point. A Warp-disabled hull still exists and can be destroyed in battle.

Persistent damage retains the existing **fleet-wide percentage** representation:
remaining armor is divided by the full armor of surviving ships. Damage is
therefore averaged across designs between battles. Individual damaged-ship
records are a future refinement. Shields recharge for the next battle.

Cargo and colonists are lost in proportion to destroyed cargo capacity; fuel is
lost in proportion to destroyed tank capacity. Surviving cargo/fuel cannot exceed
surviving capacity. There is no salvage creation yet. Existing end-of-year
shipyard/field repair also applies to battle damage, once per year.

## Reports and information

Each participant receives an immutable battle report through its existing
communication delay. Result packets are dispatched using the pre-casualty relay
network. Reports retain fleet/design names and counts even if those objects no
longer exist. Uninvolved empires receive no battle report or hidden enemy fits.

A destroyed remote owner's last confirmed contact remains in its map, fleet
lists and player export until the result packet arrives. This contact is not a
physical fleet and cannot move, mine, relay messages or accept host orders.
Player exports strip undelivered reports, delivery times and the host loss ledger.

Turn Messages → Combat shows the outcome, observation/receipt years, participant
losses and volley log. Show on map uses the historical battle location. History
adds a marker when the report arrives. Up to 4096 volleys are saved; the compact
reader shows the first 256 and states when limited. Fleet totals cover the full
battle regardless of log limits.

Navigation forecasts exclude enemy fleets and never resolve a battle using
hidden enemy designs, paths or the host seed. Their route ETA remains conditional
on avoiding hostile encounters.

Save v61 introduced battle reports and undelivered loss contacts. Reading save
v12–60 remains supported. The current format is documented in [Strategic operations](strategic-operations.md); all PBEM participants should use
the same build. Station fitting,
retreat doctrines and diplomacy remain separate follow-up work.

[Fleet strategic missions](strategic-operations.md) resolve bombing after fleet encounters, apply mine hazards to movement, and leave recoverable wreckage after losses.
