# Dockable workspace

The galaxy map is the central canvas. System — Status & Minerals, Production, System Details, Fleet — Orders & Logistics, Research and Turn Messages are `QDockWidget` panels around it.

Every panel can be moved, resized, closed, tabbed with another panel or detached into its own operating-system window. Fleet overview/logistics and Route Program share one right-side panel. System status, minerals and Production occupy the outer left column in the default layout. System Details fills a second column directly to its right, beside the map. **View** contains a visibility toggle for each panel and **Reset panel layout** restores the default arrangement.

The system panel shows intelligence age, ownership, habitability and, for a friendly colony, population/capacity, factories, mines, yearly resources and orbital station status. Its mineral table keeps stock, yearly extraction and concentration visible together. Unknown or foreign stock and extraction remain hidden; concentration follows the player's geology knowledge.

The production queue has its own expanding list, with work, ETA, reorder/remove controls and compact ship/factory/mine/orbital-dock builders below it. Only the list scrolls; the surrounding panel has no outer scroll area. System Details is visible by default, with the selected planet first: portrait, habitability and survey information, population/capacity and growth, mineral geology, and environment bars with racial habitable ranges. Empire and current-order summaries follow below it. Its content scrolls inside the dock on small windows. The existing widgets keep their live selection updates and knowledge gates. **Production details…** still opens a non-modal infrastructure window. Research shares the bottom report area and cannot replace the production queue.

Dock tabs use a dedicated high-contrast style: inactive tabs remain visibly
bounded, while the active tab has a brighter surface, bold white label and blue
selection edge. They should read as navigation rather than ordinary command
buttons in the dark theme.

Window geometry and dock state are saved on normal shutdown and restored on the next launch. Layout version 5 resets the previous one-column arrangement once while retaining window geometry. Custom workspaces from versions 3 and 4 remain restorable; new ones save both columns. This provides a GIMP-like multi-window workspace without making map selection or game state depend on a particular screen arrangement.

Map clicks, fleet tools and Turn Messages share a star/fleet selection context
made of stable IDs. Activating a report selects its referenced object in the
other panels; if a referenced friendly fleet still exists, the map centers on
its current player-visible position. Enemy contacts and vanished fleets use
the report's system or recorded position instead.

The **Fullscreen map** toolbar button or **F11** expands the map to the full screen, hiding docks (including detached ones), the main toolbar, menu and status bar. Map controls remain available. **Return to panels**, **F11** or **Esc** restores the previous geometry, dock arrangement and visibility, zoom and map center. Closing the app in this mode saves the normal workspace.

The **View → Workspaces** section offers Map (the temporary fullscreen view),
Fleet Operations (colony panels plus fleet and route tools), Empire (colony
panels and reports), and Ship Design (colony panels plus the non-modal designer).
The three panel presets start from the default arrangement. **Save current as Custom** stores the current dock layout and
window geometry; **Restore Custom** brings it back after trying a preset. Every
panel remains independently available in View, and Reset panel layout still
restores the single-monitor default.

Help is placed last after all top-level menus are installed. The fixed status-bar distance readout compares the previous distinct selected object with the current object, including both stars and fleets. Fleet positions use player-visible telemetry. Selecting the same object repeatedly does not replace the reference object; loading or creating a galaxy resets selection history.

The fleet selector, compact portrait, fuel/cargo/damage gauges, communications and cargo transfer stay above the scrolling editor. **Details & logistics** expands ship composition, dockside colonist loading, rename/merge/split and colonization in that same panel. The former independent Route Program dock is retired. Workspace format 4 resets older default dock layouts once to reveal the colony panels, while retaining window geometry. Explicit custom workspaces saved in format 3 remain restorable; newly saved custom workspaces use format 4. The Fleet Operations preset and Reset panel layout both restore the combined fleet panel.

System status and Production default to a 340 px column. Bottom reports use
the center area without taking height from the colony column; wide report forms
scroll inside their dock on small windows. Long design names
use the dropdown popup without forcing a wider column. The map is fitted after
the initial dock layout is ready.

Map clicks hit each star's visible disc with a small margin, rather than the
large rectangle needed to paint its glow. When selectable markers overlap,
the nearest marker center wins, with equal distances resolved by scene
stacking order. Ordinary selection, map-target picking and right-click route
orders use the same rule; a system core takes precedence over its own orbit
ring. Labels, glow/selection brackets and sensor circles do
not intercept these object clicks. Scene redraws remain deferred until after
the current mouse event.

Controls use shorter minimum heights and tighter padding, group spacing, dock
titles and tabs. Planet portraits are 80 pixels across. Resetting the workspace
targets a 190-pixel report dock; history charts can shrink further so the map
retains more vertical space.
The fleet selector and fuel/cargo/damage header remain outside the editor's
scroll area.
