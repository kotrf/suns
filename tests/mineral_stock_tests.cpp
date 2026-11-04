#include "mineral_stock_panel.hpp"
#include "save_game.hpp"
#include "suns/campaign.hpp"
#include "suns/communications.hpp"
#include <QApplication>
#include <QDataStream>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <cassert>
#include <cmath>
#include <limits>

using namespace suns;

static GameState fixture()
{
    auto s = make_demo_game(); s.turn = 10;
    s.stars[0].position = {0, 0}; s.stars[1].position = {600, 0};
    auto& p = s.planets[1]; p.owner = 0; p.minerals = {25, 0, 75};
    s.fleets.resize(1);
    auto& f = s.fleets.front(); f.position = s.stars[1].position; f.destination.reset();
    f.waypointQueue.clear(); f.pendingCommands.clear(); f.telemetryInTransit.clear();
    f.electronics = {}; f.task = FleetTask::RemoteMining;
    f.design = 99; f.ships = {{99, 2}};
    s.shipDesigns.push_back({99, 1, "Mining fixture", ShipHullType::MiniMiner,
        {ShipComponentType::QuickJump5, ShipComponentType::RoboMiniMiner}});
    s.players.front().technology.levels.fill(20);
    s.players.front().pendingSurveyReports.clear();
    s.players.front().surveyKnowledge.clear(); s.players.front().surveyedStars.clear();
    return s;
}
static void received_stocks_are_frozen_and_independently_dated()
{
    auto s = fixture(); s.turn = 9; const auto id = s.planets[1].id; const auto star = s.planets[1].star;
    auto& reports = s.players.front().pendingSurveyReports;
    assert(!known_surface_minerals(s, 1, id));
    queue_survey_report(s, 1, star, s.fleets.front().id, s.fleets.front().position, 10, SurveyLevel::GeologicalSurvey);
    assert(reports.size() == 1 && reports.front().deliveryTurn > 10);
    const auto arrival = reports.front();
    s.planets[1].minerals = {40, 2, 90};
    publish_surface_mineral_reports(s, 10);
    assert(reports.size() == 2); // A final-surface observation survives an earlier geology packet.
    auto final = reports.back(); assert(final.planetSnapshot->surfaceMinerals->finalSurface);
    assert(!final.planetSnapshot->xenocrystal); // Ordinary minerals do not discover exotic deposits.
    s.planets[1].minerals = {9999, 9999, 9999};
    s.turn = final.deliveryTurn;
    (void)deliver_due_survey_reports(s);
    const auto stock = known_surface_minerals(s, 1, id);
    assert(stock && stock->observedTurn == 10 && stock->finalSurface);
    assert(stock->minerals.ironium == 40 && stock->minerals.boranium == 2);
    auto view = make_player_view(s, 1).state;
    assert(find_planet(view, id)->minerals.ironium == 40);
    assert(known_surface_minerals(view, 1, id)->observedTurn == 10);
    // A late same-year arrival scan cannot roll back the final stock.
    auto older = arrival; older.deliveryTurn = s.turn; reports = {older};
    (void)deliver_due_survey_reports(s);
    assert(known_surface_minerals(s, 1, id)->minerals.ironium == 40);
    // Nor can an older higher-quality survey or a newer rough scan.
    older.observedTurn = 9; older.planetSnapshot->surfaceMinerals->observedTurn = 9;
    older.level = SurveyLevel::DeepSurvey; reports = {older};
    (void)deliver_due_survey_reports(s);
    set_survey_level(s, 1, star, SurveyLevel::BasicScan, s.turn);
    assert(known_surface_minerals(s, 1, id)->minerals.ironium == 40);
    // No received surface evidence is not a known zero, even on a geological map.
    s.players.front().surveyKnowledge.back().planetSnapshot->surfaceMinerals.reset();
    assert(!mineral_display_data(make_player_view(s, 1).state, view, 1, id).stock);
}
static void passive_observer_holds_stock_reports()
{
    auto s = fixture(); const auto id = s.planets[1].id;
    set_survey_level(s, 1, s.planets[1].star, SurveyLevel::OrbitalSurvey, s.turn);
    s.players.front().surveyKnowledge.back().planetSnapshot->surfaceMinerals.reset();
    s.fleets.front().electronics.mode = EmissionMode::Passive;
    publish_surface_mineral_reports(s, s.turn);
    auto& reports = s.players.front().pendingSurveyReports;
    assert(reports.size() == 1 && reports.front().deliveryTurn == kAwaitingTransmission);
    for (int year = 0; year < 3; ++year) { ++s.turn; s.planets[1].minerals.ironium += 5; publish_surface_mineral_reports(s, s.turn); }
    assert(reports.size() == 1); // Held repeated observations coalesce instead of growing forever.
    assert(!known_surface_minerals(s, 1, id));
    s.fleets.front().electronics.mode = EmissionMode::Standard;
    dispatch_fleet_report_outbox(s);
    assert(reports.front().deliveryTurn != kAwaitingTransmission);
    s.turn = reports.front().deliveryTurn;
    (void)deliver_due_survey_reports(s);
    assert(known_surface_minerals(s, 1, id)->minerals.ironium == 40);
}
static void actual_turn_publishes_post_mining_stock_without_a_scanner()
{
    auto s = fixture();
    const auto id = s.planets[1].id;
    const auto unexplored = TurnProcessor{}.process(s, {});
    assert(survey_level(unexplored, 1, s.planets[1].star) == SurveyLevel::Detected);
    assert(!known_surface_minerals(unexplored, 1, id));
    // Previously surveyed world, with no stock evidence in its old report.
    set_survey_level(s, 1, s.planets[1].star, SurveyLevel::OrbitalSurvey, s.turn);
    s.players.front().surveyKnowledge.back().planetSnapshot->surfaceMinerals.reset();
    const auto initial = s.planets[1].minerals.ironium;
    const auto rate = projected_remote_mining(s, s.planets[1], s.shipDesigns.back()).ironium * 2;
    assert(!fleet_has_scanner(s, s.fleets.front()));
    const auto next = TurnProcessor{}.process(s, {});
    const auto* surface = find_planet(next, id);
    assert(std::abs(surface->minerals.ironium - initial - rate) < 1e-8);
    assert(!known_surface_minerals(next, 1, id)); // Six hundred ly away: packet still in flight.
    bool captured = false;
    for (const auto& r : next.players.front().pendingSurveyReports)
        if (r.star == surface->star && r.planetSnapshot && r.planetSnapshot->surfaceMinerals) {
            const auto& stock = *r.planetSnapshot->surfaceMinerals;
            if (stock.finalSurface && std::abs(stock.minerals.ironium - surface->minerals.ironium) < 1e-8) captured = true;
        }
    assert(captured);
}
static void mining_estimate_and_shared_scale()
{
    auto s = fixture(); const auto id = s.planets[1].id;
    set_survey_level(s, 1, s.planets[1].star, SurveyLevel::GeologicalSurvey, s.turn);
    const auto data = mineral_display_data(s, s, 1, id);
    const auto rate = projected_remote_mining(s, s.planets[1], s.shipDesigns.back());
    assert(data.stock && data.stock->ironium == 25 && data.stock->boranium == 0);
    assert(data.mining && std::abs(data.mining->ironium - 2 * rate.ironium) < 1e-8);
    auto afterCargo = s; afterCargo.planets[1].minerals.ironium = 15;
    assert(mineral_display_data(s, afterCargo, 1, id).stock->ironium == 15);
    assert(known_surface_minerals(s, 1, id)->minerals.ironium == 25);
    s.fleets.front().task = FleetTask::None;
    assert(mineral_display_data(s, s, 1, id).mining->ironium == 0);
    MineralStockPanel panel(nullptr); panel.resize(320, 150); panel.setMinerals(data, s.turn + 3); panel.show();
    QApplication::processEvents();
    const auto* age = panel.findChild<QLabel*>("mineralStockAge"); assert(age && age->text().contains("3 years old"));
    auto* iron = panel.findChild<MineralReserveBar*>("ironiumConcentration");
    auto* boron = panel.findChild<MineralReserveBar*>("boraniumConcentration");
    assert(iron && boron && iron->isEnabled() && boron->isEnabled());
    assert(iron->toolTip().contains("25.0 kt") && boron->toolTip().contains("0.0 kt"));
    const auto conc = iron->value(); const auto initialScale = panel.stockScale();
    panel.findChild<QPushButton*>("mineralStockScale")->click();
    assert(panel.stockScale() != initialScale && iron->value() == conc);
    const auto nextScale = panel.stockScale();
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier); QApplication::sendEvent(boron, &key);
    assert(panel.stockScale() != nextScale && iron->value() == conc);
    // Render both known and unknown states; neither a stock nor a concentration implies the other.
    assert(!panel.grab().isNull());
    if (const auto preview = qEnvironmentVariable("SUNS_MINERAL_PREVIEW"); !preview.isEmpty()) assert(panel.grab().save(preview));
    MineralDisplayData stockOnly; stockOnly.stock = MineralCargo{0, 2, 3}; stockOnly.observedTurn = s.turn;
    panel.setMinerals(stockOnly, s.turn); assert(iron->isEnabled() && iron->toolTip().contains("Concentration: unknown"));
    panel.setMinerals({}, s.turn); assert(!iron->isEnabled()); assert(!panel.grab().isNull());
}
static QByteArray bytes(const QString& path) { QFile f(path); assert(f.open(QIODevice::ReadOnly)); return f.readAll(); }
static void put(const QString& path, const QByteArray& data) { QFile f(path); assert(f.open(QIODevice::WriteOnly)); assert(f.write(data) == data.size()); }
static void serialization()
{
    QTemporaryDir dir; QString error; const auto path = dir.filePath("stocks.suns");
    SaveGameData original; original.mode = SessionMode::Host; original.playerTokens = {{1, 789}}; original.campaignId = 123; original.turnToken = 456; original.state = fixture(); original.pendingOrders = {1, {}};
    auto& s = original.state; const auto id = s.planets[1].id;
    set_survey_level(s, 1, s.planets[1].star, SurveyLevel::GeologicalSurvey, s.turn);
    s.planets[1].minerals.ironium = 30; publish_surface_mineral_reports(s, s.turn);
    SaveGameData loaded;
    assert(write_save_game_file(path, original, error));
    const auto current = bytes(path);
    assert(read_save_game_file(path, loaded, error));
    assert(known_surface_minerals(loaded.state, 1, id)->minerals.ironium == 25);
    assert(loaded.state.players.front().pendingSurveyReports.front().planetSnapshot->surfaceMinerals->minerals.ironium == 30);
    const auto packet = make_player_turn(original, 1);
    assert(write_save_game_file(path, packet, error) && read_save_game_file(path, loaded, error));
    assert(known_surface_minerals(loaded.state, 1, id)->minerals.ironium == 25);
    assert(loaded.state.players.front().pendingSurveyReports.empty());
    // v109 has identical preceding bodies and simply lacks the new trailing extension.
    QByteArray marker; { QDataStream out(&marker, QIODevice::WriteOnly); out << quint32{0x534d494e}; }
    const auto tail = current.lastIndexOf(marker); assert(tail > 0);
    auto legacy = current.left(tail); { QDataStream out(&legacy, QIODevice::ReadWrite); assert(out.device()->seek(4)); out << quint32{109}; }
    put(path, legacy); assert(read_save_game_file(path, loaded, error));
    assert(!known_surface_minerals(loaded.state, 1, id));
    // Reject nonfinite stock and an invalid phase/date on write and truncated packets on read.
    auto& evidence = *s.players.front().surveyKnowledge.back().planetSnapshot->surfaceMinerals;
    evidence.minerals.ironium = std::numeric_limits<double>::quiet_NaN();
    assert(!write_save_game_file(path, original, error)); evidence.minerals.ironium = 25;
    evidence.observedTurn = s.turn + 1; assert(!write_save_game_file(path, original, error));
    auto corrupt = current; corrupt[corrupt.size() - 1] = char(2); // Invalid final-surface flag in the pending report.
    put(path, corrupt); assert(!read_save_game_file(path, loaded, error));
    corrupt = current;
    { QDataStream out(&corrupt, QIODevice::ReadWrite); assert(out.device()->seek(tail + 25)); out << std::numeric_limits<double>::quiet_NaN(); }
    put(path, corrupt); assert(!read_save_game_file(path, loaded, error));
    put(path, current.left(current.size() - 1)); assert(!read_save_game_file(path, loaded, error));
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    received_stocks_are_frozen_and_independently_dated(); passive_observer_holds_stock_reports();
    actual_turn_publishes_post_mining_stock_without_a_scanner();
    mining_estimate_and_shared_scale(); serialization();
}
