# Mobile expedition bases — first slice of #61

An Expedition Base is a costly mobile logistics asset for operations beyond a
colony. It uses ordinary ship designs, paid production, fleet movement, combat,
communications and player knowledge. Fixed #54 depots and relays stay permanently
anchored. This slice does not complete issue #61: mobile shipyards, refineries,
science facilities and gate support remain later design work.

## Design and build

| Item | Technology | Mass, kt | Production | I / B / G, kt | Capacity |
| --- | --- | --- | --- | --- | --- |
| Expedition Base hull | Construction 12 | 1200 | 900 | 400 / 180 / 220 | 6000 mg fuel, 1000 kt shared hold, four engines |
| Expedition Dock | Energy 6, Construction 12 | 120 | 200 | 80 / 30 / 40 | External refueling and cargo docking |
| Expedition Workshop | Energy 6, Construction 12 | 200 | 300 | 100 / 60 / 50 | 100 kt restored structure/year |

The hull has six mechanical, two electrical, two armor/shield and one scanner
slot, plus its four engine slots. Service modules fit only this hull. There are
no weapon banks or constructor-kit deployment. Tanks, cargo pods, protection,
ordinary generators and Relay Arrays compete for fitting space and add their
normal mass and costs. Maximum speed is Warp 5, including fleets escorting it.
Build through an ordinary colony shipyard; movement and fuel consumption are
ordinary paid fleet operations. No module creates an unlimited fuel supply.

## Year resolution and resources

A base that physically moves during a year provides no external docking services
that year, including a round trip ending where it began, a gate hop or a wormhole
transit. Services resume the following year if it is idle. An outstanding route,
repeat route, disabled service policy, missing dock or 100% base damage also blocks
service. Radio, relay, scanners, fuel generation and onboard equipment continue
their ordinary rules while travelling. No extra deployment timer is required.

After movement/combat, production and ordinary onboard/shipyard repair, each
stationary base serves its ordered list of friendly FleetIds. A client arriving
at the already stationary base can be served that year. Co-location is required;
foreign fleets and permanently anchored outposts are not clients. Bases resolve
in FleetId order. Destruction removes physical service; a stale report cannot
supply resources from a lost base.

Refueling moves actual stock, bounded by recipient capacity and the base's
configured absolute fuel reserve. Any generator adds only its normal yearly
output. Explicit fuel deliveries also retain the donor's specified reserve and,
when the donor is a base, its service-policy reserve.

Workshops share one annual budget across all clients. Repair work equals
`dry ship mass × restored damage percentage / 100`; dry mass includes the hull
and fitted equipment, excluding fuel, minerals and colonists. Each restored kt
costs **0.10 ironium, 0.05 boranium, 0.05 germanium** from the base's ordinary hold.
Work stops at the tightest material, damage or workshop limit. Missing, distant
and undamaged clients do not consume repair work or block later clients.
A healthy queued client can still refuel. Resources are charged for actual work.
Existing Field Repair Bays keep their own-fleet behavior; workshops add external
repair rather than replacing it. The base does not repair itself through its
external queue.

Services run once after all orders, so repeating policies does not create
capacity. Split fleets inherit movement/service history; a detached fleet starts
with an empty client list. Merging retains the destination policy and the latest
movement/service history. Damage merging conserves damaged dry mass across unlike
ships. Missing client IDs remain in the list until the player removes them;
merging a client does not silently rewrite its base's standing policy.

## Player workflow and knowledge

Open **Mobile expedition bases** from the menu bar. Choose a received base,
set its fuel reserve and ordered client list, then send the service policy.
Add/remove and Up/Down edit priorities. The selected map fleet can fly to the
base's received coordinates using an ordinary route. The base may subsequently
move; arrival and co-location are rechecked physically.

The panel shows dated received stocks/condition and a service-policy preview,
including restored percentage points, transferred fuel and blocking reasons.
The preview uses received client snapshots and available materials; command
travel time, future deliveries, combat and other orders can change the outcome.
Policy changes use the same delayed command transport as fleet routes. Physical,
confirmed and in-flight policies are separate; forecasts never read hidden base
stocks, damage, movement or policy changes.

At a received shared position, select a client and send mineral/fuel delivery
commands in either direction. Commands reach the base through the ordinary radio
delay; they reserve no stocks in transit and recheck actual co-location and
resources during the first year resolution after delivery. Failed conditions
leave stocks untouched; the order makes one attempt. Minerals use the existing
validated `TransferCargoOrder`, with the ordinary shared cargo capacity and
ownership/resource checks; explicit fuel delivery uses `TransferFleetFuelOrder`.
Both resolve after movement and before automatic queued service. A moving base
cannot transfer through an intermediate stop that year. Loading a base at a
colony can also use the existing cargo manifest controls.

Human, computer and PBEM submissions use the same core validation and costs.
The computer planner does not yet design or schedule its own expedition-base
operations; this slice adds no privileged AI logistics path.

## Persistence and checks

Save **v84** adds a bounded MOBI extension after the v83 survey extension. It
stores physical, confirmed, in-flight and retained loss/transit evidence plus
delayed policies and host-only transfer packets. Player exports omit undelivered
logistics packets. Existing supported saves migrate to empty client lists.
Orders **v30** adds service policies and explicit fuel deliveries, retaining the
ordinary cargo order. Player exports carry delivered evidence only.

Core and UI/save suites cover priorities, finite fuel and all three mineral
limits, dry mass, missing/foreign clients, movement and arrivals, reorganization,
repeat orders, delayed commands, UI authoring, save/PBEM round trips and hidden
physical changes leaving exported evidence unchanged.
