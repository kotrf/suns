# Interception of physical transmissions

A dispatch creates immutable host-only emission pulses: physical transmitter,
emission year and automatically selected fast/slow channel. Delay/knowledge/UI
queries have no emission side effects. Reports held aboard Passive/Radio silence
fleets create no communication pulse until actual dispatch. Fleet telemetry,
colony forwarding, received observations and headquarters commands use the same
route calculation as their delivery. A detached scanner island emits the slow
hop from its selected physical relay; its sender-to-relay fast hop is also visible.
A local ship/surface upload exposes the forwarding colony, not a silent ship.

Active scanner/jammer operation also emits on the fast channel during the annual
telemetry phase; a communication-only window leaves active emission off. Passive
listening never emits. No new equipment, channel controls or periodic orders are
required. Channels do not change when jamming merely delays delivery.

Fast signals are sampled at the dispatch/next annual planning boundary. Slow
signals propagate in shells at 150 ly/year: a receiver 200 ly away intercepts a
turn-10 transmission on turn 12. Its report then independently follows that
observer's connection. With a 400 ly slow connection to headquarters it arrives
on turn 15. A passive observer instead holds the dated evidence until dispatch;
that second journey starts at the observer's then-current position.

Receivers use their actual fitted scanners and current interference. Planetary
None/Local/Full means no reception / 60 ly / 300 ly emission reach. Ship reception
uses twice the fitted ordinary scanner range, including when listening passively.
Per-receiver receipts prevent repeated observation inside an already passed
wavefront. Pulses expire after passing the range of receivers fitted at emission
(with a 300 ly minimum envelope), bounding host storage. Newly built receivers
outside that historical envelope cannot retroactively recover an expired pulse.

Player evidence reuses StrategicIntel and Turn Messages. A 20 ly grid cell with
15 ly uncertainty reveals neither owner, source IDs, composition nor authenticated
contents. It records emission, interception and delivery years, and never follows
a moved/destroyed emitter. The map marker lasts one receipt planning year; the
message remains in history. Decoys use the emission-time jammer strength and
position; sufficiently strong tachyon reception rejects only the false area.
Contemporaneous known hull evidence can suppress a redundant anonymous marker;
old pulses never query a departed source's current state.

Save v76 stores in-flight pulses, receiver receipts and dated report/message
metadata. Player exports contain delivered anonymous evidence, never host pulses,
source identities, unsent reports or incoming intelligence. Orders remain v25.
Advanced traffic classification, specialized SIGINT hardware, physical decoys and
channel-specific jamming remain later work under #58/#101/#205.
