#include "suns/game_state.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <stdexcept>
#include <unordered_set>

namespace suns {

namespace {

std::uint64_t bounded(std::mt19937_64& rng, std::uint64_t upperExclusive)
{
    return upperExclusive == 0 ? 0 : rng() % upperExclusive;
}

StarClass generated_star_class(std::mt19937_64& rng)
{
    const auto roll = bounded(rng, 100);
    if (roll < 4) {
        return StarClass::BlueWhite;
    }
    if (roll < 10) {
        return StarClass::White;
    }
    if (roll < 20) {
        return StarClass::YellowWhite;
    }
    if (roll < 35) {
        return StarClass::Yellow;
    }
    if (roll < 62) {
        return StarClass::Orange;
    }
    return StarClass::Red;
}

std::string generated_star_name(
    std::mt19937_64& rng,
    std::size_t index,
    std::unordered_set<std::string>& usedNames)
{
    static constexpr std::array<const char*, 26> beginnings = {
        "Al", "Ari", "Bel", "Cor", "Dar", "Eri", "Fen", "Gal", "Hel", "Ily", "Jan", "Kal", "Lor",
        "Mer", "Nor", "Ori", "Pra", "Qua", "Rig", "Sar", "Tal", "Uma", "Ver", "Wex", "Yri", "Zen",
    };
    static constexpr std::array<const char*, 24> endings = {
        "aris", "ora", "ion", "eth", "ara", "os", "ea", "iri", "on", "alis", "eron", "une",
        "ax", "ira", "iel", "or", "eus", "oria", "ionis", "era", "oth", "arae", "eron", "is",
    };

    std::string name = std::string(beginnings[bounded(rng, beginnings.size())])
        + endings[bounded(rng, endings.size())];

    if (!usedNames.insert(name).second) {
        name += "-" + std::to_string(index);
        usedNames.insert(name);
    }
    return name;
}

std::string generated_planet_name(std::mt19937_64& rng, const std::string& starName)
{
    static constexpr std::array<const char*, 4> numerals = {"II", "III", "IV", "V"};
    return starName + " " + numerals[bounded(rng, numerals.size())];
}

std::uint32_t generated_habitability(std::mt19937_64& rng)
{
    const auto a = bounded(rng, 86);
    const auto b = bounded(rng, 86);
    return static_cast<std::uint32_t>(15 + (a + b) / 2);
}

bool far_enough(Position candidate, const std::vector<StarSystem>& stars, double minimumSeparation)
{
    const double minimumSquared = minimumSeparation * minimumSeparation;
    return std::all_of(stars.begin(), stars.end(), [&](const StarSystem& star) {
        const double dx = candidate.x - star.position.x;
        const double dy = candidate.y - star.position.y;
        return dx * dx + dy * dy >= minimumSquared;
    });
}

Position generated_position(
    std::mt19937_64& rng,
    const GalaxyConfig& config,
    const std::vector<StarSystem>& stars)
{
    const int halfWidth = std::max(250, static_cast<int>(config.width / 2.0));
    const int halfHeight = std::max(200, static_cast<int>(config.height / 2.0));
    const double requestedSeparation = std::clamp(config.minimumSeparation, 24.0, 100.0);

    for (int relaxation = 0; relaxation < 6; ++relaxation) {
        const double separation = requestedSeparation * std::pow(0.90, relaxation);
        for (int attempt = 0; attempt < 12000; ++attempt) {
            const auto x = static_cast<int>(bounded(rng, static_cast<std::uint64_t>(halfWidth * 2 + 1))) - halfWidth;
            const auto y = static_cast<int>(bounded(rng, static_cast<std::uint64_t>(halfHeight * 2 + 1))) - halfHeight;
            const Position candidate{static_cast<double>(x), static_cast<double>(y)};
            if (far_enough(candidate, stars, separation)) {
                return candidate;
            }
        }
    }

    throw std::runtime_error("Unable to place generated star systems with the requested galaxy density");
}

} // namespace

const StarSystem* find_star(const GameState& state, StarId id)
{
    const auto it = std::find_if(state.stars.begin(), state.stars.end(), [id](const StarSystem& star) {
        return star.id == id;
    });
    return it == state.stars.end() ? nullptr : &*it;
}

const Planet* find_planet_at_star(const GameState& state, StarId star)
{
    const auto it = std::find_if(state.planets.begin(), state.planets.end(), [star](const Planet& planet) {
        return planet.star == star;
    });
    return it == state.planets.end() ? nullptr : &*it;
}

const Player* find_player(const GameState& state, PlayerId id)
{
    const auto it = std::find_if(state.players.begin(), state.players.end(), [id](const Player& player) {
        return player.id == id;
    });
    return it == state.players.end() ? nullptr : &*it;
}

bool same_position(Position a, Position b)
{
    constexpr double epsilon = 0.000001;
    return std::abs(a.x - b.x) < epsilon && std::abs(a.y - b.y) < epsilon;
}

double distance_between(Position a, Position b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

double fleet_speed(FleetRole role)
{
    return role == FleetRole::Scout ? kScoutTravelSpeed : kColonyShipTravelSpeed;
}

std::uint32_t travel_turns(Position from, Position to, double speed)
{
    if (speed <= 0.0 || same_position(from, to)) {
        return 0;
    }
    return static_cast<std::uint32_t>(std::ceil(distance_between(from, to) / speed));
}

std::uint32_t fleet_eta(const Fleet& fleet)
{
    return fleet.destination ? travel_turns(fleet.position, *fleet.destination, fleet_speed(fleet.role)) : 0;
}

bool is_surveyed(const GameState& state, PlayerId player, StarId star)
{
    const auto* knownPlayer = find_player(state, player);
    if (!knownPlayer) {
        return false;
    }

    return std::find(
               knownPlayer->surveyedStars.begin(),
               knownPlayer->surveyedStars.end(),
               star)
        != knownPlayer->surveyedStars.end();
}

void mark_surveyed(GameState& state, PlayerId player, StarId star)
{
    const auto it = std::find_if(state.players.begin(), state.players.end(), [player](const Player& candidate) {
        return candidate.id == player;
    });
    if (it == state.players.end()) {
        return;
    }

    if (std::find(it->surveyedStars.begin(), it->surveyedStars.end(), star) == it->surveyedStars.end()) {
        it->surveyedStars.push_back(star);
    }
}

std::uint64_t population_capacity(const Planet& planet)
{
    return static_cast<std::uint64_t>(planet.habitability) * 25;
}

std::uint64_t projected_population_growth(const Planet& planet)
{
    if (planet.owner == 0 || planet.population == 0 || planet.habitability == 0) {
        return 0;
    }

    const auto capacity = population_capacity(planet);
    if (capacity == 0 || planet.population >= capacity) {
        return 0;
    }

    const auto rawGrowth = std::max<std::uint64_t>(
        1,
        planet.population * planet.habitability / 1000);
    const auto headroom = capacity - planet.population;
    auto growth = rawGrowth * headroom / capacity;
    if (growth == 0) {
        growth = 1;
    }
    return std::min(growth, headroom);
}

std::uint32_t colony_output(const Planet& planet)
{
    if (planet.owner == 0) {
        return 0;
    }

    return planet.industry + static_cast<std::uint32_t>(planet.population / 500);
}

GameState generate_game(const GalaxyConfig& config)
{
    const auto starCount = std::clamp<std::size_t>(config.starCount, 2, 64);
    std::mt19937_64 rng(config.seed);

    GameState state;
    state.galaxySeed = config.seed;
    state.players.push_back({1, "Terrans", {1}});
    state.stars.reserve(starCount);
    state.planets.reserve(starCount);

    state.stars.push_back({1, "Sol", {0.0, 0.0}, StarClass::Yellow});
    state.planets.push_back({1, 1, "Earth", 100, 1, 1000, 4, 0, {}});

    std::unordered_set<std::string> usedNames{"Sol"};
    for (std::size_t index = 2; index <= starCount; ++index) {
        const auto id = static_cast<StarId>(index);
        const auto name = generated_star_name(rng, index, usedNames);
        const auto position = generated_position(rng, config, state.stars);
        const auto stellarClass = generated_star_class(rng);

        state.stars.push_back({id, name, position, stellarClass});
        state.planets.push_back({
            static_cast<PlanetId>(index),
            id,
            generated_planet_name(rng, name),
            generated_habitability(rng),
            0,
            0,
            1,
            0,
            {},
        });
    }

    state.fleets.push_back({1, 1, "Scout 1", FleetRole::Scout, {0.0, 0.0}});
    state.nextFleetId = 2;
    return state;
}

GameState make_demo_game()
{
    GameState state;
    state.galaxySeed = 0;
    state.players.push_back({1, "Terrans", {1}});

    state.stars = {
        {1, "Sol", {0.0, 0.0}, StarClass::Yellow},
        {2, "Alpha Centauri", {86.0, -42.0}, StarClass::YellowWhite},
        {3, "Sirius", {-118.0, 76.0}, StarClass::BlueWhite},
        {4, "Vega", {154.0, 96.0}, StarClass::BlueWhite},
        {5, "Altair", {42.0, 142.0}, StarClass::White},
        {6, "Tau Ceti", {-72.0, -126.0}, StarClass::Yellow},
        {7, "Epsilon Eridani", {142.0, -132.0}, StarClass::Orange},
        {8, "Procyon", {-168.0, -34.0}, StarClass::YellowWhite},
    };

    state.planets = {
        {1, 1, "Earth", 100, 1, 1000, 4, 0, {}},
        {2, 2, "Centauri II", 82, 0, 0, 1, 0, {}},
        {3, 3, "Sirius III", 48, 0, 0, 1, 0, {}},
        {4, 4, "Vega II", 71, 0, 0, 1, 0, {}},
        {5, 5, "Altair IV", 63, 0, 0, 1, 0, {}},
        {6, 6, "Tau Ceti III", 91, 0, 0, 1, 0, {}},
        {7, 7, "Eridani II", 56, 0, 0, 1, 0, {}},
        {8, 8, "Procyon II", 76, 0, 0, 1, 0, {}},
    };

    state.fleets.push_back({1, 1, "Scout 1", FleetRole::Scout, {0.0, 0.0}});
    state.nextFleetId = 2;
    return state;
}

} // namespace suns
