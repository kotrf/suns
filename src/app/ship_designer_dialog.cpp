#include "ship_designer_dialog.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QComboBox>
#include <QColor>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QStandardItemModel>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
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

// Presentation art only: logical slot IDs and simulation rules stay in core.
QPixmap hullPortrait(ShipHullType hull)
{
    QPixmap image(300, 140);
    image.fill(QColor("#101d29"));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#263b4d"), 1));
    for (int x = 20; x < 300; x += 20) painter.drawLine(x, 0, x, 140);
    for (int y = 20; y < 140; y += 20) painter.drawLine(0, y, 300, y);
    painter.translate(150, 70);

    QPainterPath outline;
    switch (hull) {
    case ShipHullType::Scout:
        outline.moveTo(0, -57);
        outline.lineTo(17, -14); outline.lineTo(70, 28); outline.lineTo(38, 35);
        outline.lineTo(21, 23); outline.lineTo(17, 55); outline.lineTo(-17, 55);
        outline.lineTo(-21, 23); outline.lineTo(-38, 35); outline.lineTo(-70, 28);
        outline.lineTo(-17, -14); outline.closeSubpath();
        break;
    case ShipHullType::LightTransport:
        outline.moveTo(0, -55); outline.lineTo(25, -33); outline.lineTo(29, -9);
        outline.lineTo(69, -9); outline.lineTo(78, 36); outline.lineTo(34, 46);
        outline.lineTo(25, 55); outline.lineTo(-25, 55); outline.lineTo(-34, 46);
        outline.lineTo(-78, 36); outline.lineTo(-69, -9); outline.lineTo(-29, -9);
        outline.lineTo(-25, -33); outline.closeSubpath();
        break;
    case ShipHullType::MediumTransport:
        outline.moveTo(0, -57); outline.lineTo(35, -45); outline.lineTo(48, -17);
        outline.lineTo(95, -12); outline.lineTo(104, 37); outline.lineTo(42, 47);
        outline.lineTo(32, 58); outline.lineTo(-32, 58); outline.lineTo(-42, 47);
        outline.lineTo(-104, 37); outline.lineTo(-95, -12); outline.lineTo(-48, -17);
        outline.lineTo(-35, -45); outline.closeSubpath();
        break;
    case ShipHullType::Utility:
        outline.moveTo(0, -48); outline.lineTo(25, -42); outline.lineTo(44, -20);
        outline.lineTo(90, -29); outline.lineTo(102, 27); outline.lineTo(51, 38);
        outline.lineTo(29, 54); outline.lineTo(-29, 54); outline.lineTo(-51, 38);
        outline.lineTo(-102, 27); outline.lineTo(-90, -29); outline.lineTo(-44, -20);
        outline.lineTo(-25, -42); outline.closeSubpath();
        break;
    case ShipHullType::RemoteMiner:
        outline.moveTo(0, -51); outline.lineTo(28, -35); outline.lineTo(33, -6);
        outline.lineTo(88, -30); outline.lineTo(108, -10); outline.lineTo(82, 4);
        outline.lineTo(94, 46); outline.lineTo(46, 51); outline.lineTo(29, 31);
        outline.lineTo(24, 56); outline.lineTo(-24, 56); outline.lineTo(-29, 31);
        outline.lineTo(-46, 51); outline.lineTo(-94, 46); outline.lineTo(-82, 4);
        outline.lineTo(-108, -10); outline.lineTo(-88, -30); outline.lineTo(-33, -6);
        outline.lineTo(-28, -35); outline.closeSubpath();
        break;
    }
    painter.setPen(QPen(QColor("#9fc8dc"), 2));
    painter.setBrush(QColor("#294459"));
    painter.drawPath(outline);
    painter.setPen(QPen(QColor("#59869e"), 1));
    painter.setBrush(QColor("#172b3a"));
    painter.drawRoundedRect(QRectF(-17, -30, 34, 62), 10, 10);
    painter.setPen(QPen(QColor("#85c9e6"), 2));
    painter.drawLine(QPointF(0, -44), QPointF(0, 20));
    if (hull == ShipHullType::LightTransport || hull == ShipHullType::MediumTransport) {
        painter.setPen(QPen(QColor("#9db8c9"), 1));
        painter.drawRoundedRect(QRectF(-68, 0, 30, 27), 3, 3);
        painter.drawRoundedRect(QRectF(38, 0, 30, 27), 3, 3);
    } else if (hull == ShipHullType::RemoteMiner) {
        painter.setPen(QPen(QColor("#e2bb70"), 3));
        painter.drawLine(QPointF(-91, -12), QPointF(-109, -26));
        painter.drawLine(QPointF(91, -12), QPointF(109, -26));
    } else if (hull == ShipHullType::Utility) {
        painter.setPen(QPen(QColor("#7ebac8"), 2));
        painter.drawEllipse(QPointF(0, 8), 26, 26);
    }
    painter.setBrush(QColor("#e2bb70"));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(-13, 53), 4, 3);
    painter.drawEllipse(QPointF(13, 53), 4, 3);
    return image;
}

