# Variable stars

A small deterministic subset of non-home stars varies in luminosity. The
feature is intended to make occasional systems memorable, not to turn every
colony into an environmental timing puzzle.

Generated variable stars have a 6–16 turn sinusoidal cycle and a luminosity
amplitude of 5–16 percent. Sol is always stable. Variability uses a separate
hash of the galaxy seed and stable star ID, so adding the feature does not
change existing star positions, spectral classes, names or baseline planets.

`Planet::habitability` remains the world's long-term baseline. Current
habitability adds the star's luminosity displacement and is clamped to
0–100. The current value affects population capacity and growth; established
colonies do not suffer surprise population loss merely because a cycle enters
its dim phase.

## Player knowledge

Authoritative cycle parameters are not automatically public:

- long-range and basic scans do not diagnose variability;
- orbital survey identifies whether a star is variable, but not its cycle;
- one full turn in orbit (the existing geological-survey step) reveals period,
  amplitude and phase;
- an owned colony knows and can forecast its local star.

Before characterization, confirmed planetary habitability remains the value
observed at the scan turn. It does not silently track future authoritative
conditions. Once the cycle is characterized, the player can calculate the
current value on every later turn.

The map marks known variable systems with `[VAR]`. System details show either
that variability was detected or, after characterization, the period,
amplitude and current luminosity. Survey messages record both discovery stages.

Future extensions may add uncertain period estimates, dedicated long-baseline
observatories, stellar-weather sensor interference, energy-production effects
and mitigation technology. Those effects require a separate balance pass.
