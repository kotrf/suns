# Unknown objects and scientific expeditions

This is the first persistent-landmark slice of #55 and the next field-science
slice of #59. Ancient probes and compact remnants are physical stationary objects,
separate from wormholes and planetary artifact sites. They are not pop-up rewards.

## Generation and knowledge

A galaxy of at least 16 stars contains up to `min(4, 1 + stars / 80)` objects.
Positions and kinds come from an independent seed-derived generator, so stellar
and planetary generation is unchanged. Placement is bounded, at least 30 ly from
stars and 60 ly from another object; crowded maps may contain fewer. Stable IDs,
positions and kinds are saved. Objects persist after every empire has studied them.
Older host/local saves seed the same sparse population once on migration to v79;
player-turn imports never generate hidden objects.

Active fleet scanners and built colony radars can detect objects within their
ordinary, interference-adjusted sensor range. Orbit-only scanners have a 1 ly local
investigation envelope. Passive/silent scanners do not actively survey objects.
A distant detection gives only a 20 ly cell center and a 15 ly uncertainty radius,
with no kind or scientific value. Within half the receiver range, capped at 40 ly,
the object is classified and localized. Each observation is an immutable packet
using the actual receiver's communications route. Only delivery updates headquarters
knowledge. Classification cannot be downgraded by a later distant observation;
routine same-quality updates refresh the measurement date without a new briefing.
A higher-quality old packet can still add classification of these static landmarks.

Raw objects, the generation seed, global allocation counter and pending evidence
are withheld from player exports. The map, object journal, messages, navigation
and computer empires all use received snapshots. Moving/deleted test sources and
changes to unobserved truth do not rewrite a dispatched report or exported packet.

## Expeditions

Use **View → Unknown objects / expeditions**. Select a fleet and a received row:
**Show area**, **Fly to object**, or **Fly and study**. The last uses an ordinary
coordinate route with **On arrival → Field research**; the same action is available
in the shared route editor. An unclassified area can be targeted without knowing
its contents. Existing command delay, fuel use, electronics and standing tasks apply.

A stationary fleet running Field research with active scanners studies an object
within 20 ly, further limited by its effective sensor range (1 ly for an orbit-only
scanner). It collects a finite objective once per empire: an Ancient probe gives
24 Electronics RP; a Compact remnant gives 32 Energy RP. This extends the existing
field-science ledger with a distinct stable-object key. Repeating work, sending a
second fleet or replaying a delivered study packet cannot farm additional points.
The body stays in the world. Data does not improve headquarters research until its
normal report is delivered, and already dispatched data survives source destruction.

The journal shows Unknown → Classified → Studied, observation/receipt years,
measurement age, uncertainty and known science objectives. Map diamonds/circles
show received coordinates, and Turn Messages → Exploration / Research retain the
reports. Selecting a message centers on its observed object area. History records
object reports and completed expeditions in the receipt year with their stable IDs.

Computer empires compare known unstudied contacts with ordinary stellar scouting,
budget fuel for a depot return, and use the same Move/Field research commands.
Remembered goals prevent repeated orders while acknowledgements travel. They stop
completed study only after receiving both the result and relevant fleet telemetry;
military goal cleanup preserves civilian expedition goals. No controller reads
hidden objects or receives free RP.

## Persistence and remaining scope

Save v79 preserves raw host objects, delivered/in-flight observations, science
claims, controller goals, messages and object history. Orders stay at v27: no new
order type or payload is needed. v78 host and player-turn migrations, earlier v77
reconstruction fixtures, invalid states and byte-identical exports after hidden
object changes are covered by persistence tests.

#55 remains open for moving/transient/rogue objects, hazards, unusual resources,
unique usable finds and richer discovery outcomes/reference integration. #59 still
includes physical sample cargo, research requirements based on observations, and
salvage-versus-retained-artifact decisions. Current wreck novelty and other finite
stellar/planetary/wormhole science rules are unchanged.
