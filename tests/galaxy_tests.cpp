#include "suns/game_state.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

int main()
{
    using namespace suns;
    const auto normal = galaxy_preset(GalaxySize::Tiny, GalaxyDensity::Normal, 42);
    assert(normal.starCount == 32 && normal.width == 400 && normal.height == 400);
    assert(galaxy_preset(GalaxySize::Huge, GalaxyDensity::Normal, 42).starCount == 800);
    assert(galaxy_preset(GalaxySize::Huge, GalaxyDensity::Packed, 42).starCount == 1000);
    for (int size = 0; size < 5; ++size) {
        std::size_t previous = 0;
        for (int density = 0; density < 4; ++density) {
            auto config = galaxy_preset(static_cast<GalaxySize>(size), static_cast<GalaxyDensity>(density), 42);
            assert(config.starCount >= previous);
            previous = config.starCount;
            const auto state = generate_game(config);
            assert(state.stars.size() == config.starCount);
            assert(state.planets.size() == config.starCount);
            assert(state.wormholeRules.width == config.width);
            assert(state.wormholeRules.height == config.height);
            std::set<std::string> names;
            for (const auto& star : state.stars) {
                assert(names.insert(star.name).second);
                assert(std::abs(star.position.x) <= config.width / 2);
                assert(std::abs(star.position.y) <= config.height / 2);
                for (const auto& other : state.stars) {
                    if (star.id == other.id) continue;
                    assert(std::hypot(star.position.x - other.position.x,
                        star.position.y - other.position.y) >= config.minimumSeparation);
                }
            }
            const auto again = generate_game(config);
            for (std::size_t i = 0; i < state.stars.size(); ++i) {
                assert(state.stars[i].name == again.stars[i].name);
                assert(same_position(state.stars[i].position, again.stars[i].position));
            }
        }
    }
    // Test the density as experienced by a scout, across independent seeds.
    // The old 900 x 650 / 24 / 48 configuration averaged about 98 ly.
    double nearestTotal = 0;
    std::size_t samples = 0;
    for (std::uint64_t seed = 1; seed <= 100; ++seed) {
        auto config = normal;
        config.seed = seed;
        const auto state = generate_game(config);
        for (const auto& star : state.stars) {
            double nearest = std::numeric_limits<double>::max();
            for (const auto& other : state.stars) {
                if (star.id != other.id) nearest = std::min(nearest,
                    std::hypot(star.position.x - other.position.x, star.position.y - other.position.y));
            }
            nearestTotal += nearest;
            ++samples;
        }
    }
    assert(nearestTotal / samples > 30 && nearestTotal / samples < 50);
    // Custom rectangular maps use their real bounds, without the old 500x400 floor.
    const auto custom = generate_game({42, 24, 410, 295, 15});
    assert(custom.wormholeRules.width == 410 && custom.wormholeRules.height == 295);
    for (const auto& star : custom.stars) {
        assert(std::abs(star.position.x) <= 205 && std::abs(star.position.y) <= 147.5);
    }
    auto invalid = normal;
    invalid.width = std::numeric_limits<double>::quiet_NaN();
    bool rejected = false;
    try { (void)generate_game(invalid); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
