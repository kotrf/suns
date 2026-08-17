# Order-aware route preview

The map route preview should describe the fleet state that will enter the movement phase, not merely the fleet state at the start of the turn.

For preview purposes the Qt client runs a disposable copy of the current game state through the real `TurnProcessor` with only pending logistics orders retained and all existing destinations cleared. This means the preview reuses authoritative rules for:

- turn-start onboard fuel generation;
- colonist loading and unloading;
- cargo-capacity checks;
- colony population availability;
- explicit refuelling.

No preview movement is executed. The resulting fleet copy is then used for gross-mass and route-fuel calculations at the selected Warp.

If the player changes the target colonist count more than once in a turn, the previous `SetFleetColonistsOrder` for that fleet is replaced. The order list therefore represents the final desired cargo state rather than an artificial sequence of intermediate transfers.
