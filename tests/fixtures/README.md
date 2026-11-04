# Economy migration fixture

`economy-v110.suns` is synthetic test data, not a user's saved game.

It was produced by compiling `economy_v110_writer.cpp` against the original
headers, core and `src/app/save_game.cpp` at
`a1a5b87916dbdf9a8f0d6724b058d15c38afc17f` (PR #277, save v110), with
GCC 14.2 / Qt 6.8.2 and QDataStream's Qt 6.4 wire format.

SHA-256: `32e29da6207a2b7e7bbde5fabc0ef2421b9f2256ec4a15e34f1881bce032d298`.

The source uses explicit campaign/token IDs and contains a partially paid
factory batch, annual factory rule, production template, pending order and
received owner observation. The v111 migration test checks the four resources
already paid on the first factory, consistent template/annual repricing, basic
building counts, unchanged technology/RP/history, and actual continuation after
a new-format round trip. This is an original v110 writer result, not a v111 file
with its header renamed.

To regenerate, build Suns::Core at that commit without app/tests, compile the
fixture writer together with that commit's `src/app/save_game.cpp`, link the old
core archive and Qt6::Core, and pass the desired fixture path as its one argument.
Do not build this writer against current headers: its purpose is preserving the
previous release's layout and recipes.
