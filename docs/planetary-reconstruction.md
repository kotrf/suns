# Planetary war damage and paid reconstruction

This is the infrastructure stage of #106. Bombardment records actual factories
and mines destroyed, cumulative reconstructions and the last bombardment/recovery
years on the physical planet. Unreconstructed ruins are destroyed minus rebuilt.
Repeated bombardment cannot destroy absent installations. Surviving factories,
mines, stocks and damage remain after capture, depopulation and recolonization.
Fresh uncolonized worlds keep their original starting infrastructure.

Destroyed factories already stop contributing production; destroyed mines already
stop contributing extraction. This stage adds no extra percentage penalty and
makes no additional change to natural environment, population growth or capacity.
Existing weapon-specific terraforming reversion remains the existing rule.

The normal Production selector offers **Reconstruct factories** and **Reconstruct
mines** as one-time batches. A factory costs 4 production (new: 6), a mine costs 3
(new: 5). Each consumes the same minerals as new construction: I/B/G 2/1/2 and
1/2/1 kt respectively. Surviving foundations explain the production saving; no
installation or mineral stock is granted by changing ownership or waiting.

Each completed unit restores one working installation and clears one ruin. Queue
priority, partial progress, research allocation, mineral shortages, cancellation
and forecasts all use the shared production resolver. Minerals are paid only on
completion. Existing queued units reserve ruins; excess orders are rejected and
obsolete surplus work retires without further spending. New construction creates
a new installation rather than clearing a historical ruin. Planet-specific repair
cannot be put in colony-development templates or recurring annual rules.

Owners see exact ruins/history, per-unit costs and queue completion estimates in
Production and colony status. Reconstructed installations immediately contribute
their normal output/mining from the following production/mining phase. Production
completion reports travel over the colony's real connection and add receipt-year
Reconstruction markers to History. Attackers retain delivered bombardment loss
reports; other players' exports do not include current physical damage counters.
Computer empires can queue the same paid reconstruction batches from their own
knowledge, without overriding existing work or bypassing costs.

Save v77 and orders v26 preserve counters, partial/batched repair, received
messages and history milestones. Older saves start with zero reconstruction
history; past losses are not invented. The remainder of #106 is still open:
contamination and cleanup, biosphere damage, special weapons/scorched earth,
observer-specific historical damage estimates and later balance work.
