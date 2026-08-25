#include "main_window.hpp"

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGraphicsScene>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace suns {
namespace {

quint32 portraitSeed(std::uint64_t galaxySeed, PlanetId planet)
{
    const auto mixed = galaxySeed ^ (static_cast<std::uint64_t>(planet) * 0x9E3779B97F4A7C15ULL);
    return static_cast<quint32>(mixed ^ (mixed >> 32U));
}

QColor basePlanetColor(const Planet& planet)
{
    if (planet.habitability >= 80) return QColor("#3d83a6");
    if (planet.habitability >= 60) return QColor("#8b7b46");
    if (planet.habitability >= 40) return QColor("#9b5f45");
    if (planet.habitability >= 20) return QColor("#7a7770");
    return QColor("#7d91a5");
}

QPixmap unknownPortrait()
{
    QPixmap pixmap(156, 156);
    pixmap.fill(QColor("#09111b"));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QRadialGradient gradient(QPointF(68, 62), 62);
    gradient.setColorAt(0.0, QColor("#536170"));
    gradient.setColorAt(0.65, QColor("#242e39"));
    gradient.setColorAt(1.0, QColor("#080d14"));
    painter.setBrush(gradient);
    painter.setPen(QPen(QColor("#657587"), 1.2, Qt::DashLine));
    painter.drawEllipse(QRectF(24, 24, 108, 108));
    painter.setPen(QColor("#91a0b0"));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "?");
    return pixmap;
}

QPixmap renderPlanetPortrait(const Planet& planet, StarClass stellarClass, std::uint64_t galaxySeed)
{
    constexpr int size = 156;
    QPixmap pixmap(size, size);
    pixmap.fill(QColor("#070d15"));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QRandomGenerator rng(portraitSeed(galaxySeed, planet.id));
    const QRectF disk(21, 21, 114, 114);
    QPainterPath clip;
    clip.addEllipse(disk);

    const QColor base = basePlanetColor(planet);
    QRadialGradient globe(QPointF(61, 53), 94);
    globe.setColorAt(0.0, base.lighter(150));
    globe.setColorAt(0.58, base);
    globe.setColorAt(1.0, QColor("#080b10"));
    painter.setPen(Qt::NoPen);
    painter.setBrush(globe);
    painter.drawEllipse(disk);

    painter.save();
    painter.setClipPath(clip);

    const int patches = 8 + static_cast<int>(rng.bounded(8u));
    QColor land = base.lighter(planet.habitability >= 55 ? 120 : 108);
    if (planet.habitability >= 70) land = QColor("#698957");
    land.setAlpha(185);
    painter.setBrush(land);
    for (int i = 0; i < patches; ++i) {
        const qreal x = 25.0 + rng.generateDouble() * 100.0;
        const qreal y = 31.0 + rng.generateDouble() * 88.0;
        const qreal w = 12.0 + rng.generateDouble() * 34.0;
        const qreal h = 6.0 + rng.generateDouble() * 19.0;
        painter.drawEllipse(QRectF(x - w / 2.0, y - h / 2.0, w, h));
    }

    if (planet.habitability >= 55) {
        QColor clouds(238, 246, 250, 52 + static_cast<int>(rng.bounded(55u)));
        painter.setBrush(clouds);
        for (int i = 0; i < 7; ++i) {
            const qreal y = 34.0 + rng.generateDouble() * 78.0;
            const qreal x = 30.0 + rng.generateDouble() * 83.0;
            painter.drawEllipse(QRectF(x, y, 30.0 + rng.generateDouble() * 35.0, 3.0 + rng.generateDouble() * 5.0));
        }
    }

    if (planet.habitability < 24) {
        painter.setBrush(QColor(225, 235, 244, 105));
        painter.drawEllipse(QRectF(39, 25, 78, 18));
        painter.drawEllipse(QRectF(39, 111, 78, 18));
    }

    painter.restore();

    QColor atmosphere = planet.habitability >= 60 ? QColor(93, 183, 227, 100) : QColor(175, 115, 84, 55);
    QPen atmospherePen(atmosphere);
    atmospherePen.setWidthF(2.2);
    painter.setPen(atmospherePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(disk.adjusted(-1.5, -1.5, 1.5, 1.5));

    QLinearGradient shadow(52, 0, 132, 0);
    shadow.setColorAt(0.0, QColor(0, 0, 0, 0));
    shadow.setColorAt(0.55, QColor(0, 0, 0, 45));
    shadow.setColorAt(1.0, QColor(0, 0, 0, 205));
    painter.setBrush(shadow);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(disk);

    QColor starTint("#ffd36b");
    if (stellarClass == StarClass::BlueWhite) starTint = QColor("#9bc5ff");
    else if (stellarClass == StarClass::White) starTint = QColor("#e7eeff");
    else if (stellarClass == StarClass::Orange) starTint = QColor("#ff9955");
    else if (stellarClass == StarClass::Red) starTint = QColor("#ff6b62");
    painter.setPen(starTint);
    painter.drawPoint(12, 12);

    return pixmap;
}

QProgressBar* makeMineralBar(const QString& name, const char* objectName, QWidget* parent)
{
    auto* bar = new QProgressBar(parent);
    bar->setObjectName(objectName);
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setTextVisible(true);
    bar->setFormat(name + " — unknown");
    bar->setToolTip("Geological concentration; higher concentration produces more mineral units each turn.");
    return bar;
}

} // namespace

void MainWindow::installPlanetPolish()
{
    if (galaxyLabel_) galaxyLabel_->hide();

    auto* helpMenu = menuBar()->findChild<QMenu*>("sunsHelpMenu");
    if (!helpMenu) {
        helpMenu = menuBar()->addMenu("&Help");
        helpMenu->setObjectName("sunsHelpMenu");
    }
    auto* reference = helpMenu->addAction("Game &Reference…");
    connect(reference, &QAction::triggered, this, [this] {
        QDialog dialog(this);
        dialog.setWindowTitle("Suns! — Game Reference");
        dialog.resize(520, 420);
        auto* layout = new QVBoxLayout(&dialog);
        auto* text = new QLabel(&dialog);
        text->setWordWrap(true);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        text->setText(QString(
            "<h2>Suns! quick reference</h2>"
            "<b>Galaxy seed:</b> %1<br>"
            "<b>Systems:</b> %2<br><br>"
            "<b>Distance:</b> light-years (ly)<br>"
            "<b>Movement:</b> Warp² light-years per turn<br>"
            "<b>Fuel:</b> engine rate × gross mass / 100 × distance<br>"
            "<b>Cargo:</b> colonists and minerals share the same hold; 100 colonists = 1 cargo unit<br>"
            "<b>Sensors:</b> fly-bys give a basic estimate, arrival confirms habitability, and one turn in orbit reveals geology<br>"
            "<b>Minerals:</b> concentration controls automatic colony extraction. Ship and factory completion consumes I/B/G stocks.<br>"
            "<b>Orders:</b> commands are queued and resolved together at End Turn<br><br>"
            "Map modes: <b>Spectral</b> shows stellar class, <b>Habitability</b> shows surveyed world value, "
            "and <b>Population</b> scales colony markers by population.")
            .arg(static_cast<qulonglong>(state_.galaxySeed))
            .arg(static_cast<qulonglong>(state_.stars.size())));
        layout->addWidget(text);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        dialog.exec();
    });

    auto* planetGroup = findChild<QGroupBox*>("planetGroup");
    if (planetGroup) {
        auto* layout = qobject_cast<QVBoxLayout*>(planetGroup->layout());
        if (layout) {
            auto* portrait = new QLabel(planetGroup);
            portrait->setObjectName("planetPortrait");
            portrait->setAlignment(Qt::AlignCenter);
            portrait->setMinimumHeight(160);
            portrait->setPixmap(unknownPortrait());
            layout->insertWidget(0, portrait);

            auto* mineralsTitle = new QLabel("<b>Mineral geology</b>", planetGroup);
            mineralsTitle->setObjectName("mineralGeologyTitle");
            layout->insertWidget(2, mineralsTitle);
            layout->insertWidget(3, makeMineralBar("Ironium", "ironiumConcentration", planetGroup));
            layout->insertWidget(4, makeMineralBar("Boranium", "boraniumConcentration", planetGroup));
            layout->insertWidget(5, makeMineralBar("Germanium", "germaniumConcentration", planetGroup));
        }
    }

    connect(scene_, &QGraphicsScene::changed, this, [this](const QList<QRectF>&) {
        if (shuttingDown_ || planetPolishRefreshPending_) return;
        planetPolishRefreshPending_ = true;
        QTimer::singleShot(0, this, [this] {
            planetPolishRefreshPending_ = false;
            if (!shuttingDown_) refreshPlanetPolish();
        });
    });
    refreshPlanetPolish();
}

void MainWindow::refreshPlanetPolish()
{
    auto* portrait = findChild<QLabel*>("planetPortrait");
    auto* ironium = findChild<QProgressBar*>("ironiumConcentration");
    auto* boranium = findChild<QProgressBar*>("boraniumConcentration");
    auto* germanium = findChild<QProgressBar*>("germaniumConcentration");
    if (!portrait || !ironium || !boranium || !germanium) return;

    const auto* star = selectedStar();
    const auto* planet = selectedPlanet();
    const auto level = star ? survey_level(state_, 1, star->id) : SurveyLevel::Detected;
    if (!star || !planet || level < SurveyLevel::OrbitalSurvey) {
        portrait->setPixmap(unknownPortrait());
        for (auto* bar : {ironium, boranium, germanium}) {
            bar->setValue(0);
            bar->setFormat(level >= SurveyLevel::BasicScan
                    ? "Unknown until orbital survey"
                    : "Unknown until basic scan");
            bar->setEnabled(false);
        }
        return;
    }

    portrait->setPixmap(renderPlanetPortrait(*planet, star->stellarClass, state_.galaxySeed));
    if (level < SurveyLevel::GeologicalSurvey && planet->owner != 1) {
        for (auto* bar : {ironium, boranium, germanium}) {
            bar->setValue(0);
            bar->setFormat("Unknown until geological survey");
            bar->setEnabled(false);
        }
        return;
    }
    const auto concentration = planet_mineral_concentration(state_, *planet);
    const auto mining = projected_mineral_mining(state_, *planet);

    const struct {
        QProgressBar* bar;
        const char* name;
        double concentration;
        double stock;
        double mining;
    } entries[] = {
        {ironium, "Ironium", concentration.ironium, planet->minerals.ironium, mining.ironium},
        {boranium, "Boranium", concentration.boranium, planet->minerals.boranium, mining.boranium},
        {germanium, "Germanium", concentration.germanium, planet->minerals.germanium, mining.germanium},
    };
    for (const auto& entry : entries) {
        entry.bar->setEnabled(true);
        entry.bar->setValue(static_cast<int>(std::lround(entry.concentration)));
        if (planet->owner != 0) {
            entry.bar->setFormat(QString("%1 %2% • stock %3 • +%4/turn")
                                     .arg(entry.name)
                                     .arg(entry.concentration, 0, 'f', 0)
                                     .arg(entry.stock, 0, 'f', 1)
                                     .arg(entry.mining, 0, 'f', 1));
        } else {
            entry.bar->setFormat(QString("%1 %2% • stock %3")
                                     .arg(entry.name)
                                     .arg(entry.concentration, 0, 'f', 0)
                                     .arg(entry.stock, 0, 'f', 1));
        }
    }
}

} // namespace suns
