# Galaxy size and density

New Galaxy and New Campaign use the same size/density selector. Presets expand
into the existing saved `GalaxyConfig` (seed, count, width, height, separation).
The default is **Tiny / Normal: 400 × 400 ly, 32 systems**, with a 15 ly minimum
separation. Existing saves retain their positions and explicit configuration;
non-preset maps appear as **Custom / saved map** in the setup dialog.

The original Stars! 2.6b help, distributed in
[st26bhlp.zip](https://starsautohost.org/files/st26bhlp.zip), gives square map
sides of 400, 800, 1200, 1600 and 2000 ly. It describes approximately one planet
per 6500 sq ly for Sparse, 5000 for Normal and 4000 for Dense. Packed adds 30%
above Dense, except Huge is limited to the Dense count of 1000. Suns rounds
these approximate area ratios to whole systems; these are not claims of exact
original executable counts. The 15 ly separation is our placement choice.

| Size | Side (ly) | Sparse | Normal | Dense | Packed |
| --- | ---: | ---: | ---: | ---: | ---: |
| Tiny | 400 | 25 | 32 | 40 | 52 |
| Small | 800 | 98 | 128 | 160 | 208 |
| Medium | 1200 | 222 | 288 | 360 | 468 |
| Large | 1600 | 394 | 512 | 640 | 832 |
| Huge | 2000 | 615 | 800 | 1000 | 1000 |

Previously 24 systems shared a 900 × 650 ly field with 48 ly separation:
24,375 sq ly per system, almost five times Normal's area ratio. A 1000-seed
sample averaged 98 ly to each system's nearest neighbor. The new Normal default
averages 41.2 ly across 1000 seeds and targets the original 5000 sq ly ratio.
Regression tests sample 100 independent
seeds and require a mean nearest-neighbor distance between 30 and 50 ly.

Generation now supports up to 1000 systems. Once the curated name deck is
exhausted, stable unique `System <id>` names fill the remainder. Generation,
map display and wormhole bounds use the same dimensions, including custom
rectangular maps below the former hidden 500 × 400 ly floor. Non-finite
dimensions are rejected; custom dimensions are bounded to 100–10,000 ly and
separation to 0–100 ly. The existing six placement relaxation passes remain
available for unusually crowded custom configurations.
