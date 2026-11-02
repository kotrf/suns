#include "main_window.hpp"
#include "ship_designer_dialog.hpp"
#include "suns/intergalactic_visitors.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w)
    {
        w.state_ = make_demo_game();
        w.state_.players.front().technology.levels.fill(0);
        w.pendingOrders_ = {1, {}};
        w.pendingDescriptions_.clear();
        w.installUiPolish();
        w.installTurnMessages();
        w.rebuildScene();
        w.show();
        w.openShipDesigner();
    }
    static GameState& state(MainWindow& w) { return w.state_; }
    static ShipDesignerDialog* designer(const MainWindow& w) { return w.shipDesigner_; }
    static std::size_t orderCount(const MainWindow& w) { return w.pendingOrders_.orders.size(); }
    static void changed(MainWindow& w) { w.updateControls(); }
    static void open(MainWindow& w) { w.openShipDesigner(); }
    static void nextTurn(MainWindow& w) { w.endTurn(); }
    static void researchNext(MainWindow& w, ResearchField field)
    {
        auto& tech = w.state_.players.front().technology;
        tech.focus = field;
        tech.queuedFocuses.clear();
        tech.researchActive = true;
        tech.researchAllocationPercent = 100;
        const auto i = std::size_t(field);
        tech.progress[i] = research_level_cost(field, tech.levels[i] + 1) - 1;
        for (auto& planet : w.state_.planets)
            if (planet.owner == 1) planet.productionQueue.clear();
    }
};
} // namespace suns

