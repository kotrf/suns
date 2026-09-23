# Ground invasions

Colonization applies only to unowned worlds. A fleet at an enemy colony may
instead unload colonists to start a ground invasion. This works both through
the **Transfer cargo** dialog and the waypoint `UnloadAll` action; no colony
module is required.

The first combat model intentionally uses only population strength. With `A`
attackers and `D` defenders, the attacker's success probability is:

`A² / (A² + D²)`

Equal populations therefore have 50% odds, a 2:1 advantage has 80% odds and a
1:2 attack has 20% odds. The roll is derived deterministically from the galaxy
seed, turn, fleet, planet and force sizes. Replaying the same submitted turn
produces the same result, while otherwise identical attacks in different turns
can differ.

The losing force is destroyed. The winner loses a deterministic 65–100% of the
opposing population, capped so at least one colonist survives. On a successful
invasion the planet changes owner, the surviving attackers become its surface
population and the former owner's production queue is cleared. Mines,
factories, surface minerals and planetary history survive capture. Ownership
of an orbital station is unchanged: capturing or destroying orbital assets
belongs to the later space-combat layer.

An invasion order cannot smuggle minerals onto an enemy surface. A waypoint
using `UnloadAll` for all cargo commits the colonists first; minerals unload
only after a successful capture and otherwise remain in the fleet. The fleet
itself is not consumed.

Both empires receive delayed Turn Messages appropriate to their perspective:
invasion won or repelled, defense held, or colony lost. The report includes the
winning population that remained after combat.
