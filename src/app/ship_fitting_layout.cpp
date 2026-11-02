#include "ship_fitting_layout.hpp"

#include <algorithm>
#include <set>

namespace suns {
namespace {

struct BankAnchor { int column, row, width{1}; };
struct DeckProfile { HullDeckShape shape; std::vector<BankAnchor> banks; };

DeckProfile deck_profile(ShipHullType hull)
{
    using H = ShipHullType;
    using S = HullDeckShape;
    // Each entry follows the hull's bank order. Bow is right, engines are aft.
    // Cells within one bank remain adjacent, with the original bank identity.
    switch (hull) {
    case H::Scout: return {S::Dart, {{0,1},{3,0}}};
    case H::LightTransport: return {S::Transport, {{0,1},{3,0,2}}};
    case H::MediumTransport: return {S::Transport, {{0,1},{3,0,3}}};
    case H::HeavyTransport: return {S::Transport, {{0,1},{3,0,3}}};
    case H::Utility: return {S::Support, {{0,1},{3,0,3}}};
    case H::RemoteMiner: return {S::Miner, {{0,1},{2,1},{4,0}}};
    case H::MiniColonyShip: return {S::Transport, {{0,1},{3,1}}};
    case H::SurveyProbe: return {S::Dart, {{0,1},{5,1},{2,0},{3,2}}};
    case H::ExpeditionBase: return {S::Base, {{0,2,2},{2,0},{4,0},{6,1},{6,4},{4,5},{2,5},{2,1},{2,4},{5,2},{4,2}}};
    case H::SmallFreighter: return {S::Transport, {{0,1},{4,1},{2,0}}};
    case H::MediumFreighter: return {S::Transport, {{0,2},{5,2},{3,0}}};
    case H::LargeFreighter: return {S::Transport, {{0,1},{5,1},{2,0,2}}};
    case H::SuperFreighter: return {S::Transport, {{0,1},{6,1},{2,0,2},{2,4,2}}};
    case H::StarsScout: return {S::Dart, {{0,1},{4,1},{2,0}}};
    case H::Frigate: return {S::Combat, {{0,1},{4,0},{2,1},{3,3,2}}};
    case H::Destroyer: return {S::Combat, {{0,1},{5,0},{5,2},{3,1},{2,0},{2,2},{4,1}}};
    case H::Cruiser: return {S::Combat, {{0,1},{3,0},{3,3},{5,0,2},{5,3,2},{3,1},{2,1}}};
    case H::BattleCruiser: return {S::Combat, {{0,2},{2,0,2},{2,5,2},{5,0,3},{5,5,3},{3,1},{4,1}}};
    case H::Battleship: return {S::Combat, {{0,2},{3,3},{1,1,2},{6,0,2},{6,5,2},{4,1,2},{4,6,2},{6,3,2},{1,5,2},{4,2},{5,2}}};
    case H::Dreadnought: return {S::Combat, {{0,4},{1,0,2},{1,10,2},{6,0,2},{6,9,2},{2,2,2},{2,8,2},{4,0,2},{4,8,2},{1,4,2},{6,3,2},{6,6,2},{4,5}}};
    case H::Privateer: return {S::Transport, {{0,1},{2,0},{5,1},{3,0},{3,2}}};
    case H::Rogue: return {S::Support, {{0,2},{2,1},{4,0,2},{6,2},{4,1},{4,3},{4,5,2},{3,1},{3,4}}};
    case H::Galleon: return {S::Transport, {{0,1},{2,0,2},{2,5,2},{4,0,3},{4,5,3},{6,2},{3,2},{4,2}}};
    case H::StarsMiniColonyShip: return {S::Transport, {{0,0},{3,1}}};
    case H::ColonyShip: return {S::Transport, {{0,1},{4,1}}};
    case H::MiniBomber: return {S::Bomber, {{0,1},{3,0}}};
    case H::B17Bomber: return {S::Bomber, {{0,2},{3,0,2},{3,4,2},{5,2}}};
    case H::StealthBomber: return {S::Bomber, {{0,2},{3,0,2},{3,5,2},{6,3},{2,2}}};
    case H::B52Bomber: return {S::Bomber, {{0,3},{3,0,2},{5,0,2},{3,6,2},{5,6,2},{6,3},{2,3}}};
    case H::MidgetMiner: return {S::Miner, {{0,1},{3,0}}};
    case H::MiniMiner: return {S::Miner, {{0,1},{2,1},{4,0},{4,2}}};
    case H::Miner: return {S::Miner, {{0,2},{2,2},{4,0,2},{6,1},{4,5,2},{6,4}}};
    case H::MaxiMiner: return {S::Miner, {{0,2},{2,2},{4,0,2},{7,1},{4,5,2},{7,5}}};
    case H::UltraMiner: return {S::Miner, {{0,3},{2,2},{4,0,2},{6,0},{4,5,2},{6,5}}};
    case H::FuelTransport: return {S::Support, {{0,1},{3,0}}};
    case H::SuperFuelXport: return {S::Support, {{0,1},{4,0},{5,2}}};
    case H::MiniMineLayer: return {S::Support, {{0,1},{3,0,2},{3,3,2},{5,1}}};
    case H::SuperMineLayer: return {S::Support, {{0,2},{4,0,2},{4,5,2},{1,1},{2,4},{6,3}}};
    case H::Nubian: return {S::Support, {{0,2},{2,0},{3,0},{4,0},{5,0},{6,0},{7,0},{2,4},{3,4},{4,4},{5,4},{6,4},{7,4}}};
    case H::MiniMorph: return {S::Support, {{0,2},{3,1,3},{6,2},{4,2},{4,4},{2,0,2},{2,5,2}}};
    case H::MetaMorph: return {S::Support, {{0,2},{3,2,2},{2,0,2},{5,0,2},{7,3},{2,7,2},{5,7,2}}};
    }
    return {S::Support, {}};
}

} // namespace

HullFittingLayout ship_fitting_layout(const ShipHullSpec& hull)
{
    const auto profile = deck_profile(hull.type);
    HullFittingLayout result{hull.fittingSlots, profile.shape};
    std::vector<int> keys;
    std::vector<std::vector<std::size_t>> groups;
    for (std::size_t i = 0; i < result.cells.size(); ++i) {
        const auto& slot = result.cells[i];
        const int key = slot.bank ? slot.bank : 256 + int(slot.category);
        const auto found = std::find(keys.begin(), keys.end(), key);
        if (found == keys.end()) { keys.push_back(key); groups.push_back({i}); }
        else groups[static_cast<std::size_t>(found - keys.begin())].push_back(i);
    }
    std::set<std::pair<int,int>> occupied;
    for (std::size_t bank = 0; bank < groups.size(); ++bank) {
        auto anchor = bank < profile.banks.size() ? profile.banks[bank]
            : BankAnchor{0, result.rows + 1, 4};
        const auto& cells = groups[bank];
        const auto fits = [&] {
            for (std::size_t i = 0; i < cells.size(); ++i)
                if (occupied.contains({anchor.column + int(i) % anchor.width,
                    anchor.row + int(i) / anchor.width})) return false;
            return true;
        };
        // A future larger bank still keeps every ID and stays disjoint.
        while (!fits()) ++anchor.row;
        for (std::size_t i = 0; i < cells.size(); ++i) {
            auto& slot = result.cells[cells[i]];
            const int column = anchor.column + int(i) % anchor.width;
            const int row = anchor.row + int(i) / anchor.width;
            slot.column = static_cast<std::uint8_t>(column);
            slot.row = static_cast<std::uint8_t>(row);
            occupied.emplace(column,row);
            result.columns = std::max(result.columns,column + 1);
            result.rows = std::max(result.rows,row + 1);
        }
    }
    return result;
}

} // namespace suns
