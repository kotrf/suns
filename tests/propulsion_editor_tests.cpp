#include "main_window.hpp"
#include "ship_designer_dialog.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QListWidget>
#include <QLabel>
#include <QMimeData>
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

    auto state = generate_campaign(GalaxyConfig{}, {{"Designer", RacePreset::Terran, true}});
    state.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Propulsion)] = 20;
    state.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Energy)] = 5;
    ShipDesignerDialog designer(state, 1);
    auto* components = designer.findChild<QListWidget*>("shipComponentCatalog");
    assert(components && designer.draft().components.front() == ShipComponentType::QuickJump5);
    int engines = 0;
    for (int row = 0; row < components->count(); ++row) {
        const auto component = static_cast<ShipComponentType>(components->item(row)->data(Qt::UserRole).toInt());
        assert(!legacy_propulsion_component(component));
        engines += component_spec(component).kind == ShipComponentKind::Engine;
    }
    assert(engines == 15);
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
    state.players.front().technology.levels[static_cast<std::size_t>(ResearchField::Construction)] = 2;
    ShipDesignerDialog heavyDesigner(state, 1);
    auto* hulls = heavyDesigner.findChild<QComboBox*>("shipHullCatalog");
    assert(hulls);
    hulls->setCurrentIndex(hulls->findData(static_cast<int>(ShipHullType::HeavyTransport)));
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
    return 0;
}
