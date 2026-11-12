#pragma once
#include "suns/native_orbital.hpp"
#include "suns/native_orbital_support.hpp"
#include <QString>

namespace suns {
inline QString native_orbital_support_text(const GameState& received, const Planet& planet, bool compact = false)
{
    if (!native_orbital_station(received, planet)) return {};
    const auto support = native_orbital_support_quote(received, planet);
    if (compact) {
        const auto name = support.service == HabitatService::Normal ? "Normal"
            : support.service == HabitatService::Reserve ? "On reserves"
            : support.service == HabitatService::Failing ? "Supply shortage" : "Empty";
        return QString("Life support ≈ %1; reserve %2 kits.\nPopulation change ≈ %3 / year.")
            .arg(name).arg(planet.nativeOrbit.reserveUnits)
            .arg(qlonglong(projected_native_population_change(received, planet)));
    }
    return QString("Life support at current stock: %1; reserve %2 kits (up to 25,000 residents/year each).\n"
                   "Approx. next debit: %3 work; I/B/G %4/%4/%4 kt (including reserve kits). Population change: %5 / year.")
        .arg(QString::fromStdString(native_orbital_support_name(support.service)))
        .arg(planet.nativeOrbit.reserveUnits).arg(support.productionPaid).arg(support.mineralUnitsPaid)
        .arg(qlonglong(projected_native_population_change(received, planet)));
}
inline QString native_orbital_radiation_text(const NativeOrbitalRadiationStatus& dose)
{
    return QString("Orbital radiation: background %1 - protection %2 = exposure %3; tolerance %4.\n"
                   "Estimated radiation losses at current conditions: %5 residents / year.")
        .arg(dose.background).arg(dose.protection).arg(dose.exposure).arg(dose.tolerance)
        .arg(qulonglong(dose.annualLoss));
}
}
