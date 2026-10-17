# Deep-space infrastructure

First playable slice of [#54](https://github.com/kotrf/suns/issues/54): paid
constructors become permanent fuel depots and communications relays away from
planets. Orbital stations remain a separate system. There is no remote placement
or free teleportation of material.

## Build and deploy

In Ship Designer fit exactly one construction kit to an ordinary engine-equipped
hull, then build it through the normal shipyard queue. All hull and component
production/mineral costs apply, including the kit:

| Kit | Requirements | Production | I / B / G, kt | Mass, kt | Service after deployment |
| --- | --- | --- | --- | --- | --- |
| Fuel Depot Kit | Construction 4, Energy 3 | 80 | 40 / 20 / 30 | 80 | +2500 mg finite reservoir; visiting fleets use Refuel |
| Deep-space Relay Kit | Construction 4, Electronics 5 | 60 | 25 / 10 / 50 | 50 | 300 ly communications field while powered and transmitting |

The kit's reservoir is physical storage aboard the constructor too. Kits grant
no free fuel on deployment; the existing stock remains aboard. The relay kit
adds no communications range while mobile. Ordinary scanner/Relay Array fittings
continue to obey their existing rules.

Open **View → Deep-space infrastructure**. Select the constructor and enter
coordinates, then choose **Fly to coordinates and deploy**, or **Deploy selected
ship here** after reaching the site. Deployment is also a final route arrival
action. It follows normal physical command-delivery delays.

Deployment needs one ship, exactly one kit, no colonists and a hull below 100%
damage. The site must be at least 5 ly from every star and 1 ly from every other
deployed installation. A terminal deployment leg cannot repeat or have later
waypoints. The host rechecks all conditions at execution; simultaneous competing
builds resolve in FleetId order. Planning does not disclose hidden enemy sites.

The installation retains its FleetId, owner, position, fitted design, fuel,
damage and defenses. Anchoring is permanent: Stop never restores movement, and
movement, split/merge, colonization, mining, stargates and wormholes cannot move
or consume a deployed installation. Optional fitted weapons, armor, shields,
scanners and field repair retain the ordinary physical rules.

## Fuel, maintenance and routes

Each deployed installation consumes **1 mg/year**, at the start of the year
after ordinary onboard generation, starting the year after deployment. A fitted
fuel generator can support that upkeep. At zero fuel, or 100% hull damage, depot,
relay and scanning services stop; the installation remains in place. Exhaustion
queues one dated warning rather than repeating every year. Ordinary emergency
radio/report propagation retains fleet rules; an empty relay adds no mesh field.

**Refuel** draws only from an owned, operational, co-located fuel depot, limited
by its actual stock and the visitor's available capacity. It cannot draw from a
relay or hostile installation. Orbital refueling keeps its existing rules.

**Supply fuel to outpost** transfers actual tanker fuel to either owned kind,
limited by receiving capacity, while retaining the specified **Keep tanker
fuel** reserve. Empty installations accept deliveries and resume service when
supplied. Supply is not a terminal action: a tanker can continue or repeat its
route. Fuel burned while travelling is charged before delivery.

The panel's Fly/Refuel/Deliver buttons use received fixed coordinates; the route
editor displays and edits tanker reserves, including fractional mg. A depleted
or destroyed site cannot fulfill a stale order. Route forecasts simulate received
stocks and explicitly describe future stock uncertainty.

## Knowledge, combat and persistence

The infrastructure table and optional map overlay use received owner telemetry:
kind, fixed position, damage, fuel/capacity and observation age. A remote build,
stock change or destruction does not update a player's snapshot early. Outposts
use square map markers and remain selectable through the normal fleet tools.
Deployment success/failure and fuel exhaustion appear in Turn Messages. Losses
use ordinary immutable Space Battle reports and delayed missing-fleet contacts.

Sites participate in the existing detection and combat systems and have zero
tactical movement. Destroying or disabling a relay removes its real network
field. Enemy sites initially appear as ordinary uncertain fleet contacts; this
slice does not disclose an enemy's installation kind, fittings or stock through
the owner panel. Computer empires exclude anchored sites from mobile missions.

Save **v82**, orders **v29**. A bounded OUTP extension records physical,
received, in-flight and delayed-loss deployment flags without changing old fleet
record layouts. Supply alone adds a fuel-reserve payload to the new action ID.
Old v81 and earlier supported campaigns load with no deployed sites. Invalid
kinds, mismatched kits, moving/colonist-carrying deployed sites and malformed
reserves are rejected. Player exports omit host-only flags and stocks; order
packets carry deployment and fuel-delivery intent, never authoritative sites.

Further #54 stages remain open: construction projects beyond one paid kit,
dedicated listening/sensor posts, cargo caches, separate fortified outpost types,
capture, maintenance/traffic statistics and richer infrastructure management.
Deep-space shipyards, gates and mobile services belong to later work, including
[#61](https://github.com/kotrf/suns/issues/61). Deferred transport networks #53
are not required for this slice. Computer empires do not yet plan deployment or
resupply networks automatically.
