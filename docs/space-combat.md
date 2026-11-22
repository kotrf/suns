# Space combat and fleet battle plans

Suns! resolves fleet encounters using fitted weapons, range, initiative,
accuracy, armor and shields. Each fleet's battle plan governs initiation,
target selection and tactical movement.

## Fleet battle plans

Select a fleet and open **Battle plan…** in **Fleet — Orders & Logistics**.
The dialog shows the dated onboard report and any plan queued for this year.
**Queue plan** sends the change with the turn's orders. It takes effect when
the command reaches the fleet. The editor uses received information and
queues the command without simulating a battle.

**Attack on contact** lets an armed fleet start a fight with an eligible
foreign target. **Return fire only** lets an attacked fleet participate
and fire at eligible enemies. Two return-fire fleets pass each other, as do
two unarmed fleets. Armed orbital stations always engage foreign fleets and
stay in place during combat.

The preferred target can be **Any target**, **Armed ships**, **Cargo ships**
or **Orbital stations**. Ships with beams or torpedoes count as armed. Hulls
and fittings with cargo capacity count as cargo ships, including armed
freighters. A mixed fleet presents the categories of all its ship designs.
An armed fleet with **Engage preferred targets only** opens fire on contact
only if the chosen category is present. Its weapons use the same restriction
in battle. With fallback enabled, weapons use other eligible targets when
the preferred category has no target the weapon can hit.

Old saves receive **Attack on contact**, **Any target** and **Approach**.
Detachments inherit their source fleet's active plan. A merge uses the
destination fleet's plan. A Stop order leaves battle-plan updates in flight.
A late packet cannot overwrite an already delivered newer plan.

Computer empires use the same orders and communications. With a fresh
manifest, they prefer armed targets for warships and send unarmed mobile
fleets a return-fire plan with withdrawal.

## Encounters and turn order

Eligible attacks use the same simultaneous motion segments as friendly
interception. Fleets stop at first contact, spend fuel for the distance
actually travelled and fight there. Paths crossing at different times do not
cause a battle. At most one motion encounter is selected per fleet per year,
by encounter time and stable FleetId.

Each attack engages its two empires. Other fleets and stations of those
empires at the encounter can support them, including idle fleets. A third
empire enters if it starts or receives an attack. In a battle with several
empires, weapons can target only empires directly engaged against their own.
An armed escort joins if it is within encounter range of its cargo fleet
at the moment of contact. Anchored outposts support the battle from their
existing coordinates. A supporting escort counts as an initiator only if
its own attack also triggers at that time.

Combat resolves after movement and before movement-arrival actions. An attack
that coincides with a rendezvous during movement resolves before the planned
merge. A destroyed fleet cannot subsequently unload, colonize or enter a wormhole. Surviving
participants interrupt their onboard route, repetition and mining task. Surviving
bombers retain their mission; an armed hostile station must be neutralized
before they can bomb. New
commands already in flight can still arrive normally. Manual transfers and
ground invasions submitted at the starting planning boundary retain their
existing pre-movement order. Manual colony founding is blocked when a nearby
fleet would start a battle. Orbital control also governs landing rights.
[Station combat and colony defenses](orbital-stations.md)

## Tactical resolution

- Each design stack is a combat group. Owners start on a circle of radius
  four tactical squares. Orbital stations start in the center with fleets one
  square away.
- **Approach** closes to the range of the longest weapon that can hit the
  selected target. **Keep distance** also backs away when the target closes
  inside this range. **Escort transports** keeps armed groups one square ahead
  of a nearby friendly cargo group in the direction of the enemy. With no
  transport to screen, escorts approach normally. Unarmed groups hold position
  in these three plans.
- Movement considers targets the group's weapons can affect. Sappers can
  close on other shielded targets when the plan permits fallback.
- The preferred category ranks first. Within a category, target priority is
  fitted weapon power times surviving ships, then distance, then stable IDs.
- Movement is simultaneous: one square per round, plus fitted jets and
  overthrusters, minus the strongest Energy Dampener in the battle. Movement is
  bounded to 0.25–2.5 squares. Strategic Warp uses its own movement rules.
- A weapon fires once per round when in range. Initiative is hull initiative
  plus fitted computer bonuses plus weapon initiative. Equal initiative volleys
  fire simultaneously; casualties from an earlier initiative cannot fire later.
- Beams always hit. Damage falls by 10% of nominal power per square, with a
  floor of 10%. Capacitor multipliers are capped at 2.5. Deflector multipliers
  cannot reduce damage below 10%. Gatlings attack every eligible group in range
  at the best available target priority.
- Ordinary beams remove shields before armor. Sappers only remove shields and
  do not fire at unshielded groups.
- Torpedo probability is `(1 - (1 - baseAccuracy) × computerFailureFactor) ×
  jammerFactor`, with multiplicative fitted computer/jammer factors. Half the
  damage bypasses shields; unused shield damage also reaches armor. Missiles
  double damage when their target is already unshielded. Misses cause no damage.
- Hit counts use deterministic, seed-based stochastic rounding of expected
  hits. A single projectile is a Bernoulli trial; large volleys remain cheap to
  resolve. This is not a per-projectile binomial simulation.
- Battles stop when no opposing pair remains active or after 30 rounds. A
  round-limit result preserves all survivors.

### Withdrawal

**Withdraw** directs the whole fleet outward at the tactical speed of its
slowest surviving group. Ships can fire and take damage as they withdraw.
Groups that reach eight squares from the center escape after that round's
volleys. Anchored outposts and fleets at 100% damage cannot withdraw.

Escaped ships regroup in open space 0.04 ly from the encounter. The fleet's
route and active mission end. A visitor mission is cancelled. An autonomous
survey programme pauses with its settings and survey notebook available
for Resume. This tactical maneuver uses no additional strategic fuel. The
fleet keeps the cargo and fuel left after normal battle losses. If the battle
ends before the fleet escapes, survivors remain at the encounter and follow
the ordinary post-battle rules.

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

Turn Messages → Combat shows the outcome, observation and receipt years,
initiators, withdrawals, participant losses and volley log. Show on map uses
the historical battle location. History adds a marker when the report arrives.
Up to 4096 volleys are saved. The compact reader shows the first 256 and states
when limited. Fleet totals cover the full battle regardless of log limits.

Navigation forecasts exclude enemy fleets and never resolve a battle using
hidden enemy designs, paths or the host seed. Their route ETA remains conditional
on avoiding hostile encounters.

Save v133 preserves active, queued and last reported plans, as well as
initiators and withdrawal results. PBEM order files use v59. The game reads
older saves and orders. Player turn files carry plans from received telemetry.
All PBEM participants should use the same build.

[Fleet strategic missions](strategic-operations.md) resolve bombing after fleet encounters, apply mine hazards to movement, and leave recoverable wreckage after losses.
