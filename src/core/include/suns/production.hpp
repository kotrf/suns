#pragma once

#include "suns/turn_processor.hpp"

namespace suns {

struct ProductionCompletion {
    ProductionItem item;
    std::size_t originalIndex{};
    bool finishedBatch{};
    std::uint32_t localQuantity{};
};

struct ColonyProductionResolution {
    std::vector<ProductionCompletion> completions;
    std::uint32_t researchPoints{};
    bool hasShipyard{};
    std::vector<std::size_t> queueOrigins;
};

// Common yearly queue resolver used by the host and the production forecast.
// Mutates local buildings, minerals and environment; callers materialize ships
// and orbital stations from the completion list.
[[nodiscard]] ColonyProductionResolution resolve_colony_production(
    const GameState& state, Planet& planet, bool hasShipyard);
[[nodiscard]] bool production_batch_valid(const QueueProductionBatchOrder& order);
bool apply_production_batch(const GameState& state, Planet& planet, const QueueProductionBatchOrder& order);
[[nodiscard]] bool production_template_valid(const ProductionTemplate& value);
bool set_production_template(Player& player, const SetProductionTemplateOrder& order);
bool apply_production_template(Planet& planet, const Player& player, const std::string& name);
void apply_default_production_template(const GameState& state, Planet& planet);
[[nodiscard]] GameState planned_production_state(const GameState& state, const PlayerOrders& pending);

} // namespace suns
