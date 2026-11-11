#pragma once
#include "suns/native_orbital.hpp"
#include <QString>

namespace suns {
inline QString native_orbital_radiation_text(const NativeOrbitalRadiationStatus& dose)
{
    return QString("Orbital radiation: background %1 - protection %2 = exposure %3; tolerance %4.\n"
                   "Estimated radiation losses at current conditions: %5 residents / year.")
        .arg(dose.background).arg(dose.protection).arg(dose.exposure).arg(dose.tolerance)
        .arg(qulonglong(dose.annualLoss));
}
}
