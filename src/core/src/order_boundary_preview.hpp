#pragma once

#include <cstddef>
#include <functional>

namespace suns {
struct GameState;
struct PlayerOrders;
namespace detail {
// Integrate production edits with the existing local command/cargo preview.
// The callback may advance the index over one atomic template edit. Taking
// ownership lets the production preview reuse its one prepared state copy.
using PlanningOrderPreview = std::function<void(GameState&, std::size_t&)>;
[[nodiscard]] GameState project_order_boundary_state(
    GameState, const PlayerOrders&, const PlanningOrderPreview&);
}
}
