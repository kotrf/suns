# Multiplayer Turn Exchange

Suns! resolves a planning year from one authoritative `GameState` and a set of
`PlayerOrders`. Multiplayer therefore uses an authoritative host model: players
submit orders, the host validates all envelopes, resolves the year once, and
produces the next player-specific turn. Peers never merge competing copies of
the complete game state.

## First transport: order files

The desktop app can export and import `.sunsorders` files from the File menu.
An order envelope contains:

- a stable random campaign id;
- the planning turn number;
- a random token for the exact planning boundary;
- the issuing player id;
- that player's typed `PlayerOrders` and UI descriptions.

Campaign id, turn and token are persisted in `.suns` saves. The token changes
after a turn is resolved, while restarting a galaxy creates a new campaign id.
Import rejects a packet from another campaign, turn, planning boundary or
player before it can replace the local pending orders. The token prevents
ordinary stale-file mistakes; it is not yet authentication or a cryptographic
signature.

The order format deliberately does **not** contain `GameState`. An authoritative
save includes hidden planetary information, other players' orders and future
simulation state, so distributing it as a player turn would expose secrets.

## Transport boundary

`.sunsorders` is the serialized turn envelope, not PBEM-specific game logic. A
future directory watcher, HTTP client or hosted server can carry the same bytes
and feed the same validation path. WebSockets may later provide notifications
or chat, but turn resolution does not require a permanent connection.

## Planned layers

1. Introduce a fog-of-war-safe `PlayerView` data-transfer object and export a
   `.sunsturn` packet containing only knowledge available to its recipient.
2. Add a host inbox that collects one accepted envelope per player, shows who
   is ready, and passes the complete order set to `TurnProcessor`.
3. Persist an append-only resolution ledger: initial state identity, accepted
   order envelopes, resulting turn hashes and host version. This supports audit,
   deterministic replay and recovery.
4. Put file and network implementations behind one `TurnTransport` interface.
5. Add authenticated HTTPS exchange; use signatures or server-issued credentials
   so a player cannot submit orders as another player.

## Playing with an AI assistant

Once the player-safe `.sunsturn` packet exists, it can be attached to a chat.
An assistant can inspect the known empire state and return a `.sunsorders` file
for import. This works naturally as asynchronous PBEM. A chat session should not
be treated as a permanently connected autonomous client, so a hosted live bot
would use the same protocol through a separate service process.
