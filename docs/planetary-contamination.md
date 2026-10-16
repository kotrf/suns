# Residual contamination, cleanup and dated aftermath

This continues #106 after the infrastructure/reconstruction stage in #209.

## Physical hazard

Neutron Bombs add 4 contamination per fitted bomb/year; Enriched Neutron Bombs
add 8. This is an explicit Suns! extension to their reference equipment ratings,
shown in the designer/research equipment description. Colony defense coverage
attenuates the dose by `(1 - coverage)`, rounded up. The physical level is capped
at 100. Other current bombs add none; conventional bombardment does not acquire
a new environmental punishment. Armed hostile orbital stations still block
bombardment under the existing combat rule.

Contamination is independent from natural and terraformed temperature, gravity
and radiation. It survives conquest, abandonment and recolonization. Each annual
resolution removes 2 points everywhere, including unowned worlds, before movement,
new bombardment and production. Newly deposited contamination therefore does not
immediately decay in the same year. Without another attack, even a saturated
world clears naturally within 50 years.

For non-immune races the temporary Hab penalty is `ceil(contamination / 5)`, at
most 20 points. Capacity and growth use that derived Hab normally; there is no
second direct population-loss roll. Radiation-immune races ignore this penalty.
Physical environment/natural baselines remain distinguishable and ordinary
terraforming does not erase the hazard.

## Paid cleanup

Production offers **Clean contamination**, a one-time batch only. Each unit costs
6 production and B/G 1/1 kt, removing up to 10 points. Normal priority, research
allocation, partial work, mineral shortages and completion-time mineral spending
apply. A final smaller cleanup still has the same unit cost. Queued units reserve
current work, preventing duplicate overbooking. Unneeded work retires without
further charge if natural decay or an earlier project has already cleared it;
previously spent partial production is not refunded. Cleanup is planet-specific,
so annual rules and colony-development templates cannot create it.

The shared forecast includes annual decay, the same cleanup resolver and resulting
population growth, assuming no new attacks. Work that will become unnecessary is
labelled **Unneeded** with its retirement year. Owner colony/Production panels show
level, Hab penalty, decay, natural clearing time and project costs. Computer
empires choose the same ordinary paid orders when their queue is empty; they do
not bypass cost or overwrite existing work. Delivered completion reports add
receipt-year cleanup milestones to History.

## Observer knowledge

Bombardment reports carry a frozen aftermath snapshot: exact for the owner at
observation time, estimated for the attacker. Other active scanner fleets can
measure aftermath while in orbit; passive, off-orbit and heavily jammed
(interference at least 50%) observers cannot. Only planets with actual bombardment
history produce these measurements. The fastest real receiver wins each
planet/year, with stable fleet-ID tie-breaking.

Foreign ruins use 5-installation intervals and contamination uses 10-point
intervals, bounded at 100; zero is reported as zero. These are deterministic
estimates, not current host counters. Every snapshot records observation and
receipt years. Packets use the observer's actual transmission/communications
route, and a bombardment report can wait aboard a passive bomber until it
transmits or exchanges reports locally. Source movement/destruction, later cleanup
or attacks do not rewrite an in-flight snapshot. Only delivery updates the
observer's knowledge; older packets cannot roll it back. Unchanged orbital
estimates refresh their dates without producing a briefing every year.

Turn Messages classifies aftermath reports as Combat and selects their planet.
The colony panel shows last-known ranges, observation/receipt years and age.
Player exports keep only delivered evidence and withhold current foreign physical
counters and pending reports. Hab calculations likewise use delivered contamination
estimates rather than current hidden contamination. A foreign-projection marker
prevents stale ownership intel from turning an old estimate into local truth.
History records newly delivered aftermath/cleanup milestones in the receipt year.

Save v78 / orders v27 preserve physical state, partial cleanup, delivered and
in-flight evidence, messages and milestones. v77 saves retain their reconstruction
state and start with no contamination or invented observations. Earlier save/order
formats remain readable through their existing migrations.

## Remaining #106 scope

Biosphere damage, biological/ecological weapons, explicit scorched-earth choices,
technology-dependent cleanup/orbital assistance and campaign balance remain later
stages. This change does not close the umbrella issue.
