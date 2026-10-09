# Terraforming

Normal terraforming follows the Stars! physical-axis ladder. Research unlocks
capability; a colony must pay for and complete each unit of work in its production
queue. New games no longer expand racial environment ranges merely by researching
Biology. Pre-v59 saves retain their previous automatic adaptation bonus explicitly.

| Maximum change from natural value | Biology | Secondary field |
| --- | --- | --- |
| 3 | 1 | 1 |
| 7 | 2 | 5 |
| 11 | 3 | 10 |
| 15 | 4 | 16 |

The secondary field is Energy for temperature, Propulsion for gravity and Weapons
for radiation. Radiation-immune races do not need or use radiation terraforming.
The research catalog exposes both prerequisites and queues missing levels.

A unit costs **12 production** and no minerals, and moves exactly one normalized
environment axis by one point. This price is an initial Suns! economy adaptation,
equivalent to two factories' production cost, not the Stars! resource price.
Habitability is recomputed from physical conditions; one axis point does not imply
one habitability point. The resolver first maximizes habitability, then reduces
out-of-range distance and distance from racial optima. This lets work continue
when several hostile axes temporarily leave the same negative habitability.

The natural baseline is captured before the first completed change and persists
in saves. Repeating a project never resets the technology limit. Changes survive
abandonment and conquest. A new owner with terraforming capability can reverse
inherited changes towards its own optimum, including work back towards the natural
baseline when inherited offsets exceed its lower technology limit.

**Terraform** adds a finite batch. **Min Terraform / year** is a persistent rule
that works while habitability is negative, up to its annual unit limit, and stops
once the world is habitable. **Max Terraform / year** continues towards racial
optima up to technology and annual limits. An idle rule stays in the queue and
can react to later research. Surplus manual work with no eligible changes is
discarded without further charges. Work completes before end-of-year population
growth; it needs neither a shipyard nor a separate orbital installation.

The System environment panel displays current and reachable habitability, with
natural/current/reachable axis values in tooltips. Forecasts use current technology
and stellar conditions. Potential and yellow Habitability map markers require an
owned planet or a confirmed orbital survey; basic scan estimates do not reveal
hidden physical parameters or terraforming feasibility.

This is the colony-scale first step related to #56. Orbital adjusters, Total
Terraforming race traits, mirrors, magnetic shields and system-scale engineering
remain future work.

Sources: original [Stars! Player's Guide](https://www.bestoldgames.net/download/games/stars/stars-win-manual.pdf),
sections 6-14–6-20 and 7-9–7-10, and [Technical Reference Guide](https://www.bestoldgames.net/download/games/stars/stars-win-guide.pdf).
