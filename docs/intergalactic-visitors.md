# Intergalactic visitors — first playable #46 slice

A rare neutral visitor crosses the map along a fixed physical trajectory. A
player must detect its broadcast, bring a loaded ship to a timed rendezvous and
complete the exchange before it leaves. It is not a playable empire or a popup.

## Try it now

Launch `suns --visitor-demo`, or choose **Start visitor encounter demo…** from
the menu bar. The menu asks before replacing the current game; save first.
The prepared single-player encounter starts in year 60 with a courier at Earth,
50 kt Germanium in its ordinary hold, full tanks and a received visitor broadcast.
It does not alter generation or grant resources in ordinary campaigns.

In **Intergalactic visitors**, select the contact/courier, keep Warp 8 and choose
**Intercept and exchange**, then End Turn. The courier meets the visitor during
the year, spends fuel and Germanium, and the visitor continues on its trajectory.
The local report delivers the blueprint. Inspect **Multi Cargo Pod** in Designer.
Cancel/decline sends a normal delayed cancellation instead of accepting a trade.
In a real campaign load cargo using the normal Cargo controls first.

## One offer and one useful gift

The first physical customer exchanges **50 kt Germanium** for a **Multi Cargo
Pod blueprint**. Each visitor has one offer for the whole galaxy: another empire
can arrive first, and a stale available offer does not reserve it. Insufficient
Germanium consumes nothing and produces a delayed failure receipt. FleetId breaks
ties between contacts at the same physical time. One fleet cannot be charged
twice for the same mission.

This reuses the existing Mystery Trader equipment rather than inventing another
commodity. Multi Cargo Pod adds **250 kt cargo and 50 armour**, weighs **9 kt**,
and costs **25 production / I-B-G 12/0/3** per installed unit. The blueprint
unlocks access; the existing Energy 5 / Construction 11 / Electronics 5
requirements, hull slots, mineral/production costs and ordinary manufacture still
apply. It grants no free ship or installed equipment. This is a substantial late
logistics option, not a random direct combat victory. Repeated copies of this
first blueprint give no additional unlock.

## Rarity, movement and contact

Ordinary galaxies with at least 16 stars become eligible after year 60. A separate
seed/year generator has a 1/80 chance per eligible year, with at most one visitor
in flight. Horizontal paths start 150 ly outside either map edge and leave at
the opposite outer edge. Duration is `ceil((width + 300) / 121)` years; speed is
the corresponding constant velocity, normally above player Warp 10. Stable IDs,
positions, velocity, arrival/departure years and the physical customer are saved.
The first slice bounds the historical visitor roster at 128.

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
contains a measured position, constant velocity, announced departure and current
offer availability. It is a separate contact class from stationary unknown
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
They load missing Germanium at a received owned colony through the ordinary cargo
order and issue the same encounter order. They receive no free cargo or blueprint.
Broader ship construction/staging for visitors remains future policy work.

## Persistence and remaining scope

Save **v89** appends bounded **VISI** data after v88; orders **v35** adds encounter
and cancellation. Physical visitors, received contacts, in-flight reports,
acquired blueprints, physical/telemetry missions and incoming commands remain
separate. v88 and earlier load with empty visitor state.

Tests cover the complete demo, timed contact, fuel and exact cargo payment,
insufficient cargo, competing customers, deterministic generation, immutable
delayed commands/reports, hidden-state isolation, cancellation/manual control,
paid AI use, full save continuation, PBEM and UI visibility.
Review regressions cover delayed physical refusals, unchanged evidence age after
navigation failures, invalid VISI report/gift enums and every truncated prefix of
a VISI payload whose outer length remains valid.

#46 stays open for additional request/gift profiles, richer dialogue/portraits,
alternative trajectories and strategic AI preparation. Visitors are neutral and
cannot be attacked in this slice. General moving/transient anomalies remain #55.
