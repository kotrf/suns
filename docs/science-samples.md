# Physical science samples and working artifacts (#59)

Observations and physical specimens are separate rewards. Existing finite
stellar/planetary/wormhole/object studies and novelty-gated wreck research remain
unchanged. Samples add something an expedition must actually bring home.

## Player workflow and prerequisites

Study an Ancient probe or Compact remnant with active scanners and **Field
research**, then wait for the completed study to reach headquarters. Select a
cargo-capable scanner fleet and **Fly and recover specimen** in **Unknown objects /
expeditions**, or use the corresponding arrival task in the route editor.
Recovery requires physical arrival at the landmark, active scanners, that empire's
delivered study and 5 kt of free shared hold. Ordinary command delay applies.

There is one specimen per landmark for the whole galaxy. Recoveries resolve in
FleetId order. The depleted source persists after analysis or destruction; later
fleets receive no substitute. Other empires can still study the landmark for data.
The journal never reveals another empire's hidden recovery or current stock.
An attempt ends after success, a depleted source or a full hold. Failures send a
normal delayed warning and keep cargo/stock unchanged. Clear capacity and issue a
new recovery order when needed; an empty source cannot regenerate.

Return to a friendly colony with **On arrival → Unload all cargo**, or use a local
transfer in **Science samples / artifacts**. At a populated owned colony choose:

| Specimen | Destructive analysis | Retained instrument |
| --- | --- | --- |
| Ancient archive | 64 Electronics RP once | +4 RP/year with Electronics as the active focus |
| Compact-matter specimen | 96 Energy RP once | +4 RP/year with Energy as the active focus |

Analysis consumes the specimen and dispatches ordinary scientific data from the
colony; research is awarded only after delivery. Repeated orders cannot award it
twice. A dispatched report survives source loss under the existing communications
rules. An activated artifact can later be consumed, with explicit UI confirmation.

Activation and operation require the current owner's delivered completed study of
the source landmark. The colony must be populated, research active in the matching
field, and allocation nonzero. The artifact supplements research without adding
local production. The Research summary includes its contribution and accepted
pending orders. These small fixed rewards are prototype balance.

## Physical cargo, ownership and received knowledge

Each specimen is an indivisible 5 kt item, sharing the hold with colonists and
minerals and contributing to gross mass, flight fuel and gate limits. Local
transfers require owned co-located endpoints, current local contact for fleets
and destination capacity. Previews require current received telemetry at both
fleet endpoints: a historical manifest stays visible but cannot authorize a
transfer or a subsequent analysis/activation. Pending transfers/dispositions
replay in order.

Manual and on-arrival merging preserve samples in the surviving FleetId, with
received evidence separate from the physical combined inventory. A merge over
the 64-specimen bound leaves both fleets intact. Splitting keeps as many whole
samples as fit in the source, moves the rest into the detachment, then shares bulk
cargo across the remaining free holds. Impossible indivisible placement rejects
the split. Integer population is allocated within both holds before minerals;
both results must fit before the split commits. Combat retains only samples
fitting the surviving capacity and reduces bulk cargo within the remaining
space. Lost samples do not regenerate.
Colonization deposits carried specimens before dismantling the fleet.

Surface conquest keeps specimens and deactivates the former owner's instruments.
The captor may analyze them or obtain the observation needed to operate them.
Transfers also deactivate instruments. Fleet inventories use separate immutable
confirmed/in-flight telemetry; hidden recovery or loss cannot rewrite received
or dispatched manifests. Foreign surface inventories are omitted from surveys.
Recovery, delivery and activation have dated reports; analysis uses scientific
data and breakthrough reports.

## Computer empires and persistence

The computer planner uses PlayerView and ordinary orders. After receiving an
expedition's study it may recover a sample when its fitted hold and fuel permit
return. It waits for cargo telemetry, returns to a reachable colony, unloads and
analyzes. A confirmed empty recovery retires; hidden depletion does not affect
planning. No direct research grant or privileged host-state access is added.

Save **v86** appends a bounded SAMP extension after v85, storing depletion,
physical manifests and each confirmed/in-flight/loss/wormhole contact separately.
Older saves start with empty manifests and unclaimed specimens. Host loading
rejects invalid references/kinds, duplicate physical specimens, retained fleet
cargo and overfilled holds; historical evidence may overlap. Orders **v32** adds
sample transfers/dispositions and recovery work. Previous formats remain readable.

Tests cover the complete expedition/return/disposition path, observation gates,
stock/capacity, duplicate rewards, ordered previews, split/merge conservation,
retained research, computer decisions, delayed manifests, malformed saves,
migration, byte-identical hidden exports, UI authoring and PBEM orders.
