# Multiplayer turn exchange

## Playing a campaign

1. The host chooses **File → New campaign (races / multiplayer)**, sets the seed,
   galaxy size, and 1–8 empires with names, race presets and Human/Computer control.
   Player 1 is human and plays locally; each remote human receives its own turn
   file. Computers submit ordinary orders automatically on the host. Homeworlds are chosen
   far apart and start with equal population, industry, minerals, orbital services
   and two basic ship designs with globally unique IDs.
2. Choose **File → Export player turns**. The app first saves the host `.suns`
   campaign, including its human turn identities, then writes one `.sunsturn` file per
   remote human into the selected folder. Send each participant only their file.
3. Participants open their file with **File → Open game**, plan normally, and
   choose **Export orders** (also the main turn button). They can save an unfinished
   draft as `.suns`; its player-turn mode survives saving and reopening. A player
   view cannot resolve the authoritative turn.
4. The host imports the returned `.sunsorders` files. The inbox retains one
   accepted submission per remote empire. Reimport prompts before replacing it;
   imported orders and tokens are saved with the host campaign. Empty orders are
   an explicit pass.
5. Once every human has submitted, the host generates computer orders from their
   own PlayerView projections and resolves the turn. Resolution
   sorts submissions by player ID, applies ownership/technology validation in the
   core, rotates all player tokens, clears the inbox and saves the new boundary.
   Export and distribute the next player turns.

This is asynchronous file multiplayer with a trusted host. There is no deployed
server, matchmaking, network login or signature verification yet. Ground invasions
and [automatic fleet combat](space-combat.md) resolve on the host through the
ordinary turn processor; participants receive only their delivered reports.
Copying a token gives its holder the ability to submit for that player that turn;
use a private channel for each participant's files. Keep the host save private.

## Envelopes and persistence

`.sunsorders` includes campaign ID, turn, a random player-specific token, issuing
player ID, typed orders and display descriptions. It contains no game state.
The host rejects another campaign/turn/player or an incorrect/expired token.
All players resolve together, and missing human submissions never silently become passes.
Computer empires have no PBEM identities; their imported order files are rejected.
The order stream rejects nonfinite coordinates and negative/nonfinite minerals.

Save v31 introduced session mode, host inbox, player tokens, racial environment rules
and projected observations. Earlier saves (v12–30) remain readable with their
original scalar habitability. Controller integration uses save v67 and orders v21;
supported save readers cover v12–67 and order readers v1–21. Older saves default
to human controllers. Multiplayer clients
should use the same build.
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
- Delivered battle reports preserve observed participants and losses. A destroyed
  remote fleet's last confirmed contact remains until its result arrives; the
  host's loss ledger, delivery times and undelivered battle payloads are stripped.
- Enemy contacts use received or connected snapshots, including observation age,
  confidence, position uncertainty and gated hull/count evidence. Their equipment,
  fuel, cargo, destination and route are omitted. Route targets use the last
  reported position; detached/dead scouts' delivered observations remain available.
- Only own orbital station internals and own delivered messages are exported.

Static orbital/geological observations follow the existing survey-level model.
Ownership is the last observed owner. Host
UI uses authoritative state because the host is trusted. No claim is made that
an untrusted host cannot inspect or alter a campaign.

## Remaining layers

- Append-only resolution ledger with accepted envelopes, result hashes and host
  version, for audit/replay and recovery after crashes.
- A transport interface and authenticated network exchange.
- Advanced traffic analysis and conditional emission/combat doctrines.
- Independent computer exploration, colonization, economy and military behavior;
  [controller integration](computer-players.md) is implemented.
- Diplomacy and retreat/battle doctrines beyond the current combat rules.
- Genuinely asymmetric primary racial traits.
