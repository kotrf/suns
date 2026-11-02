#include "technology_catalog_dialog.hpp"
#include "ship_catalog_info.hpp"
#include "suns/campaign.hpp"
#include "suns/hulls.hpp"
#include "suns/equipment.hpp"
#include "suns/stations.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QColor>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QSplitter>
#include <QStyle>
#include <QStringList>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <map>

namespace suns {
namespace {
enum Role {EntryKind=Qt::UserRole, Identity, Available, Description, Requirements, Search};
enum Kind {ShipHull, OrbitalHull, Component};
QIcon lockedIcon()
{
    QPixmap image(20,20);image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#8296a7"),1.5));p.setBrush(Qt::NoBrush);p.drawRoundedRect(QRectF(6,2,8,11),4,4);
    p.setBrush(QColor("#243747"));p.drawRoundedRect(QRectF(3,9,14,10),2,2);p.end();return QIcon(image);
}

QString minerals(MineralCargo value)
{
    return QString("Minerals: I %1 / B %2 / G %3 kt").arg(value.ironium).arg(value.boranium).arg(value.germanium);
}
QString hullRequirements(const ShipHullSpec& hull)
{
    for (const auto& unlock : research_unlocks()) if (unlock.hull == hull.type) {
        auto result=QString::fromStdString(research_unlock_requirement(unlock));
        if(unlock.legacyHull) result+="; legacy campaign catalog";
        return result;
    }
    if(hull.type==ShipHullType::MiniColonyShip) return "Settler engine access; legacy campaign catalog";
    return QString("Construction %1; legacy campaign catalog").arg(
        hull.type==ShipHullType::RemoteMiner ? 1 : hull.type==ShipHullType::HeavyTransport ? 2 : 0);
}
QString hullDescription(const ShipHullSpec& hull)
{
    QStringList lines{QString("Mass %1 kt • Production %2").arg(hull.mass).arg(hull.buildCost),minerals(hull.baseMineralCost),
        QString("Base armor %1 • Initiative %2").arg(hull.armor).arg(hull.initiative),
        QString("Cargo %1 kt • Fuel %2 mg • Engines %3").arg(hull.baseCargoCapacity).arg(hull.baseFuelCapacity).arg(hull.requiredEngines)};
    if(hull.fuelGenerationPerTurn) lines<<QString("Generates %1 mg fuel/year").arg(hull.fuelGenerationPerTurn);
    if(hull.fleetRepairBonus) lines<<QString("Stationary fleet repair bonus +%1 damage points/year").arg(hull.fleetRepairBonus);
    if(hull.doublesMineLaying) lines<<"Doubles fitted mine-laying output";
    lines<<""<<"Fitting banks:";
    std::map<unsigned,std::pair<QString,unsigned>> banks;
    for(const auto& cell:hull.fittingSlots) {
        auto& bank=banks[cell.bank ? cell.bank : 1000+cell.id];
        bank.first=QString::fromStdString(ship_slot_name(cell));++bank.second;
    }
    std::map<QString,unsigned> groups;
    for(const auto& [id,bank]:banks)++groups[QString("%1 — %2 cells").arg(bank.first).arg(bank.second)];
    for(const auto& [label,count]:groups) lines<<(count==1 ? label : QString("%1 × %2 banks").arg(label).arg(count));
    return lines.join("\n");
}
QString componentRequirements(ShipComponentType component)
{
    if(const auto* gate=stargate_spec(component))return QString("Energy %1 + Electronics %2 + Construction %3%4")
        .arg(gate->energy).arg(gate->electronics).arg(gate->construction)
        .arg(gate->specialistOnly ? "; Stargate Specialist" : "");
    auto result=componentCatalogRequirements(component);
    for(const auto& unlock:research_unlocks()) if(unlock.component==component) {
        if(unlock.legacyEquipment || unlock.legacyScanner || unlock.legacyMining || unlock.legacyPropulsion)
            result+="; legacy campaign catalog";
        break;
    }
    return result.isEmpty() ? QString("Starting technology / applicable campaign catalog") : result;
}
}

TechnologyCatalogDialog::TechnologyCatalogDialog(const GameState& received,PlayerId player,QWidget* parent)
    : QDialog(parent,Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint)
{
    setObjectName("technologyCatalogDialog");setWindowTitle("Hull and equipment catalog");resize(1040,740);
    auto* layout=new QVBoxLayout(this);
    context_=new QLabel(this);context_->setObjectName("technologyCatalogContext");context_->setWordWrap(true);layout->addWidget(context_);
    auto* filters=new QHBoxLayout;
    search_=new QLineEdit(this);search_->setObjectName("technologyCatalogSearch");search_->setPlaceholderText("Search names, requirements and effects…");filters->addWidget(search_,1);
    category_=new QComboBox(this);category_->setObjectName("technologyCatalogCategory");category_->addItem("All entries");filters->addWidget(category_);
    availableOnly_=new QCheckBox("Available only",this);availableOnly_->setObjectName("technologyCatalogAvailableOnly");filters->addWidget(availableOnly_);layout->addLayout(filters);
    auto* split=new QSplitter(Qt::Horizontal,this);split->setObjectName("technologyCatalogSplitter");layout->addWidget(split,1);
    entries_=new QTreeWidget(split);entries_->setObjectName("technologyCatalogEntries");entries_->setHeaderLabels({"Name","Category","Availability"});
    entries_->setRootIsDecorated(false);entries_->setUniformRowHeights(true);entries_->setAlternatingRowColors(true);
    entries_->setMinimumWidth(250);entries_->header()->setSectionResizeMode(0,QHeaderView::Stretch);
    entries_->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);entries_->header()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    details_=new QTextBrowser(split);details_->setObjectName("technologyCatalogDetails");details_->setMinimumWidth(240);details_->setOpenLinks(false);
    split->setSizes({580,440});split->setStretchFactor(0,3);split->setStretchFactor(1,2);
    const auto add=[&](int kind,int id,QString name,QString category,QString requirement,QString description) {
        auto* item=new QTreeWidgetItem(entries_,{name,category,{}});
        item->setData(0,EntryKind,kind);item->setData(0,Identity,id);
        item->setData(0,Description,description);item->setData(0,Requirements,requirement);
        item->setData(0,Search,(name+"\n"+category+"\n"+requirement+"\n"+description).toCaseFolded());
        item->setToolTip(0,requirement);
        if(category_->findText(category)<0) category_->addItem(category);
    };
    const auto addHull=[&](const ShipHullSpec& hull) {add(ShipHull,int(hull.type),QString::fromStdString(hull.name)
        +(reference_hull(hull.type) ? "" : " (legacy)"),"Ship hulls",hullRequirements(hull),hullDescription(hull));};
    for(int type=int(ShipHullType::Scout);type<=int(ShipHullType::MiniColonyShip);++type)addHull(hull_spec(ShipHullType(type)));
    for(const auto& hull:reference_hulls())addHull(hull);
    for(auto type:{OrbitalStationHullType::OrbitalDock,OrbitalStationHullType::Starbase,OrbitalStationHullType::Fortress}) {
        const auto& spec=station_hull_spec(type);
        add(OrbitalHull,int(type),QString::fromUtf8(spec.name),"Orbital hulls",QString("Construction %1").arg(spec.construction),
            QString("Production %1\n%2\nBase armor %3\nSlots: %4 weapons / %5 armor-shields / %6 electronics\nStationary orbital hull; one station per planet. Shipyard / repair and refueling services are configurable.")
            .arg(spec.cost).arg(minerals(spec.minerals)).arg(spec.armor).arg(spec.weapons).arg(spec.defenses).arg(spec.electronics));
    }
    for(unsigned id=0;id<=unsigned(kLastShipComponent);++id) {
        const auto component=ShipComponentType(id);const auto spec=component_spec(component);const auto* gate=stargate_spec(component);
        auto description=componentCatalogDescription(component);
        if(gate) description+=QString("\nStation fitting only — minimum hull: %1\nGate range %2 ly; maximum loaded mass %3 kt")
            .arg(station_hull_spec(gate->minimumHull).name).arg(gate->rangeLy ? QString::number(gate->rangeLy) : QString("Unlimited"))
            .arg(gate->maximumMassKt ? QString::number(gate->maximumMassKt) : QString("Unlimited"));
        add(Component,id,gate ? QString::fromUtf8(gate->name) : QString::fromStdString(spec.name),
            gate ? QString("Station gates") : QString::fromStdString(ship_component_equipment_name(component)),componentRequirements(component),description);
    }
    entries_->sortItems(0,Qt::AscendingOrder);
    count_=new QLabel(this);count_->setObjectName("technologyCatalogCount");layout->addWidget(count_);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,this);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::close);layout->addWidget(buttons);
    connect(entries_,&QTreeWidget::currentItemChanged,this,[this]{updateDetails();});
    connect(search_,&QLineEdit::textChanged,this,[this]{filter();});connect(category_,&QComboBox::currentIndexChanged,this,[this]{filter();});
    connect(availableOnly_,&QCheckBox::toggled,this,[this]{filter();});
    refresh(received,player);
}

