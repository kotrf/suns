#pragma once
#include "suns/game_state.hpp"
#include <cstddef>

namespace suns {
// Visits one order, or two for an explicitly marked rename whose source exists.
// Rejected renames preserve the original library/default flags. Unmarked
// delete/save actions remain independent; an orphan rename cannot delete.
// Other order kinds are skipped. Only the bounded template library is copied.
[[nodiscard]] std::size_t apply_production_template_edit(
    Player&, const std::vector<Order>&, std::size_t index);
}
