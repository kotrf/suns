#include "ship_designer_dialog.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QStandardItemModel>
#include <QStringList>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <optional>

namespace suns {

namespace {

constexpr auto kComponentMimeType = "application/x-suns-ship-component";

template <typename Enum>
void addEnumItem(QComboBox* combo, const QString& text, Enum value)
{
    combo->addItem(text, static_cast<int>(value));
}

QString signedFuelRate(double value)
{
    if (value < -0.000001) return QString("%1 (gain)").arg(-value, 0, 'f', 2);
    if (value > 0.000001) return QString::number(value, 'f', 2);
    return "0.00";
}

QString slotCategoryName(ShipSlotCategory category)
{
    switch (category) {
    case ShipSlotCategory::Engine: return "Engine";
    case ShipSlotCategory::General: return "General";
    case ShipSlotCategory::Mining: return "Mining";
    }
    return "Unknown";
}

QString unlockRequirement(ShipComponentType component)
{
    switch (component) {
    case ShipComponentType::AntimatterGenerator: return "Energy 1";
    case ShipComponentType::AdvancedFusionDrive: return "Propulsion 1";
    case ShipComponentType::CompactLongRangeScanner: return "Electronics 1";
    case ShipComponentType::ExtendedRangeScanner: return "Electronics 2";
    case ShipComponentType::PenetratingScanner: return "Electronics 3";
    case ShipComponentType::RemoteMiningModule: return "Construction 1";
    default: return {};
    }
}

QString componentTooltip(ShipComponentType component)
{
    const auto spec = component_spec(component);
    QStringList facts{
        QString("%1 slot").arg(slotCategoryName(ship_component_slot_category(component))),
        QString("Mass %1 kt").arg(spec.mass, 0, 'f', 1),
        QString("Cost %1").arg(spec.buildCost),
    };
    if (spec.maxWarp > 0) facts << QString("Maximum Warp %1").arg(spec.maxWarp);
    if (spec.sensorRange > 0.0) facts << QString("Scanner %1 ly").arg(spec.sensorRange, 0, 'f', 0);
    if (spec.fuelCapacity > 0.0) facts << QString("Fuel capacity +%1").arg(spec.fuelCapacity, 0, 'f', 0);
    if (spec.fuelGenerationPerTurn > 0.0) {
        facts << QString("Fuel generation +%1/turn").arg(spec.fuelGenerationPerTurn, 0, 'f', 0);
    }
    if (spec.cargoCapacity > 0.0) facts << QString("Cargo +%1").arg(spec.cargoCapacity, 0, 'f', 0);
    if (spec.remoteMiningUnits > 0.0) facts << "Remote mining equipment";
    if (spec.enablesColonization) facts << "Enables colonization";
    return facts.join("\n");
}

QByteArray encodeComponentDrag(ShipComponentType component, ShipSlotId sourceSlot = 0)
{
    return QByteArray::number(static_cast<int>(component)) + ':' + QByteArray::number(sourceSlot);
}

struct ComponentDrag {
    ShipComponentType component{ShipComponentType::FusionDrive};
    ShipSlotId sourceSlot{};
};

std::optional<ComponentDrag> decodeComponentDrag(const QMimeData* mime)
{
    if (!mime || !mime->hasFormat(kComponentMimeType)) return std::nullopt;
    const auto parts = mime->data(kComponentMimeType).split(':');
    if (parts.size() != 2) return std::nullopt;
    bool componentOk = false;
    bool slotOk = false;
    const auto component = parts[0].toInt(&componentOk);
    const auto slot = parts[1].toUInt(&slotOk);
    if (!componentOk || !slotOk
        || component < static_cast<int>(ShipComponentType::FusionDrive)
        || component > static_cast<int>(ShipComponentType::ExtendedRangeScanner)
        || slot > std::numeric_limits<ShipSlotId>::max()) {
        return std::nullopt;
    }
    return ComponentDrag{
        static_cast<ShipComponentType>(component), static_cast<ShipSlotId>(slot)};
}

class ComponentCatalog final : public QListWidget {
public:
    using QListWidget::QListWidget;

protected:
    void startDrag(Qt::DropActions) override
    {
        const auto* item = currentItem();
        if (!item || !(item->flags() & Qt::ItemIsEnabled)) return;
        auto* mime = new QMimeData;
        mime->setData(kComponentMimeType,
            encodeComponentDrag(static_cast<ShipComponentType>(item->data(Qt::UserRole).toInt())));
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->exec(Qt::CopyAction);
    }
};

class SlotButton final : public QToolButton {
public:
    using DropHandler = std::function<void(ShipComponentType, ShipSlotId)>;
    using SlotHandler = std::function<void(ShipSlotId)>;

