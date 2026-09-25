# Multiplayer turn exchange

## Playing a campaign

1. The host chooses **File → New campaign (races / multiplayer)**, sets the seed,
   galaxy size, and 1–8 human empires with names and race presets. Player 1 plays
   locally; each other empire receives its own turn file. Homeworlds are chosen
   far apart and start with equal population, industry, minerals, orbital services
   and two basic ship designs with globally unique IDs.
2. Choose **File → Export player turns**. The app first saves the host `.suns`
   campaign, including its turn identities, then writes one `.sunsturn` file per
   remote player into the selected folder. Send each participant only their file.
3. Participants open their file with **File → Open game**, plan normally, and
   choose **Export orders** (also the main turn button). They can save an unfinished
   draft as `.suns`; its player-turn mode survives saving and reopening. A player
   view cannot resolve the authoritative turn.
4. The host imports the returned `.sunsorders` files. The inbox retains one
   accepted submission per remote empire. Reimport prompts before replacing it;
   imported orders and tokens are saved with the host campaign. Empty orders are
   an explicit pass.
5. Once every participant has submitted, the host resolves the turn. Resolution
   sorts submissions by player ID, applies ownership/technology validation in the
   core, rotates all player tokens, clears the inbox and saves the new boundary.
   Export and distribute the next player turns.

This is asynchronous file multiplayer with a trusted host. There is no deployed
server, matchmaking, network login, signature verification or fleet combat yet.
Copying a token gives its holder the ability to submit for that player that turn;
use a private channel for each participant's files. Keep the host save private.

## Envelopes and persistence

`.sunsorders` includes campaign ID, turn, a random player-specific token, issuing
player ID, typed orders and display descriptions. It contains no game state.
The host rejects another campaign/turn/player or an incorrect/expired token.
All players resolve together, and missing submissions never silently become passes.
The order stream rejects nonfinite coordinates and negative/nonfinite minerals.

Save v31 stores session mode, host inbox, player tokens, racial environment rules
and projected observations. Earlier saves (v12–30) remain readable with their
original scalar habitability. Multiplayer clients should use the same build.
Reopening and restarting a new-style host campaign preserves its preset choices;
a restart creates a new campaign identity.

## Player view boundary

`make_player_view` constructs a separate `PlayerView` for serialization into a
`.sunsturn`. The renderer reuses `GameState` containers for known entities; these
are a projection, not simulation authority:

- Only the recipient's race, technology, history and ship designs are included.
- The galaxy seed is omitted. Public stellar positions/names/classes remain.
- Unknown planets are absent. A basic scan delivers an estimated habitability;
  orbital survey adds physical environment and ownership, but never foreign
  population, industry, production, cargo stocks or artifact-site internals.
- Known mineral concentrations and habitability are materialized observations,
  so clients do not need the generation seed to reproduce the displayed values.
  Unowned surface stocks are current only with an immediately connected fleet
  on site; otherwise the projection does not expose their changing host value.
- Own fleets use delivered telemetry/projection, with physical pending command
  and telemetry queues removed. Undelivered survey/operational reports are absent.
- Enemy contacts appear only in the connected sensor mesh, with ID, owner and
  position. Their design, fuel, cargo, destination and route are omitted. They
  can be selected as fixed-position route targets, as in the existing route UI.
- Only own orbital station internals and own delivered messages are exported.

Known limitations: detached scouts do not yet deliver delayed enemy-contact
reports; there is no historical foreign-colony ownership/contact cache. Static
orbital/geological observations follow the existing survey-level model. Host
UI uses authoritative state because the host is trusted. No claim is made that
an untrusted host cannot inspect or alter a campaign.

## Remaining layers

- Append-only resolution ledger with accepted envelopes, result hashes and host
  version, for audit/replay and recovery after crashes.
- A transport interface and authenticated network exchange.
- Delayed enemy-contact snapshots and richer opponent intel.
- Computer opponents consuming `PlayerView` and emitting ordinary `PlayerOrders`.
- Combat/diplomacy and genuinely asymmetric primary racial traits.