void TechnologyCatalogDialog::refresh(const GameState& received,PlayerId player)
{
    context_->setText(QString("Complete reference catalog • Empire %1 • Turn %2\nSelect any entry to inspect its requirements and characteristics, including locked equipment.").arg(player).arg(received.turn));
    bool changed=false;
    for(int i=0;i<entries_->topLevelItemCount();++i) {
        auto* item=entries_->topLevelItem(i);const auto kind=item->data(0,EntryKind).toInt(),id=item->data(0,Identity).toInt();
        const auto component=ShipComponentType(id);
        const bool available=kind==ShipHull ? ship_hull_available_to_player(received,player,ShipHullType(id))
            : kind==OrbitalHull ? station_loadout_available(received,player,StationLoadout{OrbitalStationHullType(id)})
            : stargate_spec(component) ? stargate_component_available(received,player,component)
            : component_available_to_player(received,player,component);
        if(item->data(0,Available).isValid() && item->data(0,Available).toBool()==available)continue;
        changed=true;item->setData(0,Available,available);item->setText(2,available ? "Available" : "Locked");
        for(int column=0;column<3;++column)item->setForeground(column,QColor(available ? "#d6e5f2" : "#8296a7"));
        item->setIcon(0,available ? style()->standardIcon(QStyle::SP_DialogApplyButton) : lockedIcon());
    }
    if(changed)filter();
}
void TechnologyCatalogDialog::filter()
{
    const auto query=search_->text().trimmed().toCaseFolded();int visible=0;QTreeWidgetItem* first=nullptr;
    for(int i=0;i<entries_->topLevelItemCount();++i) {
        auto* item=entries_->topLevelItem(i);
        const bool show=(category_->currentIndex()==0 || item->text(1)==category_->currentText())
            && (!availableOnly_->isChecked() || item->data(0,Available).toBool())
            && item->data(0,Search).toString().contains(query);
        item->setHidden(!show);if(show){++visible;if(!first)first=item;}
    }
    count_->setText(QString("%1 / %2 entries").arg(visible).arg(entries_->topLevelItemCount()));
    if(!entries_->currentItem() || entries_->currentItem()->isHidden())entries_->setCurrentItem(first);
    updateDetails();
}
void TechnologyCatalogDialog::updateDetails()
{
    const auto* item=entries_->currentItem();
    const auto text=!item || item->isHidden() ? QString("No entries match these filters.")
        : item->text(0)+"\n"+item->text(1)+" • "+item->text(2)+"\n\nRequires: "+item->data(0,Requirements).toString()+"\n\n"+item->data(0,Description).toString();
    if(details_->toPlainText()!=text)details_->setPlainText(text);
}
} // namespace suns
