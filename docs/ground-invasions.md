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
population and the former owner's production queue is cleared. A new-colony
default template is not a conquest program and is not applied; captured worlds
start with an empty queue and no built or automatically scheduled radar. Mines,
factories, surface minerals and planetary history survive capture.

## Orbital control before landing

Following the Stars! rule accepted on 18 October 2026, **any hostile orbital
station blocks colonist landing**, including an unarmed dock. A blocked landing
transfers no colonists or minerals and does not start ground combat. The fleet
owner receives a delayed **Landing blocked** warning explaining that the station
must be destroyed first. Other players receive no landing-warning packet, and
radio silence holds it until the fleet can transmit. The Cargo dialog states
this condition without inspecting hidden foreign station fits.

The rule covers manual Transfer cargo, waypoint UnloadAll (Colonists or All)
and founding a colony on an empty world. Space battles precede arrival actions
and clear the routes of participating fleets. After the station is destroyed,
a surviving transport therefore needs a new landing order. Manual cargo orders
run at the planning boundary, so a blocked manual invasion must likewise be
resubmitted after the orbit has been cleared. This change preserves that combat
and order timing; it adds the station prerequisite without rescheduling cargo.

Surface conquest does not capture a station. New invasions cannot create the
old unarmed-station ownership split. Host and player-turn readers still accept
existing saves with different surface/station owners, validating the station's
owner, planet and identity. Such a foreign station supplies no friendly orbital
services and also blocks further colonist landing above that colony.

An invasion order cannot smuggle minerals onto an enemy surface. A waypoint
using `UnloadAll` for all cargo commits the colonists first; minerals unload
only after a successful capture and otherwise remain in the fleet. The fleet
itself is not consumed.

Both empires receive delayed Turn Messages appropriate to their perspective:
invasion won or repelled, defense held, or colony lost. The report includes the
winning population that remained after combat.

## Casualty history

Save v64 ground reports include exact own/opposing colonist losses, captured
before subsequent growth or ownership changes. Both participants receive a
summary, including the former defender on colony capture. Empire History credits
these casualties only when the report arrives, in Ground losses and Operational
reports. This does not change the battle outcome or casualty formula.

## Orbital habitats (#56)

A habitat does not create a separate owner or a second invasion. Defending
strength is surface plus orbital residents. A successful defense distributes
its population losses proportionally between the two groups, in whole people.
If the defender loses, its orbital residents are destroyed along with surface
defenders; attacker survivors settle the surface and inherit the empty structure,
paid work and reserves. The new owner's own technology and support govern use.
An orbital-only colony therefore cannot be captured by bypassing its defenders.
Surface bombardment does not kill orbital residents or abandon their colony;
direct habitat combat, surrender and loyalty remain outside this slice.
