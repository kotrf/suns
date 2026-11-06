# Asymmetric empires (#50): next-trait design

Status: design proposal, 6 November 2026. The user requested continuing the
recorded queue after #46; #50 is the next open, unfrozen task. Stargate Specialist
is already implemented, including scheduled gate programmes. The next primary
trait has not been chosen; a selection question is pending. Habitat Civilization
is the recommended candidate below, not an implemented or approved rule set.

## Existing foundation

- Generalist and Stargate Specialist are playable. Habitat Civilization and
  Remote Logistics Specialist have stable reserved IDs and remain unavailable.
- Environmental presets, hull families, engine access, mining access, racial
  workforce rates, growth and research costs are separate settings. Hull access
  does not imply the corresponding full Stars! primary racial mechanics.
- Existing orbital habitats have a separate population, paid upkeep, emergency
  reserves, cargo endpoints, growth, received evidence and capture/defense rules.
  They are late-game installations: Construction 18 / Energy 16 / Biology 14,
  4,800 work and support from a Starbase or Fortress with a shipyard.
- Expedition bases provide mobile refueling, docking and repair. Independent
  orbital shipyards provide staffed fixed manufacture. Neither is a mobile
  residential civilization with a self-sustaining starting economy.

The current main is ea7749f (#281 merged, #46 completed); its separate CI passed.
Current formats are save v114 / orders v49 / portable race v3. Earlier numbers in
the thematic habitat documents describe their original slices; current economy
constants and [economy-rebalance.md](economy-rebalance.md) take precedence.

## Candidate comparison

| Candidate | Strategic difference to design | Existing support | Main missing loop |
| --- | --- | --- | --- |
| Habitat Civilization | Orbital settlement and infrastructure replace favorable surface conditions as the main expansion constraint | Habitats, orbital cargo, independent yards, paid station support | Affordable founding, population growth, mineral extraction and construction from the first turn |
| Remote Logistics Specialist | A distributed extraction fleet and delivered supplies replace a large part of local colony development | Remote mining, finite cargo/fuel, ordinary routes, mining hull access | A distinct production/extraction tradeoff beyond the existing ARM setting, plus a viable starting fleet |

Either candidate uses ordinary free flight and delivered commands. Neither
requires reopening frozen transport-network #53 or exotic-material #63 work.

## Recommended first prototype: stationary orbital civilization

This is a proposed first playable slice, not the full migratory-civilization
roadmap. Fixed orbital settlements come before moving residential habitats.
The first implementation must include all of the following together:

1. **A complete start.** Keep the normal 25,000-person starting total, with an
   explicitly defined orbital home, actual starting infrastructure and stocks.
   The player must be able to grow, mine, research and manufacture without first
   unlocking the ordinary late-game habitat technologies.
2. **Paid expansion.** A transport brings real colonists and a paid founding
   capability. The founding package must account for housing, initial support
   and supplies, rather than conjure a late-game station from an ordinary colony
   module. Both another suitable world and a hostile-surface world must be
   playable expansion choices with their real costs.
3. **A closed material economy.** Orbital residents currently do not operate
   surface installations. Specify whether native extraction uses real robotic
   ground mines operated from orbit or a separate paid extraction installation.
   Preserve actual deposits, concentrations, stocks and cargo; there is no free
   mineral income or implicit supply service.
4. **An explicit economic tradeoff.** Decide the role of surface residents and
   factories. If orbit is the main economy, define its production inputs and
   opportunity cost relative to surface factories, not just a growth multiplier.
5. **Growth that belongs to the race.** The proposed native orbital population
   uses the race designer's growth choice and a specified crowding rule. Ordinary
   empires' existing late-game habitat growth remains a separate rule. Capacity,
   minimum sustainable founding population and initial support costs must fit
   the starting/expansion budget together.
6. **Physical vulnerability.** Support consumes real production/minerals. Station
   loss, reserves, attrition and evacuation reuse the established paid habitat
   model. Define which early support structure suffices; an unresearched generic
   Starbase is not a viable implicit starting prerequisite.
7. **Capture and resettlement.** Preserve population physically through ordinary
   invasion and cargo operations. Decide how a native owner uses a captured
   surface colony and how a generalist supports a captured native habitat. No
   unexplained conversion, teleportation or deletion of its residents.

These are design requirements, not locked balance numbers. Existing habitat
upkeep (2 production plus 1 kt each I/B/G annually), three reserve years and
2,000,000-person capacity are reference values, not an automatic early-race
endowment. The implementation PR must show founding cost, capacity, growth,
mining and support as one viable budget before enabling the reserved option.

## Implementation map

| Area | Existing entry points | What the chosen slice must address |
| --- | --- | --- |
| Identity and creation | `PrimaryRaceTrait`, `generate_campaign`, `RaceSetupWidget`, portable race reader/writer | Playable validation, understandable race rules, start/restart and independent multiplayer race files |
| Population and output | `orbital_habitats.cpp`, `production.cpp`, `economy.cpp` | Real growth/output/maintenance cycle and the explicit surface/orbit workforce policy |
| Founding and construction | Colonization in `turn_processor.cpp`, `planetary_engineering.cpp`, stations/designs | Paid native founding and support, queue eligibility, no free late-game research |
| Ownership and movement | Cargo/orbital endpoints, ground invasion, independent-base lifecycle | Correct counting, capture, evacuation, split/merge and transport losses |
| Knowledge and control | PlayerView, survey/habitat evidence, received station fits, command packets | Stale evidence remains stale; no host-only completion, stock or owner lookup in UI/AI |
| Persistence and UI | Save extensions, orders, race files, setup, System Details and cargo | Explicit version decisions, useful labels, deterministic replay and responsive algebraic estimates |
| Computer play | Existing ordinary orders and received colony/fleet planning | A viable paid starter/expansion policy through PlayerView, without a separate omniscient economy |

## Acceptance for the implementation PR

- A new native campaign sustains its capital, builds a first paid colonizer and
  establishes a second working colony using real residents and minerals.
- A favorable surface and an unfavorable surface expose the intended strategic
  difference; ownership alone does not produce resources.
- Loss of support, no resupply and evacuation have bounded physical outcomes.
  Capturing either colony type preserves the agreed ownership/population rules.
- Generalist and Stargate Specialist campaigns retain their existing behavior.
  Human and computer players pay the same costs and receive the same evidence.
- Race design, imported multiplayer races, restart, host save/load, player exports
  and turn packets preserve the chosen trait without leaking hidden state.
- UI selection/cargo/queue actions retain the minimal-estimate performance
  contract; no annual live simulation or new polling is added.
- Comparative starter scenarios record population, stock, output, expansion time
  and maintenance pressure. They establish a prototype baseline, not a claim of
  finished competitive balance or manual playtesting.

## Scope boundary

The pending selection supplies the next trait's scope; its implementation will
include the starting economy and acceptance cases together. This design does
not select new balance values, enable either reserved ID or change current games.
Moving residential arks, shipboard manufacture, self-replication, new
transport automation and system engineering remain separate later slices.
#50 remains open after a first prototype for further traits and comparative play.