QIcon componentIcon(ShipComponentType component, bool available = true)
{
    QPixmap image(28, 28);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!available) painter.setOpacity(0.4);
    const auto kind = component_spec(component).kind;
    painter.setPen(QPen(kind == ShipComponentKind::Engine ? QColor("#e2bb70")
        : kind == ShipComponentKind::Scanner ? QColor("#78b8f0")
        : kind == ShipComponentKind::Mining ? QColor("#d5a478")
        : QColor("#8dcc9e"), 2));
    painter.setBrush(Qt::NoBrush);
    switch (kind) {
    case ShipComponentKind::Engine:
        painter.drawPolygon(QPolygonF{QPointF(8, 4), QPointF(20, 4), QPointF(18, 18), QPointF(10, 18)});
        painter.drawLine(11, 21, 9, 26); painter.drawLine(17, 21, 19, 26);
        break;
    case ShipComponentKind::Scanner:
        painter.drawEllipse(QPointF(14, 14), 3, 3);
        painter.drawArc(QRectF(5, 5, 18, 18), 30 * 16, 120 * 16);
        painter.drawArc(QRectF(1, 1, 26, 26), 30 * 16, 120 * 16);
        break;
    case ShipComponentKind::Mining:
        painter.drawLine(8, 5, 18, 22);
        painter.drawLine(14, 5, 23, 18);
        painter.drawLine(7, 22, 21, 22);
        break;
    case ShipComponentKind::Fuel:
        painter.drawRoundedRect(QRectF(8, 4, 12, 20), 4, 4);
        painter.drawLine(8, 12, 20, 12);
        break;
    case ShipComponentKind::Cargo:
        painter.drawRect(QRectF(5, 8, 18, 15));
        painter.drawLine(5, 13, 23, 13);
        painter.drawLine(14, 8, 14, 23);
        break;
    case ShipComponentKind::Special:
        painter.drawEllipse(QRectF(5, 5, 18, 18));
        painter.drawLine(14, 1, 14, 8);
        painter.drawLine(14, 20, 14, 27);
        break;
    }
    return QIcon(image);
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
    const auto minerals = component_mineral_cost(component);
    facts << QString("Minerals (kt): I %1 / B %2 / G %3")
        .arg(minerals.ironium).arg(minerals.boranium).arg(minerals.germanium);
    if (spec.maxWarp > 0) {
        facts << QString("Safe maximum Warp %1 • Thrust %2").arg(spec.maxWarp).arg(spec.engineThrust);
        facts << "One engine model fills the entire propulsion bank. Costs and mass above are per engine.";
        facts << "Fuel per 100 kt per ly (gain means fuel collected):";
        for (std::uint8_t warp = 1; warp <= kMaxWarp; ++warp) {
            auto line = QString("W%1: %2").arg(warp).arg(signedFuelRate(spec.fuelPer100MassLy[warp]));
            if (spec.overdriveDamagePercent[warp] > 0.0)
                line += QString(" • hull damage %1%/turn").arg(spec.overdriveDamagePercent[warp]);
            facts << line;
        }
    }
    if (spec.sensorRange > 0.0) {
        facts << QString("Scanner range %1 ly").arg(spec.sensorRange, 0, 'f', 0);
        facts << (spec.penetratesPlanets
            ? "Penetrating: surveys planets; does not extend the communications network."
            : "Ordinary: detects ships and extends the communications network; does not survey planets.");
    }
    if (spec.fuelCapacity > 0.0) facts << QString("Fuel capacity +%1").arg(spec.fuelCapacity, 0, 'f', 0);
    if (spec.fuelGenerationPerTurn > 0.0) {
        facts << QString("Fuel generation +%1/turn").arg(spec.fuelGenerationPerTurn, 0, 'f', 0);
    }
    if (spec.cargoCapacity > 0.0) facts << QString("Cargo +%1 kt").arg(spec.cargoCapacity, 0, 'f', 0);
    if (spec.remoteMiningUnits > 0.0)
        facts << "Remote mining: 1.25 extraction units per turn × concentration of each mineral, on unowned planets.";
    if (spec.radiationHazard > 0.0) facts << "Radiation hazard: 10% colonist losses per travel turn unless the race is immune or has radiation tolerance ≥ 85%.";
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
        if (!item || !item->data(Qt::UserRole + 1).toBool()) return;
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
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        setIconSize(QSize(28, 28));
        if (component_) setIcon(componentIcon(*component_));
        baseStyle_ = chosen
                ? "QToolButton { border: 2px solid #52b6d9; background: #193346; padding: 5px; }"
                : "QToolButton { border: 1px solid #52677a; background: #142433; padding: 5px; }"
                  "QToolButton:hover, QToolButton:focus { border: 2px solid #78c8e5; }";
        setStyleSheet(baseStyle_);
        refreshText();
        connect(this, &QToolButton::clicked, this, [this] { selected_(slot_.id); });
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        const auto payload = decodeComponentDrag(event->mimeData());
        if (!payload) return;
        const bool compatible = ship_component_slot_category(payload->component) == slot_.category;
        setStyleSheet(compatible
            ? "QToolButton { border: 3px solid #66d69b; background: #193a31; padding: 5px; }"
            : "QToolButton { border: 3px solid #e07575; background: #3c222a; padding: 5px; }");
        // Accept the drop even on a red cell so the editor can explain the
        // mismatch in its persistent feedback label instead of silently ignoring it.
        event->acceptProposedAction();
    }

    void dragLeaveEvent(QDragLeaveEvent* event) override
    {
        setStyleSheet(baseStyle_);
        QToolButton::dragLeaveEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent* event) override
    {
        if (decodeComponentDrag(event->mimeData())) event->acceptProposedAction();
    }

    void dropEvent(QDropEvent* event) override
    {
        const auto payload = decodeComponentDrag(event->mimeData());
        setStyleSheet(baseStyle_);
        if (!payload) return;
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
    QString baseStyle_;
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

    templateCombo_ = new QComboBox(this);
    templateCombo_->setObjectName("shipDesignTemplate");
    templateCombo_->setToolTip("Open an existing design as a new draft. Built ships and the original design stay unchanged.");
    templateCombo_->addItem("New design", static_cast<quint32>(0));
    for (const auto& design : state.shipDesigns) {
        if (design.owner != player_) continue;
        templates_.push_back(design);
        templateCombo_->addItem(QString::fromStdString(design.name), static_cast<quint32>(design.id));
    }
    form->addRow("Start from", templateCombo_);

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
    auto* detailsScroll = new QScrollArea(catalogGroup);
    detailsScroll->setWidgetResizable(true);
    detailsScroll->setMinimumHeight(160);
    componentDetails_ = new QLabel(detailsScroll);
    componentDetails_->setObjectName("shipComponentDetails");
    componentDetails_->setTextFormat(Qt::PlainText);
    componentDetails_->setWordWrap(true);
    componentDetails_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    componentDetails_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    componentDetails_->setMargin(6);
    detailsScroll->setWidget(componentDetails_);
    catalogLayout->addWidget(detailsScroll, 1);
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
        item->setIcon(componentIcon(component, available));
        item->setData(Qt::UserRole, static_cast<int>(component));
        item->setData(Qt::UserRole + 1, available);
        item->setToolTip(componentTooltip(component)
            + (available ? QString{} : QString("\nLocked — requires %1").arg(unlockRequirement(component))));
        if (!available) {
            item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
            item->setForeground(QBrush(QColor("#7b8999")));
        }
    }
    componentCatalog_->setCurrentRow(0);

    auto* fittingGroup = new QGroupBox("Hull fitting cells", this);
    auto* fittingLayout = new QVBoxLayout(fittingGroup);
    hullPortrait_ = new QLabel(fittingGroup);
    hullPortrait_->setObjectName("shipHullPortrait");
    hullPortrait_->setAlignment(Qt::AlignCenter);
    hullPortrait_->setToolTip("Hull silhouette is visual reference; cell positions and fitting rules come from the hull specification.");
    fittingLayout->addWidget(hullPortrait_);
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
    connect(templateCombo_, &QComboBox::currentIndexChanged, this, [this] {
        const auto id = static_cast<ShipDesignId>(templateCombo_->currentData().toUInt());
        const auto it = std::find_if(templates_.begin(), templates_.end(), [id](const ShipDesign& design) {
            return design.id == id;
        });
        if (it == templates_.end()) {
            const QSignalBlocker blockHull(hullCombo_);
            hullCombo_->setCurrentIndex(hullCombo_->findData(static_cast<int>(ShipHullType::Scout)));
            placements_ = autoplace_ship_components(ShipHullType::Scout,
                {ShipComponentType::FusionDrive, ShipComponentType::LongRangeScanner});
            nameEdit_->setText("New Design");
        } else {
            const QSignalBlocker blockHull(hullCombo_);
            hullCombo_->setCurrentIndex(hullCombo_->findData(static_cast<int>(it->hull)));
            placements_ = it->placements.empty()
                ? autoplace_ship_components(it->hull, it->components) : it->placements;
            nameEdit_->setText(QString::fromStdString(it->name).left(43) + " Copy");
        }
        selectedSlot_ = 0;
        rebuildSlotGrid();
        updatePreview();
    });
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
            const auto fittingSlots = hull_spec(hullType).fittingSlots;
            for (const auto component : components) {
                const auto target = std::find_if(fittingSlots.begin(), fittingSlots.end(), [&](const ShipSlotSpec& slot) {
                    if (slot.category != ship_component_slot_category(component)) return false;
                    return std::none_of(placements_.begin(), placements_.end(), [&](const ShipComponentPlacement& placed) {
                        return placed.slot == slot.id;
                    });
                });
                if (target != fittingSlots.end()) placements_.push_back({target->id, component});
            }
        }
        selectedSlot_ = 0;
        rebuildSlotGrid();
        updatePreview();
        if (!allFit) fitMessage_->setText("Some equipment was removed because the new hull has fewer compatible cells.");
    });
    connect(componentCatalog_, &QListWidget::currentItemChanged, this, [this] {
        fitButton_->setEnabled(selectedSlot_ != 0 && selectedCatalogComponent().has_value());
        updateComponentDetails();
    });
    connect(componentCatalog_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        const auto component = selectedCatalogComponent();
        if (!component) return;
        const auto hull = hull_spec(static_cast<ShipHullType>(hullCombo_->currentData().toInt()));
        if (component_spec(*component).kind == ShipComponentKind::Engine) {
            const auto target = std::find_if(hull.fittingSlots.begin(), hull.fittingSlots.end(), [](const ShipSlotSpec& slot) {
                return slot.category == ShipSlotCategory::Engine;
            });
            if (target != hull.fittingSlots.end()) fitComponent(*component, target->id);
            return;
        }
        auto target = std::find_if(hull.fittingSlots.begin(), hull.fittingSlots.end(), [&](const ShipSlotSpec& slot) {
            return slot.id == selectedSlot_
                && slot.category == ship_component_slot_category(*component);
        });
        if (target == hull.fittingSlots.end()) {
            target = std::find_if(hull.fittingSlots.begin(), hull.fittingSlots.end(), [&](const ShipSlotSpec& slot) {
                if (slot.category != ship_component_slot_category(*component)) return false;
                return std::none_of(placements_.begin(), placements_.end(), [&](const ShipComponentPlacement& placed) {
                    return placed.slot == slot.id;
                });
            });
        }
        if (target != hull.fittingSlots.end()) fitComponent(*component, target->id);
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
    updateComponentDetails();
    if (parent && !QCoreApplication::arguments().contains("--smoke-test")) {
        const auto geometry = QSettings("SunsProject", "Suns").value("designer/geometry").toByteArray();
        if (!geometry.isEmpty()) restoreGeometry(geometry);
    }
}

