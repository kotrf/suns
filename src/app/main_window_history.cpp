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
#include <set>

namespace suns {
namespace {

constexpr quint32 kFollowSelectedColony = std::numeric_limits<quint32>::max();
constexpr quint32 kNoComparison = kFollowSelectedColony - 1;

struct HistorySeries {
    QString name;
    QColor color;
    QVector<double> values;
    QVector<QString> exactValues;
    bool dashed{};
};

struct ChartMarker {
    int index{};
    QString description;
    QColor color;
};

class HistoryChart final : public QWidget {
public:
    explicit HistoryChart(QWidget* parent) : QWidget(parent)
    {
        setObjectName("empireHistoryChart");
        setMinimumSize(400, 230);
        setMouseTracking(true);
    }

    void setData(QVector<std::uint64_t> turns, QVector<HistorySeries> series,
        QVector<ChartMarker> markers = {})
    {
        turns_ = std::move(turns);
        series_ = std::move(series);
        markers_ = std::move(markers);
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
            painter.setPen(QPen(series.color, 2,
                series.dashed ? Qt::DashLine : Qt::SolidLine));
            painter.drawLine(legendX, legendY - 6, legendX + 11, legendY - 6);
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
            bool drawing = false;
            for (int index = 0; index < series.values.size(); ++index) {
                if (!std::isfinite(series.values[index])) {
                    drawing = false;
                    continue;
                }
                const QPointF point(xAt(index), plot.bottom()
                    - plot.height() * std::clamp(series.values[index] / maximum, 0.0, 1.0));
                if (!drawing) path.moveTo(point);
                else path.lineTo(point);
                drawing = true;
            }
            painter.setPen(QPen(series.color, 2,
                series.dashed ? Qt::DashLine : Qt::SolidLine));
            painter.drawPath(path);
            const auto visiblePoints = std::count_if(series.values.begin(), series.values.end(),
                [](double value) { return std::isfinite(value); });
            if (turns_.size() <= 50 || visiblePoints == 1) {
                painter.setBrush(series.color);
                for (int index = 0; index < series.values.size(); ++index) {
                    if (!std::isfinite(series.values[index])) continue;
                    painter.drawEllipse(QPointF(xAt(index), plot.bottom()
                        - plot.height() * std::clamp(series.values[index] / maximum, 0.0, 1.0)), 2.5, 2.5);
                }
                painter.setBrush(Qt::NoBrush);
            }
        }
        for (const auto& marker : markers_) {
            const double x = xAt(marker.index);
            painter.setPen(QPen(marker.color, 1.5, Qt::DashLine));
            painter.drawLine(QPointF(x, plot.top() + 7), QPointF(x, plot.bottom()));
            painter.fillRect(QRectF(x - 3, plot.top(), 6, 6), marker.color);
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
        for (const auto& marker : markers_)
            if (marker.index == index) lines << marker.description;
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
    QVector<ChartMarker> markers_;
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
        "Technology levels", "Research invested (RP)", "Mineral extraction (kt/year)",
        "Freight delivered to colonies (kt/year)", "Remote extraction (kt/year)"});
    controls->addWidget(historyMetric_, 1);
    historyScope_ = new QComboBox(content);
    historyScope_->setObjectName("historyScope");
    historyScope_->addItem("Whole empire", static_cast<quint32>(0));
    historyScope_->setToolTip("Show an owned colony's recorded years, or follow the map selection; gaps mean it was not owned");
    controls->addWidget(historyScope_, 1);
    historyCompare_ = new QComboBox(content);
    historyCompare_->setObjectName("historyCompare");
    historyCompare_->setToolTip("Overlay a second colony or the whole empire using dashed lines");
    historyCompare_->addItem("Compare: none", kNoComparison);
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
    layout->addWidget(historyCompare_);

    historyChart_ = new HistoryChart(content);
    layout->addWidget(historyChart_, 1);
    historyDock_->setWidget(content);
    addDockWidget(Qt::BottomDockWidgetArea, historyDock_);
    historyDock_->hide();

