# Autonomous survey probes — first #60 slice

The playable loop is a paid probe, a delivered programme, several autonomous
system surveys, delayed reports, and a bounded return/hold. There are no new
science rewards, copies, resource harvesting or beacon deployments in this slice.

## Hardware and price

**Survey Probe** requires Construction 16 and Electronics 18. The hull is 12 kt,
90 production and I/B/G **24/8/20**, with 300 fuel capacity, 35 armour and no
inherent cargo hold. Fittings are one engine, one scanner, one electrical slot
and one electrical/mechanical utility slot. It cannot carry construction kits.

**Survey Autopilot** requires Construction 16 / Electronics 18, fits only this
hull, and costs 3 kt, 70 production and I/B/G **10/5/20**. A usable autonomous
fleet consists entirely of probe hulls with autopilots and has a working scanner.
The basic design adds the existing Quick Jump 5 and Mole Scanner; their ordinary
cost, mass and unlocks are additional. The Designer supports other available
engines/scanners and utility equipment under normal fitting/access rules.
At this balance the basic fitted design is **21 kt, 172 production, I/B/G
39/13/43**, with 300 fuel and safe cruise Warp 5.

The small, fragile hull makes a distant expendable expedition practical; the
late electronics and substantial automation price pay for independent decisions.
It provides operational reach rather than a research multiplier. Shipyards,
production, minerals, fuel, repairs, combat and losses use normal mechanics.

## Programme and completion

The programme specifies a circular area (chart coordinates and radius), desired
Orbital/Geological/Deep survey, maximum Warp, flight policy, extra fuel reserve,
completed-target limit, mission-year limit, and whether to return to launch.
Configure starts a new mission at the position where the command is received;
that position becomes its return anchor. Reconfiguration retains the physical
onboard survey notebook but resets mission counters and the time limit. Resume
keeps the programme, counters, anchor and original time limit; use Configure for
a fresh expedition after limits have been reached.

Targets are the nearest unfinished charted star inside the circle; equal
distances use the lowest stable StarId. Stellar chart coordinates are public.
Target choice never reads planet ownership, stocks, deposits, unknown objects,
other fleets' notebooks, pending HQ reports or the current HQ survey database.
Already received or locally completed surveys are skipped at the requested depth.
Each normal end-of-movement orbital observation advances the observer's own
Orbital → Geological → Deep progression. The probe stays until the requested
stage is locally observed, then chooses its next target on the following turn.

Safe cruise chooses the fastest affordable Warp up to the programme cap and all
engines' cruise limits, without overdrive. Economical chooses the affordable Warp
with least conservative fuel use (lowest Warp breaks equal-cost ties). Annual
fuel rounding, the conservative interstellar-medium envelope, extra reserve and
the return leg are budgeted; future scooping/generation/refuelling is not credited
in advance. Actual movement uses the ordinary physical environment and costs.
Safe cruise does not promise immunity from mines or hostile fleets.

After target/year limits or no unfinished targets, return is attempted when
enabled; otherwise the probe completes and holds. If no affordable target exists,
it returns when possible, or holds awaiting fuel. A probe holding at its anchor
can retry after real refuelling; a completed return requires explicit resume/new
programme. The anchor is a coordinate, not a guarantee of a friendly fuel depot.
Unavailable supply never grants fuel or redirects the probe using hidden station
knowledge. Damage of 25% or more holds for ordinary repair; work can continue
after repair. Missing necessary equipment stops autonomy until explicit resume
after restoration. Inactive sensors obey normal emission rules and cannot advance
survey; the programme does not silently enable them.

## Two knowledge histories and delayed control

At issue time Configure/Resume/Upload capture only the owner's **delivered** HQ
survey levels, StarIds and evidence dates. The immutable payload travels through
PendingFleetCommand with the ordinary propagation/jamming delay. Receipt merges
it into the notebook; it does not query the newer host HQ state. Upload is an
explicit knowledge-only order and does not resume a paused programme. There is
no automatic empire-wide notebook sharing or free annual synchronization.

Fitted probes retain their own observations even before their first programme
arrives. Local observations update that probe's notebook immediately, even when HQ reports
are in transit or held by silence. Existing survey reports independently deliver
the observations to HQ with their usual timing. Telemetry contains only the dated
programme/status/target/completed-target summary, never the private notebook.
PlayerView and PBEM exports use received telemetry and delivered survey snapshots.
Navigation projections do not run private autonomous planning from an empty notebook.
Hidden changes to physical missions or third-party stock cannot rewrite them.

Programme replacement, stop, resume, upload and manual routes take effect only
on receipt. A received manual route/task pauses autonomy; the probe follows that
ordinary manual programme and needs explicit Configure/Resume to select survey
targets again. Stop clears movement on receipt and supersedes older in-flight
commands like an ordinary clear-route command. Electronics orders keep their
separate normal transmission policy. Autonomy needs neither an annual command
nor a current connection.

Splitting copies the physically shared notebook to both groups and pauses both.
An explicit or on-arrival merge retains the destination programme (or adopts the
source's if absent), merges only those physically co-located notebooks and pauses
autonomy. Existing destination routing follows ordinary merge rules; no new
autonomous targets are selected until explicit resume. A destroyed ship contributes
no notebook to unrelated probes; already transmitted reports retain normal delivery.
Incoming commands retain their FleetId: the surviving group keeps its packets,
the detachment receives no cloned commands, and packets addressed to an absorbed
source disappear with that fleet under the ordinary reorganization rules.

## Interface and computer players

The **Autonomous survey probes** dock can order a basic design and its paid
construction, choose a fleet, load received settings, edit a programme and send
Configure/Stop/Resume/Upload. The ordinary Designer/production UI supports custom
fittings. The panel labels telemetry year/age, last reported target, mission state,
completed targets and estimated position. It does not predict unknown future
autonomous choices. The received-survey table uses HQ evidence, not notebook truth.

Computer empires build a basic probe at an affordable idle colony with a shipyard
once the normal technology is available, then issue a basic bounded survey
programme from current received telemetry. Dedicated probes are excluded from
generic expansion route assignments. The policy uses PlayerView and ordinary
design, production and programme orders; there is no host-state planning shortcut.

## Persistence, verification and remaining scope

Save **v88** adds a bounded **PROB** extension after v87. Physical notebooks,
confirmed/in-flight telemetry, incoming immutable commands, lost contacts and
wormhole last contacts are separate records. v87 and earlier keep empty missions.
Orders **v34** adds programme control and the new design identifiers. Readers
validate bounds, survey IDs, duplicate records, dates, enum values and private
notebooks in player/telemetry records. Hull/component readers and radar-contact
limits include the new hull without changing old-format enum bounds.

Regressions cover a multi-system expedition without further orders, three-stage
survey timing, delayed control, local-before-HQ knowledge, immutable uploads,
hidden-world isolation, deterministic target ties, fuel/damage/equipment limits,
both merge paths/splits, paid computer construction/launch, full save continuation,
PBEM, v87 migration, malformed saves and Designer/UI presentation.

Self-replication, material harvesting for copies, autonomous beacon deployment,
new scientific rewards, tactical threat routing, arbitrary conditional programmes
and autonomous searches for supply remain later #60 slices. **#60 stays open**;
this implementation requires review before merge.
