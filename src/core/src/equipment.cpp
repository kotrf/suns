#include "suns/equipment.hpp"
#include "suns/hulls.hpp"

#include <algorithm>
#include <sstream>

namespace suns {
namespace {
EquipmentTechnology make_equipment(ShipComponentType component, const char* name,
    ShipComponentKind kind, double mass, std::uint32_t cost, MineralCargo minerals,
    std::array<std::uint8_t, kResearchFieldCount> levels, EquipmentAccess access,
    bool excludesIS, bool excludesWM, bool transportOnly, ShipComponentSpec effects)
{
    return {component, name, kind, mass, cost, minerals, levels, access,
        excludesIS, excludesWM, transportOnly, std::move(effects)};
}
} // namespace

std::span<const EquipmentTechnology> equipment_technologies()
{
    // Numerical facts from the original component export UNEDITED.MOD, with
    // operational ratings cross-checked against the original Technical Guide.
    // MOD is authoritative where the printed guide has discrepant masses.
    using C = ShipComponentType;
    using K = ShipComponentKind;
    using A = EquipmentAccess;
    static const std::vector<EquipmentTechnology> catalog{
        make_equipment(C::Laser, "Laser", K::BeamWeapon, 1, 5, {0, 6, 0}, {0, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.weaponPower = 10, .weaponRange = 1, .weaponInitiative = 9}),
        make_equipment(C::XRayLaser, "X-Ray Laser", K::BeamWeapon, 1, 6, {0, 6, 0}, {0, 0, 0, 0, 0, 3}, A::Any, false, false, false, {.weaponPower = 16, .weaponRange = 1, .weaponInitiative = 9}),
        make_equipment(C::MiniGun, "Mini Gun", K::BeamWeapon, 3, 10, {0, 16, 0}, {0, 0, 0, 0, 0, 5}, A::InnerStrength, false, false, false, {.weaponPower = 13, .weaponRange = 2, .weaponInitiative = 12, .gatling = true}),
        make_equipment(C::YakimoraLightPhaser, "Yakimora Light Phaser", K::BeamWeapon, 1, 7, {0, 8, 0}, {0, 0, 0, 0, 0, 6}, A::Any, false, false, false, {.weaponPower = 26, .weaponRange = 1, .weaponInitiative = 9}),
        make_equipment(C::Blackjack, "Blackjack", K::BeamWeapon, 10, 7, {0, 16, 0}, {0, 0, 0, 0, 0, 7}, A::Any, false, false, false, {.weaponPower = 90, .weaponInitiative = 10}),
        make_equipment(C::PhaserBazooka, "Phaser Bazooka", K::BeamWeapon, 2, 11, {0, 8, 0}, {0, 0, 0, 0, 0, 8}, A::Any, false, false, false, {.weaponPower = 26, .weaponRange = 2, .weaponInitiative = 7}),
        make_equipment(C::PulsedSapper, "Pulsed Sapper", K::BeamWeapon, 1, 12, {0, 0, 4}, {5, 0, 0, 0, 0, 9}, A::Any, false, false, false, {.weaponPower = 82, .weaponRange = 3, .weaponInitiative = 14, .shieldOnly = true}),
        make_equipment(C::ColloidalPhaser, "Colloidal Phaser", K::BeamWeapon, 2, 18, {0, 14, 0}, {0, 0, 0, 0, 0, 10}, A::Any, false, false, false, {.weaponPower = 26, .weaponRange = 3, .weaponInitiative = 5}),
        make_equipment(C::GatlingGun, "Gatling Gun", K::BeamWeapon, 3, 13, {0, 20, 0}, {0, 0, 0, 0, 0, 11}, A::Any, false, false, false, {.weaponPower = 31, .weaponRange = 2, .weaponInitiative = 12, .gatling = true}),
        make_equipment(C::MiniBlaster, "Mini Blaster", K::BeamWeapon, 1, 9, {0, 10, 0}, {0, 0, 0, 0, 0, 12}, A::Any, false, false, false, {.weaponPower = 66, .weaponRange = 1, .weaponInitiative = 9}),
        make_equipment(C::Bludgeon, "Bludgeon", K::BeamWeapon, 10, 9, {0, 22, 0}, {0, 0, 0, 0, 0, 13}, A::Any, false, false, false, {.weaponPower = 231, .weaponInitiative = 10}),
        make_equipment(C::MarkIVBlaster, "Mark IV Blaster", K::BeamWeapon, 2, 15, {0, 12, 0}, {0, 0, 0, 0, 0, 14}, A::Any, false, false, false, {.weaponPower = 66, .weaponRange = 2, .weaponInitiative = 7}),
        make_equipment(C::PhasedSapper, "Phased Sapper", K::BeamWeapon, 1, 16, {0, 0, 6}, {8, 0, 0, 0, 0, 15}, A::Any, false, false, false, {.weaponPower = 211, .weaponRange = 3, .weaponInitiative = 14, .shieldOnly = true}),
        make_equipment(C::HeavyBlaster, "Heavy Blaster", K::BeamWeapon, 2, 25, {0, 20, 0}, {0, 0, 0, 0, 0, 16}, A::Any, false, false, false, {.weaponPower = 66, .weaponRange = 3, .weaponInitiative = 5}),
        make_equipment(C::GatlingNeutrinoCannon, "Gatling Neutrino Cannon", K::BeamWeapon, 3, 17, {0, 28, 0}, {0, 0, 0, 0, 0, 17}, A::WarMonger, false, false, false, {.weaponPower = 80, .weaponRange = 2, .weaponInitiative = 13, .gatling = true}),
        make_equipment(C::MyopicDisruptor, "Myopic Disruptor", K::BeamWeapon, 1, 12, {0, 14, 0}, {0, 0, 0, 0, 0, 18}, A::Any, false, false, false, {.weaponPower = 169, .weaponRange = 1, .weaponInitiative = 9}),
        make_equipment(C::Blunderbuss, "Blunderbuss", K::BeamWeapon, 10, 13, {0, 30, 0}, {0, 0, 0, 0, 0, 19}, A::WarMonger, false, false, false, {.weaponPower = 592, .weaponInitiative = 11}),
        make_equipment(C::Disruptor, "Disruptor", K::BeamWeapon, 2, 20, {0, 16, 0}, {0, 0, 0, 0, 0, 20}, A::Any, false, false, false, {.weaponPower = 169, .weaponRange = 2, .weaponInitiative = 8}),
        make_equipment(C::MultiContainedMunition, "Multi Contained Munition", K::BeamWeapon, 8, 40, {6, 40, 6}, {21, 0, 0, 16, 12, 21}, A::MysteryTrader, false, false, false, {.weaponPower = 140, .weaponRange = 3, .weaponInitiative = 6}),
        make_equipment(C::SyncroSapper, "Syncro Sapper", K::BeamWeapon, 1, 21, {0, 0, 8}, {11, 0, 0, 0, 0, 21}, A::Any, false, false, false, {.weaponPower = 541, .weaponRange = 3, .weaponInitiative = 14, .shieldOnly = true}),
        make_equipment(C::MegaDisruptor, "Mega Disruptor", K::BeamWeapon, 2, 33, {0, 30, 0}, {0, 0, 0, 0, 0, 22}, A::Any, false, false, false, {.weaponPower = 169, .weaponRange = 3, .weaponInitiative = 6}),
        make_equipment(C::BigMuthaCannon, "Big Mutha Cannon", K::BeamWeapon, 3, 23, {0, 36, 0}, {0, 0, 0, 0, 0, 23}, A::Any, false, false, false, {.weaponPower = 204, .weaponRange = 2, .weaponInitiative = 13, .gatling = true}),
        make_equipment(C::StreamingPulverizer, "Streaming Pulverizer", K::BeamWeapon, 1, 16, {0, 20, 0}, {0, 0, 0, 0, 0, 24}, A::Any, false, false, false, {.weaponPower = 433, .weaponRange = 1, .weaponInitiative = 9}),
        make_equipment(C::AntiMatterPulverizer, "Anti-Matter Pulverizer", K::BeamWeapon, 2, 27, {0, 22, 0}, {0, 0, 0, 0, 0, 26}, A::Any, false, false, false, {.weaponPower = 433, .weaponRange = 2, .weaponInitiative = 8}),
        make_equipment(C::AlphaTorpedo, "Alpha Torpedo", K::Torpedo, 25, 5, {9, 3, 3}, {0, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.weaponPower = 5, .weaponRange = 4, .weaponAccuracy = 35}),
        make_equipment(C::BetaTorpedo, "Beta Torpedo", K::Torpedo, 25, 6, {18, 6, 4}, {0, 1, 0, 0, 0, 5}, A::Any, false, false, false, {.weaponPower = 12, .weaponRange = 4, .weaponInitiative = 1, .weaponAccuracy = 45}),
        make_equipment(C::DeltaTorpedo, "Delta Torpedo", K::Torpedo, 25, 8, {22, 8, 5}, {0, 2, 0, 0, 0, 10}, A::Any, false, false, false, {.weaponPower = 26, .weaponRange = 4, .weaponInitiative = 1, .weaponAccuracy = 60}),
        make_equipment(C::EpsilonTorpedo, "Epsilon Torpedo", K::Torpedo, 25, 10, {30, 10, 6}, {0, 3, 0, 0, 0, 14}, A::Any, false, false, false, {.weaponPower = 48, .weaponRange = 5, .weaponInitiative = 2, .weaponAccuracy = 65}),
        make_equipment(C::RhoTorpedo, "Rho Torpedo", K::Torpedo, 25, 12, {34, 12, 8}, {0, 4, 0, 0, 0, 18}, A::Any, false, false, false, {.weaponPower = 90, .weaponRange = 5, .weaponInitiative = 2, .weaponAccuracy = 75}),
        make_equipment(C::UpsilonTorpedo, "Upsilon Torpedo", K::Torpedo, 25, 15, {40, 14, 9}, {0, 5, 0, 0, 0, 22}, A::Any, false, false, false, {.weaponPower = 169, .weaponRange = 5, .weaponInitiative = 3, .weaponAccuracy = 75}),
        make_equipment(C::OmegaTorpedo, "Omega Torpedo", K::Torpedo, 25, 18, {52, 18, 12}, {0, 6, 0, 0, 0, 26}, A::Any, false, false, false, {.weaponPower = 316, .weaponRange = 5, .weaponInitiative = 4, .weaponAccuracy = 80}),
        make_equipment(C::AntiMatterTorpedo, "Anti Matter Torpedo", K::Torpedo, 8, 50, {3, 8, 1}, {0, 12, 0, 0, 21, 11}, A::MysteryTrader, false, false, false, {.weaponPower = 60, .weaponRange = 6, .weaponAccuracy = 85}),
        make_equipment(C::JihadMissile, "Jihad Missile", K::Torpedo, 35, 13, {37, 13, 9}, {0, 6, 0, 0, 0, 12}, A::Any, false, false, false, {.weaponPower = 85, .weaponRange = 5, .weaponAccuracy = 20, .missile = true}),
        make_equipment(C::JuggernautMissile, "Juggernaut Missile", K::Torpedo, 35, 16, {48, 16, 11}, {0, 8, 0, 0, 0, 16}, A::Any, false, false, false, {.weaponPower = 150, .weaponRange = 5, .weaponInitiative = 1, .weaponAccuracy = 20, .missile = true}),
        make_equipment(C::DoomsdayMissile, "Doomsday Missile", K::Torpedo, 35, 20, {60, 20, 13}, {0, 10, 0, 0, 0, 20}, A::Any, false, false, false, {.weaponPower = 280, .weaponRange = 6, .weaponInitiative = 2, .weaponAccuracy = 25, .missile = true}),
        make_equipment(C::ArmageddonMissile, "Armageddon Missile", K::Torpedo, 35, 24, {67, 23, 16}, {0, 10, 0, 0, 0, 24}, A::Any, false, false, false, {.weaponPower = 525, .weaponRange = 6, .weaponInitiative = 3, .weaponAccuracy = 30, .missile = true}),
        make_equipment(C::LadyFingerBomb, "Lady Finger Bomb", K::Bomb, 40, 5, {1, 20, 0}, {0, 0, 0, 0, 0, 2}, A::Any, false, false, false, {.bombPopulationPercent = 0.6, .bombMinimumKills = 300, .bombInstallations = 2}),
        make_equipment(C::BlackCatBomb, "Black Cat Bomb", K::Bomb, 45, 7, {1, 22, 0}, {0, 0, 0, 0, 0, 5}, A::Any, false, false, false, {.bombPopulationPercent = 0.9, .bombMinimumKills = 300, .bombInstallations = 4}),
        make_equipment(C::M70Bomb, "M-70 Bomb", K::Bomb, 50, 9, {1, 24, 0}, {0, 0, 0, 0, 0, 8}, A::Any, false, false, false, {.bombPopulationPercent = 1.2, .bombMinimumKills = 300, .bombInstallations = 6}),
        make_equipment(C::M80Bomb, "M-80 Bomb", K::Bomb, 55, 12, {1, 25, 0}, {0, 0, 0, 0, 0, 11}, A::Any, false, false, false, {.bombPopulationPercent = 1.7, .bombMinimumKills = 300, .bombInstallations = 7}),
        make_equipment(C::CherryBomb, "Cherry Bomb", K::Bomb, 52, 11, {1, 25, 0}, {0, 0, 0, 0, 0, 14}, A::Any, false, false, false, {.bombPopulationPercent = 2.5, .bombMinimumKills = 300, .bombInstallations = 10}),
        make_equipment(C::Lbu17Bomb, "LBU-17 Bomb", K::Bomb, 30, 7, {1, 15, 15}, {0, 0, 0, 8, 0, 5}, A::Any, false, false, false, {.bombPopulationPercent = 0.2, .bombInstallations = 16}),
        make_equipment(C::Lbu32Bomb, "LBU-32 Bomb", K::Bomb, 35, 10, {1, 24, 15}, {0, 0, 0, 10, 0, 10}, A::Any, false, false, false, {.bombPopulationPercent = 0.3, .bombInstallations = 28}),
        make_equipment(C::Lbu74Bomb, "LBU-74 Bomb", K::Bomb, 45, 14, {1, 33, 12}, {0, 0, 0, 12, 0, 15}, A::Any, false, false, false, {.bombPopulationPercent = 0.4, .bombInstallations = 45}),
        make_equipment(C::HushABoom, "Hush-a-Boom", K::Bomb, 5, 5, {1, 5, 0}, {0, 0, 0, 12, 12, 12}, A::MysteryTrader, false, false, false, {.bombPopulationPercent = 3.0, .bombInstallations = 2}),
        make_equipment(C::RetroBomb, "Retro Bomb", K::Bomb, 45, 50, {15, 15, 10}, {0, 0, 0, 0, 12, 10}, A::ClaimAdjuster, false, false, false, {.unterraformingBomb = true}),
        make_equipment(C::SmartBomb, "Smart Bomb", K::Bomb, 50, 27, {1, 22, 0}, {0, 0, 0, 0, 7, 5}, A::Any, true, false, false, {.bombPopulationPercent = 1.3, .smartBomb = true}),
        make_equipment(C::NeutronBomb, "Neutron Bomb", K::Bomb, 57, 30, {1, 30, 0}, {0, 0, 0, 0, 10, 10}, A::Any, true, false, false, {.bombPopulationPercent = 2.2, .smartBomb = true}),
        make_equipment(C::EnrichedNeutronBomb, "Enriched Neutron Bomb", K::Bomb, 64, 25, {1, 36, 0}, {0, 0, 0, 0, 12, 15}, A::Any, true, false, false, {.bombPopulationPercent = 3.5, .smartBomb = true}),
        make_equipment(C::PeerlessBomb, "Peerless Bomb", K::Bomb, 55, 32, {1, 33, 0}, {0, 0, 0, 0, 15, 22}, A::Any, true, false, false, {.bombPopulationPercent = 5.0, .smartBomb = true}),
        make_equipment(C::AnnihilatorBomb, "Annihilator Bomb", K::Bomb, 50, 28, {1, 30, 0}, {0, 0, 0, 0, 17, 26}, A::Any, true, false, false, {.bombPopulationPercent = 7.0, .smartBomb = true}),
        make_equipment(C::AlienMiner, "Alien Miner", K::Mining, 20, 20, {8, 0, 2}, {5, 0, 10, 5, 5, 0}, A::MysteryTrader, false, false, false, {.remoteMiningUnits = 10}),
        make_equipment(C::OrbitalAdjuster, "Orbital Adjuster", K::Mining, 80, 50, {25, 25, 25}, {0, 0, 0, 0, 6, 0}, A::ClaimAdjuster, false, false, false, {}),
        make_equipment(C::MineDispenser40, "Mine Dispenser 40", K::MineLayer, 25, 45, {2, 10, 8}, {0, 0, 0, 0, 0, 0}, A::SpaceDemolition, false, true, false, {.minesPerYear = 40}),
        make_equipment(C::MineDispenser50, "Mine Dispenser 50", K::MineLayer, 30, 55, {2, 12, 10}, {2, 0, 0, 0, 4, 0}, A::Any, false, true, false, {.minesPerYear = 50}),
        make_equipment(C::MineDispenser80, "Mine Dispenser 80", K::MineLayer, 30, 65, {2, 14, 10}, {3, 0, 0, 0, 7, 0}, A::SpaceDemolition, false, true, false, {.minesPerYear = 80}),
        make_equipment(C::MineDispenser130, "Mine Dispenser 130", K::MineLayer, 30, 80, {2, 18, 10}, {6, 0, 0, 0, 12, 0}, A::SpaceDemolition, false, true, false, {.minesPerYear = 130}),
        make_equipment(C::HeavyDispenser50, "Heavy Dispenser 50", K::MineLayer, 10, 50, {2, 20, 5}, {5, 0, 0, 0, 3, 0}, A::SpaceDemolition, false, true, false, {.minesPerYear = 50, .mineFieldKind = 1}),
        make_equipment(C::HeavyDispenser110, "Heavy Dispenser 110", K::MineLayer, 15, 70, {2, 30, 5}, {9, 0, 0, 0, 5, 0}, A::SpaceDemolition, false, true, false, {.minesPerYear = 110, .mineFieldKind = 1}),
        make_equipment(C::HeavyDispenser200, "Heavy Dispenser 200", K::MineLayer, 20, 90, {2, 45, 5}, {14, 0, 0, 0, 7, 0}, A::SpaceDemolition, false, true, false, {.minesPerYear = 200, .mineFieldKind = 1}),
        make_equipment(C::SpeedTrap20, "Speed Trap 20", K::MineLayer, 100, 60, {30, 0, 12}, {0, 2, 0, 0, 2, 0}, A::InnerStrengthOrSpaceDemolition, false, true, false, {.minesPerYear = 20, .mineFieldKind = 2}),
        make_equipment(C::SpeedTrap30, "Speed Trap 30", K::MineLayer, 135, 72, {32, 0, 14}, {0, 3, 0, 0, 6, 0}, A::SpaceDemolition, false, true, false, {.minesPerYear = 30, .mineFieldKind = 2}),
        make_equipment(C::SpeedTrap50, "Speed Trap 50", K::MineLayer, 140, 80, {40, 0, 15}, {0, 5, 0, 0, 11, 0}, A::SpaceDemolition, false, true, false, {.minesPerYear = 50, .mineFieldKind = 2}),
        make_equipment(C::StarsColonizationModule, "Colonization Module", K::Mechanical, 32, 10, {12, 10, 10}, {0, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.enablesColonization = true}),
        make_equipment(C::OrbitalConstructionModule, "Orbital Construction Module", K::Mechanical, 50, 20, {20, 15, 15}, {0, 0, 0, 0, 0, 0}, A::AlternateReality, false, false, false, {}),
        make_equipment(C::StarsCargoPod, "Cargo Pod", K::Mechanical, 5, 10, {5, 0, 2}, {0, 0, 3, 0, 0, 0}, A::Any, false, false, false, {.cargoCapacity = 50}),
        make_equipment(C::SuperCargoPod, "Super Cargo Pod", K::Mechanical, 7, 15, {8, 0, 2}, {3, 0, 9, 0, 0, 0}, A::Any, false, false, false, {.cargoCapacity = 100}),
        make_equipment(C::MultiCargoPod, "Multi Cargo Pod", K::Mechanical, 9, 25, {12, 0, 3}, {5, 0, 11, 5, 0, 0}, A::MysteryTrader, false, false, false, {.cargoCapacity = 250, .armor = 50}),
        make_equipment(C::StarsFuelTank, "Fuel Tank", K::Mechanical, 3, 4, {6, 0, 0}, {0, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.fuelCapacity = 250}),
        make_equipment(C::SuperFuelTank, "Super Fuel Tank", K::Mechanical, 8, 8, {8, 0, 0}, {6, 4, 14, 0, 0, 0}, A::Any, false, false, false, {.fuelCapacity = 500}),
        make_equipment(C::ManeuveringJet, "Maneuvering Jet", K::Mechanical, 5, 10, {5, 0, 5}, {2, 3, 0, 0, 0, 0}, A::Any, false, false, false, {.battleMovementBonus = 0.25}),
        make_equipment(C::Overthruster, "Overthruster", K::Mechanical, 5, 20, {10, 0, 8}, {5, 12, 0, 0, 0, 0}, A::Any, false, false, false, {.battleMovementBonus = 0.5}),
        make_equipment(C::JumpGate, "Jump Gate", K::Mechanical, 10, 40, {0, 0, 50}, {16, 20, 20, 16, 0, 0}, A::MysteryTrader, false, false, false, {}),
        make_equipment(C::BeamDeflector, "Beam Deflector", K::Mechanical, 1, 8, {0, 0, 10}, {6, 0, 6, 6, 0, 6}, A::Any, false, false, false, {.beamDeflectionPercent = 10}),
        make_equipment(C::TransportCloaking, "Transport Cloaking", K::Electrical, 1, 3, {2, 0, 2}, {0, 0, 0, 0, 0, 0}, A::SuperStealth, false, false, true, {.cloakPercent = 75}),
        make_equipment(C::StealthCloak, "Stealth Cloak", K::Electrical, 2, 5, {2, 0, 2}, {2, 0, 0, 5, 0, 0}, A::Any, false, false, false, {.cloakPercent = 35}),
        make_equipment(C::SuperStealthCloak, "Super-Stealth Cloak", K::Electrical, 3, 15, {8, 0, 8}, {4, 0, 0, 10, 0, 0}, A::Any, false, false, false, {.cloakPercent = 55}),
        make_equipment(C::UltraStealthCloak, "Ultra-Stealth Cloak", K::Electrical, 5, 25, {10, 0, 10}, {10, 0, 0, 12, 0, 0}, A::SuperStealth, false, false, false, {.cloakPercent = 85}),
        make_equipment(C::MultiFunctionPod, "Multi Function Pod", K::Electrical, 2, 15, {5, 0, 5}, {11, 11, 0, 11, 0, 0}, A::MysteryTrader, false, false, false, {}),
        make_equipment(C::BattleComputer, "Battle Computer", K::Electrical, 1, 6, {0, 0, 15}, {0, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.accuracyBonusPercent = 20, .initiativeBonus = 1}),
        make_equipment(C::BattleSuperComputer, "Battle Super Computer", K::Electrical, 1, 14, {0, 0, 25}, {5, 0, 0, 11, 0, 0}, A::Any, false, false, false, {.accuracyBonusPercent = 30, .initiativeBonus = 2}),
        make_equipment(C::BattleNexus, "Battle Nexus", K::Electrical, 1, 15, {0, 0, 30}, {10, 0, 0, 19, 0, 0}, A::Any, false, false, false, {.accuracyBonusPercent = 50, .initiativeBonus = 3}),
        make_equipment(C::Jammer10, "Jammer 10", K::Electrical, 1, 6, {0, 0, 2}, {2, 0, 0, 6, 0, 0}, A::InnerStrength, false, false, false, {.jammingPercent = 10}),
        make_equipment(C::Jammer20, "Jammer 20", K::Electrical, 1, 20, {1, 0, 5}, {4, 0, 0, 10, 0, 0}, A::Any, false, false, false, {.jammingPercent = 20}),
        make_equipment(C::Jammer30, "Jammer 30", K::Electrical, 1, 20, {1, 0, 6}, {8, 0, 0, 16, 0, 0}, A::Any, false, false, false, {.jammingPercent = 30}),
        make_equipment(C::Jammer50, "Jammer 50", K::Electrical, 1, 20, {2, 0, 7}, {16, 0, 0, 22, 0, 0}, A::InnerStrength, false, false, false, {.jammingPercent = 50}),
        make_equipment(C::EnergyCapacitor, "Energy Capacitor", K::Electrical, 1, 5, {0, 0, 8}, {7, 0, 0, 4, 0, 0}, A::Any, false, false, false, {.beamBonusPercent = 10}),
        make_equipment(C::FluxCapacitor, "Flux Capacitor", K::Electrical, 1, 5, {0, 0, 8}, {14, 0, 0, 8, 0, 0}, A::HyperExpansion, false, false, false, {.beamBonusPercent = 20}),
        make_equipment(C::EnergyDampener, "Energy Dampener", K::Electrical, 2, 50, {5, 10, 0}, {14, 8, 0, 0, 0, 0}, A::SpaceDemolition, false, false, false, {.battleMovementPenalty = 1}),
        make_equipment(C::TachyonDetector, "Tachyon Detector", K::Electrical, 1, 70, {1, 5, 0}, {8, 0, 0, 14, 0, 0}, A::InnerStrength, false, false, false, {.tachyonPercent = 5}),
        make_equipment(C::StarsAntimatterGenerator, "Anti-matter Generator", K::Electrical, 10, 10, {8, 3, 3}, {0, 0, 0, 0, 7, 12}, A::InterstellarTraveler, false, false, false, {.fuelCapacity = 200, .fuelGenerationPerTurn = 50}),
        make_equipment(C::MoleSkinShield, "Mole-skin Shield", K::Shield, 1, 4, {1, 0, 1}, {0, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.shields = 25}),
        make_equipment(C::CowHideShield, "Cow-hide Shield", K::Shield, 1, 5, {2, 0, 2}, {3, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.shields = 40}),
        make_equipment(C::WolverineDiffuseShield, "Wolverine Diffuse Shield", K::Shield, 1, 6, {3, 0, 3}, {6, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.shields = 60}),
        make_equipment(C::CrobySharmor, "Croby Sharmor", K::Shield, 10, 15, {7, 0, 4}, {7, 0, 4, 0, 0, 0}, A::InnerStrength, false, false, false, {.armor = 65, .shields = 60}),
        make_equipment(C::ShadowShield, "Shadow Shield", K::Shield, 2, 7, {3, 0, 3}, {7, 0, 0, 3, 0, 0}, A::SuperStealth, false, false, false, {.shields = 75, .cloakPercent = 35}),
        make_equipment(C::BearNeutrinoBarrier, "Bear Neutrino Barrier", K::Shield, 1, 8, {4, 0, 4}, {10, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.shields = 100}),
        make_equipment(C::LangstonShell, "Langston Shell", K::Shield, 10, 20, {10, 2, 6}, {12, 9, 0, 9, 0, 0}, A::MysteryTrader, false, false, false, {.armor = 65, .shields = 125}),
        make_equipment(C::GorillaDelagator, "Gorilla Delagator", K::Shield, 1, 11, {5, 0, 6}, {14, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.shields = 175}),
        make_equipment(C::ElephantHideFortress, "Elephant Hide Fortress", K::Shield, 1, 15, {8, 0, 10}, {18, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.shields = 300}),
        make_equipment(C::CompletePhaseShield, "Complete Phase Shield", K::Shield, 1, 20, {12, 0, 15}, {22, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.shields = 500}),
        make_equipment(C::Tritanium, "Tritanium", K::Armor, 60, 10, {5, 0, 0}, {0, 0, 0, 0, 0, 0}, A::Any, false, false, false, {.armor = 50}),
        make_equipment(C::Crobmnium, "Crobmnium", K::Armor, 56, 13, {6, 0, 0}, {0, 0, 3, 0, 0, 0}, A::Any, false, false, false, {.armor = 75}),
        make_equipment(C::CarbonicArmor, "Carbonic Armor", K::Armor, 25, 15, {0, 0, 5}, {0, 0, 0, 0, 4, 0}, A::Any, false, false, false, {.armor = 100}),
        make_equipment(C::Strobnium, "Strobnium", K::Armor, 54, 18, {8, 0, 0}, {0, 0, 6, 0, 0, 0}, A::Any, false, false, false, {.armor = 120}),
        make_equipment(C::OrganicArmor, "Organic Armor", K::Armor, 15, 20, {0, 0, 6}, {0, 0, 0, 0, 7, 0}, A::Any, false, false, false, {.armor = 175}),
        make_equipment(C::Kelarium, "Kelarium", K::Armor, 50, 25, {9, 1, 0}, {0, 0, 9, 0, 0, 0}, A::Any, false, false, false, {.armor = 180}),
        make_equipment(C::FieldedKelarium, "Fielded Kelarium", K::Armor, 50, 28, {10, 0, 2}, {4, 0, 10, 0, 0, 0}, A::InnerStrength, false, false, false, {.armor = 175, .shields = 50}),
        make_equipment(C::DepletedNeutronium, "Depleted Neutronium", K::Armor, 50, 28, {10, 0, 2}, {0, 0, 10, 3, 0, 0}, A::SuperStealth, false, false, false, {.armor = 200, .cloakPercent = 25}),
        make_equipment(C::Neutronium, "Neutronium", K::Armor, 45, 30, {11, 2, 1}, {0, 0, 12, 0, 0, 0}, A::Any, false, false, false, {.armor = 275}),
        make_equipment(C::MegaPolyShell, "Mega Poly Shell", K::Armor, 20, 65, {18, 6, 6}, {14, 0, 14, 14, 6, 0}, A::MysteryTrader, false, false, false, {.armor = 400}),
        make_equipment(C::Valanium, "Valanium", K::Armor, 40, 50, {15, 0, 0}, {0, 0, 16, 0, 0, 0}, A::Any, false, false, false, {.armor = 500}),
        make_equipment(C::Superlatanium, "Superlatanium", K::Armor, 30, 100, {25, 0, 0}, {0, 0, 24, 0, 0, 0}, A::Any, false, false, false, {.armor = 1500}),
    };
    return catalog;
}

const EquipmentTechnology* equipment_technology(ShipComponentType component)
{
    for (const auto& entry : equipment_technologies())
        if (entry.component == component) return &entry;
    return nullptr;
}

ShipComponentSpec equipment_component_spec(const EquipmentTechnology& entry)
{
    auto spec = entry.effects;
    spec.type = entry.component; spec.name = entry.name; spec.kind = entry.kind;
    spec.mass = entry.mass; spec.buildCost = entry.cost;
    return spec;
}

bool equipment_access_applicable(const GameState& state, PlayerId player, const EquipmentTechnology& entry)
{
    const auto* owner = find_player(state, player);
    if (!owner) return false;
    const auto access = owner->race.hullAccess;
    if ((entry.excludesInnerStrength && access == HullAccess::InnerStrength)
        || (entry.excludesWarMonger && access == HullAccess::WarMonger)) return false;
    switch (entry.access) {
    case EquipmentAccess::Any: return true;
    case EquipmentAccess::InnerStrength: return access == HullAccess::InnerStrength;
    case EquipmentAccess::SuperStealth: return access == HullAccess::SuperStealth;
    case EquipmentAccess::WarMonger: return access == HullAccess::WarMonger;
    case EquipmentAccess::SpaceDemolition: return access == HullAccess::SpaceDemolition;
    case EquipmentAccess::HyperExpansion: return access == HullAccess::HyperExpansion;
    case EquipmentAccess::InnerStrengthOrSpaceDemolition:
        return access == HullAccess::InnerStrength || access == HullAccess::SpaceDemolition;
    case EquipmentAccess::ClaimAdjuster:
    case EquipmentAccess::InterstellarTraveler:
    case EquipmentAccess::AlternateReality: return false; // These races do not exist yet.
    case EquipmentAccess::MysteryTrader:
        return std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(), [&](const auto& design) {
            return design.owner == player && std::find(design.components.begin(), design.components.end(),
                entry.component) != design.components.end();
        });
    }
    return false;
}