    SlotButton(
        ShipSlotSpec slot,
        std::optional<ShipComponentType> component,
        bool chosen,
        DropHandler dropped,
        SlotHandler selected,
        SlotHandler removed,
        QWidget* parent)
        : QToolButton(parent)
        , slot_(slot)
        , component_(component)
        , dropped_(std::move(dropped))
        , selected_(std::move(selected))
        , removed_(std::move(removed))
    {
        setAcceptDrops(true);
        setObjectName(QString("shipSlot_%1").arg(slot_.id));
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(150, 72);
        setStyleSheet(chosen
                ? "QToolButton { border: 2px solid #52b6d9; background: #193346; padding: 5px; }"
                : "QToolButton { border: 1px solid #52677a; background: #142433; padding: 5px; }"
                  "QToolButton:hover, QToolButton:focus { border: 2px solid #78c8e5; }");
        refreshText();
        connect(this, &QToolButton::clicked, this, [this] { selected_(slot_.id); });
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        const auto payload = decodeComponentDrag(event->mimeData());
        if (payload && ship_component_slot_category(payload->component) == slot_.category) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent* event) override
    {
        const auto payload = decodeComponentDrag(event->mimeData());
        if (!payload || ship_component_slot_category(payload->component) != slot_.category) return;
        event->acceptProposedAction();
        dropped_(payload->component, payload->sourceSlot);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        dragStart_ = event->pos();
        QToolButton::mousePressEvent(event);
        selected_(slot_.id);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!component_ || !(event->buttons() & Qt::LeftButton)
            || (event->pos() - dragStart_).manhattanLength() < QApplication::startDragDistance()) {
            QToolButton::mouseMoveEvent(event);
            return;
        }
        auto* mime = new QMimeData;
        mime->setData(kComponentMimeType, encodeComponentDrag(*component_, slot_.id));
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->exec(Qt::MoveAction);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        event->accept();
        if (component_) removed_(slot_.id);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (component_ && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
            event->accept();
            removed_(slot_.id);
            return;
        }
        QToolButton::keyPressEvent(event);
    }

private:
    void refreshText()
    {
        const auto title = QString("%1 slot #%2").arg(slotCategoryName(slot_.category)).arg(slot_.id);
        if (component_) {
            setText(QString("%1\n%2").arg(title, QString::fromStdString(component_spec(*component_).name)));
            setToolTip(componentTooltip(*component_)
                + "\n\nDrag to another compatible slot. Double-click or press Delete to remove.");
        } else {
            setText(QString("%1\nEmpty").arg(title));
            setToolTip("Select this cell and use Fit selected, or drag a compatible component here.");
        }
    }

    ShipSlotSpec slot_;
    std::optional<ShipComponentType> component_;
    DropHandler dropped_;
    SlotHandler selected_;
    SlotHandler removed_;
    QPoint dragStart_;
};

} // namespace

