# Star naming

Suns! uses a curated name pool rather than assembling most star names from runtime syllables.

The goal is the same feeling that makes a good old-school 4X map memorable: neighbouring systems may sound astronomical, mythological, geographic, scientific, natural, historical or simply unfamiliar. The variety should feel authored even though the galaxy itself is procedural.

## Source policy

The pool intentionally does **not** copy a proprietary game's star list. It mixes:

- real astronomical names;
- mythology and ancient cultural names;
- geography and natural regions;
- scientists and mathematicians;
- nature, minerals and atmospheric words;
- historical places;
- original Suns! names written for this project.

Obvious names coined for other fictional universes should not be added merely as references. Incidental overlap with real, public-domain or ordinary names is fine.

## Determinism

`Sol` is always the home system. All other generated systems draw without replacement from the curated pool, so a supported galaxy never needs `Name-23` style collision suffixes.

Naming uses an RNG stream derived from the galaxy seed but separate from the physical-generation RNG. Adding, removing or reordering names may change which names a seed receives, but it must not move stars or change stellar class or habitability for that seed.

This separation is deliberate: names are presentation/content; geometry and physical properties are simulation inputs.

## Maintenance

Categories in `src/core/src/star_name_pool.hpp` exist only to keep the source understandable. The game should present the names as one mixed deck rather than exposing categories to the player.

The current pool contains 300 unique names, comfortably above the maximum supported 64-system galaxy. When expanding it, prefer distinctive short-to-medium names and keep the mix broad enough that a generated map does not look like it came from a single naming algorithm.
