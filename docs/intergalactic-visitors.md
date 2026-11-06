# Intergalactic visitors — encounters and offer profiles (#46)

A rare neutral visitor crosses the map along a physical trajectory. A
player must detect its broadcast, bring a loaded ship to a timed rendezvous and
complete the exchange before it leaves. It is not a playable empire or a popup.

## Try it now

Launch `suns --visitor-demo`, or choose a profile in **Visitor encounter demos**
from the menu bar. The console flag uses Freight envoy. The menu asks before replacing the current game; save first.
The default prepared single-player encounter starts in year 60 with a courier
at Earth, 50 kt Germanium in its ordinary hold, full tanks and a received visitor
broadcast. Freight demonstrates a horizontal track, Prospector a vertical one,
and Shieldwright a diagonal one. The other demos load their own advertised mineral mix.
It does not alter generation or grant resources in ordinary campaigns.

In **Intergalactic visitors**, select the contact/courier, keep Warp 8 and choose
**Intercept and exchange**, then End Turn. The courier meets the visitor during
the year, spends fuel and Germanium, and the visitor continues on its trajectory.
The local report delivers the blueprint. Inspect **Multi Cargo Pod** in Designer.
Cancel/decline sends a normal delayed cancellation instead of accepting a trade.
In a real campaign load cargo using the normal Cargo controls first.

## Three offers, ordinary cargo and manufacture

Each visitor advertises one fixed request and one blueprint. A generated visitor
chooses one of these three profiles using its existing independent seed/year RNG;
the profile is saved and never rerolled at contact or per player. Demos load the
matching 50/60/65 kt of ordinary minerals into the same 70 kt courier.

| Profile | Request | Blueprint | Existing effects |
| --- | --- | --- | --- |
| Freight envoy | 50 kt Germanium | Multi Cargo Pod | 250 kt cargo, 50 armour |
| Prospector | 40 kt Ironium + 20 kt Boranium | Alien Miner | 10 remote mining units |
| Shieldwright | 25 kt Ironium + 25 kt Boranium + 15 kt Germanium | Langston Shell | 65 armour, 125 shields |

The first physical customer pays **all** requested mineral amounts together.
If even one is short, nothing is deducted. Other cargo is retained. Each visitor
still has one offer for the whole galaxy: a stale available offer does not reserve
it, and another empire can arrive first. FleetId breaks simultaneous contact ties.
One fleet cannot be charged twice for the same mission. Duplicate receipts do not
grant a second unlock; repeated copies of a blueprint add no further benefit.

A gift grants acquisition only, never research, a free ship or fitted equipment.
Normal technology requirements, hull slots, resource/mineral costs and manufacture
still apply. The roster uses existing Mystery Trader equipment with implemented
cargo/mining/armour/shield effects, rather than new commodities or free combat
ships. These prices are a starting balance, not a claim of completed playtesting.
The short greeting, portrait and requested/offered terms belong to the selected
received contact; the UI does not expose the hidden physical roster. All three
profiles have prepared demos, with the same confirmation before replacing a game.
The card labels its price/blueprint as **last known terms**, with a separate
received available/withdrawn/departure-observed status and the announced departure
year. Passing that year closes the announced window without inventing an observed
departure. A completed exchange no longer leaves an invitation to trade in the
card, while its historical terms remain visible.

## Rarity, movement and contact

Ordinary galaxies with at least 16 stars become eligible after year 60. A separate
seed/year generator has a 1/80 chance per eligible year, with at most one visitor
in flight. A new track starts 150 ly outside any of the four edges. A seeded
interior point in the central half of the real galaxy defines its straight ray;
the ray is extended to a different edge of the rectangle enlarged by 150 ly.
Thus oblique routes cross useful interior space rather than grazing a corner,
including in strongly rectangular galaxies. Geometry uses the actual validated
width and height (normal campaigns allow 100–10000 ly per side), with no extra
minimum-size padding. The nominal speed budget stays
121 ly/year: duration is `ceil(track length / 121)`, and the constant velocity
reaches the outer exit at that announced whole-year boundary. Most tracks remain
straight; a rare visitor can change course once as described below. Stable IDs,
positions, velocity, arrival/departure years, the physical customer and the private
course-change schedule are saved. The historical visitor roster remains bounded at 128.

