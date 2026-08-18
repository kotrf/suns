# Route-program forecasts

The fleet route dock distinguishes navigation that is already determined from navigation that depends on a future dynamic logistics result.

For the selected fleet, the Qt client projects the programmed route by running a disposable copy of `GameState` through the real `TurnProcessor`:

- the current turn includes all orders already queued by the player;
- later projected turns assume no additional orders are issued;
- onboard fuel generation, ram-scoop behaviour, fuel-limited movement, population growth and arrival actions therefore use the same rules as the authoritative simulation;
- the preview horizon is 96 turns.

## Exact versus projected

A leg before any dynamic `Load All` result is labelled **exact navigation**. Its route geometry and departure mass are already known from the current/pending state.

`Load All` itself remains explicitly projected because the amount depends on the colony population that exists when the fleet actually arrives.

Every later leg is labelled **projected navigation** because its gross mass and fuel burn may depend on that dynamic load result.

The forecast is intentionally phrased as "if no further orders are issued". Future player actions, combat and other systems may invalidate a projection; the authoritative result always comes from `TurnProcessor` when the real turn is resolved.
