# Orbital stations and colony defense

Stations are separate player-owned objects with stable IDs, a planet, a fitted
hull, components, service modules and persistent damage. One station occupies a
planet's orbit. Homeworlds start with an empty basic dock; new colonies do not.

## Fitting and production

Select an owned colony and use **Station…** to inspect, build or refit its
station. The dialog shows armor, shields, damage, technology requirements and
production/mineral costs. Equipment choices reuse the ship catalog and its
technology/racial access gates. Stations accept beams/torpedoes, armor/shields
and combat electronics. Engines, bombs, scanners and cloaks are excluded.

These hulls and prices are initial Suns! balance, rather than reproduced Stars!
station statistics:

| Hull | Construction | Weapon / defense / electronics slots | Base armor | Production | I / B / G minerals |
| --- | --- | --- | --- | --- | --- |
| Orbital Dock | 0 | 2 / 2 / 2 | 400 | 24 | 12 / 6 / 8 |
| Starbase | 9 | 6 / 6 / 4 | 1600 | 160 | 70 / 40 / 60 |
| Fortress | 16 | 12 / 12 / 6 | 6000 | 500 | 200 / 160 / 180 |

Fitted components add their ordinary production and mineral costs. Shipyard and
refueling services can be toggled; removing them gives no cost refund. A refit
pays the full fitted build price, preserves the station ID and repairs its hull.
The existing station continues operating until completion. Minerals are charged
on completion, through the normal colony queue. A missing/destroyed/foreign
refit target discards the project without rebuilding a station or charging more
resources. One station project may be queued per colony; cancel it before
replacing it. A partly completed project retains the usual cancellation policy.

Shipyard service permits ship production and repairs docked friendly fleets by
20 damage percentage points per year. It does not automatically repair the
station itself. Refueling service supports dockside and arrival refueling.
Destroying a station immediately removes both physical services. Surface cargo
transfer remains a colony operation.

Any hostile station blocks colonist landing and colony founding, even without
weapons. It must be destroyed before a ground invasion; station ownership is
not transferred by surface conquest. This Stars!-style orbital-control rule
was explicitly accepted on 18 October 2026. [Ground invasions](ground-invasions.md)
describes manual versus arrival timing and delayed blocked-landing warnings.

Older saves may contain a station above another player's colony. Its owner and
ID remain intact; it provides no shipyard, refueling, gate or engineering support
to the colony owner, and ordinary combat must clear the orbit before replacement.
Persistence accepts those legacy states with valid station identities and
owner/planet references. Capturing orbital assets is not implemented.

## Station combat and knowledge

Hostile armed encounters along fleet movement include stationary stations.
A station can intercept an unarmed bomber, and an armed fleet can attack an
unarmed dock. Stations remain fixed at the tactical origin; fleets begin one
tactical square away so short-range guns can defend orbit. Fleet-only battles
retain their previous four-square starting radius. Weapons, shields, initiative,
computers, jammers and dampeners use the shared deterministic combat resolver.
Armor damage persists until refit; shields recharge at the next battle.

Manual founding/landing cannot bypass any hostile station, including an unarmed dock. An armed hostile station must be neutralized before bombardment. Surviving
bombers retain their mission and can bomb after the station is destroyed. Surface defenses do not shoot
at ships and do not substitute for an armed station.

Battle packets identify stations separately from fleets/designs and preserve
their names and casualties. Stations lost are counted separately in History →
**Station losses (stations/year)**, at the report receipt boundary. Uninvolved
players receive no station fit or battle result. While the owner's report is in
transit, a host-only snapshot preserves the last confirmed station for its UI
and player export; it has no physical services. Player exports omit that host
ledger, undelivered reports and foreign stations/defenses.

## Surface defenses

**Energy 1** unlocks Colony defense and Auto defenses in the Production batch
selector and empire templates. Each unit costs **5 production, I 2 / B 1 / G 1**;
the maximum is **100**. Annual rules remain in the queue at the cap and can
replenish losses later. The colony panel and station dialog explain coverage.

Population protection is `1 - 0.97^defenses` (about 95.2% at 100). Infrastructure
protection is half that percentage. Protection reduces both percentage-based
bomb kills and minimum kills, and reduces installation casualties. Bombs can
destroy defenses: up to one third of installation casualties, rounded up, hit
defenses; remaining casualties use the existing factory/mine distribution.
Protection for each bombing fleet is calculated before that fleet's casualties,
so successive attackers face depleted defenses. The ordinary immutable
bombardment summary includes defenses among lost installations.

These are initial balance rules for playtesting. Planetary scars/restoration
have their own production stages; diplomacy and retreat doctrines remain later
work. Paid finite gates and trait-exclusive Network/Infinity fittings are
described in [Stargate Specialist](stargate-specialist.md).

## Persistence

Save **v65** adds station fits/damage, queued refit targets, colony defenses,
station-loss history and undelivered station contacts. Orders **v20** adds fitted
station construction/refit and defense batches/templates. Existing readers
remain; all PBEM participants should use the same build.

## Orbital habitat integration (#56)

Orbital habitat structures persist separately from the supporting station. A friendly Starbase/Fortress shipyard supports construction and occupied operation; its loss starts finite emergency reserve use rather than deleting residents instantly. No extra armed station or new station-capture rule is added. See [orbital habitats](orbital-habitats.md) for the complete rules and save v94 / orders v38.

## Independent orbital founding (#54 / #61)

[Independent orbital shipyards](independent-orbital-shipyards.md) add a transported
constructor permanently anchored at a planet without surface colonization. Real
workers, delivered materials/fuel, separate manufacture and fitted radiation
protection are required. It shares the single orbital station slot and reuses
the anchored hull/combat/contact mechanisms; it is not a flying shipyard.

An evacuated independent yard can now be dismantled into a real recovery
transport. Over an owned populated colony it can instead be replaced by one
full-price ordinary station project: the old hull is removed before normal
construction starts, so services are unavailable during rebuilding. Stocks,
fuel, partial material recovery, delayed cancellation and report rules are in
the linked independent-yard document. Station capture is unchanged and remains
outside this slice.