std::string equipment_access_requirement(const EquipmentTechnology& entry)
{
    std::string text;
    switch (entry.access) {
    case EquipmentAccess::Any: break;
    case EquipmentAccess::InnerStrength: text = "Inner Strength"; break;
    case EquipmentAccess::SuperStealth: text = "Super Stealth"; break;
    case EquipmentAccess::WarMonger: text = "War Monger"; break;
    case EquipmentAccess::SpaceDemolition: text = "Space Demolition"; break;
    case EquipmentAccess::HyperExpansion: text = "Hyper Expansion"; break;
    case EquipmentAccess::InnerStrengthOrSpaceDemolition: text = "Inner Strength or Space Demolition"; break;
    case EquipmentAccess::ClaimAdjuster: text = "Claim Adjuster (not available yet)"; break;
    case EquipmentAccess::AlternateReality: text = "Alternate Reality (not available yet)"; break;
    case EquipmentAccess::InterstellarTraveler: text = "Interstellar Traveler (not available yet)"; break;
    case EquipmentAccess::MysteryTrader: text = "Mystery Trader acquisition"; break;
    }
    if (entry.excludesInnerStrength) text += (text.empty() ? "" : "; ") + std::string("unavailable to Inner Strength");
    if (entry.excludesWarMonger) text += (text.empty() ? "" : "; ") + std::string("unavailable to War Monger");
    if (entry.unarmedTransportOnly) text += (text.empty() ? "" : "; ") + std::string("unarmed freighter hulls only");
    return text;
}

