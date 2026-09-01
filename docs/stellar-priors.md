# Stellar-class planetary priors

Spectral class now supplies weak statistical priors for both habitability and
mineral geology. It is a scouting hint, never a deterministic planet table.

Planet habitability begins with the same broad triangular random distribution
for every system, then receives a small class adjustment. Yellow stars have the
most favourable mean; blue-white and white stars tend to be harsher. Every
adjustment is at most ten points against a much wider random range, preserving
large overlap and exceptional worlds around every class.

Mineral concentrations use independent deterministic noise per planet and
mineral. Hotter stars weakly favour Ironium while cooler stars weakly favour
Germanium; Boranium changes only slightly. Concentrations remain hidden until
the appropriate survey result reaches the player.

The values are deterministic from the galaxy seed and stable IDs. Future world
generation may derive the same priors through hidden age, radiation and
metallicity variables, but spectral colour must remain evidence rather than an
answer.
