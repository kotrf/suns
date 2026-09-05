# Dockable workspace

The galaxy map is the central canvas. Overview, Production, Fleet — Overview & Logistics, Fleet Route Program, Research and Turn Messages are `QDockWidget` panels around it.

Every panel can be moved, resized, closed, tabbed with another panel or detached into its own operating-system window. Fleet overview/logistics and Route Program start as tabs in one right-side Fleet area; Overview and Production start on the left. **View** contains a visibility toggle for each panel and **Reset panel layout** restores the default arrangement.

Dock tabs use a dedicated high-contrast style: inactive tabs remain visibly
bounded, while the active tab has a brighter surface, bold white label and blue
selection edge. They should read as navigation rather than ordinary command
buttons in the dark theme.

Window geometry and dock state are saved on normal shutdown and restored on the next launch. This provides a GIMP-like multi-window workspace without making map selection or game state depend on a particular screen arrangement.

Help is placed last after all top-level menus are installed. The fixed status-bar distance readout compares the previous distinct selected object with the current object, including both stars and fleets. Fleet positions use player-visible telemetry. Selecting the same object repeatedly does not replace the reference object; loading or creating a galaxy resets selection history.