ShipDesignerDialog::ShipDesignerDialog(const GameState& state, PlayerId player, QWidget* parent)
    : QDialog(parent)
    , player_(player)
    , remoteMiningAvailable_(component_available_to_player(
          state, player, ShipComponentType::RemoteMiningModule))
{
    setWindowTitle("Suns! — Ship Designer");
    resize(980, 760);

    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(
        "Choose a hull, then fit components into its visible cells. Drag from the catalog or select a catalog item and a cell. "
        "Move fitted components by dragging; double-click a cell or press Delete to remove its component.",
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* form = new QFormLayout;
    nameEdit_ = new QLineEdit("New Design", this);
    nameEdit_->setObjectName("shipDesignName");
    nameEdit_->setMaxLength(48);
    form->addRow("Design name", nameEdit_);

    hullCombo_ = new QComboBox(this);
    hullCombo_->setObjectName("shipHullCatalog");
    addEnumItem(hullCombo_, "Scout Hull", ShipHullType::Scout);
    addEnumItem(hullCombo_, "Light Transport", ShipHullType::LightTransport);
    addEnumItem(hullCombo_, "Medium Transport", ShipHullType::MediumTransport);
    addEnumItem(hullCombo_, "Utility Hull", ShipHullType::Utility);
    addEnumItem(hullCombo_, remoteMiningAvailable_ ? "Remote Miner" : "Remote Miner (locked — Construction 1)",
        ShipHullType::RemoteMiner);
    if (!remoteMiningAvailable_) {
        if (auto* model = qobject_cast<QStandardItemModel*>(hullCombo_->model())) {
            model->item(hullCombo_->count() - 1)->setEnabled(false);
        }
    }
    form->addRow("Hull", hullCombo_);
    layout->addLayout(form);

    auto* workspace = new QHBoxLayout;
    auto* catalogGroup = new QGroupBox("Component catalog", this);
    auto* catalogLayout = new QVBoxLayout(catalogGroup);
    componentCatalog_ = new ComponentCatalog(catalogGroup);
    componentCatalog_->setObjectName("shipComponentCatalog");
    componentCatalog_->setSelectionMode(QAbstractItemView::SingleSelection);
    componentCatalog_->setDragEnabled(true);
    componentCatalog_->setDragDropMode(QAbstractItemView::DragOnly);
    catalogLayout->addWidget(componentCatalog_, 1);
    fitButton_ = new QPushButton("Fit selected →", catalogGroup);
    fitButton_->setObjectName("fitSelectedComponent");
    fitButton_->setEnabled(false);
    catalogLayout->addWidget(fitButton_);
    workspace->addWidget(catalogGroup, 1);

    const std::array catalog{
        ShipComponentType::FusionDrive,
        ShipComponentType::RamScoopDrive,
        ShipComponentType::RadiatingRamScoopDrive,
        ShipComponentType::AdvancedFusionDrive,
        ShipComponentType::LongRangeScanner,
        ShipComponentType::CompactLongRangeScanner,
        ShipComponentType::ExtendedRangeScanner,
        ShipComponentType::PenetratingScanner,
        ShipComponentType::RemoteMiningModule,
        ShipComponentType::ColonyModule,
        ShipComponentType::FuelTank,
        ShipComponentType::CargoPod,
        ShipComponentType::AntimatterGenerator,
    };
    for (const auto component : catalog) {
        const auto available = component_available_to_player(state, player, component);
        auto label = QString("%1 • %2")
                         .arg(slotCategoryName(ship_component_slot_category(component)),
                             QString::fromStdString(component_spec(component).name));
        if (!available) label += QString("  [locked — %1]").arg(unlockRequirement(component));
        auto* item = new QListWidgetItem(label, componentCatalog_);
        item->setData(Qt::UserRole, static_cast<int>(component));
        item->setToolTip(componentTooltip(component)
            + (available ? QString{} : QString("\nLocked — requires %1").arg(unlockRequirement(component))));
        if (!available) item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsDragEnabled);
    }
    componentCatalog_->setCurrentRow(0);

    auto* fittingGroup = new QGroupBox("Hull fitting cells", this);
    auto* fittingLayout = new QVBoxLayout(fittingGroup);
    slotPanel_ = new QWidget(fittingGroup);
    slotGrid_ = new QGridLayout(slotPanel_);
    slotGrid_->setSpacing(8);
    fittingLayout->addWidget(slotPanel_, 1);
    fitMessage_ = new QLabel(fittingGroup);
    fitMessage_->setWordWrap(true);
    fittingLayout->addWidget(fitMessage_);
    removeButton_ = new QPushButton("Remove selected component", fittingGroup);
    removeButton_->setObjectName("removeSelectedComponent");
    removeButton_->setEnabled(false);
    fittingLayout->addWidget(removeButton_);
    workspace->addWidget(fittingGroup, 2);
    layout->addLayout(workspace, 2);

    previewLabel_ = new QLabel(this);
    previewLabel_->setWordWrap(true);
    previewLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(previewLabel_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    saveButton_ = buttons->button(QDialogButtonBox::Save);
    saveButton_->setObjectName("saveShipDesign");
    layout->addWidget(buttons);
    placements_ = autoplace_ship_components(ShipHullType::Scout,
        {ShipComponentType::FusionDrive, ShipComponentType::LongRangeScanner});

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(nameEdit_, &QLineEdit::textChanged, this, [this] { updatePreview(); });
    connect(hullCombo_, &QComboBox::currentIndexChanged, this, [this] {
        const auto hullType = static_cast<ShipHullType>(hullCombo_->currentData().toInt());
        ShipDesign migrated;
        migrated.hull = hullType;
        for (const auto& placement : placements_) migrated.components.push_back(placement.component);
        normalize_ship_design_engine_bank(migrated);
        const auto& components = migrated.components;
        const auto replacement = migrated.placements;
        const auto allFit = replacement.size() == components.size();
        if (allFit) {
            placements_ = replacement;
        } else {
            placements_.clear();
            const auto slots = hull_spec(hullType).slots;
            for (const auto component : components) {
                const auto target = std::find_if(slots.begin(), slots.end(), [&](const ShipSlotSpec& slot) {
                    if (slot.category != ship_component_slot_category(component)) return false;
                    return std::none_of(placements_.begin(), placements_.end(), [&](const ShipComponentPlacement& placed) {
                        return placed.slot == slot.id;
                    });
                });
                if (target != slots.end()) placements_.push_back({target->id, component});
            }
        }
        selectedSlot_ = 0;
        rebuildSlotGrid();
        updatePreview();
        if (!allFit) fitMessage_->setText("Some equipment was removed because the new hull has fewer compatible cells.");
    });
    connect(componentCatalog_, &QListWidget::currentItemChanged, this, [this] {
        fitButton_->setEnabled(selectedSlot_ != 0 && selectedCatalogComponent().has_value());
    });
    connect(componentCatalog_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        const auto component = selectedCatalogComponent();
        if (!component) return;
        const auto hull = hull_spec(static_cast<ShipHullType>(hullCombo_->currentData().toInt()));
        if (component_spec(*component).kind == ShipComponentKind::Engine) {
            const auto target = std::find_if(hull.slots.begin(), hull.slots.end(), [](const ShipSlotSpec& slot) {
                return slot.category == ShipSlotCategory::Engine;
            });
            if (target != hull.slots.end()) fitComponent(*component, target->id);
            return;
        }
        auto target = std::find_if(hull.slots.begin(), hull.slots.end(), [&](const ShipSlotSpec& slot) {
            return slot.id == selectedSlot_
                && slot.category == ship_component_slot_category(*component);
        });
        if (target == hull.slots.end()) {
            target = std::find_if(hull.slots.begin(), hull.slots.end(), [&](const ShipSlotSpec& slot) {
                if (slot.category != ship_component_slot_category(*component)) return false;
                return std::none_of(placements_.begin(), placements_.end(), [&](const ShipComponentPlacement& placed) {
                    return placed.slot == slot.id;
                });
            });
        }
        if (target != hull.slots.end()) fitComponent(*component, target->id);
    });
    connect(fitButton_, &QPushButton::clicked, this, [this] {
        const auto component = selectedCatalogComponent();
        if (component && selectedSlot_ != 0) fitComponent(*component, selectedSlot_);
    });
    connect(removeButton_, &QPushButton::clicked, this, [this] {
        if (selectedSlot_ != 0) removeComponent(selectedSlot_);
    });

    rebuildSlotGrid();
    updatePreview();
}

