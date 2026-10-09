# Stars! mining robots

The six mining robots follow the original Stars! Technical Reference Guide,
printed page 12: [reference PDF](https://www.bestoldgames.net/download/games/stars/stars-win-guide.pdf).
The [Player's Guide](https://www.bestoldgames.net/download/games/stars/stars-win-manual.pdf),
printed sections 13-3 and 20-11/12, describes remote operations and ARM/OBRM access.
The shared core catalog supplies prerequisites, costs, mass and output to research,
the ship designer and the turn processor.

## Catalog

Rate is kt of **each** mineral per year at concentration 100, before multiplying
by the number of fitted robots and ships. Mass and mineral costs are in kt.

| Robot | Rate | Mass | Construction | Electronics | Resources | I / B / G | Access |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| Robo-Midget Miner | 5 | 80 | 0 | 0 | 50 | 14 / 0 / 4 | Advanced Remote Mining |
| Robo-Mini-Miner | 4 | 240 | 2 | 1 | 100 | 30 / 0 / 7 | Ordinary |
| Robo-Miner | 12 | 240 | 4 | 2 | 100 | 30 / 0 / 7 | Excludes Basic Remote Mining |
| Robo-Maxi-Miner | 18 | 240 | 7 | 4 | 100 | 30 / 0 / 7 | Excludes Basic Remote Mining |
| Robo-Super-Miner | 27 | 240 | 12 | 6 | 100 | 30 / 0 / 7 | Excludes Basic Remote Mining |
| Robo-Ultra-Miner | 25 | 80 | 15 | 8 | 50 | 14 / 0 / 4 | Advanced Remote Mining |

Robo-Ultra trades some peak output for much lower mass and cost than Robo-Super;
it does not simply replace every ordinary robot with a larger number. Both field
requirements and race access are validated by the host, not only the UI.

An ordinary empire can fit two Robo-Mini robots in a Mini-Miner at Construction 2
and Electronics 1: 8 kt of each mineral/year at concentration 100, or 4 at
concentration 50. Separate compatible banks can use different models. The designer
shows the total reference rate; the planet/fleet panels show site-specific I/B/G
output. Minerals remain in the surface stockpile for transports to collect.

This imports equipment progression. Suns! retains its current colony mine output,
surface-stockpile logistics and persistent terminal route task. Mineral depletion,
Alternate Reality mining on owned planets, ARM bonus starting ships and Orbital
Adjuster terraforming are outside this slice.

## Compatibility

The old Remote Mining Module ID and costs remain unchanged. Its explicit rate is
1.25, preserving previous extraction exactly while allowing the simulation to sum
different robots' actual rates. Existing designs retain their equipment.

Save format 58 and order format 16 append six IDs; readers retain older formats.
Files declaring format 57 / order 15 cannot contain the new IDs. All PBEM clients
and the host must use the updated build before exchanging mining robot designs.
