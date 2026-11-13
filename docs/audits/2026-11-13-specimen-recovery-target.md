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

## Repeat-review P2: keep route edits after coarse received fixes

[Review5892821657](https://github.com/kotrf/suns/pull/295#pullrequestreview-5892821657)
on3619dfa confirms the first correction. A newer coarse report keeps Studied but
removes the precise course; Qt correctly stages Warp2→5 for namedX7 at129,0,
but host admission calls the kind-only coordinate check after validating the ID
and silently discards the edit. Source CI43440521704 passed134/134 in147.44 s
with exact3619dfa/Qt6.12.0 verified.

The shared work validator now accepts a complete arrival action. A named recovery
checks that source's received completed study and fleet equipment; it does not
require the old intercept to match a newly received position/course. Coordinate
lookup remains for new unnamed choices, using unique received evidence. Host
admission validates full active/queued actions; Qt append/action editing passes
the same complete intent and retains an existing ID before action validation.
Physical location and availability stay local execution concerns. Save125,
orders53 and race3 are unchanged.

Both new regressions fail before the correction. The Qt test delivers an actual
coarse observation, edits the existing route and executes its pending orders
through TurnProcessor, checking Warp5, real125,0 movement and retained target7.
Core cases cover active/queued routes, immediate/delayed delivery, absent hidden
physical objects and rejection of unnamed/missing/unreceived/unstudied targets,
missing scanners and missing cargo capacity. The complete GCC14.2/Qt6.12 Debug build and134/134 CTest passed in208.19 s; after strengthening the two initial-course fixtures, final12/12 profile tests passed in3.68 s. All158 local documentation targets were checked.

Both review discussions remain open for repeat review and user-authorized merge.
