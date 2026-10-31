#pragma once

#include <QIcon>

namespace suns {

enum class ToolbarGlyph {
    EndTurn, Designer, Research, Plans, History, Orders, Fleets, Cargo,
    Merge, Split, ZoomOut, ZoomIn, Fit, Sensors, Fullscreen
};

QIcon toolbarIcon(ToolbarGlyph glyph);

} // namespace suns
