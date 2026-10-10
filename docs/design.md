# Suns! design direction

Suns! is a turn-based space 4X strategy game inspired by the systemic clarity of classic games such as *Stars!*.

## Design principles

1. **Depth from interaction, not feature count.** Prefer a small number of mechanics that affect one another over many isolated meters and subsystems.
2. **Orders, then resolution.** Players prepare orders against an immutable current state. The turn processor resolves those orders into the next state.
3. **Deterministic core.** Given the same state and orders, the simulation must produce the same result. Randomness will be explicit and seedable.
4. **Headless simulation first.** The GUI is a client of the game core, not the owner of game rules.
5. **Readable information.** Complexity should come from decisions, not from hiding the state behind presentation layers.
6. **Incremental vertical slices.** New mechanics should become usable in the running game as early as practical.

## Original vertical slice (completed)

The first playable path is intentionally small:

- view a galaxy;
- select destinations;
- issue fleet orders;
- end a turn and resolve orders;
- add planets and colonies;
- produce a colony ship;
- colonize a second world.

That loop is now implemented, alongside research, fitted ship designs, scanning,
logistics, paid terraforming, ground invasions and automatic fleet combat.
Diplomacy and the remaining combat systems are still future work. See
[project status](project-status.md) for the current baseline and follow-ups.
