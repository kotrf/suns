# Mineral economy

Suns! models three bulk strategic minerals: **Ironium**, **Boranium**, and **Germanium**.

## Concentration

Every planet has an immutable geological concentration for each mineral, expressed as a percentage. Concentrations are deterministic from the galaxy seed and planet id, so they remain stable across saves and machines.

Stellar spectral class provides only a weak prior. Hot blue/white systems are biased somewhat toward Ironium, while cooler orange/red systems are biased somewhat toward Germanium. Random overlap deliberately remains large: spectral colour is a scouting hint, not a substitute for surveying a world.

## Extraction

The first economy slice gives every settled world baseline extraction. Yield scales with population and concentration:

`extraction units × concentration / 100`

Dedicated mine infrastructure is intentionally deferred. When added, mines should multiply or add extraction capacity without changing concentration semantics.

## Stocks and cargo

Mined minerals accumulate in the planet's existing mineral stock. The same minerals may be loaded into fleet cargo, transported, unloaded elsewhere, or delivered with a colonization mission. Minerals continue to share cargo capacity with colonists.

## Production

Production has two independent constraints:

1. production points, represented by `remainingCost` and colony stockpile/output;
2. the required I/B/G material bill.

A construction item may reach zero remaining production points and then wait at the head of the queue until the colony has all required minerals. Minerals are consumed atomically when construction completes.

Factory mineral bill: `2 I / 1 B / 2 G`.

Ship mineral bills are derived from hull plus fitted components. This makes ship design affect industrial logistics as well as mass, fuel, cargo, sensing, and mission capability.
