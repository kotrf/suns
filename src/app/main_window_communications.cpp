#include "main_window.hpp"

#include "suns/communications.hpp"

#include <QBoxLayout>
#include <QGraphicsScene>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QRectF>
#include <QStatusBar>
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

    auto* status = new QLabel(statusBar());
    status->setObjectName("statusBarComms");
    status->setTextFormat(Qt::RichText);
    statusBar()->addPermanentWidget(status);

    if (auto* box = qobject_cast<QBoxLayout*>(fleetLabel_->parentWidget()->layout())) {
        const auto index = box->indexOf(fleetLabel_);
        box->insertWidget(index >= 0 ? index + 1 : box->count(), summary);
    }

    const auto refresh = [this, summary, status] {
        if (shuttingDown_) return;
        const auto* fleet = selectedFleet();
        if (!fleet) {
            summary->setText("<b>Communications:</b> no fleet selected");
            summary->setToolTip({});
            status->setText("Comms: —");
            status->setToolTip("Select an owned fleet to view its communications state.");
            if (fleetMoveButton_) fleetMoveButton_->setToolTip({});
            return;
        }

        const auto telemetry = confirmed_fleet_telemetry(state_, *fleet);
        const auto age = fleet_telemetry_age(state_, *fleet);
        const auto radar = fleet_radar_position(state_, *fleet);
        const auto positionAge = fleet_position_age(state_, *fleet);
        const auto projected = projected_fleet_position(state_, *fleet);
        const auto estimatedDelay = communication_delay_turns(state_, fleet->owner, projected);
        const bool connected = fleet_has_instant_link(state_, *fleet) && age == 0;

        QString text = connected
            ? "<b>Comms:</b> <span style='color:#83c99a'><b>LIVE</b></span>"
            : QString("<b>Comms:</b> <span style='color:#e4b77d'><b>DELAYED</b></span> • telemetry %1 turn%2 old")
                  .arg(static_cast<qulonglong>(age))
                  .arg(age == 1 ? "" : "s");

        QString positionText;
        if (radar) positionText = QString("<br>Radar position: %1 • uncertainty %2 ly. Onboard telemetry remains separate.")
            .arg(positionAge == 0 ? "current" : QString("%1 turn(s) old").arg(qulonglong(positionAge)))
            .arg(radar->uncertainty, 0, 'f', 1);
        else if (age > 0) positionText = "<br>Position unconfirmed; a plotted orbit is based on old telemetry.";
        summary->setText(text + positionText);
        status->setText(text);
        QString details = QString("Last confirmed: turn %1 at (%2, %3)")
                              .arg(static_cast<qulonglong>(
                                  telemetry.observedTurn == 0 ? state_.turn : telemetry.observedTurn))
                              .arg(telemetry.position.x, 0, 'f', 1)
                              .arg(telemetry.position.y, 0, 'f', 1);
        details += "\nCommand reception remains on in Passive and Radio silence. Receiving alone sends no acknowledgement or telemetry."
            "\nIn friendly populated orbit, all modes share full status with the colony; its network forwards it with normal delay.";
        if (radar) details += QString("\nPosition measured by %1 %2 in year %3; received year %4. Uncertainty: %5 ly.\n"
                                     "Remote radar detection sees the hull, without cargo, fuel, mission or radio settings. Friendly orbital exchange is separate.")
            .arg(radar->sourceColony ? "colony" : "fleet")
            .arg(radar->sourceColony ? radar->sourceColony : radar->sourceFleet)
            .arg(qulonglong(radar->lastSeenTurn)).arg(qulonglong(radar->deliveryTurn)).arg(radar->uncertainty, 0, 'f', 1);
        if (connected) {
            details += "\nReal-time relay link; orders arrive immediately."
                "\nFriendly ordinary-scanner fields relay through overlapping circles to a colony. "
                "The fleet can be outside a colony's own radar circle and still have LIVE communications.";
        } else {
            details += QString("\nEstimated current position: (%1, %2)\nEstimated latency at the plotted position: %3 turn%4\n"
                               "Radar position and onboard telemetry have independent ages. "
                               "Command reception is not confirmed until delayed telemetry reports it.")
                           .arg(projected.x, 0, 'f', 1)
                           .arg(projected.y, 0, 'f', 1)
                           .arg(estimatedDelay)
                           .arg(estimatedDelay == 1 ? "" : "s");
        }
        summary->setToolTip(details);
        status->setToolTip(details);
        if (fleetMoveButton_) {
            fleetMoveButton_->setToolTip(connected
                ? "Command will reach this fleet immediately."
                : QString("Estimated delivery from last report: %1 turn%2; receipt is unconfirmed. "
                          "The fleet keeps executing its onboard program until the command arrives.")
                      .arg(estimatedDelay)
                      .arg(estimatedDelay == 1 ? "" : "s"));
        }
    };

    connect(scene_, &QGraphicsScene::selectionChanged, summary, refresh);
    connect(scene_, &QGraphicsScene::changed, summary, [refresh](const QList<QRectF>&) { refresh(); });
    connect(this, &MainWindow::routeProgramContextChanged, summary, [refresh](bool) { refresh(); });
    connect(endTurnButton_, &QPushButton::clicked, summary, [summary, refresh] {
        QTimer::singleShot(0, summary, refresh);
    });
    refresh();
}

} // namespace suns
