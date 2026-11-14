#pragma once
#include "suns/turn_processor.hpp"
namespace suns {
struct ShipRefitQuote {
    std::vector<ShipComponentType> removed, installed;
    MineralCargo recovered, installedMinerals, netMinerals;
    std::uint32_t componentWork{}, work{};
};
[[nodiscard]] std::optional<ShipRefitQuote> ship_refit_quote(const ShipDesign&, const ShipDesign&);
// A queued stationary ship reserves its future tanks even without a working yard.
[[nodiscard]] std::optional<double> ship_refit_fuel_capacity(const GameState&, const Planet&, const ShipRefitJob&);
[[nodiscard]] bool ship_refit_available(const GameState&, const Planet&, const ShipRefitJob&, std::optional<bool> shipyard = std::nullopt);
bool queue_ship_refit(GameState&, PlayerId, const RefitShipOrder&);
bool complete_ship_refit(GameState&, Planet&, const ShipRefitJob&);
bool delete_ship_design(GameState&, PlayerId, ShipDesignId);
}
