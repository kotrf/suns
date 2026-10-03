# Transient natural wormholes

Natural WHs are rare opportunities, independent of ordinary Warp flight and future
constructed stargates. Starting with year 5, the default host policy has a 2.5%
chance per year to create a pair, with at most three active pairs. Lifetime is
5–30 years; stable pairs tend to live longer. All balance parameters live in
`GameState::wormholeRules` and are saved with the campaign.

Each mouth has a stable ID, its own signature and an independently seeded drift
direction. Unstable pairs have larger drift and a chance of relocating a mouth.
Both mouths move even when nobody observes them. Pair creation, movement and
transit outcomes use integer seed mixing and explicitly converted unit samples,
without implementation-dependent random distributions or mutable RNG streams.

## Detection and knowledge

Ordinary fleet scanners and colony sensors detect strong signatures across their
normal range. Faint mouths use 45% of that range. Weak mouths require an Anomaly
Detector and use 35% of its range. The detector unlocks at Electronics 6, weighs
20 kt, costs 12 production and I 4 / B 3 / G 9, and provides a 200 ly ordinary
sensor. It occupies a general slot.

A distant contact is an unclassified spatial anomaly. Close observation (within
35% of its detection range), or a detector, classifies stability as unstable,
variable or stable. These are qualitative estimates; lifetime remains unknown.
Detections along a movement segment and stationary coverage both stage reports
through the existing finite-speed communications model.

Only delivered reports update owner knowledge: mouth ID, last observed position
and year, qualitative stability, observed collapse and any experimentally learned
link. Discovering one mouth never reveals the other. An unobserved collapse does
not remove a marker or silently update its coordinates. A later observation of
collapse produces a delayed report, including when the discovery report is still
in flight. Player-turn exports strip physical pairs, generation seed/counter,
pending observations and undelivered transit details.

## Explicit entry

Open **View → Spatial anomalies & wormholes**, select a fleet and contact, then
use **Fly to anomaly** to investigate. A classified contact enables **Enter
wormhole**. Entry is a terminal `EnterWormhole` arrival action carrying the mouth
ID, serialized in the normal movement order. It cannot repeat or precede later
waypoints. Ordinary proximity or passing through a mouth never initiates transit.

The course begins at the last reported coordinates. Onboard sensors may reacquire
the actual moving mouth locally and update the approach. They do not obtain its
current position outside their detection range. Reaching a stale coordinate or a
collapsed mouth yields an entry-missed report instead of teleporting.

At entry the default whole-fleet loss probability is
`0.01 + 0.12 * (1 - stability)`. An equipped detector multiplies this by 0.65,
with the 1% minimum retained. Hull mass and additional mitigation equipment are
future extensions. Survivors emerge at the opposite mouth's current position;
its next location can drift or relocate. Their emergence report reveals the exit
and last known link only when it reaches the owner. The route ends on either
outcome. Route forecasts never run the hidden transit lottery or expose the exit.

## Missing fleets

A failed transit destroys the authoritative fleet immediately. The owner retains
a knowledge-only copy of its last confirmed contact until a loss assessment can
be made. Entry and emergence reports have independently computed communication
latencies; an emergence report can arrive before the entry report.

After a delivered entry, the owner waits for emergence. A conservative timeout
uses entry-report latency plus the whole-map light-signal bound and two grace
years, independent of the secret outcome or exit. With no emergence report the
contact becomes **OVERDUE / NO CONTACT**, then **PRESUMED LOST** five years later.
The latter removes the retained contact from the active fleet view and statistics.
An eventual emergence report can still supersede that assessment. Owner packets,
map markers and empire aggregates do not reveal authoritative destruction early.

Save format 54 and turn-order format 12 preserve mouths, policy, pending reports,
knowledge, transit contacts and explicit entry actions. Earlier formats remain
readable and start without WHs. Regression coverage includes detection strength,
independent hidden drift, stale-coordinate misses, local reacquisition, successful
and failed transitions, timeouts, observed/unobserved collapse, deterministic
generation, byte-identical player exports under hidden changes, serialization
and the offscreen anomaly/entry UI.