bool legacy_equipment_component(ShipComponentType component)
{
    return component == ShipComponentType::ColonyModule || component == ShipComponentType::CargoPod
        || component == ShipComponentType::FuelTank || component == ShipComponentType::AntimatterGenerator;
}

bool player_uses_legacy_equipment(const GameState& state, PlayerId player)
{
    return player_uses_legacy_hulls(state, player)
        || std::any_of(state.shipDesigns.begin(), state.shipDesigns.end(), [player](const auto& design) {
            return design.owner == player && std::any_of(design.components.begin(), design.components.end(), legacy_equipment_component);
        });
}

double ship_design_armor(const ShipDesign& design)
{
    double value = hull_spec(design.hull).armor;
    for (const auto component : design.components) value += component_spec(component).armor;
    return value;
}

double ship_design_shields(const ShipDesign& design)
{
    double value = 0;
    for (const auto component : design.components) value += component_spec(component).shields;
    return value;
}

std::string equipment_effect_description(const EquipmentTechnology& entry)
{
    const auto& s = entry.effects;
    std::ostringstream out;
    if (s.armor) out << "Armor +" << s.armor << " dp.\n";
    if (s.shields) out << "Shields +" << s.shields << " dp.\n";
    if (s.weaponPower) {
        out << "Power " << s.weaponPower << "; battle range " << int(s.weaponRange)
            << " squares; initiative " << int(s.weaponInitiative) << ".\n";
        if (entry.kind == ShipComponentKind::Torpedo) out << "Base accuracy " << s.weaponAccuracy << "%.\n";
        if (s.shieldOnly) out << "Shield damage only.\n";
        if (s.gatling) out << "Gatling beam: multiple targets.\n";
        if (s.missile) out << "Missile: double nominal damage against unshielded targets.\n";
    }
    if (entry.kind == ShipComponentKind::Bomb) {
        if (s.unterraformingBomb) out << "Reverses terraforming.\n";
        else out << "Population kill " << s.bombPopulationPercent << "%; minimum " << s.bombMinimumKills
            << "; installations " << s.bombInstallations << ".\n";
        if (s.smartBomb) out << "Smart bomb; effectiveness depends on population coverage.\n";
        out << "Reference bomb ratings; bombardment is not implemented yet.\n";
    }
    if (s.minesPerYear) out << "Lays " << s.minesPerYear << (s.mineFieldKind == 1 ? " heavy" : s.mineFieldKind == 2 ? " speed-trap" : " standard")
        << " mines/year. Reference rating; minefields are not implemented yet.\n";
    if (s.cloakPercent) out << "Reference cloak rating " << s.cloakPercent << "%; stealth detection is not implemented yet.\n";
    if (s.jammingPercent) out << "Torpedo jamming " << s.jammingPercent << "%.\n";
    if (s.accuracyBonusPercent) out << "Reduces torpedo inaccuracy by " << s.accuracyBonusPercent << "%; initiative +" << int(s.initiativeBonus) << ".\n";
    if (s.beamBonusPercent) out << "Beam damage bonus " << s.beamBonusPercent << "%.\n";
    if (s.beamDeflectionPercent) out << "Beam deflection " << s.beamDeflectionPercent << "%.\n";
    if (s.battleMovementBonus) out << "Battle movement +" << s.battleMovementBonus << " squares/round; no change to strategic Warp.\n";
    if (s.battleMovementPenalty) out << "All ships in battle move " << s.battleMovementPenalty << " fewer squares/round; does not stack.\n";
    if (s.tachyonPercent) out << "Reduces opposing cloak by " << s.tachyonPercent << "%; stealth detection is not implemented yet.\n";
    if (s.armor || s.shields || s.weaponPower || s.jammingPercent || s.accuracyBonusPercent || s.beamBonusPercent
        || s.beamDeflectionPercent || s.battleMovementBonus || s.battleMovementPenalty)
        out << "Active in automatic space combat. Different empires are hostile in the current rules.\n";
    if (entry.component == ShipComponentType::JumpGate) out << "Ship jump gates are not implemented yet.\n";
    if (entry.component == ShipComponentType::OrbitalAdjuster) out << "Orbital terraforming is not implemented yet.\n";
    if (entry.component == ShipComponentType::OrbitalConstructionModule) out << "Alternate Reality colonization is not implemented yet.\n";
    if (entry.access == EquipmentAccess::MysteryTrader) out << "Requires acquisition; ordinary research alone never grants this item. Additional alien special effects are not implemented yet.\n";
    return out.str();
}
} // namespace suns
