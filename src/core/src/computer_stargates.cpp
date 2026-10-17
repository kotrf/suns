#include "computer_policy.hpp"
#include "suns/stations.hpp"

#include <algorithm>
#include <type_traits>

namespace suns::computer_detail {

void plan_gate_construction(const PlayerView& view, ComputerTurnPlan& plan, std::string& reason)
{
    const auto& state = view.state;
    const auto* player = find_player(state, view.player);
    if (!player || player->race.primaryTrait != PrimaryRaceTrait::StargateSpecialist) return;
    std::vector<const Planet*> colonies;
    for (const auto& planet : state.planets) if (planet.owner == view.player) colonies.push_back(&planet);
    if (colonies.size() < 2) return;
    std::sort(colonies.begin(), colonies.end(), [](auto a, auto b) { return a->id < b->id; });
    for (const auto* colony : colonies) {
        if (!colony->productionQueue.empty() || colony->industry < 8 || colony->population < 100'000) continue;
        // Defense/emergency work planned above keeps priority; civilian
        // production below observes the same reservation.
        if (std::any_of(plan.orders.orders.begin(), plan.orders.orders.end(), [&](const Order& order) {
            return std::visit([&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, QueueStationOrder> || std::is_same_v<T, QueueShipDesignOrder>
                    || std::is_same_v<T, QueueProductionOrder> || std::is_same_v<T, QueueProductionBatchOrder>)
                    return value.colony == colony->id;
                return false;
            }, order);
        })) continue;
        const auto* station = find_orbital_station_at_planet(state, colony->id);
        const auto* current = station ? station_stargate(*station) : nullptr;
        const auto gates = stargate_technologies();
        for (auto candidate = gates.rbegin(); candidate != gates.rend(); ++candidate) {
            if (!stargate_component_available(state, view.player, candidate->component)
                || (current && current->component >= candidate->component)) continue;
            auto fit = station ? station_loadout(*station) : StationLoadout{};
            fit.hull = std::max(fit.hull, candidate->minimumHull);
            std::erase_if(fit.components, [](auto component) { return stargate_spec(component) != nullptr; });
            fit.components.push_back(candidate->component);
            StationConstruction construction{fit, station ? station->id : 0};
            if (!station_construction_valid(state, *colony, construction)
                || !mineral_cargo_sufficient(colony->minerals, station_mineral_cost(fit))) continue;
            plan.orders.orders.push_back(QueueStationOrder{colony->id, std::move(construction)});
            explain(reason, "Build " + std::string(candidate->name) + " at colony " + std::to_string(colony->id));
            break;
        }
    }
}

} // namespace suns::computer_detail