ShipDesignerDialog::~ShipDesignerDialog()
{
    if (parentWidget() && !QCoreApplication::arguments().contains("--smoke-test")) {
        QSettings("SunsProject", "Suns").setValue("designer/geometry", saveGeometry());
    }
}

std::optional<ShipComponentType> ShipDesignerDialog::selectedCatalogComponent() const
{
    const auto* item = componentCatalog_->currentItem();
    if (!item || !item->data(Qt::UserRole + 1).toBool()) return std::nullopt;
    return static_cast<ShipComponentType>(item->data(Qt::UserRole).toInt());
}

void ShipDesignerDialog::updateComponentDetails()
{
    const auto* item = componentCatalog_->currentItem();
    if (!item) { componentDetails_->clear(); return; }
    const auto component = static_cast<ShipComponentType>(item->data(Qt::UserRole).toInt());
    componentDetails_->setText(QString::fromStdString(component_spec(component).name)
        + "\n" + item->toolTip());
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
    for (int row = 0; row < componentCatalog_->count(); ++row) {
        const auto* item = componentCatalog_->item(row);
        if (item->data(Qt::UserRole).toInt() == static_cast<int>(component)
            && !item->data(Qt::UserRole + 1).toBool()) return;
    }
    const auto hull = hull_spec(static_cast<ShipHullType>(hullCombo_->currentData().toInt()));
    const auto targetSlot = std::find_if(hull.fittingSlots.begin(), hull.fittingSlots.end(), [&](const ShipSlotSpec& slot) {
        return slot.id == target;
    });
    if (targetSlot == hull.fittingSlots.end()
        || targetSlot->category != ship_component_slot_category(component)) {
        fitMessage_->setText("That component is incompatible with the selected cell.");
        return;
    }

    if (component_spec(component).kind == ShipComponentKind::Engine) {
        std::erase_if(placements_, [](const ShipComponentPlacement& placement) {
            return component_spec(placement.component).kind == ShipComponentKind::Engine;
        });
        for (const auto& slot : hull.fittingSlots) {
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
        const auto sourceSlot = std::find_if(hull.fittingSlots.begin(), hull.fittingSlots.end(), [&](const ShipSlotSpec& slot) {
            return slot.id == source;
        });
        if (sourceSlot == hull.fittingSlots.end()
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
    const auto selected = std::find_if(hull.fittingSlots.begin(), hull.fittingSlots.end(), [&](const ShipSlotSpec& candidate) {
        return candidate.id == slot;
    });
    const auto removingEngineBank = selected != hull.fittingSlots.end()
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
    for (const auto& slot : hull.fittingSlots) {
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
    for (const auto& slot : hull.fittingSlots) {
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
    hullPortrait_->setPixmap(hullPortrait(design.hull));
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