    connect(historyMetric_, &QComboBox::currentIndexChanged, this,
        [this] { refreshEmpireHistory(); });
    connect(historyScope_, &QComboBox::currentIndexChanged, this,
        [this] { refreshEmpireHistory(); });
    connect(historyCompare_, &QComboBox::currentIndexChanged, this,
        [this] { refreshEmpireHistory(); });
    connect(this, &MainWindow::routeProgramContextChanged, this, [this] {
        if (historyDock_->isVisible()
            && historyScope_->currentData().toUInt() == kFollowSelectedColony)
            refreshEmpireHistory();
    });
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
    const auto selectedScope = historyScope_->currentData().toUInt();
    const auto selectedComparison = historyCompare_->currentData().toUInt();
    const bool remoteMetric = historyMetric_->currentIndex() == 10;
    const bool scopedMetric = historyMetric_->currentIndex() <= 3
        || historyMetric_->currentIndex() == 8 || historyMetric_->currentIndex() == 9 || remoteMetric;
    std::set<PlanetId> knownSites;
    for (const auto& snapshot : history) {
        if (remoteMetric) {
            for (const auto& site : snapshot.remoteMineHistory) knownSites.insert(site.planet);
        } else {
            for (const auto& colony : snapshot.colonyHistory) knownSites.insert(colony.planet);
        }
    }
    const auto* planetOnMap = selectedPlanet();
    const PlanetId selectedColony = planetOnMap
        && (remoteMetric ? knownSites.contains(planetOnMap->id)
                         : planetOnMap->owner == pendingOrders_.player)
        ? planetOnMap->id : PlanetId{};
    {
        const QSignalBlocker blocker(historyScope_);
        historyScope_->clear();
        historyScope_->addItem("Whole empire", static_cast<quint32>(0));
        historyScope_->addItem(selectedColony
            ? QString("Follow map — %1").arg(QString::fromStdString(planetOnMap->name))
            : remoteMetric ? QString("Follow map — select mined world")
                           : QString("Follow map — select owned colony"), kFollowSelectedColony);
        const QSignalBlocker compareBlocker(historyCompare_);
        historyCompare_->clear();
        historyCompare_->addItem("Compare: none", kNoComparison);
        historyCompare_->addItem("Compare: whole empire", static_cast<quint32>(0));
        for (const auto id : knownSites) {
            const auto planet = std::find_if(state_.planets.begin(), state_.planets.end(),
                [id](const auto& candidate) { return candidate.id == id; });
            const auto name = remoteMetric
                ? (planet == state_.planets.end()
                    ? QString("Mined world %1").arg(id)
                    : QString::fromStdString(planet->name))
                : (planet == state_.planets.end() || planet->owner != pendingOrders_.player
                    ? QString("Former colony %1").arg(id)
                    : QString::fromStdString(planet->name));
            historyScope_->addItem(name, static_cast<quint32>(id));
            historyCompare_->addItem(QString("Compare: %1").arg(name), static_cast<quint32>(id));
        }
        const int restored = historyScope_->findData(selectedScope);
        historyScope_->setCurrentIndex(!scopedMetric || restored < 0 ? 0 : restored);
        const int comparisonIndex = historyCompare_->findData(selectedComparison);
        historyCompare_->setCurrentIndex(comparisonIndex < 0 ? 0 : comparisonIndex);
    }
    historyScope_->setEnabled(scopedMetric && !history.empty());
    historyCompare_->setEnabled(scopedMetric && !history.empty());
    const bool followingMap = scopedMetric
        && historyScope_->currentData().toUInt() == kFollowSelectedColony;
    const auto colonyId = followingMap ? selectedColony
        : scopedMetric ? static_cast<PlanetId>(historyScope_->currentData().toUInt()) : PlanetId{};
    if (scopedMetric && historyCompare_->currentData().toUInt() == colonyId) {
        const QSignalBlocker blocker(historyCompare_);
        historyCompare_->setCurrentIndex(0);
    }
    const auto compareSelection = historyCompare_->currentData().toUInt();
    const bool comparing = scopedMetric && compareSelection != kNoComparison
        && compareSelection != colonyId;
    const auto compareColonyId = static_cast<PlanetId>(compareSelection);
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
    if (followingMap && colonyId == 0) {
        historySummary_->setText(remoteMetric
            ? "Select a recorded mining world on the map to view its history."
            : "Select an owned colony on the map to view its history.");
        chart->setData({}, {});
        return;
    }

    QVector<const EmpireTurnStatistics*> shown;
    QVector<std::uint64_t> turns;
    for (const auto& snapshot : history) {
        if (snapshot.turn < std::uint64_t(historyFirstTurn_->value())
            || snapshot.turn > std::uint64_t(historyLastTurn_->value())) continue;
        shown.push_back(&snapshot);
        turns.push_back(snapshot.turn);
    }

