#include "main_window.hpp"
#include "ship_designer_dialog.hpp"
#include "suns/scanners.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QListWidget>
#include <QLabel>
#include <QMimeData>
#include <QKeyEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>
#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& window)
    {
        window.state_ = generate_campaign(GalaxyConfig{}, {{"Research", RacePreset::Terran}});
        window.pendingOrders_ = {1, {}};
        window.pendingDescriptions_.clear();
        window.installResearch();
        window.refreshResearchPanel();
    }
};
} // namespace suns

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    using namespace suns;
    MainWindow window;
    MainWindowTestAccess::setup(window);
    auto* catalog = window.findChild<QTreeWidget*>("technologyCatalog");
    auto* plan = window.findChild<QTreeWidget*>("researchPlanTree");
    assert(catalog && plan);
    const auto find = [catalog](const QString& name) {
        for (int row = 0; row < catalog->topLevelItemCount(); ++row)
            if (catalog->topLevelItem(row)->text(0) == name) return catalog->topLevelItem(row);
        return static_cast<QTreeWidgetItem*>(nullptr);
    };
    auto* scoop = find("Sub-Galactic Fuel Scoop");
    auto* mizer = find("Fuel Mizer");
    assert(scoop && mizer && scoop->text(1).contains("Energy 2") && scoop->text(1).contains("Propulsion 8"));
    assert(mizer->text(2) == "Race restriction");
    assert(find("Rhino Scanner") && find("Rhino Scanner")->text(2) == "Available");
    assert(find("Ferret Scanner") && find("Ferret Scanner")->text(1).contains("Biology 2"));
    assert(find("Chameleon Scanner")->text(2) == "Race restriction");
    assert(!find("Compact Scanner (legacy)"));
    const auto count = plan->topLevelItemCount();
    catalog->itemDoubleClicked(mizer, 0);
    assert(plan->topLevelItemCount() == count);
    catalog->itemDoubleClicked(scoop, 0);
    int energy = 0, propulsion = 0;
    for (int row = 0; row < plan->topLevelItemCount(); ++row) {
        const auto field = static_cast<ResearchField>(plan->topLevelItem(row)->data(0, Qt::UserRole).toInt());
        energy += field == ResearchField::Energy;
        propulsion += field == ResearchField::Propulsion;
    }
    assert(energy == 2 && propulsion == 8);
    const auto after = plan->topLevelItemCount();
    scoop = find("Sub-Galactic Fuel Scoop");
    catalog->itemDoubleClicked(scoop, 0);
    assert(plan->topLevelItemCount() == after);
    auto* dreadnought = find("Dreadnought");
    auto* morph = find("Mini Morph");
    assert(dreadnought && morph && morph->text(2) == "Acquisition required");
    catalog->itemDoubleClicked(dreadnought, 0);
    catalog->itemDoubleClicked(morph, 0);
    assert(plan->topLevelItemCount() == after);
    auto* nubian = find("Nubian");
    assert(nubian && nubian->text(1) == "Construction 26");
    catalog->itemDoubleClicked(nubian, 0);
    int construction = 0;
    for (int row = 0; row < plan->topLevelItemCount(); ++row)
        construction += plan->topLevelItem(row)->data(0, Qt::UserRole).toInt() == int(ResearchField::Construction);
    assert(construction == 26);
    const auto withNubian = plan->topLevelItemCount();
    catalog->itemDoubleClicked(find("Nubian"), 0);
    assert(plan->topLevelItemCount() == withNubian);

    auto state = generate_campaign(GalaxyConfig{}, {{"Designer", RacePreset::Terran, true}});
    state.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Propulsion)] = 20;
    state.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Energy)] = 5;
    ShipDesignerDialog designer(state, 1);
    auto* components = designer.findChild<QListWidget*>("shipComponentCatalog");
    assert(components && designer.draft().components.front() == ShipComponentType::QuickJump5);
    const auto initialDraft = designer.draft();
    assert(std::find(initialDraft.components.begin(), initialDraft.components.end(), ShipComponentType::RhinoScanner)
        != initialDraft.components.end());
    int scanners = 0;
    int engines = 0;
    for (int row = 0; row < components->count(); ++row) {
        const auto component = static_cast<ShipComponentType>(components->item(row)->data(Qt::UserRole).toInt());
        assert(!legacy_propulsion_component(component));
        assert(!legacy_scanner_component(component));
        if (scanner_technology(component)) {
            ++scanners;
            assert(components->item(row)->toolTip().contains("penetrating"));
        }
        engines += component_spec(component).kind == ShipComponentKind::Engine;
    }
    assert(engines == 15 && scanners == 16);
    auto* initialHulls = designer.findChild<QComboBox*>("shipHullCatalog");
    assert(initialHulls && initialHulls->count() == 32);
    auto* hullModel = qobject_cast<QStandardItemModel*>(initialHulls->model());
    assert(hullModel && !hullModel->item(initialHulls->findData(int(ShipHullType::Battleship)))->isEnabled());
    assert(initialHulls->findData(int(ShipHullType::Scout)) < 0);
    designer.show();
    QApplication::processEvents();
    auto* target = designer.findChild<QToolButton*>("shipSlot_100");
    assert(target);
    QMimeData mime;
    mime.setData("application/x-suns-ship-component",
        QByteArray::number(static_cast<int>(ShipComponentType::GalaxyScoop)) + ":0");
    QDragEnterEvent enter(target->rect().center(), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &enter);
    assert(enter.isAccepted());
    QDropEvent drop(QPointF(target->rect().center()), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &drop);
    assert(drop.isAccepted());
    assert(designer.draft().components.front() == ShipComponentType::GalaxyScoop);

    auto* preview = designer.findChild<QLabel*>("shipDesignPreview");
    assert(preview && preview->text().contains("collects 1 mg/ly"));
    assert(preview->text().contains("Fuel efficiency applies in flight"));
    state.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Construction)] = 13;
    state.players.front().race.hullAccess = HullAccess::InnerStrength;
    ShipDesignerDialog heavyDesigner(state, 1);
    auto* hulls = heavyDesigner.findChild<QComboBox*>("shipHullCatalog");
    assert(hulls);
    const auto superFreighter = hulls->findData(static_cast<int>(ShipHullType::SuperFreighter));
    assert(superFreighter >= 0);
    hulls->setCurrentIndex(superFreighter);
    heavyDesigner.show();
    QApplication::processEvents();
    auto* bank = heavyDesigner.findChild<QToolButton*>("shipSlot_100");
    assert(bank);
    QDragEnterEvent bankEnter(bank->rect().center(), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(bank, &bankEnter);
    QDropEvent bankDrop(QPointF(bank->rect().center()), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(bank, &bankDrop);
    assert(bankDrop.isAccepted());
    preview = heavyDesigner.findChild<QLabel*>("shipDesignPreview");
    assert(preview && preview->text().contains("collects 3 mg/ly"));

    state.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Electronics)] = 5;
    ShipDesignerDialog bankDesigner(state, 1);
    auto* bankHulls = bankDesigner.findChild<QComboBox*>("shipHullCatalog");
    bankHulls->setCurrentIndex(bankHulls->findData(int(ShipHullType::Frigate)));
    bankDesigner.show(); QApplication::processEvents();
    const auto fit = [&](ShipComponentType component, int slot, int source = 0) {
        auto* cell = bankDesigner.findChild<QToolButton*>(QString("shipSlot_%1").arg(slot));
        assert(cell);
        QMimeData payload;
        payload.setData("application/x-suns-ship-component", QByteArray::number(int(component)) + ":" + QByteArray::number(source));
        QDragEnterEvent enter(cell->rect().center(), Qt::CopyAction, &payload, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(cell, &enter);
        QDropEvent drop(QPointF(cell->rect().center()), Qt::CopyAction, &payload, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(cell, &drop);
        assert(drop.isAccepted());
    };
    const auto remove = [&](int slot) {
        auto* cell = bankDesigner.findChild<QToolButton*>(QString("shipSlot_%1").arg(slot));
        assert(cell);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
        QApplication::sendEvent(cell, &event);
    };
    const auto quantity = [&](ShipComponentType component) {
        const auto draft = bankDesigner.draft();
        return std::count(draft.components.begin(), draft.components.end(), component);
    };
    fit(ShipComponentType::CompactLongRangeScanner, 200);
    assert(quantity(ShipComponentType::CompactLongRangeScanner) == 2);
    assert(quantity(ShipComponentType::LongRangeScanner) == 0);
    remove(201);
    assert(quantity(ShipComponentType::CompactLongRangeScanner) == 1);
    fit(ShipComponentType::FuelTank, 202);
    assert(quantity(ShipComponentType::FuelTank) == 3);
    remove(204);
    assert(quantity(ShipComponentType::FuelTank) == 2);
    const auto placementsBefore = bankDesigner.draft().placements;
    fit(ShipComponentType::CompactLongRangeScanner, 204, 200);
    assert(bankDesigner.draft().placements == placementsBefore); // Cannot mix models in a partly filled bank.
    fit(ShipComponentType::CargoPod, 205); // Shield/armor bank rejects a cargo pod.
    assert(bankDesigner.draft().placements == placementsBefore);
    const auto bankDraft = bankDesigner.draft();
    assert(ship_design_valid({42, 1, bankDraft.name, bankDraft.hull, bankDraft.components, bankDraft.placements}));
    if (qEnvironmentVariableIsSet("SUNS_HULL_SCREENSHOTS"))
        assert(bankDesigner.grab().save("/tmp/suns-frigate-designer.png"));

    state.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Construction)] = 26;
    ShipDesignerDialog nubianDesigner(state, 1);
    auto* nubianHulls = nubianDesigner.findChild<QComboBox*>("shipHullCatalog");
    nubianHulls->setCurrentIndex(nubianHulls->findData(int(ShipHullType::Nubian)));
    nubianDesigner.show(); QApplication::processEvents();
    auto* scroll = nubianDesigner.findChild<QScrollArea*>("shipHullSlotScroll");
    auto* lastCell = nubianDesigner.findChild<QToolButton*>("shipSlot_235");
    assert(scroll && lastCell && scroll->verticalScrollBar()->maximum() > 0);
    scroll->ensureWidgetVisible(lastCell); QApplication::processEvents();
    assert(scroll->viewport()->rect().contains(lastCell->mapTo(scroll->viewport(), lastCell->rect().center())));
    const auto nubianDraft = nubianDesigner.draft();
    assert(ship_design_valid({43, 1, nubianDraft.name, nubianDraft.hull, nubianDraft.components, nubianDraft.placements}));
    if (qEnvironmentVariableIsSet("SUNS_HULL_SCREENSHOTS"))
        assert(nubianDesigner.grab().save("/tmp/suns-nubian-designer.png"));
    return 0;
}
