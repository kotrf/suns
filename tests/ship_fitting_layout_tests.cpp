#include "main_window.hpp"
#include "ship_designer_dialog.hpp"
#include "ship_fitting_layout.hpp"
#include "suns/hulls.hpp"
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QToolButton>
#include <algorithm>
#include <cassert>
#include <set>

int main(int argc,char** argv)
{
    QApplication app(argc,argv);
    using namespace suns;
    QTemporaryDir settings;
    assert(settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    std::vector<ShipHullSpec> hulls = reference_hulls();
    for (int h=int(ShipHullType::Scout);h<=int(ShipHullType::MiniColonyShip);++h)
        hulls.push_back(hull_spec(ShipHullType(h)));
    for (const auto& hull : hulls) {
        const auto deck = ship_fitting_layout(hull);
        assert(deck.cells.size()==hull.fittingSlots.size() && deck.columns<=8 && deck.rows<=12);
        std::set<std::pair<int,int>> occupied;
        for (const auto& cell : deck.cells) {
            assert(occupied.emplace(cell.column,cell.row).second);
            const auto old = std::find_if(hull.fittingSlots.begin(),hull.fittingSlots.end(),
                [&](const auto& s){return s.id==cell.id;});
            assert(old!=hull.fittingSlots.end());
            assert(old->category==cell.category && old->bank==cell.bank
                && old->bankCapacity==cell.bankCapacity && old->allowedEquipment==cell.allowedEquipment);
        }
    }
    assert(ship_fitting_layout(hull_spec(ShipHullType::MediumFreighter)).columns
        > ship_fitting_layout(hull_spec(ShipHullType::SmallFreighter)).columns);
    assert(ship_fitting_layout(hull_spec(ShipHullType::ExpeditionBase)).shape==HullDeckShape::Base);

    auto state = make_demo_game();
    state.players.front().technology.levels.fill(26);
    state.players.front().race.hullAccess = HullAccess::WarMonger;
    const ShipDesign archived{state.nextShipDesignId++,1,"Stable fitting",ShipHullType::Utility,
        {ShipComponentType::FusionDrive,ShipComponentType::FusionDrive,ShipComponentType::LongRangeScanner},
        {{100,ShipComponentType::FusionDrive},{101,ShipComponentType::FusionDrive},{200,ShipComponentType::LongRangeScanner}}};
    state.shipDesigns.push_back(archived);
    MainWindow host;
    host.installUiPolish();
    host.installFleetReadabilityPolish();
    ShipDesignerDialog designer(state,1,&host);
    designer.show();
    auto* hull = designer.findChild<QComboBox*>("shipHullCatalog");
    auto* templates = designer.findChild<QComboBox*>("shipDesignTemplate");
    assert(hull && templates);
    auto* model = qobject_cast<QStandardItemModel*>(hull->model());
    const auto locked = hull->findData(int(ShipHullType::MiniMorph));
    assert(locked>=0 && !model->item(locked)->isEnabled());
    assert(!model->item(locked)->icon().isNull() && model->item(locked)->font().italic());
    assert(model->item(locked)->foreground().color()==QColor("#718698"));
    if (app.arguments().contains("--capture")) {
        hull->showPopup();
        hull->view()->scrollTo(model->index(locked,0),QAbstractItemView::PositionAtCenter);
        QApplication::processEvents();
        assert(hull->view()->window()->grab().save("/tmp/suns-hulls-locked.png"));
        hull->hidePopup();
    }

    for (const auto h : {ShipHullType::StarsScout,ShipHullType::LargeFreighter,ShipHullType::Destroyer,
             ShipHullType::MaxiMiner,ShipHullType::Battleship,ShipHullType::Dreadnought,
             ShipHullType::Nubian,ShipHullType::ExpeditionBase,ShipHullType::StarsScout}) {
        const auto index = hull->findData(int(h));
        assert(index>=0 && model->item(index)->isEnabled());
        hull->setCurrentIndex(index);
        designer.resize(1360,1000);
        QApplication::processEvents();
        auto* panel = designer.findChild<QWidget*>("shipHullDeck");
        const auto buttons = panel->findChildren<QToolButton*>();
        assert(std::size_t(buttons.size())==hull_spec(h).fittingSlots.size());
        for (int i=0;i<buttons.size();++i) {
            assert(panel->rect().contains(buttons[i]->geometry()));
            for (int j=i+1;j<buttons.size();++j) assert(!buttons[i]->geometry().intersects(buttons[j]->geometry()));
        }
        if (app.arguments().contains("--capture")) {
            assert(designer.grab().save(QString("/tmp/suns-deck-%1.png").arg(int(h))));
            assert(panel->grab().save(QString("/tmp/suns-deck-canvas-%1.png").arg(int(h))));
        }
    }
    assert(designer.findChild<QWidget*>("shipHullDeck")->minimumSizeHint().width()<550);
    templates->setCurrentIndex(templates->findData(archived.id));
    designer.resize(1000,760);
    QApplication::processEvents();
    assert(designer.width()==1000);
    auto* save = designer.findChild<QPushButton*>("saveShipDesign");
    assert(designer.rect().contains(QRect(save->mapTo(&designer,QPoint{}),save->size())));
    assert(designer.draft().placements==archived.placements);
    auto* engine = designer.findChild<QToolButton*>("shipSlot_100");
    auto* right = designer.findChild<QToolButton*>("shipSlot_203");
    engine->click();
    QKeyEvent key(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier);
    QApplication::sendEvent(engine,&key);
    assert(right && right->isChecked() && !engine->isChecked());
    auto* scanner = designer.findChild<QToolButton*>("shipSlot_200");
    const QPointer<QToolButton> stable(scanner);
    assert(scanner && !scanner->property("emptySlot").toBool());
    scanner->click();
    QKeyEvent remove(QEvent::KeyPress,Qt::Key_Delete,Qt::NoModifier);
    QApplication::sendEvent(scanner,&remove);
    assert(stable && scanner->property("emptySlot").toBool());
    auto* catalog = designer.findChild<QListWidget*>("shipComponentCatalog");
    for (int i=0;i<catalog->count();++i)
        if(catalog->item(i)->data(Qt::UserRole).toInt()==int(ShipComponentType::LongRangeScanner))catalog->setCurrentRow(i);
    designer.findChild<QPushButton*>("fitSelectedComponent")->click();
    assert(stable && !scanner->property("emptySlot").toBool());
    assert(designer.draft().placements==archived.placements);
}
