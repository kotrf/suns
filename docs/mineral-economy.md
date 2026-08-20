# Mineral economy

Suns! models three bulk strategic minerals: **Ironium**, **Boranium**, and **Germanium**.

## Concentration

Every planet has an immutable geological concentration for each mineral, expressed as a percentage. Concentrations are deterministic from the galaxy seed and planet id, so they remain stable across saves and machines.

Stellar spectral class provides only a weak prior. Hot blue/white systems are biased somewhat toward Ironium, while cooler orange/red systems are biased somewhat toward Germanium. Random overlap deliberately remains large: spectral colour is a scouting hint, not a substitute for surveying a world.

## Extraction

Every settled world has baseline extraction capacity from its population. Yield is:

`extraction units × concentration / 100`

Baseline extraction units are:

`1 + population / 750`

A built **Mine** adds another `0.75` extraction units permanently. Because this capacity is still multiplied by mineral concentration, the same Mine is much more valuable on a geologically rich world than on a poor one. The colony UI therefore shows both current extraction and the marginal I/B/G gain from the next Mine before the player queues it.

A Mine costs `5` production and `1 I / 2 B / 1 G`. Mines do not alter or deplete concentration; they represent extraction infrastructure, not a finite geological meter.

## Stocks and cargo

Mined minerals accumulate in the planet's existing mineral stock. The same minerals may be loaded into fleet cargo, transported, unloaded elsewhere, or delivered with a colonization mission. Minerals continue to share cargo capacity with colonists.

## Production

Production has two independent constraints:

1. production points, represented by `remainingCost` and colony stockpile/output;
2. the required I/B/G material bill.

A construction item may reach zero remaining production points and then wait at the head of the queue until the colony has all required minerals. Minerals are consumed atomically when construction completes.

Factory mineral bill: `2 I / 1 B / 2 G`.

Mine mineral bill: `1 I / 2 B / 1 G`.

Ship mineral bills are derived from hull plus fitted components. This makes ship design affect industrial logistics as well as mass, fuel, cargo, sensing, and mission capability.

## Save compatibility

Save format v2 persists the Mine count on each planet. The v2 reader still accepts v1 saves; older planets simply load with zero built Mines.
