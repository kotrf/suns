# Preserve the selected specimen source — PR #295

[Review5891409220](https://github.com/kotrf/suns/pull/295#pullrequestreview-5891409220)
on5c38dc5 finds one P2. A RecoverSample voyage reaches studied staticX8 in
year11 while radio-silent until12. On wake-up, the old preparation phase chooses
nearby studied driftingX7, moves the fleet, spends5 mg fuel and claims the wrong
Architectural core. X8 remains untouched. The actual two-turn regression fails
before the fix at the fixed-position/fuel assertion.

The recovery programme now names its chosen UnknownObjectId. Human journal and
computer orders carry an explicit ID; generic coordinates bind only one unique
received completed study/course at issue time. New ambiguous/missing intent
rejects before transmission. Arrival converts that same ID into standing work;
delayed task packets freeze it rather than reselecting on delivery. Local moving
rendezvous and physical pickup accept only that body, even if a different source
is closer or the chosen source is depleted. Completion/Stop clears the intent.
No hidden physical position, availability or current course influences authoring.

Save125/orders53/race3 preserve the conditional ID in recovery arrival actions,
standing fleets, immutable telemetry and delayed task packets. Queued routes,
drafts, inboxes, host/player files and missing contacts reuse the shared codecs.
Unrelated action/task layouts stay unchanged; wrong-kind and out-of-range IDs
reject. Older saves read target0 and keep fixed-coordinate pickup without pursuit.
Warp/same-action edits retain the selected journal ID and the route identifies it.

Regressions exercise static wake-up without movement/fuel, a named moving source
versus a closer decoy, no fallback after depletion, legacy0, ambiguous new orders,
in-progress host reload, current/frozen telemetry, hidden-target PBEM equality,
queued/delayed route/task identity and decoded .sunsorders, malformed targets,
and actual Qt authoring/editing. No new binary fixtures or UI forecasts are added.

The complete GCC14.2/Qt6.12 Debug build, focused11/11 in5.06 s and final134/134 CTest in207.14 s passed; the actual Qt panel and214 local link targets were checked.

The original source CI134/134 was green before the P2 correction. The new source
awaits repeat review; its thread stays open until an authorized merge.