std::optional<ShipComponentType> ShipDesignerDialog::selectedCatalogComponent() const
{
    const auto* item = componentCatalog_->currentItem();
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) return std::nullopt;
    return static_cast<ShipComponentType>(item->data(Qt::UserRole).toInt());
}

void ShipDesignerDialog::selectSlot(ShipSlotId slot)
{
    selectedSlot_ = slot;
    const auto placement = std::find_if(
        placements_.begin(), placements_.end(), [&](const ShipComponentPlacement& candidate) {
            return candidate.slot == slot;
        });
    removeButton_->setEnabled(placement != placements_.end());
    fitButton_->setEnabled(selectedCatalogComponent().has_value());
}

void ShipDesignerDialog::fitComponent(
    ShipComponentType component, ShipSlotId target, ShipSlotId source)
{
    const auto hull = hull_spec(static_cast<ShipHullType>(hullCombo_->currentData().toInt()));
    const auto targetSlot = std::find_if(hull.slots.begin(), hull.slots.end(), [&](const ShipSlotSpec& slot) {
        return slot.id == target;
    });
    if (targetSlot == hull.slots.end()
        || targetSlot->category != ship_component_slot_category(component)) {
        fitMessage_->setText("That component is incompatible with the selected cell.");
        return;
    }

    if (component_spec(component).kind == ShipComponentKind::Engine) {
        std::erase_if(placements_, [](const ShipComponentPlacement& placement) {
            return component_spec(placement.component).kind == ShipComponentKind::Engine;
        });
        for (const auto& slot : hull.slots) {
            if (slot.category == ShipSlotCategory::Engine) placements_.push_back({slot.id, component});
        }
        selectedSlot_ = target;
        rebuildSlotGrid();
        updatePreview();
        return;
    }

    auto sourcePlacement = placements_.end();
    if (source != 0) {
        sourcePlacement = std::find_if(placements_.begin(), placements_.end(), [&](const ShipComponentPlacement& placed) {
            return placed.slot == source && placed.component == component;
        });
        if (sourcePlacement == placements_.end() || source == target) return;
    }
    auto targetPlacement = std::find_if(placements_.begin(), placements_.end(), [&](const ShipComponentPlacement& placed) {
        return placed.slot == target;
    });
    if (sourcePlacement == placements_.end()) {
        if (targetPlacement != placements_.end()) {
            targetPlacement->component = component;
        } else {
            placements_.push_back({target, component});
        }
    } else if (targetPlacement == placements_.end()) {
        sourcePlacement->slot = target;
    } else {
        const auto displaced = targetPlacement->component;
        const auto sourceSlot = std::find_if(hull.slots.begin(), hull.slots.end(), [&](const ShipSlotSpec& slot) {
            return slot.id == source;
        });
        if (sourceSlot == hull.slots.end()
            || sourceSlot->category != ship_component_slot_category(displaced)) {
            fitMessage_->setText("Those components cannot be swapped between different slot categories.");
            return;
        }
        sourcePlacement->component = displaced;
        targetPlacement->component = component;
    }
    selectedSlot_ = target;
    rebuildSlotGrid();
    updatePreview();
}

