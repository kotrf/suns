# Fleet arrival actions

A fleet course may carry a one-shot action that resolves only when the fleet actually reaches its destination.

This is intentionally different from an immediate logistics order. `SetFleetColonistsOrder{500}` specifies an exact state for a fleet that is already docked. An arrival action specifies an **intent whose exact result may depend on future game state**.

## Load colonists to capacity

`LoadAllAvailable` with cargo type `Colonists` computes the loaded amount at arrival time from:

- the fleet's free cargo capacity;
- the colony population at that future turn;
- the requested reserve population to leave behind.

The action never predicts and stores an exact colonist count when the course is issued. Natural growth and any other state changes before arrival therefore affect the actual load.

Until explicit colony-abandonment rules exist, the core always leaves at least one colonist on the source colony even if a reserve of zero is supplied.

## Resolution order

For a normal turn:

1. onboard fuel generation occurs;
2. submitted orders are applied;
3. the fleet moves using its pre-arrival mass and fuel;
4. if the destination is reached, the arrival action resolves immediately;
5. sensor intel, production and colony growth resolve normally.

Thus cargo loaded on arrival does not change fuel consumption for the leg that has just completed. It will affect the next leg.

## Persistence

The action is attached to the active course and persists while the ship is in transit, including turns where insufficient fuel prevents arrival. Replotting the course replaces the previous arrival action. The action is consumed once arrival is reached, even if the destination no longer satisfies its requirements.

## Current Qt exposure

The direct-course UI keeps `Plot course + Load All` as a colonist shortcut with a `Leave on Load All` reserve field. The route-program dock exposes `Load All Available` and `Unload All` for Colonists, Ironium, Boranium and Germanium, plus `Refuel`.

This is the first step toward persistent multi-waypoint fleet programs such as:

`fly -> load all leaving 1000 -> fly -> unload all -> refuel -> continue`.
