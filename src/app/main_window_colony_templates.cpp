#include "main_window.hpp"
#include "suns/production.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

namespace suns {
namespace {

QString stepName(ProductionKind kind)
{
    switch (kind) {
    case ProductionKind::Factory: return "Factories";
    case ProductionKind::Mine: return "Mines";
    case ProductionKind::LocalRadar: return "Local Radar (30 ly)";
    case ProductionKind::PlanetaryRadar: return "Full Radar (150 ly)";
    case ProductionKind::Terraforming: return "Terraforming";
    case ProductionKind::ColonyDefense: return "Colony defenses";
    default: return "Unavailable item";
    }
}

QString modeName(ProductionAutomation mode)
{
    switch (mode) {
    case ProductionAutomation::None: return "Once";
    case ProductionAutomation::Annual: return "Per year";
    case ProductionAutomation::MinTerraform: return "Min terraform";
    case ProductionAutomation::MaxTerraform: return "Max terraform";
    }
    return {};
}

} // namespace

std::vector<ProductionTemplate> MainWindow::colonyTemplatesForOrders() const
{
    const auto* player = find_player(state_, pendingOrders_.player);
    if (!player) return {};
    Player planned;
    planned.id = player->id;
    planned.productionTemplates = player->productionTemplates;
    for (const auto& order : pendingOrders_.orders)
        if (const auto* edit = std::get_if<SetProductionTemplateOrder>(&order))
            (void)set_production_template(planned, *edit);
    return planned.productionTemplates;
}

void MainWindow::openColonyTemplates()
{
    if (colonyTemplatesDialog_) {
        colonyTemplatesDialog_->showNormal();
        colonyTemplatesDialog_->raise();
        colonyTemplatesDialog_->activateWindow();
        return;
    }
    auto* dialog = new QDialog(this);
    colonyTemplatesDialog_ = dialog;
    dialog->setObjectName("colonyTemplatesDialog");
    dialog->setWindowTitle("Colony Development Templates");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(false);
    dialog->resize(930, 590);
    auto* layout = new QVBoxLayout(dialog);
    auto* intro = new QLabel("Programs are optional. Colonize starts with None; choosing a program sends a frozen copy with that order.", dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto* columns = new QHBoxLayout;
    layout->addLayout(columns, 1);
    auto* library = new QVBoxLayout;
    columns->addLayout(library, 1);
    auto* list = new QListWidget(dialog);
    list->setObjectName("colonyTemplateList");
    library->addWidget(list, 1);
    auto* libraryButtons = new QHBoxLayout;
    auto* fresh = new QPushButton("New", dialog);
    fresh->setObjectName("colonyTemplateNew");
    auto* duplicate = new QPushButton("Duplicate", dialog);
    duplicate->setObjectName("colonyTemplateDuplicate");
    auto* removeTemplate = new QPushButton("Delete", dialog);
    removeTemplate->setObjectName("colonyTemplateDelete");
    removeTemplate->setToolTip("Delete the saved template. Copies already attached to Colonize orders remain intact.");
    for (auto* button : {fresh, duplicate, removeTemplate}) libraryButtons->addWidget(button);
    library->addLayout(libraryButtons);
    auto* presets = new QComboBox(dialog);
    presets->setObjectName("colonyTemplatePreset");
    presets->addItem("Optional starter program…");
    for (const auto& value : colony_development_presets()) presets->addItem(QString::fromStdString(value.name));
    library->addWidget(presets);
    auto* usePreset = new QPushButton("Use starter as draft", dialog);
    usePreset->setObjectName("colonyTemplateUsePreset");
    usePreset->setEnabled(false);
    library->addWidget(usePreset);

    auto* editor = new QVBoxLayout;
    columns->addLayout(editor, 3);
    auto* name = new QLineEdit(dialog);
    name->setObjectName("colonyTemplateName");
    name->setMaxLength(80);
    name->setPlaceholderText("Name (edit to rename a saved template)");
    editor->addWidget(name);
    auto* steps = new QTreeWidget(dialog);
    steps->setObjectName("colonyTemplateSteps");
    steps->setHeaderLabels({"Step", "Quantity", "Mode", "Production", "I / B / G kt", "Example"});
    steps->setRootIsDecorated(false);
    steps->setSelectionMode(QAbstractItemView::SingleSelection);
    steps->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i < 6; ++i) steps->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    editor->addWidget(steps, 1);
    auto* stepButtons = new QHBoxLayout;
    auto* up = new QPushButton("Move up", dialog);
    up->setObjectName("colonyTemplateStepUp");
    auto* down = new QPushButton("Move down", dialog);
    down->setObjectName("colonyTemplateStepDown");
    auto* removeStep = new QPushButton("Remove step", dialog);
    removeStep->setObjectName("colonyTemplateStepRemove");
    for (auto* button : {up, down, removeStep}) stepButtons->addWidget(button);
    editor->addLayout(stepButtons);
    auto* kind = new QComboBox(dialog);
    kind->setObjectName("colonyTemplateStepKind");
    for (auto value : {ProductionKind::Factory, ProductionKind::Mine, ProductionKind::LocalRadar,
            ProductionKind::PlanetaryRadar, ProductionKind::Terraforming, ProductionKind::ColonyDefense})
        kind->addItem(stepName(value), int(value));
    auto* quantity = new QSpinBox(dialog);
    quantity->setObjectName("colonyTemplateStepQuantity");
    quantity->setRange(1, kMaximumProductionBatch);
    auto* mode = new QComboBox(dialog);
    mode->setObjectName("colonyTemplateStepMode");
    auto* stepForm = new QHBoxLayout;
    stepForm->addWidget(kind, 2);
    stepForm->addWidget(quantity);
    stepForm->addWidget(mode, 1);
    editor->addLayout(stepForm);
    auto* editButtons = new QHBoxLayout;
    auto* add = new QPushButton("Add step", dialog);
    add->setObjectName("colonyTemplateStepAdd");
    auto* update = new QPushButton("Update selected", dialog);
    update->setObjectName("colonyTemplateStepUpdate");
    auto* save = new QPushButton("Save template", dialog);
    save->setObjectName("colonyTemplateSave");
    for (auto* button : {add, update, save}) editButtons->addWidget(button);
    editor->addLayout(editButtons);
    auto* status = new QLabel(dialog);
    status->setObjectName("colonyTemplateStatus");
    status->setWordWrap(true);
    editor->addWidget(status);
    auto* example = new QLabel("Example forecast: 2,500 settlers, one factory, no mines/radar, I/B/G 30 kt each, temperate world, current research allocation; 64 years. Dates are illustrative. Annual steps repeat in priority order and can postpone later work. Unavailable defenses/terraforming are skipped by ordinary production rules.", dialog);
    example->setWordWrap(true);
    layout->addWidget(example);
    auto* close = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(close, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(close);

    struct Draft {
        ProductionTemplate value;
        std::string source;
        std::vector<ProductionTemplate> saved;
    };
    const auto draft = std::make_shared<Draft>();
    draft->saved = colonyTemplatesForOrders();
    const auto rebuildList = [=] {
        const QSignalBlocker blocked(list);
        list->clear();
        int selected = -1;
        for (const auto& value : draft->saved) {
            list->addItem(QString::fromStdString(value.name));
            if (value.name == draft->source) selected = list->count() - 1;
        }
        list->setCurrentRow(selected);
    };
    const auto render = [=, this](int selected = -1) {
        draft->value.name = name->text().trimmed().toStdString();
        const auto state = planned_production_state(make_player_view(state_, pendingOrders_.player).state, pendingOrders_);
        Planet sample;
        sample.owner = pendingOrders_.player;
        sample.population = 2500;
        sample.industry = 1;
        sample.minerals = {30, 30, 30};
        sample.environment = {50, 50, 15};
        const auto forecast = forecast_production_queue(state, sample, draft->value.items, 64);
        const QSignalBlocker blocked(steps);
        steps->clear();
        int radar = -1;
        for (std::size_t i = 0; i < draft->value.items.size(); ++i) {
            const auto& item = draft->value.items[i];
            if (radar < 0 && (item.kind == ProductionKind::LocalRadar || item.kind == ProductionKind::PlanetaryRadar)) radar = int(i);
            const auto minerals = production_item_mineral_cost(state, item);
            const auto q = double(item.quantity);
            const auto date = forecast[i].completionTurn
                ? QString("T+%1%2").arg(*forecast[i].completionTurn - state.turn)
                    .arg(item.automation == ProductionAutomation::None ? "" : " first")
                : QString("—");
            new QTreeWidgetItem(steps, {stepName(item.kind), QString::number(item.quantity), modeName(item.automation),
                QString::number(std::uint64_t(production_cost(item.kind)) * item.quantity),
                QString("%1 / %2 / %3").arg(minerals.ironium*q, 0, 'f', 1).arg(minerals.boranium*q, 0, 'f', 1).arg(minerals.germanium*q, 0, 'f', 1), date});
        }
        if (selected >= 0 && selected < steps->topLevelItemCount()) {
            steps->setCurrentItem(steps->topLevelItem(selected));
            const auto& item = draft->value.items[std::size_t(selected)];
            kind->setCurrentIndex(kind->findData(int(item.kind)));
            quantity->setValue(int(item.quantity));
            mode->setCurrentIndex(mode->findData(int(item.automation)));
        }
        const bool conflict = std::any_of(draft->saved.begin(), draft->saved.end(), [&](const auto& value) {
            return value.name == draft->value.name && value.name != draft->source;
        });
        const bool full = draft->source.empty() && draft->saved.size() >= 16;
        save->setEnabled(production_template_valid(draft->value) && !conflict && !full);
        removeTemplate->setEnabled(!draft->source.empty());
        duplicate->setEnabled(!draft->value.items.empty());
        add->setEnabled(draft->value.items.size() < 64);
        up->setEnabled(selected > 0);
        down->setEnabled(selected >= 0 && selected + 1 < steps->topLevelItemCount());
        removeStep->setEnabled(selected >= 0);
        update->setEnabled(selected >= 0);
        status->setText(draft->value.name.empty() ? "Name the template and add production steps before saving."
            : draft->value.name.size() > 80 ? "Choose a shorter template name."
            : draft->value.items.empty() ? "Add at least one production step."
            : conflict ? "That name is already used. Choose another name."
            : full ? "The library contains 16 templates. Delete one before saving a new template."
            : radar < 0 ? "No radar in this program. The colony may remain offline until radar or a ship relay connects it."
            : radar > 0 ? "Radar follows earlier production steps. The colony may remain offline while those steps are built."
            : "Radar is first, but must be built with normal production and minerals before it provides a link.");
    };
    const auto loadDraft = [=](ProductionTemplate value, std::string source) {
        draft->value = std::move(value);
        draft->source = std::move(source);
        const QSignalBlocker blocked(name);
        name->setText(QString::fromStdString(draft->value.name));
        rebuildList();
        render();
    };
    const auto uniqueName = [draft](const std::string& base) {
        std::string candidate = base;
        for (int suffix = 2; std::any_of(draft->saved.begin(), draft->saved.end(),
                [&](const auto& value) { return value.name == candidate; }); ++suffix)
            candidate = base + " " + std::to_string(suffix);
        return candidate;
    };
    const auto syncModes = [=] {
        const auto selected = static_cast<ProductionKind>(kind->currentData().toInt());
        const auto previous = mode->currentData().toInt();
        const QSignalBlocker blocked(mode);
        mode->clear();
        mode->addItem("One-time batch", int(ProductionAutomation::None));
        if (selected == ProductionKind::Terraforming) {
            mode->addItem("Min terraform", int(ProductionAutomation::MinTerraform));
            mode->addItem("Max terraform", int(ProductionAutomation::MaxTerraform));
        } else mode->addItem("Annual limit", int(ProductionAutomation::Annual));
        mode->setCurrentIndex(std::max(0, mode->findData(previous)));
        const bool radar = selected == ProductionKind::LocalRadar || selected == ProductionKind::PlanetaryRadar;
        quantity->setMaximum(radar ? 1 : kMaximumProductionBatch);
    };
    connect(kind, &QComboBox::currentIndexChanged, dialog, syncModes);
    syncModes();
    const auto readStep = [=] {
        const auto selected = static_cast<ProductionKind>(kind->currentData().toInt());
        return ProductionItem{selected, production_cost(selected), 0,
            static_cast<ProductionAutomation>(mode->currentData().toInt()), std::uint32_t(quantity->value())};
    };
    connect(steps, &QTreeWidget::itemSelectionChanged, dialog, [=] {
        const int row = steps->indexOfTopLevelItem(steps->currentItem());
        if (row < 0 || std::size_t(row) >= draft->value.items.size()) return;
        const auto item = draft->value.items[std::size_t(row)];
        kind->setCurrentIndex(kind->findData(int(item.kind)));
        quantity->setValue(int(item.quantity));
        mode->setCurrentIndex(mode->findData(int(item.automation)));
        up->setEnabled(row > 0);
        down->setEnabled(row + 1 < steps->topLevelItemCount());
        removeStep->setEnabled(true); update->setEnabled(true);
    });
    connect(name, &QLineEdit::textChanged, dialog, [=] { render(steps->indexOfTopLevelItem(steps->currentItem())); });
    connect(list, &QListWidget::currentRowChanged, dialog, [=](int row) {
        if (row < 0 || std::size_t(row) >= draft->saved.size()) return;
        const auto value = draft->saved[std::size_t(row)];
        loadDraft(value, value.name);
    });
    connect(fresh, &QPushButton::clicked, dialog, [=] { loadDraft({}, {}); name->setFocus(); });
    connect(duplicate, &QPushButton::clicked, dialog, [=] {
        auto copy = draft->value;
        copy.name = uniqueName((QString::fromStdString(copy.name).left(30) + " copy").toStdString());
        copy.defaultForNewColonies = false;
        loadDraft(copy, {});
    });
    connect(presets, &QComboBox::currentIndexChanged, dialog, [=](int row) { usePreset->setEnabled(row > 0); });
    connect(usePreset, &QPushButton::clicked, dialog, [=] {
        if (presets->currentIndex() <= 0) return;
        auto value = colony_development_presets()[std::size_t(presets->currentIndex() - 1)];
        value.name = uniqueName(value.name);
        loadDraft(value, {});
    });
    connect(add, &QPushButton::clicked, dialog, [=] {
        if (draft->value.items.size() >= 64) return;
        draft->value.items.push_back(readStep()); render(int(draft->value.items.size()) - 1);
    });
    connect(update, &QPushButton::clicked, dialog, [=] {
        const int row = steps->indexOfTopLevelItem(steps->currentItem());
        if (row >= 0 && std::size_t(row) < draft->value.items.size()) { draft->value.items[std::size_t(row)] = readStep(); render(row); }
    });
    const auto move = [=](int offset) {
        const int row = steps->indexOfTopLevelItem(steps->currentItem());
        if (row < 0 || row + offset < 0 || std::size_t(row + offset) >= draft->value.items.size()) return;
        std::swap(draft->value.items[std::size_t(row)], draft->value.items[std::size_t(row + offset)]); render(row + offset);
    };
    connect(up, &QPushButton::clicked, dialog, [=] { move(-1); });
    connect(down, &QPushButton::clicked, dialog, [=] { move(1); });
    connect(removeStep, &QPushButton::clicked, dialog, [=] {
        const int row = steps->indexOfTopLevelItem(steps->currentItem());
        if (row < 0 || std::size_t(row) >= draft->value.items.size()) return;
        draft->value.items.erase(draft->value.items.begin() + row); render(std::min(row, int(draft->value.items.size()) - 1));
    });
    connect(save, &QPushButton::clicked, dialog, [=, this] {
        if (!save->isEnabled()) return;
        draft->value.defaultForNewColonies = false;
        if (!draft->source.empty() && draft->source != draft->value.name)
            appendPendingOrder(SetProductionTemplateOrder{{draft->source}, true}, "Rename colony template " + QString::fromStdString(draft->source));
        appendPendingOrder(SetProductionTemplateOrder{draft->value}, "Save colony template " + QString::fromStdString(draft->value.name));
        draft->source = draft->value.name;
        draft->saved = colonyTemplatesForOrders();
        rebuildList(); render(); refreshProductionQueue();
        emit routeProgramContextChanged(false);
    });
    connect(removeTemplate, &QPushButton::clicked, dialog, [=, this] {
        if (draft->source.empty()) return;
        appendPendingOrder(SetProductionTemplateOrder{{draft->source}, true}, "Delete colony template " + QString::fromStdString(draft->source));
        draft->saved = colonyTemplatesForOrders();
        loadDraft({}, {}); refreshProductionQueue();
        emit routeProgramContextChanged(false);
    });
    connect(this, &MainWindow::routeProgramContextChanged, dialog, [dialog](bool reset) { if (reset) dialog->close(); });
    rebuildList(); render();
    dialog->show();
}

} // namespace suns