Only newly generated tracks use these rules. An already saved pre-v114 physical
visitor, received observation or incoming mission keeps its exact position, velocity
and announced window; no migration or course change is scheduled retroactively.
Current formats are save v114 / orders v49 / race v3. Normal profile selection is independent of the
entry side; the three prepared demos choose directions purely for demonstration.

### Rare course changes

At generation, one in ten visitors is eligible for one course change. Its private
schedule chooses an integer-year boundary within the real galaxy, with
at least two years left on the original route; if no such boundary exists the
visitor stays straight. This is a per-visitor choice, not an annual chance that
would accumulate on long crossings. Existing demos and pre-v114 visitors stay
straight. This starting balance is our implementation, not a verified Stars! table.

After completing the elapsed year's movement and encounters, the visitor turns
20–45 degrees left or right. The choice uses only galaxy seed and visitor identity,
never player/fleet locations or cargo. The new ray is clipped to the expanded
galaxy boundary; its remaining duration is rounded up to whole years and velocity
adjusted down to reach that exit exactly, without exceeding the previous speed.
The new announced departure can be earlier or later. Each year's actual movement
remains linear, with no teleportation, second turn, or second simultaneous visitor.

The scheduled date is host-only. Until an actual receiver measures the changed
track, HQ's dated forecast and already dispatched missions retain the old velocity
and announced window. A courier with its own active scanner can update onboard
navigation; a newer HQ observation alone does not rewrite its existing programme.
A missed stale intercept is legitimate. When a changed sighting reaches HQ,
a dated **changed course — new track received** Turn Message explains that earlier
predictions may miss. Repeated sightings do not repeat that notice. At the same
year boundary, a measured post-turn revision wins over an older contact receipt;
the payment and immutable gift receipt still retain their ordinary semantics.

The courier's navigation solves an intercept from its received track and chosen
Warp. It then uses ordinary movement, fuel limits, mines, radiation, damage and
combat. The existing continuous encounter solver checks both actual within-year
segments at the same time with the normal 0.01 ly envelope. Crossing a stale
coordinate or crossing the visitor's path at a different time is not contact.
The courier stops at contact; the visitor continues for the rest of the year.
Faster visitor speed never tows a courier or waives its fuel cost.

No reachable intercept, an expired announced window or unusable Warp ends the
programme with a receipt. Fuel exhaustion uses ordinary movement/stall rules.
There is no automatic supply search. Cargo is validated again at physical contact.

## Knowledge and delayed control

Ordinary active scanners and built colony radars receive the cooperative visitor
broadcast within their actual interference/medium-adjusted ranges. The broadcast
contains a measured position, current segment velocity, announced departure and current
offer availability and profile. The profile also travels in navigation tracks,
telemetry, pending commands and exchange receipts. A delivered exchange event
keeps its own profile, so later contacts cannot change its price/gift text.
It is a separate contact class from stationary unknown
landmarks. Its immutable report travels from the actual receiver using ordinary
communications; passive/silent receivers do not invent active measurements.

HQ stores only delivered contacts. The panel and map show measurement/receipt
years and explicitly labelled trajectory forecasts. Hidden movement, another
customer, undelivered evidence and departure outside sensor range do not revise
old snapshots. Observed departure and an announced/predicted departure are shown
separately. Neutral visitors and private packets are excluded from player files.

The encounter command freezes HQ's delivered track at issue time. Receipt starts
onboard navigation; an intercepting ship can refresh only from its own real active
observations. It does not borrow newer HQ knowledge or another ship's notebook.
The programme continues without annual HQ orders. Cancellation and manual routes
take effect on delivery; cancellation supersedes older in-flight launches using
the ordinary stop barrier. Accepted manual routes/tasks and probe programmes replace
visitor navigation; split/merge/gate operations clear its physical mission.
Incoming commands retain the ordinary FleetId rules.

