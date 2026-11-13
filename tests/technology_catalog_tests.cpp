#include "main_window.hpp"
#include "technology_catalog_dialog.hpp"
#include "ship_catalog_info.hpp"
#include "suns/campaign.hpp"
#include "suns/stations.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <cassert>
#include <array>
#include <cmath>
#include <set>

using namespace suns;
namespace suns {
struct MainWindowTestAccess {
    static void load(MainWindow& window,const GameState& state) {
        window.state_=state;window.pendingOrders_={1,{}};window.pendingDescriptions_.clear();window.rebuildScene();
    }
    static const PlayerOrders& orders(const MainWindow& window){return window.pendingOrders_;}
};
}
namespace {
void settle(){QEventLoop loop;QTimer::singleShot(40,&loop,&QEventLoop::quit);loop.exec();}
QTreeWidgetItem* entry(QTreeWidget* list,int kind,int id)
{
    for(int i=0;i<list->topLevelItemCount();++i){auto* item=list->topLevelItem(i);
        if(item->data(0,Qt::UserRole).toInt()==kind && item->data(0,Qt::UserRole+1).toInt()==id)return item;}
    assert(false);return nullptr;
}
int visible(QTreeWidget* list)
{
    int count=0;for(int i=0;i<list->topLevelItemCount();++i)count+=!list->topLevelItem(i)->isHidden();return count;
}
void hullMineralPrices(QTreeWidget* list,QTextBrowser* details)
{
    const std::array<MineralCargo,7> legacy{{{4,1,1},{6,2,2},{9,3,3},{12,6,8},{10,5,5},{18,6,6},{2,0,2}}};
    const QRegularExpression price("^Minerals: I (\\S+) / B (\\S+) / G (\\S+) kt$",QRegularExpression::MultilineOption);
    int count=0;
    for(int id=int(ShipHullType::Scout);id<=int(ShipHullType::SurveyProbe);++id) {
        auto* item=entry(list,0,id);list->setCurrentItem(item);
        const auto match=price.match(details->toPlainText());assert(match.hasMatch());
        const MineralCargo shown{match.captured(1).toDouble(),match.captured(2).toDouble(),match.captured(3).toDouble()};
        ShipDesign bare;bare.hull=ShipHullType(id);const auto cost=ship_design_mineral_cost(bare);
        assert(std::abs(shown.ironium-cost.ironium)<1e-8);
        assert(std::abs(shown.boranium-cost.boranium)<1e-8);
        assert(std::abs(shown.germanium-cost.germanium)<1e-8);
        if(id<int(legacy.size())) {
            assert(shown.ironium==legacy[id].ironium && shown.boranium==legacy[id].boranium && shown.germanium==legacy[id].germanium);
        }
        ++count;
    }
    assert(count==41);
}
void completeReference(TechnologyCatalogDialog& dialog,GameState state,bool capture)
{
    auto* list=dialog.findChild<QTreeWidget*>("technologyCatalogEntries");
    auto* search=dialog.findChild<QLineEdit*>("technologyCatalogSearch");
    auto* category=dialog.findChild<QComboBox*>("technologyCatalogCategory");
    auto* available=dialog.findChild<QCheckBox*>("technologyCatalogAvailableOnly");
    auto* details=dialog.findChild<QTextBrowser*>("technologyCatalogDetails");
    assert(list && search && category && available && details);
    assert(list->topLevelItemCount()==41+5+int(kLastShipComponent)+1);
    std::set<std::pair<int,int>> identities;int hulls=0,orbital=0;
    for(int i=0;i<list->topLevelItemCount();++i){auto* item=list->topLevelItem(i);
        assert(identities.insert({item->data(0,Qt::UserRole).toInt(),item->data(0,Qt::UserRole+1).toInt()}).second);
        hulls+=item->data(0,Qt::UserRole).toInt()==0;orbital+=item->data(0,Qt::UserRole).toInt()==1;
        assert(item->flags().testFlag(Qt::ItemIsEnabled) && item->flags().testFlag(Qt::ItemIsSelectable));}
    assert(hulls==41 && orbital==5 && !available->isChecked());
    hullMineralPrices(list,details);
    auto* nubian=entry(list,0,int(ShipHullType::Nubian));assert(nubian->text(2)=="Locked");
    list->setCurrentItem(nubian);assert(details->toPlainText().contains("Construction 26"));
    assert(details->toPlainText().contains("Cargo") && details->toPlainText().contains("Fitting banks:"));
    search->setText("nUBIaN");assert(visible(list)==1 && list->currentItem()==nubian);
    available->setChecked(true);assert(visible(list)==0 && details->toPlainText().contains("No entries"));
    state.players[0].technology.levels.fill(26);dialog.refresh(make_player_view(state,1).state,1);
    assert(visible(list)==1 && nubian->text(2)=="Available");
    assert(entry(list,0,int(ShipHullType::MiniMorph))->text(2)=="Locked"); // Trade acquisition is distinct from tech.
    assert(entry(list,0,int(ShipHullType::MetaMorph))->text(2)=="Locked"); // Race access is distinct from tech.
    state.players[0].race.hullAccess=HullAccess::HyperExpansion;
    state.shipDesigns.push_back({state.nextShipDesignId++,1,"Acquired Mini Morph",ShipHullType::MiniMorph,{ShipComponentType::QuickJump5}});
    dialog.refresh(make_player_view(state,1).state,1);
    assert(entry(list,0,int(ShipHullType::MiniMorph))->text(2)=="Available");
    assert(entry(list,0,int(ShipHullType::MetaMorph))->text(2)=="Available");
    available->setChecked(false);search->clear();category->setCurrentText("Orbital hulls");assert(visible(list)==5);
    list->setCurrentItem(entry(list,1,int(OrbitalStationHullType::Fortress)));
    assert(details->toPlainText().contains("Construction 16") && details->toPlainText().contains("12 weapons"));
    auto* citadel=entry(list,1,int(OrbitalStationHullType::NativeCitadel));
    auto* bastion=entry(list,1,int(OrbitalStationHullType::NativeBastion));
    assert(citadel->text(2)=="Locked" && bastion->text(2)=="Locked");
    const auto originalRace=state.players[0].race;
    state.players[0].race.primaryTrait=PrimaryRaceTrait::HabitatCivilization;
    state.players[0].technology.levels[std::size_t(ResearchField::Construction)]=6;
    dialog.refresh(make_player_view(state,1).state,1);
    assert(citadel->text(2)=="Available" && bastion->text(2)=="Locked");
    list->setCurrentItem(citadel);
    assert(details->toPlainText().contains("500000") && details->toPlainText().contains("Orbital civilization only"));
    state.players[0].technology.levels.fill(26);state.players[0].race=originalRace;
    dialog.refresh(make_player_view(state,1).state,1);
    state.players[0].race.primaryTrait=PrimaryRaceTrait::StargateSpecialist;
    dialog.refresh(make_player_view(state,1).state,1);
    category->setCurrentText("Station gates");assert(visible(list)==3);
    auto* gate=entry(list,2,int(ShipComponentType::InfinityStargate));assert(gate->text(2)=="Available");
    list->setCurrentItem(gate);assert(details->toPlainText().contains("Unlimited") && details->toPlainText().contains("Station fitting only"));
    assert(details->toPlainText().contains("Energy 24 + Electronics 24 + Construction 16; Stargate Specialist"));
    assert(entry(list,0,int(ShipHullType::Scout))->text(0).contains("legacy"));
    category->setCurrentIndex(0);search->clear();
    auto* kit=entry(list,2,int(ShipComponentType::OrbitalShipyardKit));list->setCurrentItem(kit);
    assert(details->toPlainText().contains("Mass") && details->toPlainText().contains("Minerals"));
    assert(details->toPlainText().contains("permanent orbital shipyard"));
    search->setText("no_such_catalog_item_123");assert(visible(list)==0);
    search->clear();category->setCurrentText("Ship hulls");list->setCurrentItem(nubian);
    dialog.resize(1000,700);dialog.show();settle();
    if(capture)dialog.grab().save("/tmp/suns-technology-catalog-hulls.png");
    category->setCurrentText("Station gates");list->setCurrentItem(gate);settle();
    if(capture)dialog.grab().save("/tmp/suns-technology-catalog-gates.png");
    dialog.resize(720,480);settle();auto* close=dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close);
    assert(dialog.rect().contains(QRect(close->mapTo(&dialog,QPoint{}),close->size())));
}
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);QTemporaryDir temp;assert(temp.isValid());
    QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,temp.path());
    MainWindow window;window.installUiPolish();window.installFleetReadabilityPolish();
    auto state=make_demo_game();state.players[0].technology.levels.fill(0);MainWindowTestAccess::load(window,state);
    {
        TechnologyCatalogDialog standalone(make_player_view(state,1).state,1,&window);
        completeReference(standalone,state,app.arguments().contains("--capture"));
    }
    auto* menu=window.findChild<QAction*>("openTechnologyCatalogAction");
    auto* toolbar=window.findChild<QAction*>("openTechnologyCatalogToolAction");
    assert(menu && toolbar && !menu->icon().isNull() && !toolbar->icon().isNull());
    assert(window.findChild<QToolBar*>("dialogToolbar")->actions().contains(toolbar));
    menu->trigger();settle();auto* dialog=window.findChild<TechnologyCatalogDialog*>("technologyCatalogDialog");
    assert(dialog && dialog->isVisible());toolbar->trigger();settle();
    assert(window.findChildren<TechnologyCatalogDialog*>("technologyCatalogDialog").size()==1);
    auto* list=dialog->findChild<QTreeWidget*>("technologyCatalogEntries");
    auto* target=entry(list,0,int(ShipHullType::Nubian));assert(target->text(2)=="Locked");
    state.players[0].technology.levels.fill(26);MainWindowTestAccess::load(window,state);settle();
    assert(target->text(2)=="Available");
    list->setCurrentItem(entry(list,2,int(ShipComponentType::QuickJump5)));
    auto* text=dialog->findChild<QTextBrowser*>("technologyCatalogDetails");text->verticalScrollBar()->setValue(text->verticalScrollBar()->maximum());
    const auto contents=text->toPlainText();const auto position=text->verticalScrollBar()->value();
    state.players.push_back({2,"Secret adversary"});state.players.back().technology.levels.fill(26);
    MainWindowTestAccess::load(window,state);settle();
    assert(text->toPlainText()==contents && text->verticalScrollBar()->value()==position);
    assert(MainWindowTestAccess::orders(window).orders.empty());
    // Closing releases the tool and a later action creates a fresh live view.
    dialog->close();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);settle();
    toolbar->trigger();settle();assert(window.findChild<TechnologyCatalogDialog*>("technologyCatalogDialog")->isVisible());
}
