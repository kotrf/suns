#pragma once

#include "suns/player_knowledge.hpp"
#include <QColor>
#include <QProgressBar>
#include <QWidget>
#include <array>
#include <optional>

class QLabel;
class QPushButton;

namespace suns {
struct MineralDisplayData {
    std::optional<MineralCargo> stock;
    std::optional<MineralCargo> concentration;
    std::optional<MineralCargo> mining;
    std::optional<std::uint64_t> observedTurn;
};
// Inputs must already be a player view; never consult hidden host quantities.
MineralDisplayData mineral_display_data(const GameState& received, const GameState& planned, PlayerId, PlanetId);

class MineralReserveBar final : public QProgressBar {
    Q_OBJECT
public:
    MineralReserveBar(QString name, QColor color, QWidget* parent);
    void setMineral(std::optional<double> stock, std::optional<double> mining,
        std::optional<double> concentration, double scale, const QString& age);
signals:
    void scaleClicked();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    QString name_;
    QColor color_;
    std::optional<double> stock_, mining_, concentration_;
    double scale_{1000};
};

class MineralStockPanel final : public QWidget {
    Q_OBJECT
public:
    explicit MineralStockPanel(QWidget* parent);
    void setMinerals(const MineralDisplayData&, std::uint64_t turn);
    double stockScale() const;
private:
    void cycleScale();
    void refresh();
    QPushButton* scaleButton_{};
    QLabel* ageLabel_{};
    std::array<MineralReserveBar*, 3> bars_{};
    MineralDisplayData data_;
    std::uint64_t turn_{};
    std::size_t scaleIndex_{4};
};
} // namespace suns
