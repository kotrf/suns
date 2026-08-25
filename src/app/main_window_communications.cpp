#include "main_window.hpp"

#include "suns/communications.hpp"

#include <QBoxLayout>
#include <QGraphicsScene>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QRectF>
#include <QString>
#include <QTimer>

namespace suns {

void MainWindow::installCommunicationStatus()
{
    if (!fleetLabel_ || !fleetLabel_->parentWidget()) return;

    auto* summary = new QLabel(fleetLabel_->parentWidget());
    summary->setObjectName("fleetCommunicationSummary");
    summary->setWordWrap(true);
    summary->setTextInteractionFlags(Qt::TextSelectableByMouse);

    if (auto* box = qobject_cast<QBoxLayout*>(fleetLabel_->parentWidget()->layout())) {
        const auto index = box->indexOf(fleetLabel_);
        box->insertWidget(index >= 0 ? index + 1 : box->count(), summary);
    }

    const auto refresh = [this, summary] {
        if (shuttingDown_ || !summary) return;
        const auto* fleet = selectedFleet();
        if (!fleet) {
            summary->setText("<b>Communications:</b> no fleet selected");
            if (fleetMoveButton_) fleetMoveButton_->setToolTip({});
            return;
        }

        const auto telemetry = confirmed_fleet_telemetry(state_, *fleet);
        const auto age = fleet_telemetry_age(state_, *fleet);
        const auto projected = projected_fleet_position(state_, *fleet);
        const auto estimatedDelay = communication_delay_turns(state_, fleet->owner, projected);
        const bool connected = fleet_has_instant_link(state_, *fleet) && age == 0;

        QString text = connected
            ? "<b>Communications:</b> <b>CONNECTED</b> — real-time relay link"
            : QString("<b>Communications:</b> <b>DELAYED</b> — confirmed telemetry is %1 turn%2 old")
                  .arg(static_cast<qulonglong>(age))
                  .arg(age == 1 ? "" : "s");

        text += QString("<br>Last confirmed: turn %1 at (%2, %3)")
                    .arg(static_cast<qulonglong>(telemetry.observedTurn == 0 ? state_.turn : telemetry.observedTurn))
                    .arg(telemetry.position.x, 0, 'f', 1)
                    .arg(telemetry.position.y, 0, 'f', 1);

        if (!connected) {
            text += QString("<br>Estimated now: (%1, %2) — model prediction, not ground truth")
                        .arg(projected.x, 0, 'f', 1)
                        .arg(projected.y, 0, 'f', 1);
            text += QString("<br>Estimated new-command latency: %1 turn%2")
                        .arg(estimatedDelay)
                        .arg(estimatedDelay == 1 ? "" : "s");
            text += "<br>Command reception is not confirmed until delayed telemetry reports it.";
        }

        summary->setText(text);
        if (fleetMoveButton_) {
            fleetMoveButton_->setToolTip(estimatedDelay == 0
                ? "Command will reach this fleet immediately."
                : QString("Estimated command delivery delay: %1 turn%2. The fleet keeps executing its onboard program until then.")
                      .arg(estimatedDelay)
                      .arg(estimatedDelay == 1 ? "" : "s"));
        }
    };

    connect(scene_, &QGraphicsScene::selectionChanged, summary, refresh);
    connect(scene_, &QGraphicsScene::changed, summary, [refresh](const QList<QRectF>&) { refresh(); });
    connect(endTurnButton_, &QPushButton::clicked, summary, [summary, refresh] {
        QTimer::singleShot(0, summary, refresh);
    });
    refresh();
}

} // namespace suns
