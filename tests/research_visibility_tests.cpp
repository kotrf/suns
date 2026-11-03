#include "main_window.hpp"

#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QPersistentModelIndex>
#include <QSettings>
#include <QTemporaryDir>
#include <QTreeWidget>

#include <cassert>

namespace suns {
struct MainWindowTestAccess {
    static void setup(MainWindow& w)
    {
        w.state_ = make_demo_game();
        w.state_.players.front().technology.levels.fill(0);
        w.pendingOrders_ = {1, {}};
        w.pendingDescriptions_.clear();
        w.rebuildScene();
    }
    static void unlockScoop(MainWindow& w)
    {
        w.state_.players.front().technology.levels[std::size_t(ResearchField::Propulsion)] = 3;
        w.updateControls();
    }
    static void lockScoop(MainWindow& w)
    {
        w.state_.players.front().technology.levels[std::size_t(ResearchField::Propulsion)] = 0;
        w.updateControls();
    }
    static void refresh(MainWindow& w) { w.refreshResearchPanel(); }
    static void open(MainWindow& w) { w.openResearchDialog(); }
};
} // namespace suns

namespace {
QTreeWidgetItem* scoop(QTreeWidget* catalog)
{
    for (int row = 0; row < catalog->topLevelItemCount(); ++row) {
        auto* item = catalog->topLevelItem(row);
        if (item->text(0).startsWith("Efficient Ram Scoop")) return item;
    }
    assert(false);
    return nullptr;
}
} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir settings;
    assert(settings.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings.path());
    suns::MainWindow window;
    suns::MainWindowTestAccess::setup(window);
    window.installResearch();
    auto* dialog = window.findChild<QDialog*>("researchDialog");
    auto* catalog = window.findChild<QTreeWidget*>("technologyCatalog");
    assert(dialog && catalog && !dialog->isVisible());
    auto* entry = scoop(catalog);
    assert(entry->text(2) == "Locked");
    const QPersistentModelIndex retained(catalog->model()->index(catalog->indexOfTopLevelItem(entry), 0));

    // An automatic workspace update must not rebuild a hidden tool's model.
    suns::MainWindowTestAccess::unlockScoop(window);
    assert(retained.isValid() && scoop(catalog)->text(2) == "Locked");

    // Direct showing (not only the menu Open path) refreshes before display.
    dialog->show();
    assert(!retained.isValid() && scoop(catalog)->text(2) == "Available");
    suns::MainWindowTestAccess::lockScoop(window);
    assert(scoop(catalog)->text(2) == "Locked");

    dialog->hide();
    suns::MainWindowTestAccess::unlockScoop(window);
    assert(scoop(catalog)->text(2) == "Locked");
    suns::MainWindowTestAccess::open(window);
    assert(dialog->isVisible() && scoop(catalog)->text(2) == "Available");

    // Explicit refresh remains eager for diagnostic/readers even while hidden.
    dialog->hide();
    suns::MainWindowTestAccess::lockScoop(window);
    suns::MainWindowTestAccess::refresh(window);
    assert(scoop(catalog)->text(2) == "Locked");
}
