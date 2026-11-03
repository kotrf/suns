#include "main_window.hpp"
#include "route_program_dock.hpp"
#include <QApplication>
#include <QComboBox>
#include <QEventLoop>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPushButton>
#include <QSpinBox>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w)
    {
        w.state_ = make_demo_game();
        w.pendingOrders_ = {1, {}};
        w.pendingDescriptions_.clear();
        w.selection_.star = w.state_.planets.front().star;
        w.selection_.fleet = w.state_.fleets.front().id;
        w.rebuildScene();
    }
    static auto builds(const MainWindow& w) { return w.movementPreviewBuildCount_; }
    static auto orderCount(const MainWindow& w) { return w.pendingOrders_.orders.size(); }
    static auto turn(const MainWindow& w) { return w.state_.turn; }
    static void route(MainWindow& w) { w.appendPendingOrder(MoveFleetOrder{w.state_.fleets.front().id,{25,0},5},"Route"); }
    static void research(MainWindow& w) { w.appendPendingOrder(SetResearchAllocationOrder{50}, "Research"); }
    static void rename(MainWindow& w) { w.appendPendingOrder(RenameFleetOrder{w.state_.fleets.front().id,"Renamed"}, "Rename"); }
    static void reloadSameTurn(MainWindow& w)
    {
        // Change received empire knowledge, not hidden physical stock that an
        // immutable old colony report may correctly conceal.
        w.state_.players.front().technology.levels[std::size_t(ResearchField::Propulsion)] += 3;
        w.rebuildScene();
    }
    static const GameState& projection(const MainWindow& w) { return w.movementPlanningPreview(); }
    static auto& scene(MainWindow& w) { return *w.scene_; }
};
}

static void settle()
{
    QEventLoop loop; QTimer::singleShot(30,&loop,&QEventLoop::quit); loop.exec();
}

int main(int argc,char** argv)
{
    QApplication app(argc,argv);
    QTemporaryDir settings;
    assert(settings.isValid());
    QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,settings.path());
    using namespace suns;
    MainWindow w; attachRouteProgramDock(w); w.installUiPolish(); w.installProductionQueue();
    MainWindowTestAccess::setup(w); w.show(); settle();
    const auto before=MainWindowTestAccess::projection(w);
    assert(before.turn == MainWindowTestAccess::turn(w));
    const auto builds=MainWindowTestAccess::builds(w);
    QGraphicsItem* marker=nullptr;
    for(auto* item:MainWindowTestAccess::scene(w).items())
        if(item->data(1).toInt()==1){ marker=item;break; }
    assert(marker);
    marker->setData(9001,QStringLiteral("order-refresh-marker"));
    auto* kind=w.findChild<QComboBox*>("productionBatchKind");
    auto* quantity=w.findChild<QSpinBox*>("productionBatchQuantity");
    auto* add=w.findChild<QPushButton*>("productionBatchAdd");
    auto* queue=w.findChild<QTreeWidget*>("productionQueueTree");
    assert(kind && quantity && add && queue);
    kind->setCurrentIndex(kind->findData(int(ProductionKind::Factory)));
    quantity->setValue(1);
    assert(add->isEnabled());
    const auto orders=MainWindowTestAccess::orderCount(w);
    add->click(); settle();
    assert(MainWindowTestAccess::orderCount(w)==orders+1 && queue->topLevelItemCount()>0);
    assert(MainWindowTestAccess::scene(w).items().contains(marker));
    assert(marker->data(9001).toString()==QStringLiteral("order-refresh-marker"));
    assert(MainWindowTestAccess::builds(w)==builds);
    const auto& unchanged=MainWindowTestAccess::projection(w);
    assert(unchanged.fleets.front().fuel==before.fleets.front().fuel);
    assert(unchanged.planets.front().population==before.planets.front().population);
    MainWindowTestAccess::research(w); settle();
    assert(MainWindowTestAccess::builds(w)==builds);
    assert(MainWindowTestAccess::scene(w).items().contains(marker));
    MainWindowTestAccess::route(w); settle();
    const auto rows=w.selectedFleetRouteProgramRows();
    const auto summary=w.selectedFleetRouteProgramEstimate();
    assert(!rows.empty() && !summary.isEmpty());
    assert(rows.front().eta.startsWith("≈"));
    const auto routeBuilds=MainWindowTestAccess::builds(w);
    assert(routeBuilds==builds+1);
    assert((MainWindowTestAccess::projection(w).fleets.front().destination == std::optional<Position>{{25,0}}));
    add->click(); settle();
    // Geometry-only estimates do not predict future production or supplies.
    assert(w.selectedFleetRouteProgramRows().front().eta == rows.front().eta);
    assert(w.selectedFleetRouteProgramEstimate() == summary);
    assert(MainWindowTestAccess::builds(w)==routeBuilds); // Accepted current orders did not change.
    w.clearSelectedFleetRouteProgram(); settle();
    assert(MainWindowTestAccess::builds(w)==routeBuilds+1);
    assert(!MainWindowTestAccess::projection(w).fleets.front().destination);
    assert(w.selectedFleetRouteProgramRows().empty());
    // Relevant logistics input and a same-year world replacement still rebuild.
    MainWindowTestAccess::rename(w); settle();
    assert(MainWindowTestAccess::builds(w)==routeBuilds+2);
    assert(MainWindowTestAccess::projection(w).fleets.front().name=="Renamed");
    MainWindowTestAccess::reloadSameTurn(w); settle();
    assert(MainWindowTestAccess::builds(w)==routeBuilds+3);
    assert(MainWindowTestAccess::projection(w).players.front().technology.levels[std::size_t(ResearchField::Propulsion)]
        != before.players.front().technology.levels[std::size_t(ResearchField::Propulsion)]);
}
