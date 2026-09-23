#include "main_window.hpp"

#include <QComboBox>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace suns {
namespace {

struct HistorySeries {
    QString name;
    QColor color;
    QVector<double> values;
    QVector<QString> exactValues;
};

class HistoryChart final : public QWidget {
public:
    explicit HistoryChart(QWidget* parent) : QWidget(parent)
    {
        setObjectName("empireHistoryChart");
        setMinimumSize(400, 230);
        setMouseTracking(true);
    }

    void setData(QVector<std::uint64_t> turns, QVector<HistorySeries> series)
    {
        turns_ = std::move(turns);
        series_ = std::move(series);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#111a25"));

        if (turns_.isEmpty()) {
            painter.setPen(QColor("#9fb0c0"));
            painter.drawText(rect(), Qt::AlignCenter, "No recorded years for this empire.");
            return;
        }

        int legendX = 68;
        int legendY = 16;
        for (const auto& series : series_) {
            const int width = painter.fontMetrics().horizontalAdvance(series.name) + 27;
            if (legendX + width > this->width() - 12) { legendX = 68; legendY += 18; }
            painter.fillRect(legendX, legendY - 7, 11, 3, series.color);
            painter.setPen(QColor("#d5e0ed"));
            painter.drawText(legendX + 16, legendY, series.name);
            legendX += width;
        }

        const QRectF plot = plotRect(legendY);
        if (plot.width() < 1 || plot.height() < 1) return;
        double maximum = 1.0;
        for (const auto& series : series_)
            for (const double value : series.values)
                if (std::isfinite(value)) maximum = std::max(maximum, value);

        painter.setPen(QColor("#34485b"));
        for (int tick = 0; tick <= 4; ++tick) {
            const double y = plot.bottom() - plot.height() * tick / 4.0;
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            painter.setPen(QColor("#9fb0c0"));
            painter.drawText(QRectF(2, y - 9, plot.left() - 10, 18), Qt::AlignRight | Qt::AlignVCenter,
                QString::number(maximum * tick / 4.0, 'g', 4));
            painter.setPen(QColor("#34485b"));
        }

        const auto xAt = [&](int index) {
            return plot.left() + plot.width() * (turns_.size() == 1
                ? 0.5 : double(index) / double(turns_.size() - 1));
        };
        painter.setPen(QColor("#aebdcc"));
        painter.drawText(QRectF(plot.left(), plot.bottom() + 5, plot.width() / 2, 24),
            Qt::AlignLeft | Qt::AlignVCenter, QString::number(static_cast<qulonglong>(turns_.front())));
        painter.drawText(QRectF(plot.center().x(), plot.bottom() + 5, plot.width() / 2, 24),
            Qt::AlignRight | Qt::AlignVCenter, QString::number(static_cast<qulonglong>(turns_.back())));

        painter.setClipRect(plot.adjusted(-4, -4, 4, 4));
        for (const auto& series : series_) {
            QPainterPath path;
            for (int index = 0; index < series.values.size(); ++index) {
                const QPointF point(xAt(index), plot.bottom()
                    - plot.height() * std::clamp(series.values[index] / maximum, 0.0, 1.0));
                if (index == 0) path.moveTo(point);
                else path.lineTo(point);
            }
            painter.setPen(QPen(series.color, 2));
            painter.drawPath(path);
            if (turns_.size() <= 50) {
                painter.setBrush(series.color);
                for (int index = 0; index < series.values.size(); ++index)
                    painter.drawEllipse(QPointF(xAt(index), plot.bottom()
                        - plot.height() * std::clamp(series.values[index] / maximum, 0.0, 1.0)), 2.5, 2.5);
                painter.setBrush(Qt::NoBrush);
            }
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (turns_.isEmpty()) return;
        const auto plot = plotRect(legendBottom());
        if (!plot.contains(event->position())) { QToolTip::hideText(); return; }
        const int index = turns_.size() == 1 ? 0 : std::clamp(
            int(std::lround((event->position().x() - plot.left()) / plot.width() * (turns_.size() - 1))),
            0, int(turns_.size()) - 1);
        QStringList lines{QString("Year %1").arg(static_cast<qulonglong>(turns_[index]))};
        for (const auto& series : series_)
            lines << QString("%1: %2").arg(series.name, series.exactValues[index]);
        QToolTip::showText(event->globalPosition().toPoint(), lines.join('\n'), this);
    }

    void leaveEvent(QEvent*) override { QToolTip::hideText(); }

private:
    int legendBottom() const
    {
        QFontMetrics metrics(font());
        int x = 68;
        int y = 16;
        for (const auto& series : series_) {
            const int width = metrics.horizontalAdvance(series.name) + 27;
            if (x + width > this->width() - 12) { x = 68; y += 18; }
            x += width;
        }
        return y;
    }

    QRectF plotRect(int legendY) const
    {
        return QRectF(68, legendY + 20, width() - 84, height() - legendY - 53);
    }

    QVector<std::uint64_t> turns_;
    QVector<HistorySeries> series_;
};

} // namespace

void MainWindow::installEmpireHistory()
{
    if (historyDock_) return;
    historyDock_ = new QDockWidget("Empire History", this);
    historyDock_->setObjectName("empireHistoryDock");
    historyDock_->setAllowedAreas(Qt::AllDockWidgetAreas);
    auto* content = new QWidget(historyDock_);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 8, 8, 8);

