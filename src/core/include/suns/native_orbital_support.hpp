#pragma once
#include "suns/native_orbital.hpp"

namespace suns {
inline constexpr std::uint64_t kNativeSupportPopulationPerUnit = 25'000;
inline constexpr std::uint16_t kNativeSupportReserveBills = 3;
inline constexpr std::uint16_t kNativeSupportMaxAnnualUnits = 80;
inline constexpr std::uint16_t kNativeSupportMaxReserveUnits = kNativeSupportReserveBills * kNativeSupportMaxAnnualUnits;
inline constexpr std::uint32_t kNativeSupportWorkPerUnit = 2;

struct NativeOrbitalSupportQuote {
    HabitatService service{HabitatService::Empty};
    std::uint16_t requiredUnits{}, directUnits{}, reserveUsed{}, reserveAdded{}, reserveAfter{}, mineralUnitsPaid{};
    std::uint32_t productionPaid{}, output{};
    std::uint64_t populationLost{};
};
// Current-condition arithmetic only: no mining, arrivals, research or future turn.
[[nodiscard]] NativeOrbitalSupportQuote native_orbital_support_quote(const GameState&, const Planet&);
[[nodiscard]] std::string native_orbital_support_name(HabitatService);
// Exactly one debit/loss per annual production boundary, including after reload.
std::uint32_t service_native_orbit(const GameState&, Planet&);
// Radiation precedes growth; late arrivals cannot enlarge the paid allowance.
void grow_native_orbit(const GameState&, Planet&);
[[nodiscard]] std::int64_t projected_native_population_change(const GameState&, const Planet&);
void publish_native_support_reports(GameState&, const GameState& previous);
}
