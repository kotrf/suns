# Reconnaissance and emission programs

The intelligence layer separates physical fleets, immutable hull observations,
anonymous emission areas and received owner knowledge. **Fleet → Intelligence…**
lists delivered/currently connected observations, with quality, observation age,
position uncertainty, mass estimate, known hulls and the reporting source.

## Hull contacts

Each observation stores its position, observation/delivery years, source and
sensor quality. Detached scouts send these snapshots over the ordinary
communication network. Their reports survive destruction of either fleet.
The player sees the recorded position after delivery, even if the enemy has
moved. A lost track keeps its last-seen position/date in the intelligence list;
it is removed from the tactical map. Loss means no observation, not destruction.
Unrefreshed hull tracks remain dated evidence, with gray map markers and age
labels. They are never extrapolated using hidden enemy routes or speeds.

Initial Suns! quality rules:

| Quality | Acquisition | Disclosed evidence |
| --- | --- | --- |
| Probable | Outer active footprint, or ordinary passive hull detection | Approximate area; owner/count/hulls unknown; mass band ±50% |
| Confirmed | Within 70% of active effective range; alternatively very close passive detection with Electronics 6 and at least 50% tachyon strength | Owner and localized position; mass band ±25%; count/hulls unknown |
| Identified | Within 25% of active effective range, Electronics 3, interference at most 25% | Exact observed position, ship count and hull classes; mass band ±25% |

Probable positions use a 10 ly grid and 7.5 ly uncertainty. Confirmed outer
positions use a 2 ly grid and 1.5 ly uncertainty. Close confirmed contacts can
be localized exactly without disclosing their composition. A confirmed system
association retains that observed star/orbital context. Identification never
reveals enemy project IDs, names, equipment, cargo manifests, fuel or orders.
Mass bounds include carried mass but are rounded outward to 10 kt, preserving
uncertainty even for identified hulls.

A receiver holding a track retains detection to 110% of its acquisition range,
confirmation to 77%, and identification to 27.5% (subject to the same technology
and interference gates). Hysteresis belongs to that receiver; another sensor
does not inherit its private track. The first useful delivery wins; equal-delivery
candidates prefer stronger classification, then position precision and stable source IDs. Observations
from different years remain pending if older evidence can arrive earlier.

Delivery ignores older or duplicate snapshots. Loss/reacquisition cannot be
undone by a late old packet. Detection, loss and quality/interference transitions
produce dated briefing events; unchanged scans only refresh the snapshot.
Duplicate/outdated transition packets do not generate repeated history markers.

Player exports retain received contacts and sanitized fleet placeholders for
localized tracks. Probable hull areas remain separate from fleet icons. Host
observation ledgers, pending snapshots and foreign designs are removed. Map
pickers, names, distance calculations and enemy route targets use this same
knowledge: a route targets the reported position, not today's hidden enemy.
Anonymous fleet placeholders have no ship stack; they cannot create a fictitious
ship through the legacy single-ship fallback or break PBEM loading.

## Interference and anonymous signals

Fleet and fitted station jammers reduce hostile active/passive hull reception
and emission reception, capped at 75%. Station auras cover 30 ly; fleet auras
cover at least 30 ly or their ordinary scanner range. Strength combines fitted
copies with diminishing returns; the strongest local hostile platform applies.
The same strength adds 1–2 years of communication delay at the source.

Observers report measured nominal/effective active range, passive baseline and
emission reception range through the same delivery policy. Sensor interference
and clearance events are transition-based. A remote UI uses received measurements,
not current hidden jammer components. Blue/green ranges show active coverage,
gold the passive hull baseline, and violet emission reception. Passive hull
reach also depends on target motion and cloak; the overlay is a baseline rather
than guaranteed detection.

Colonies and scanner ships can intercept transmitting enemies beyond reflected
hull range. Emissions remain unidentified 20 ly cells with 15 ly uncertainty:
no fleet ID, owner, composition or truth/decoy label. Decoy signals use the same
presentation; sufficiently strong tachyon receivers reject them. Multiple
receivers coalesce an area's report onto its earliest delivery. A received
emission marker lives for one planning year; its dated briefing remains.

## Programmable silence

Standard and Passive keep their existing behavior. Radio silence still requires
a finite wake-up 1–100 years ahead. It can additionally carry:

- a one-year communication window every N years (N 1–100), beginning N years
  after the order is issued in the UI;
- an optional active scan during each window;
- wake-up to Standard at arrival at a waypoint.

The interval must fit before the failsafe wake-up. A communication-only window
publishes telemetry and admits commands while keeping active scanners off.
An active-scan window also performs surveys/contact observations and emits a
signature. After the window the fleet remains silent automatically. A late
program follows its original absolute schedule; an already expired program
falls back safely to Standard. Commands wait for the next window or finite
wake-up. Reports collected while silent wait for an allowed transmission.

The same observation year is used for active scanning, relay membership,
telemetry, jamming, survey/field/anonymous-signal reports and battle packets.
Burst windows do not silently turn into permanent Standard mode. The owner's
route/position continues to use confirmed telemetry and its known onboard plan.

## Persistence and current limits

Save **v66** and orders **v21** add these snapshots, sensor measurements and
program fields. Bounded trailing metadata preserves older record layouts;
existing readers remain. All PBEM participants use the same build.

Traffic-direction analysis, expendable physical decoy probes, full forged hull
tracks and conditional combat/contact doctrines remain follow-ups under
#58/#101. They are not extra prerequisites before computer players #184.
The AI stage must consume PlayerView and issue ordinary legal PlayerOrders.