The first successful physical exchange consumes the offer and cargo immediately.
The blueprint is acquired only when the immutable exchange receipt reaches HQ.
Already transmitted receipts survive loss of their source; silent outboxes wait
for an actual transmission and disappear if their unsent source is lost. Newer
contact dates cannot be overwritten by older delayed measurements.
An exchange refusal at physical contact also delivers the measured offer state,
even with passive/silent scanners. A navigation failure carries only its existing
track; its later mission-status date never refreshes the observation date.

Computer empires may divert an idle, currently confirmed, connected unarmed
transport using PlayerView, delivered contacts and conservative fuel budgeting.
The same existing courier policy loads every missing requested mineral at a
received owned colony through the ordinary cargo order and issues the same
encounter order. It skips a profile whose blueprint is already acquired; owning
one profile does not suppress the others. They receive no free cargo or blueprint.
Broader ship construction/staging for visitors remains future policy work.

## Persistence and remaining scope

The original save **v89** introduced bounded **VISI** data after v88; orders
**v35** introduced encounter and cancellation. Current profiles use save
**v114 / orders v49 / race v3**: VISI records persist the profile in physical
visitors and every observation/mission copy; visitor GameEvents also retain it.
v114 adds the private scheduled turn and completion flag, a measured course revision
in every observation/mission copy, and the new received course-change event. Older
records default to no scheduled change and the original measured revision. A real
v113 fixture, written by unchanged b7ac910, verifies profile-bearing legacy tracks.
All v112 and older visitor records load as the original Freight offer, without
changing cargo, unlocks or incoming orders. Order payloads remain unchanged. Physical visitors, received contacts, in-flight reports,
acquired blueprints, physical/telemetry missions and incoming commands remain
separate. v88 and earlier load with empty visitor state.

Tests cover the complete demo, timed contact, fuel and exact cargo payment,
insufficient cargo, competing customers, deterministic generation, immutable
delayed commands/reports, hidden-state isolation, cancellation/manual control,
paid AI use, full save continuation, PBEM and UI visibility.
Profile tests cover exact/all-or-nothing I/B/G payment, three acquired blueprints,
research gates, delayed immutable gifts, duplicate receipts, received-only AI
cargo choices, profile-bearing host/PBEM/mission/event round trips, invalid profile
enums, duplicate unlocks and the real v112 fixture from the unchanged f471c34
writer (`tests/fixtures/visitor_v112_writer.cpp`, hex-encoded fixture).
Review regressions cover delayed physical refusals, unchanged evidence age after
navigation failures, invalid VISI report/gift enums and every truncated prefix of
a VISI payload whose outer length remains valid.

The trajectory tests replay seeded generation across nine square/rectangular
sizes (including 100×100, 10000×100 and 100×10000, up to 100:1 aspect ratios), checking all four entry sides, a different
bounded exit, interior crossings and finite legal speed. The old horizontal-only
source fails those acceptance checks. A generated oblique track survives save/load
and byte-identical real-turn continuation; an unseen changed velocity does not
change a player export. The vertical/diagonal demos exercise the normal timed
encounter, fuel, cargo, acquisition, delayed reports, AI and UI paths.

Course-change checks replay scheduled turns across all nine map shapes, verify
the unchanged elapsed segment, a 20–45° heading change, non-increasing speed,
bounded whole-year exit and no second turn. A real two-year pursuit pays on the
straight control route and misses the changed route while blind; HQ and onboard
tracks refresh only after the appropriate measurement/delivery. Equal-year contact
receipts cannot roll back the new revision. Host continuation before/after the turn
is byte-identical after reload; the private schedule cannot alter player exports.
The received notice, profile and measured revisions round-trip in knowledge,
pending reports, navigation/telemetry/commands, player files and Turn Messages.

This slice completes the original #46 acceptance: rare deterministic outsiders,
received-only discovery, real timed interception, paid varied gifts, concise
encounters/portraits, decline/cancellation, failure and departure reporting.
The PR closes #46 after review and merge. Strategic AI construction/staging is a
separate deferred enhancement, not a requirement of the original visitor issue.
Visitors remain neutral and cannot be attacked. General moving/transient anomalies
remain #55; frozen #53/#63/evacuation are not resumed.
