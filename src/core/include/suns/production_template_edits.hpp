#pragma once
#include "suns/game_state.hpp"
#include <cstddef>

namespace suns {
// Visits one order, or two for an adjacent delete/save replacement whose source
// exists. Rejected replacements preserve the original library/default flags.
// Other order kinds are skipped. Only the bounded template library is copied.
[[nodiscard]] std::size_t apply_production_template_edit(
    Player&, const std::vector<Order>&, std::size_t index);
}