    historySummary_ = new QLabel(content);
    historySummary_->setObjectName("historySummary");
    historySummary_->setTextFormat(Qt::PlainText);
    layout->addWidget(historySummary_);

    auto* controls = new QHBoxLayout;
    historyMetric_ = new QComboBox(content);
    historyMetric_->setObjectName("historyMetric");
    historyMetric_->addItems({"Population", "Colonies and infrastructure", "Production output",
        "Mineral stocks (kt)", "Fleets and ships", "Fleet mass (kt)",
        "Technology levels", "Research invested (RP)"});
    controls->addWidget(historyMetric_, 1);
    controls->addWidget(new QLabel("Years", content));
    historyFirstTurn_ = new QSpinBox(content);
    historyFirstTurn_->setObjectName("historyFirstTurn");
    historyLastTurn_ = new QSpinBox(content);
    historyLastTurn_->setObjectName("historyLastTurn");
    for (auto* spin : {historyFirstTurn_, historyLastTurn_}) {
        spin->setMinimumWidth(74);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    }
    controls->addWidget(historyFirstTurn_);
    controls->addWidget(new QLabel("to", content));
    controls->addWidget(historyLastTurn_);
    layout->addLayout(controls);

    historyChart_ = new HistoryChart(content);
    layout->addWidget(historyChart_, 1);
    historyDock_->setWidget(content);
    addDockWidget(Qt::BottomDockWidgetArea, historyDock_);
    historyDock_->hide();

    connect(historyMetric_, &QComboBox::currentIndexChanged, this,
        [this] { refreshEmpireHistory(); });
    connect(historyFirstTurn_, &QSpinBox::valueChanged, this, [this](int year) {
        if (historyLastTurn_->value() < year) historyLastTurn_->setValue(year);
        else refreshEmpireHistory();
    });
    connect(historyLastTurn_, &QSpinBox::valueChanged, this, [this](int year) {
        if (historyFirstTurn_->value() > year) historyFirstTurn_->setValue(year);
        else refreshEmpireHistory();
    });
    connect(historyDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible) refreshEmpireHistory();
    });
    refreshEmpireHistory();
}

