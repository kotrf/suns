#include "mineral_stock_panel.hpp"
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace suns {
namespace {
constexpr std::array<double, 8> scales{10, 50, 100, 500, 1000, 5000, 10000, 50000};
std::array<double, 3> components(const MineralCargo& c) { return {c.ironium, c.boranium, c.germanium}; }
QString exact(std::optional<double> value) { return value ? QString::number(*value, 'f', 1) : QString("unknown"); }
}
MineralDisplayData mineral_display_data(const GameState& received, const GameState& planned, PlayerId player, PlanetId id)
{
    MineralDisplayData result;
    const auto* p = find_planet(received, id);
    if (!p) return result;
    const auto owner = known_planet_owner(received, player, id);
    if (!owner) return result;
    const bool own = *owner == player;
    if (own || planet_geology_known(received, player, id)) result.concentration = planet_mineral_concentration(received, *p);
    const auto evidence = known_surface_minerals(received, player, id);
    if (evidence && (own || !*owner)) {
        result.stock = evidence->minerals;
        if (const auto* afterCargo = find_planet(planned, id)) result.stock = afterCargo->minerals;
        result.observedTurn = evidence->observedTurn;
    }
    if (own) {
        if (const auto* afterCargo = find_planet(planned, id)) result.stock = afterCargo->minerals;
        // First annual extraction precedes this turn's Cargo changes.
        result.mining = projected_mineral_mining(received, *p);
    } else if (!*owner && result.concentration) {
        MineralCargo mining{};
        const auto* star = find_star(planned, p->star);
        if (star) for (const auto& f : planned.fleets) {
            if (f.owner != player || f.task != FleetTask::RemoteMining || !same_position(f.position, star->position)) continue;
            for (const auto& stack : fleet_ship_stacks(f)) {
                const auto* design = find_ship_design(planned, stack.design);
                if (!design || !ship_design_available_to_player(planned, player, *design) || !ship_design_can_remote_mine(*design)) continue;
                const auto rate = projected_remote_mining(received, *p, *design);
                mining.ironium += rate.ironium * stack.count;
                mining.boranium += rate.boranium * stack.count;
                mining.germanium += rate.germanium * stack.count;
            }
        }
        result.mining = mining;
    }
    return result;
}

MineralReserveBar::MineralReserveBar(QString name, QColor color, QWidget* parent)
    : QProgressBar(parent), name_(std::move(name)), color_(color)
{
    setRange(0, 100); setValue(0);
    setMinimumHeight(25); setMaximumHeight(28);
    setFocusPolicy(Qt::StrongFocus); setCursor(Qt::PointingHandCursor);
}
void MineralReserveBar::setMineral(std::optional<double> stock, std::optional<double> mining,
    std::optional<double> concentration, double scale, const QString& age)
{
    stock_ = stock; mining_ = mining; concentration_ = concentration; scale_ = scale;
    setValue(concentration ? qRound(*concentration) : 0);
    setEnabled(stock.has_value() || concentration.has_value());
    setFormat(name_ + (concentration ? QString(" %1%").arg(*concentration, 0, 'f', 0) : " — unknown"));
    setToolTip(QString("%1\nSurface stock: %2 kt\nCurrent-year mining before cargo: %3 kt (estimate from received conditions)\nConcentration: %4%\n%5\nLight: surface stock. Dark continuation: mining per turn. Diamond: concentration (independent 0–100% scale).\nClick or press Space to change the shared stock scale.")
        .arg(name_, exact(stock), exact(mining), exact(concentration), age));
    setAccessibleName(name_); setAccessibleDescription(toolTip()); update();
}
void MineralReserveBar::paintEvent(QPaintEvent*)
{
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const QRectF plot(76, 4, std::max(1, width() - 82), height() - 8);
    p.setPen(QColor("#31485a")); p.setBrush(QColor("#0a131d")); p.drawRoundedRect(plot, 2, 2);
    const auto fraction = [&](double v) { return std::clamp(v / scale_, 0.0, 1.0); };
    if (stock_) {
        p.setPen(Qt::NoPen); p.setBrush(color_);
        p.drawRect(QRectF(plot.left(), plot.top(), plot.width() * fraction(*stock_), plot.height()));
        if (mining_) {
            const double start = fraction(*stock_);
            const double end = fraction(*stock_ + *mining_);
            p.setBrush(color_.darker(230));
            p.drawRect(QRectF(plot.left() + plot.width() * start, plot.top(), plot.width() * (end - start), plot.height()));
        }
        if (*stock_ > scale_) {
            p.setPen(QColor("#ffffff")); p.drawText(plot.adjusted(0, -2, -2, 2), Qt::AlignRight | Qt::AlignVCenter, "> ");
        }
    } else {
        p.setPen(QColor("#8395a8")); p.drawText(plot, Qt::AlignCenter, "stock unknown");
    }
    if (concentration_) {
        const double x = plot.left() + plot.width() * std::clamp(*concentration_ / 100.0, 0.0, 1.0);
        const double y = plot.center().y();
        p.setPen(QPen(QColor("#091019"), 2)); p.setBrush(QColor("#ffffff"));
        p.drawPolygon(QPolygonF{{x, y - 5}, {x + 4, y}, {x, y + 5}, {x - 4, y}});
    }
    p.setPen(isEnabled() ? color_ : QColor("#8395a8"));
    p.drawText(QRect(0, 0, 73, height()), Qt::AlignLeft | Qt::AlignVCenter, name_);
    if (hasFocus()) { p.setPen(QPen(QColor("#94cce7"), 1, Qt::DotLine)); p.setBrush(Qt::NoBrush); p.drawRect(rect().adjusted(0, 0, -1, -1)); }
}
void MineralReserveBar::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) { setFocus(); emit scaleClicked(); e->accept(); }
    else QProgressBar::mousePressEvent(e);
}
void MineralReserveBar::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return) { emit scaleClicked(); e->accept(); }
    else QProgressBar::keyPressEvent(e);
}
MineralStockPanel::MineralStockPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName("mineralStockPanel");
    auto* layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(2);
    scaleButton_ = new QPushButton(this); scaleButton_->setObjectName("mineralStockScale"); scaleButton_->setFlat(true);
    scaleButton_->setToolTip("Shared scale for all three stocks and mining output. Click to cycle scale; concentration always uses 0–100%.");
    layout->addWidget(scaleButton_); connect(scaleButton_, &QPushButton::clicked, this, &MineralStockPanel::cycleScale);
    bars_ = {new MineralReserveBar("Ironium", QColor("#6db7e7"), this), new MineralReserveBar("Boranium", QColor("#75c586"), this), new MineralReserveBar("Germanium", QColor("#dfcc75"), this)};
    const char* names[]{"ironiumConcentration", "boraniumConcentration", "germaniumConcentration"};
    for (int i = 0; i < 3; ++i) { bars_[i]->setObjectName(names[i]); layout->addWidget(bars_[i]); connect(bars_[i], &MineralReserveBar::scaleClicked, this, &MineralStockPanel::cycleScale); }
    ageLabel_ = new QLabel(this); ageLabel_->setObjectName("mineralStockAge"); ageLabel_->setWordWrap(true); layout->addWidget(ageLabel_);
    refresh();
}
void MineralStockPanel::setMinerals(const MineralDisplayData& data, std::uint64_t turn) { data_ = data; turn_ = turn; refresh(); }
double MineralStockPanel::stockScale() const { return scales[scaleIndex_]; }
void MineralStockPanel::cycleScale() { scaleIndex_ = (scaleIndex_ + 1) % scales.size(); emit stockScaleChanged(); refresh(); }
void MineralStockPanel::refresh()
{
    scaleButton_->setText(QString("Stock: 0–%1 kt  ·  ◇ 0–100%").arg(stockScale(), 0, 'f', 0));
    const auto age = data_.observedTurn ? turn_ - std::min(turn_, *data_.observedTurn) : 0;
    const auto ageText = !data_.observedTurn ? QString("Surface stock: no received report") : age == 0 ? QString("Surface stock: current report")
        : QString("Surface stock: %1 year%2 old (year %3)").arg(qulonglong(age)).arg(age == 1 ? "" : "s").arg(qulonglong(*data_.observedTurn));
    ageLabel_->setText(ageText);
    const auto stock = components(data_.stock.value_or(MineralCargo{}));
    const auto mining = components(data_.mining.value_or(MineralCargo{}));
    const auto conc = components(data_.concentration.value_or(MineralCargo{}));
    for (int i = 0; i < 3; ++i) bars_[i]->setMineral(data_.stock ? std::optional(stock[i]) : std::nullopt,
        data_.mining ? std::optional(mining[i]) : std::nullopt, data_.concentration ? std::optional(conc[i]) : std::nullopt, stockScale(), ageText);
}
} // namespace suns