void ShipDesignerDialog::removeComponent(ShipSlotId slot)
{
    const auto hull = hull_spec(static_cast<ShipHullType>(hullCombo_->currentData().toInt()));
    const auto selected = std::find_if(hull.slots.begin(), hull.slots.end(), [&](const ShipSlotSpec& candidate) {
        return candidate.id == slot;
    });
    const auto removingEngineBank = selected != hull.slots.end()
        && selected->category == ShipSlotCategory::Engine;
    std::erase_if(placements_, [&](const ShipComponentPlacement& placement) {
        return removingEngineBank
            ? component_spec(placement.component).kind == ShipComponentKind::Engine
            : placement.slot == slot;
    });
    rebuildSlotGrid();
    updatePreview();
}

void ShipDesignerDialog::rebuildSlotGrid()
{
    while (auto* item = slotGrid_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const auto hull = hull_spec(static_cast<ShipHullType>(hullCombo_->currentData().toInt()));
    for (const auto& slot : hull.slots) {
        const auto placement = std::find_if(placements_.begin(), placements_.end(), [&](const ShipComponentPlacement& candidate) {
            return candidate.slot == slot.id;
        });
        std::optional<ShipComponentType> component;
        if (placement != placements_.end()) component = placement->component;
        auto* button = new SlotButton(
            slot,
            component,
            selectedSlot_ == slot.id,
            [this, slot](ShipComponentType dropped, ShipSlotId source) {
                fitComponent(dropped, slot.id, source);
            },
            [this](ShipSlotId selected) { selectSlot(selected); },
            [this](ShipSlotId removed) { removeComponent(removed); },
            slotPanel_);
        slotGrid_->addWidget(button, slot.row, slot.column);
    }
    slotGrid_->setRowStretch(4, 1);
    slotGrid_->setColumnStretch(3, 1);
    const auto selectedPlacement = std::find_if(
        placements_.begin(), placements_.end(), [&](const ShipComponentPlacement& placement) {
            return placement.slot == selectedSlot_;
        });
    removeButton_->setEnabled(selectedPlacement != placements_.end());
    fitButton_->setEnabled(selectedSlot_ != 0 && selectedCatalogComponent().has_value());
}

ShipDesign ShipDesignerDialog::previewDesign() const
{
    ShipDesign design;
    design.owner = player_;
    design.name = nameEdit_->text().trimmed().toStdString();
    design.hull = static_cast<ShipHullType>(hullCombo_->currentData().toInt());
    const auto hull = hull_spec(design.hull);
    for (const auto& slot : hull.slots) {
        const auto placement = std::find_if(placements_.begin(), placements_.end(), [&](const ShipComponentPlacement& candidate) {
            return candidate.slot == slot.id;
        });
        if (placement == placements_.end()) continue;
        design.components.push_back(placement->component);
        design.placements.push_back(*placement);
    }
    return design;
}

ShipDesignDraft ShipDesignerDialog::draft() const
{
    const auto design = previewDesign();
    return {design.name, design.hull, design.components, design.placements};
}

void ShipDesignerDialog::updatePreview()
{
    const auto design = previewDesign();
    const auto hull = hull_spec(design.hull);
    const auto generalUsed = ship_design_general_slots_used(design);
    const auto miningUsed = ship_design_mining_slots_used(design);
    const auto validationError = ship_design_validation_error(design);
    const auto valid = validationError.empty();

    QStringList fuelCurve;
    const auto maxWarp = ship_design_max_warp(design);
    for (std::uint8_t warp = 1; warp <= maxWarp; ++warp) {
        fuelCurve.push_back(QString("W%1: %2")
                                .arg(warp)
                                .arg(signedFuelRate(ship_design_fuel_rate(design, warp))));
    }
    QString capabilities;
    if (const auto sensor = ship_design_sensor_range(design); sensor > 0.0) {
        capabilities += QString("Scanner %1 ly").arg(sensor, 0, 'f', 0);
    }
    if (ship_design_can_colonize(design)) {
        if (!capabilities.isEmpty()) capabilities += " • ";
        capabilities += "Colony capable";
    }
    if (ship_design_can_remote_mine(design)) {
        if (!capabilities.isEmpty()) capabilities += " • ";
        capabilities += "Remote mining capable";
    }
    if (capabilities.isEmpty()) capabilities = "No special mission capability";

    fitMessage_->setText(valid
            ? "Layout valid. Drag fitted components to rearrange them."
            : QString("Cannot save: %1").arg(QString::fromStdString(validationError)));
    const auto radiation = ship_design_radiation_hazard(design);
    const auto mineralCost = ship_design_mineral_cost(design);
    previewLabel_->setText(
        QString("<hr><b>%1</b><br>"
                "Hull: %2 — required engines <b>%3</b>, general slots <b>%4/%5</b>, Mining slots <b>%6/%7</b><br>"
                "Dry mass: <b>%8 kt</b> &nbsp; Build cost: <b>%9</b><br>"
                "Minerals: <b>I %10 / B %11 / G %12</b><br>"
                "Max Warp: <b>%13</b> &nbsp; Fuel capacity: <b>%14</b> &nbsp; Fuel generation: <b>%15/turn</b><br>"
                "Cargo capacity: <b>%16</b> (%17 colonists max)<br>"
                "%18%19<br><br>"
                "<b>Engine fuel curve</b> — rate per 100 kt per ly:<br>%20")
            .arg(QString::fromStdString(design.name.empty() ? std::string("Unnamed design") : design.name))
            .arg(QString::fromStdString(hull.name))
            .arg(hull.requiredEngines)
            .arg(static_cast<qulonglong>(generalUsed))
            .arg(hull.generalSlots)
            .arg(static_cast<qulonglong>(miningUsed))
            .arg(hull.miningSlots)
            .arg(ship_design_mass(design), 0, 'f', 1)
            .arg(ship_design_cost(design))
            .arg(mineralCost.ironium, 0, 'f', 0)
            .arg(mineralCost.boranium, 0, 'f', 0)
            .arg(mineralCost.germanium, 0, 'f', 0)
            .arg(maxWarp)
            .arg(ship_design_fuel_capacity(design), 0, 'f', 0)
            .arg(ship_design_fuel_generation(design), 0, 'f', 0)
            .arg(ship_design_cargo_capacity(design), 0, 'f', 0)
            .arg(ship_design_cargo_capacity(design) * kColonistsPerCargoUnit, 0, 'f', 0)
            .arg(capabilities)
            .arg(radiation > 0.0 ? " • <b>Radiation hazard</b>" : "")
            .arg(fuelCurve.isEmpty() ? "No engine fitted" : fuelCurve.join(" &nbsp; ")));
    saveButton_->setEnabled(valid);
}

} // namespace suns