void MainWindow::refreshEmpireHistory()
{
    if (!historyDock_) return;
    const auto* player = find_player(state_, pendingOrders_.player);
    const std::vector<EmpireTurnStatistics> empty;
    const auto& history = player ? player->history : empty;
    auto* chart = static_cast<HistoryChart*>(historyChart_);
    if (history.empty()) {
        historySummary_->setText("No recorded history for this player.");
        historyFirstTurn_->setEnabled(false);
        historyLastTurn_->setEnabled(false);
        chart->setData({}, {});
        historyRangeInitialized_ = false;
        return;
    }

    const int first = static_cast<int>(std::min<std::uint64_t>(history.front().turn, std::numeric_limits<int>::max()));
    const int last = static_cast<int>(std::min<std::uint64_t>(history.back().turn, std::numeric_limits<int>::max()));
    const int previousLast = historyLastTurn_->maximum();
    const bool follow = !historyRangeInitialized_ || historyLastTurn_->value() == previousLast;
    {
        const QSignalBlocker blockFirst(historyFirstTurn_);
        const QSignalBlocker blockLast(historyLastTurn_);
        historyFirstTurn_->setRange(first, last);
        historyLastTurn_->setRange(first, last);
        if (!historyRangeInitialized_) historyFirstTurn_->setValue(std::max(first, last - 49));
        if (follow) historyLastTurn_->setValue(last);
    }
    historyRangeInitialized_ = true;
    historyFirstTurn_->setEnabled(true);
    historyLastTurn_->setEnabled(true);
    historySummary_->setText(QString("%1 — %2 recorded years (latest: %3)")
        .arg(QString::fromStdString(player->name))
        .arg(static_cast<qulonglong>(history.size()))
        .arg(static_cast<qulonglong>(history.back().turn)));

    QVector<const EmpireTurnStatistics*> shown;
    QVector<std::uint64_t> turns;
    for (const auto& snapshot : history) {
        if (snapshot.turn < std::uint64_t(historyFirstTurn_->value())
            || snapshot.turn > std::uint64_t(historyLastTurn_->value())) continue;
        shown.push_back(&snapshot);
        turns.push_back(snapshot.turn);
    }

    QVector<HistorySeries> series;
    const auto add = [&](QString name, QColor color,
                         std::function<double(const EmpireTurnStatistics&)> value, int decimals = 0) {
        HistorySeries line{std::move(name), std::move(color), {}, {}};
        for (const auto* snapshot : shown) {
            const double number = value(*snapshot);
            line.values.push_back(number);
            line.exactValues.push_back(QString::number(number, 'f', decimals));
        }
        series.push_back(std::move(line));
    };

    switch (historyMetric_->currentIndex()) {
    case 0:
        add("Population", "#78b8f0", [](const auto& s) { return double(s.population); });
        break;
    case 1:
        add("Colonies", "#78b8f0", [](const auto& s) { return double(s.colonies); });
        add("Factories", "#e2bb70", [](const auto& s) { return double(s.factories); });
        add("Mines", "#8dcc9e", [](const auto& s) { return double(s.mines); });
        break;
    case 2:
        add("Output / year", "#78b8f0", [](const auto& s) { return double(s.productionOutput); });
        break;
    case 3:
        add("Ironium", "#db9a7a", [](const auto& s) { return s.minerals.ironium; }, 2);
        add("Boranium", "#8dcc9e", [](const auto& s) { return s.minerals.boranium; }, 2);
        add("Germanium", "#78b8f0", [](const auto& s) { return s.minerals.germanium; }, 2);
        break;
    case 4:
        add("Fleets", "#78b8f0", [](const auto& s) { return double(s.fleets); });
        add("Ships", "#e2bb70", [](const auto& s) { return double(s.ships); });
        break;
    case 5:
        add("Gross mass", "#78b8f0", [](const auto& s) { return s.fleetMass; }, 2);
        break;
    case 6:
    case 7: {
        constexpr const char* names[] = {"Energy", "Propulsion", "Construction", "Electronics", "Biology", "Weapons"};
        constexpr const char* colors[] = {"#db9a7a", "#78b8f0", "#8dcc9e", "#d1a2e0", "#e2bb70", "#b4c0ce"};
        const bool progress = historyMetric_->currentIndex() == 7;
        for (std::size_t field = 0; field < kResearchFieldCount; ++field)
            add(names[field], colors[field], [field, progress](const auto& s) {
                return double(progress ? s.technologyProgress[field] : s.technologyLevels[field]);
            });
        break;
    }
    default: break;
    }
    chart->setData(std::move(turns), std::move(series));
}

} // namespace suns