    QVector<HistorySeries> series;
    const auto appendMetric = [&](PlanetId seriesColonyId, const QString& seriesLabel, bool dashed) {
        const auto named = [&](QString name) {
            return seriesLabel.isEmpty() ? name : seriesLabel + " / " + name;
        };
        const auto add = [&](QString name, QColor color,
                             std::function<double(const EmpireTurnStatistics&)> value, int decimals = 0) {
            HistorySeries line{named(std::move(name)), std::move(color), {}, {}, dashed};
            for (const auto* snapshot : shown) {
                if (historyMetric_->currentIndex() == 8 && !snapshot->extractionRecorded) {
                    line.values.push_back(std::numeric_limits<double>::quiet_NaN());
                    line.exactValues.push_back("No extraction record");
                    continue;
                }
                if (historyMetric_->currentIndex() == 9 && !snapshot->freightRecorded) {
                    line.values.push_back(std::numeric_limits<double>::quiet_NaN());
                    line.exactValues.push_back("No freight record");
                    continue;
                }
                if (remoteMetric && !snapshot->remoteExtractionRecorded) {
                    line.values.push_back(std::numeric_limits<double>::quiet_NaN());
                    line.exactValues.push_back("No remote mining record");
                    continue;
                }
                const double number = value(*snapshot);
                line.values.push_back(number);
                line.exactValues.push_back(QString::number(number, 'f', decimals));
            }
            series.push_back(std::move(line));
        };
        const auto addColony = [&](QString name, QColor color,
                                  std::function<double(const ColonyTurnStatistics&)> value, int decimals = 0) {
            HistorySeries line{named(std::move(name)), std::move(color), {}, {}, dashed};
            for (const auto* snapshot : shown) {
                const auto colony = std::find_if(snapshot->colonyHistory.begin(),
                    snapshot->colonyHistory.end(), [seriesColonyId](const auto& candidate) {
                        return candidate.planet == seriesColonyId;
                    });
                if (colony == snapshot->colonyHistory.end()
                    || (historyMetric_->currentIndex() == 8 && !snapshot->extractionRecorded)
                    || (historyMetric_->currentIndex() == 9 && !snapshot->freightRecorded)) {
                    line.values.push_back(std::numeric_limits<double>::quiet_NaN());
                    line.exactValues.push_back(colony == snapshot->colonyHistory.end()
                        ? "No owned-colony record"
                        : historyMetric_->currentIndex() == 8 ? "No extraction record" : "No freight record");
                } else {
                    const double number = value(*colony);
                    line.values.push_back(number);
                    line.exactValues.push_back(QString::number(number, 'f', decimals));
                }
            }
            series.push_back(std::move(line));
        };
        const auto addRemoteSite = [&](QString name, QColor color,
                                      std::function<double(const MineralCargo&)> value) {
            HistorySeries line{named(std::move(name)), std::move(color), {}, {}, dashed};
            for (const auto* snapshot : shown) {
                if (!snapshot->remoteExtractionRecorded) {
                    line.values.push_back(std::numeric_limits<double>::quiet_NaN());
                    line.exactValues.push_back("No remote mining record");
                    continue;
                }
                const auto site = std::find_if(snapshot->remoteMineHistory.begin(),
                    snapshot->remoteMineHistory.end(), [seriesColonyId](const auto& record) {
                        return record.planet == seriesColonyId;
                    });
                const double number = site == snapshot->remoteMineHistory.end()
                    ? 0.0 : value(site->extraction);
                line.values.push_back(number);
                line.exactValues.push_back(QString::number(number, 'f', 2));
            }
            series.push_back(std::move(line));
        };

        switch (historyMetric_->currentIndex()) {
        case 0:
            if (seriesColonyId) addColony("Population", "#78b8f0", [](const auto& s) { return double(s.population); });
            else add("Population", "#78b8f0", [](const auto& s) { return double(s.population); });
            break;
        case 1:
            if (seriesColonyId) {
                addColony("Factories", "#e2bb70", [](const auto& s) { return double(s.factories); });
                addColony("Mines", "#8dcc9e", [](const auto& s) { return double(s.mines); });
            } else {
                add("Colonies", "#78b8f0", [](const auto& s) { return double(s.colonies); });
                add("Factories", "#e2bb70", [](const auto& s) { return double(s.factories); });
                add("Mines", "#8dcc9e", [](const auto& s) { return double(s.mines); });
            }
            break;
        case 2:
            if (seriesColonyId) addColony("Output / year", "#78b8f0", [](const auto& s) { return double(s.productionOutput); });
            else add("Output / year", "#78b8f0", [](const auto& s) { return double(s.productionOutput); });
            break;
        case 3:
            if (seriesColonyId) {
                addColony("Ironium", "#db9a7a", [](const auto& s) { return s.minerals.ironium; }, 2);
                addColony("Boranium", "#8dcc9e", [](const auto& s) { return s.minerals.boranium; }, 2);
                addColony("Germanium", "#78b8f0", [](const auto& s) { return s.minerals.germanium; }, 2);
            } else {
                add("Ironium", "#db9a7a", [](const auto& s) { return s.minerals.ironium; }, 2);
                add("Boranium", "#8dcc9e", [](const auto& s) { return s.minerals.boranium; }, 2);
                add("Germanium", "#78b8f0", [](const auto& s) { return s.minerals.germanium; }, 2);
            }
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
        case 8:
            if (seriesColonyId) {
                addColony("Ironium", "#db9a7a", [](const auto& s) { return s.extraction.ironium; }, 2);
                addColony("Boranium", "#8dcc9e", [](const auto& s) { return s.extraction.boranium; }, 2);
                addColony("Germanium", "#78b8f0", [](const auto& s) { return s.extraction.germanium; }, 2);
            } else {
                add("Ironium", "#db9a7a", [](const auto& s) { return s.extraction.ironium; }, 2);
                add("Boranium", "#8dcc9e", [](const auto& s) { return s.extraction.boranium; }, 2);
                add("Germanium", "#78b8f0", [](const auto& s) { return s.extraction.germanium; }, 2);
            }
            break;
        case 9:
            if (seriesColonyId) {
                addColony("Ironium", "#db9a7a", [](const auto& s) { return s.freightDelivered.ironium; }, 2);
                addColony("Boranium", "#8dcc9e", [](const auto& s) { return s.freightDelivered.boranium; }, 2);
                addColony("Germanium", "#78b8f0", [](const auto& s) { return s.freightDelivered.germanium; }, 2);
                addColony("Colonists", "#d1a2e0", [](const auto& s) { return colonist_cargo_mass(s.colonistsDelivered); }, 2);
            } else {
                add("Ironium", "#db9a7a", [](const auto& s) { return s.freightDelivered.ironium; }, 2);
                add("Boranium", "#8dcc9e", [](const auto& s) { return s.freightDelivered.boranium; }, 2);
                add("Germanium", "#78b8f0", [](const auto& s) { return s.freightDelivered.germanium; }, 2);
                add("Colonists", "#d1a2e0", [](const auto& s) { return colonist_cargo_mass(s.colonistsDelivered); }, 2);
            }
            break;
        case 10:
            if (seriesColonyId) {
                addRemoteSite("Ironium", "#db9a7a", [](const auto& s) { return s.ironium; });
                addRemoteSite("Boranium", "#8dcc9e", [](const auto& s) { return s.boranium; });
                addRemoteSite("Germanium", "#78b8f0", [](const auto& s) { return s.germanium; });
            } else {
                add("Ironium", "#db9a7a", [](const auto& s) { return s.remoteExtraction.ironium; }, 2);
                add("Boranium", "#8dcc9e", [](const auto& s) { return s.remoteExtraction.boranium; }, 2);
                add("Germanium", "#78b8f0", [](const auto& s) { return s.remoteExtraction.germanium; }, 2);
            }
            break;
        default: break;
        }
    };
    const auto scopeLabel = [&](PlanetId id) {
        if (id == 0) return QString("Empire");
        const auto index = historyScope_->findData(static_cast<quint32>(id));
        return index < 0 ? QString(remoteMetric ? "Mined world %1" : "Colony %1").arg(id)
            : historyScope_->itemText(index);
    };
    appendMetric(colonyId, comparing ? scopeLabel(colonyId) : QString{}, false);
    if (comparing) appendMetric(compareColonyId, scopeLabel(compareColonyId), true);
    QVector<ChartMarker> markers;
    for (int index = 0; index < shown.size(); ++index) {
        for (const auto& event : shown[index]->milestones) {
            if (colonyId && (!comparing || compareColonyId != 0)
                && (event.kind == HistoryMilestoneKind::ResearchCompleted
                    || event.kind == HistoryMilestoneKind::FleetStalledForFuel
                    || (event.planet != colonyId
                        && (!comparing || event.planet != compareColonyId)))) continue;
            QString label;
            QColor color;
            switch (event.kind) {
            case HistoryMilestoneKind::ColonyFounded:
                label = QString("Colony %1 founded").arg(event.planet);
                color = QColor("#8dcc9e");
                break;
            case HistoryMilestoneKind::ColonyLost:
                label = QString("Colony %1 lost").arg(event.planet);
                color = QColor("#e07575");
                break;
            case HistoryMilestoneKind::ResearchCompleted:
                label = QString("%1 %2 completed")
                    .arg(QString::fromStdString(research_field_name(event.researchField)))
                    .arg(event.technologyLevel);
                color = QColor("#d1a2e0");
                break;
            case HistoryMilestoneKind::FleetStalledForFuel:
                label = QString("Fleet %1 stalled for fuel").arg(event.fleet);
                color = QColor("#e4b868");
                break;
            }
            if (event.observedTurn && event.observedTurn != shown[index]->turn)
                label += QString(" (observed year %1)").arg(static_cast<qulonglong>(event.observedTurn));
            markers.push_back({index, std::move(label), color});
        }
    }
    chart->setData(std::move(turns), std::move(series), std::move(markers));
}

} // namespace suns
