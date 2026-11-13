# Complete hull and equipment catalog

**Fleet → Hull and equipment catalog…** or the **Catalog** toolbar button opens
a separate modeless reference window. One live window is reused; closing it
releases it, and reopening reads the latest player view. Browsing creates no
designs or orders and does not change the fitted Ship Designer draft.

The complete registry currently contains **227 entries**: all **41 ship hulls**,
the **5 orbital station hulls**, and **181 components**, including station gates
and legacy equipment. Legacy hull names are explicitly marked. Locked entries
remain selectable and readable. The search matches names, categories,
requirements and descriptions; category and Available only filters can be
combined. Both result list and description use the available height and scroll.
Close remains outside those panes at smaller sizes.

Hull entries show production/mineral cost, mass, armor, initiative, cargo/fuel,
required engines, extra support effects and fitting bank sizes/counts. Ship hull
mineral prices use `ship_design_mineral_cost` for a bare hull with no components,
matching the production calculation for all hulls, including legacy hulls whose
specification's mineral field is empty. Station
hulls show their slot capacities and configurable services. Components show
shared Ship Designer descriptions: costs, mass, ratings, fuel tables, scanner/
mining/service effects and fitting restrictions as applicable. The shared
formatter now retains base mass/cost facts for Orbital Shipyard Kit and radiation
screens alongside their special descriptions. Sensor Array Kit facts distinguish
the transported 100 kt fitting from the powered anchored 250/75 ly scan field.

Requirements include technology, racial access, acquisition and legacy catalog
conditions. Station gates use their dedicated Energy/Electronics/Construction,
Stargate Specialist and minimum-hull rules; their availability is evaluated for
station fitting rather than ordinary ship fitting. Unlimited range/mass remains
explicit. Public catalog specifications are visible before research; availability
uses only the current received player's technology/race/acquisitions/designs.
Another empire's research, equipment or fleets cannot unlock an entry.

MainWindow passes `make_player_view` to the browser. `controlsChanged` schedules
a coalesced zero-delay refresh only while the window is visible; there is no
periodic poll. Unchanged availability leaves item widgets/selection in place,
and identical description text retains its scroll position. New turn/load/race/
technology data updates the open browser. No save/order format is added.
