# Computer empires: controller integration

Current implementation includes all three initial phases of #184, delivered in
PR #188/#189/#190: controller integration, civilian expansion and military
behavior. The detailed controller foundation below is the original step-1
checkpoint. Computer bombers now program **Bombard enemy colony** on their raid
waypoint using ordinary MoveFleetOrder, so arrival can begin work without a
second command from headquarters. Received ownership, equipment, combat and
emission rules are shared with humans; the arrival order grants no hidden data.

This is step 1 of [#184](https://github.com/kotrf/suns/issues/184). It connects
computer empires to the ordinary campaign turn. Independent scouting,
colonization, production planning and military behavior are subsequent steps.

## Playing

**File → New campaign (races / multiplayer)** chooses 1–8 empires. Player 1 is
the local human; every other row offers **Human / Computer**, defaulting to
Computer. Race and equipment-access choices are identical for both controllers,
as are starting population, minerals, industry, fleets and station services.

**End Turn** waits only for remote humans. The readiness tooltip distinguishes
received/waiting human orders from automatic computer orders. An empty human
submission is an explicit pass; an absent submission continues to block the
turn. **Export player turns** creates files only for remote humans. Computer
empires have no PBEM token and cannot receive turn files or submit imported
order packets. Campaign save/load and restart retain controller choices;
restart begins a new campaign and resets the controller memory.

**Game → Computer decisions…** shows the last 32 resolved decisions per
computer empire, including planning turn, order count and reason. The host
can inspect this journal; player-turn clients cannot open it. An unsuccessful
attempt to resolve an incomplete turn never advances or appends to the journal.

## Initial policy and boundary

The initial controller sets a 20% research allocation once when the existing
allocation is zero, and resumes a paused research plan. It preserves a nonzero
budget, current focus and queued fields. Later turns submit explicit empty
envelopes and retain standing fleet/production/research programs. It does not
yet choose exploration or colonization targets or build a fighting force.
This deliberately small policy makes automatic order submission testable with
real orders and ordinary economic costs.

`plan_computer_turn(const PlayerView&)` is a pure C++ function. It receives the
projection from `make_player_view` and returns `PlayerOrders` and proposed
controller memory. It does not take the authoritative host state, allocate
hidden global IDs, inject research/resources, or read physical pending reports.

`resolve_controlled_campaign_turn` accepts **human submissions only**, rejects
unknown/duplicate/computer envelopes, and checks human completeness by player
ID. It builds every computer's view from the same planning boundary, gathers
their ordinary envelopes, and delegates to `resolve_campaign_turn`. The regular
processor validates and resolves all empires together. Controller memory is
committed only in the successfully returned next state. The exhaustive
`resolve_campaign_turn` API remains available for explicit replays/simulations.
Headless campaigns may contain only computer empires.

## Persistence and checks

Save **v67** stores controller type, one-time initialization and a bounded
decision journal inside the existing strategic extension. Orders remain **v21**:
no new privileged order type is needed. Existing older save readers default to
Human with empty controller memory. Unknown controller values, inconsistent
initialization, overlong/oversized journals, unordered/future decision years,
computer PBEM identities/inbox entries and a computer local planner are rejected.

Coverage includes UI campaign creation, restart, mixed-human readiness, real
human-only file export, save/load and corruption rejection, computer packet
rejection, independent multi-computer envelopes, explicit passes, ordinary
processor equivalence, bounded journals, fixed-seed replays and unchanged
decisions/PBEM bytes when hidden opponent state changes. Autonomous expansion
and combat acceptance under #184 remain open.
