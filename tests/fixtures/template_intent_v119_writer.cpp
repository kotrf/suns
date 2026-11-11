// Synthetic fixtures from the unmodified v119/v49 writer and headers at
// 1d0ca997883ce3328143efc8b62630ebb7652424. Rebuild against that revision,
// never the current writer: the pending payload is identical for Delete/Rename;
// only the original editor-generated descriptions distinguish their intent.
#include "save_game.hpp"
#include "native_orbital_fixture.hpp"
#include "suns/production.hpp"
#include <QDir>
#include <cstdio>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const QDir directory(QString::fromLocal8Bit(argv[1]));
    suns::SaveGameData host;
    host.state = suns::test::native_orbital_founding_fixture();
    host.mode = suns::SessionMode::Host; host.campaignId = 115;
    host.turnToken = 17; host.playerTokens = {{1, 17}}; host.pendingOrders = {1, {}};
    const suns::ProductionTemplate source{"Radar only", {{suns::ProductionKind::LocalRadar, suns::production_cost(suns::ProductionKind::LocalRadar)}}};
    const suns::ProductionTemplate neighbor{"Other default", {{suns::ProductionKind::PlanetaryRadar, suns::production_cost(suns::ProductionKind::PlanetaryRadar)}}, true};
    host.state.players.front().productionTemplates = {source, neighbor};
    auto replacement = source; replacement.name = "Radar revised";
    replacement.items.push_back({suns::ProductionKind::Terraforming, suns::kTerraformingCost, 0, suns::ProductionAutomation::MinTerraform, 1});
    const suns::PlayerOrders orders{1, {suns::SetProductionTemplateOrder{{source.name}, true}, suns::SetProductionTemplateOrder{replacement}}};
    QString error;
    for (bool rename : {false, true}) {
        const QString action = rename ? "rename" : "delete";
        const QStringList descriptions{(rename ? "Rename colony template " : "Delete colony template ") + QString::fromStdString(source.name),
            "Save colony template " + QString::fromStdString(replacement.name)};
        for (bool pbem : {false, true}) {
            auto save = pbem ? suns::make_player_turn(host, 1) : host;
            save.pendingOrders = orders; save.pendingDescriptions = descriptions;
            const auto path = directory.filePath("template-intent-v119-" + QString(pbem ? "player-" : "host-") + action + ".suns");
            if (!suns::write_save_game_file(path, save, error)) { std::fprintf(stderr, "%s\n", qPrintable(error)); return 3; }
        }
        const suns::TurnOrderFileData packet{host.campaignId, host.state.turn, host.turnToken, orders, descriptions};
        if (!suns::write_turn_order_file(directory.filePath("template-intent-v49-" + action + ".sunsorders"), packet, error)) {
            std::fprintf(stderr, "%s\n", qPrintable(error)); return 4;
        }
    }
}
