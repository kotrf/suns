#pragma once
#include "suns/game_state.hpp"
#include <QString>

namespace suns {
// Shared with Ship Designer so the reference browser and fitting tool describe
// the same installed equipment and technology/racial restrictions.
QString componentCatalogDescription(ShipComponentType component);
QString componentCatalogRequirements(ShipComponentType component);
} // namespace suns
