# Order-aware route preview

The map and Cargo editor share the current accepted order boundary. Realtime
estimates are deliberately minimal under the [3 November decision](minimal-ui-estimates.md).

The Qt client uses projected_cargo_order_state through the cached received fleet
plan. Shared current-order validators apply:

- colonist loading and unloading;
- cargo-capacity checks;
- colony population availability;
- explicit refuelling.

No TurnProcessor or annual simulation runs in this preview. Its year stays the
current year. Onboard generation, automatic service, movement and arrival
actions are resolved during End Turn. The accepted current manifest supplies
gross mass and cheap direct-route estimates; it does not credit future fuel.

If the player changes the target colonist count more than once in a turn, the previous `SetFleetColonistsOrder` for that fleet is replaced. The order list therefore represents the final desired cargo state rather than an artificial sequence of intermediate transfers.