namespace {
using namespace suns;

void settle()
{
    QEventLoop loop;
    QTimer::singleShot(20, &loop, &QEventLoop::quit);
    loop.exec();
}

QListWidgetItem* component(ShipDesignerDialog* dialog, ShipComponentType type)
{
    auto* list = dialog->findChild<QListWidget*>("shipComponentCatalog");
    for (int row = 0; row < list->count(); ++row)
        if (list->item(row)->data(Qt::UserRole).toInt() == int(type)) return list->item(row);
    assert(false);
    return nullptr;
}

void sameDraft(const ShipDesignDraft& a, const ShipDesignDraft& b)
{
    assert(a.name == b.name && a.hull == b.hull && a.components == b.components);
    assert(a.placements.size() == b.placements.size());
    for (std::size_t i = 0; i < a.placements.size(); ++i)
        assert(a.placements[i].slot == b.placements[i].slot
            && a.placements[i].component == b.placements[i].component);
}

void research_unlocks_the_live_window_without_refitting()
{
    MainWindow w;
    MainWindowTestAccess::setup(w);
    auto* dialog = MainWindowTestAccess::designer(w);
    auto& state = MainWindowTestAccess::state(w);
    state.players.front().technology.levels[std::size_t(ResearchField::Propulsion)] = 2;
    auto* list = dialog->findChild<QListWidget*>("shipComponentCatalog");
    auto* search = dialog->findChild<QLineEdit*>("shipComponentSearch");
    auto* category = dialog->findChild<QComboBox*>("shipComponentCategory");
    auto* fits = dialog->findChild<QCheckBox*>("shipComponentFitsHull");
    auto* scoop = component(dialog, ShipComponentType::EfficientRamScoopDrive);
    dialog->findChild<QLineEdit*>("shipDesignName")->setText("Expedition draft");
    const auto engineSlot = hull_spec(dialog->draft().hull).fittingSlots.front().id;
    auto* slot = dialog->findChild<QToolButton*>(QString("shipSlot_%1").arg(engineSlot));
    assert(slot);
    slot->click();
    category->setCurrentIndex(category->findData(int(ShipComponentKind::Engine)));
    fits->setChecked(true);
    search->setText("Efficient Ram");
    list->setCurrentItem(scoop);
    assert(!scoop->data(Qt::UserRole + 1).toBool());
    assert(!dialog->findChild<QPushButton*>("fitSelectedComponent")->isEnabled());
    const auto before = dialog->draft();
    const auto designs = state.shipDesigns;
    MainWindowTestAccess::researchNext(w, ResearchField::Propulsion);
    MainWindowTestAccess::nextTurn(w);
    settle();
    assert(state.players.front().technology.levels[std::size_t(ResearchField::Propulsion)] >= 3);
    assert(MainWindowTestAccess::designer(w) == dialog);
    assert(scoop->data(Qt::UserRole + 1).toBool()); // Fails before the live refresh fix.
    assert(scoop->flags().testFlag(Qt::ItemIsDragEnabled));
    assert(!scoop->text().contains("locked") && !scoop->toolTip().contains("Locked"));
    assert(!dialog->findChild<QLabel*>("shipComponentDetails")->text().contains("Locked"));
    assert(dialog->findChild<QPushButton*>("fitSelectedComponent")->isEnabled());
    assert(list->currentItem() == scoop && search->text() == "Efficient Ram" && fits->isChecked());
    sameDraft(before, dialog->draft());
    assert(state.shipDesigns.size() == designs.size());
    for (std::size_t i = 0; i < designs.size(); ++i) {
        assert(state.shipDesigns[i].components == designs[i].components);
        assert(state.shipDesigns[i].placements.size() == designs[i].placements.size());
    }
    // A later construction breakthrough unlocks a hull without choosing it.
    auto* hulls = dialog->findChild<QComboBox*>("shipHullCatalog");
    auto* model = qobject_cast<QStandardItemModel*>(hulls->model());
    const auto miner = hulls->findData(int(ShipHullType::RemoteMiner));
    assert(miner >= 0 && !model->item(miner)->isEnabled());
    MainWindowTestAccess::researchNext(w, ResearchField::Construction);
    MainWindowTestAccess::nextTurn(w);
    settle();
    assert(model->item(miner)->isEnabled() && !hulls->itemText(miner).contains("locked"));
    sameDraft(before, dialog->draft());
    dialog->findChild<QPushButton*>("fitSelectedComponent")->click();
    const auto fitted = dialog->draft();
    assert(fitted.components != before.components);
    assert(std::find(fitted.components.begin(), fitted.components.end(),
        ShipComponentType::FusionDrive) == fitted.components.end());
    // Save rechecks live access even before the queued refresh is dispatched.
    state.players.front().technology.levels[std::size_t(ResearchField::Propulsion)] = 0;
    assert(dialog->findChild<QPushButton*>("saveShipDesign")->isEnabled());
    dialog->accept();
    assert(MainWindowTestAccess::orderCount(w) == 0);
    assert(dialog->findChild<QLabel*>("shipDesignFitMessage")->text().contains("unavailable"));
    MainWindowTestAccess::changed(w);
    settle();
    sameDraft(fitted, dialog->draft());
    assert(!scoop->data(Qt::UserRole + 1).toBool());
    assert(!scoop->flags().testFlag(Qt::ItemIsDragEnabled));
    assert(!dialog->findChild<QPushButton*>("saveShipDesign")->isEnabled());
    state.players.front().technology.levels[std::size_t(ResearchField::Propulsion)] = 3;
    MainWindowTestAccess::changed(w);
    settle();
    assert(dialog->findChild<QPushButton*>("saveShipDesign")->isEnabled());
    dialog->accept();
    assert(MainWindowTestAccess::orderCount(w) == 1);
    MainWindowTestAccess::nextTurn(w);
    const auto saved = std::find_if(state.shipDesigns.begin(), state.shipDesigns.end(),
        [&](const ShipDesign& design) { return design.owner == 1 && design.name == fitted.name; });
    assert(saved != state.shipDesigns.end() && saved->components == fitted.components);
}

void acquisition_restrictions_and_reopening_preserve_the_draft()
{
    MainWindow w;
    MainWindowTestAccess::setup(w);
    auto* dialog = MainWindowTestAccess::designer(w);
    auto& state = MainWindowTestAccess::state(w);
    if (state.players.size() == 1) {
        auto enemy = state.players.front();
        enemy.id = 2;
        state.players.push_back(enemy);
    }
    state.players.back().technology.levels.fill(26);
    state.players.back().race.hullAccess = HullAccess::HyperExpansion;
    state.players.back().acquiredEquipment.push_back(kVisitorGift);
    state.players.front().technology.levels.fill(26);
    MainWindowTestAccess::changed(w);
    settle();
    auto* gift = component(dialog, kVisitorGift);
    assert(!gift->data(Qt::UserRole + 1).toBool());
    auto* list = dialog->findChild<QListWidget*>("shipComponentCatalog");
    list->setCurrentItem(gift);
    const auto before = dialog->draft();
    state.players.front().acquiredEquipment.push_back(kVisitorGift);
    // Reopening must refresh synchronously, even before queued UI work runs.
    MainWindowTestAccess::open(w);
    assert(gift->data(Qt::UserRole + 1).toBool());
    assert(list->currentItem() == gift && MainWindowTestAccess::designer(w) == dialog);
    sameDraft(before, dialog->draft());
    auto* hulls = dialog->findChild<QComboBox*>("shipHullCatalog");
    auto* model = qobject_cast<QStandardItemModel*>(hulls->model());
    const auto morph = hulls->findData(int(ShipHullType::MetaMorph));
    assert(!model->item(morph)->isEnabled());
    state.players.front().race.hullAccess = HullAccess::HyperExpansion;
    MainWindowTestAccess::changed(w);
    settle();
    assert(model->item(morph)->isEnabled());
    const auto tradeHull = hulls->findData(int(ShipHullType::MiniMorph));
    assert(!model->item(tradeHull)->isEnabled());
    state.shipDesigns.push_back({state.nextShipDesignId++, 1, "Traded hull", ShipHullType::MiniMorph,
        {ShipComponentType::QuickJump5}});
    MainWindowTestAccess::changed(w);
    settle();
    assert(model->item(tradeHull)->isEnabled());
    // Irrelevant changes and repeated updates must not scroll a stable selection.
    const auto scroll = list->verticalScrollBar()->value();
    const auto label = gift->text();
    for (auto& player : state.players)
        if (player.id != 1) player.technology.levels.fill(0);
    for (int i = 0; i < 4; ++i) MainWindowTestAccess::changed(w);
    settle();
    assert(list->currentItem() == gift && list->verticalScrollBar()->value() == scroll);
    assert(gift->text() == label && MainWindowTestAccess::orderCount(w) == 0);
    sameDraft(before, dialog->draft());
    hulls->setCurrentIndex(morph);
    const auto racialDraft = dialog->draft();
    state.players.front().race.hullAccess = HullAccess::Standard;
    state.players.front().acquiredEquipment.clear();
    MainWindowTestAccess::changed(w);
    settle();
    assert(hulls->currentIndex() == morph && !model->item(morph)->isEnabled());
    assert(!gift->data(Qt::UserRole + 1).toBool() && gift->toolTip().contains("Locked"));
    sameDraft(racialDraft, dialog->draft());
    assert(!dialog->findChild<QPushButton*>("saveShipDesign")->isEnabled());
}
} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir settings;
    assert(settings.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings.path());
    research_unlocks_the_live_window_without_refitting();
    acquisition_restrictions_and_reopening_preserve_the_draft();
}
