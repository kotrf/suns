#include "main_window.hpp"
#include "technology_catalog_dialog.hpp"
#include <QTimer>

namespace suns {
void MainWindow::openTechnologyCatalog()
{
    if(technologyCatalog_) {
        technologyCatalog_->refresh(make_player_view(state_,pendingOrders_.player).state,pendingOrders_.player);
        technologyCatalog_->setWindowState(technologyCatalog_->windowState() & ~Qt::WindowMinimized);
        technologyCatalog_->show();technologyCatalog_->raise();technologyCatalog_->activateWindow();
        return;
    }
    auto* dialog=new TechnologyCatalogDialog(make_player_view(state_,pendingOrders_.player).state,pendingOrders_.player,this);
    technologyCatalog_=dialog;dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setModal(false);
    auto* refresh=new QTimer(dialog);refresh->setSingleShot(true);
    connect(refresh,&QTimer::timeout,dialog,[this,dialog] {
        dialog->refresh(make_player_view(state_,pendingOrders_.player).state,pendingOrders_.player);
    });
    connect(this,&MainWindow::controlsChanged,dialog,[dialog,refresh] {
        if(dialog->isVisible() && !refresh->isActive())refresh->start(0);
    });
    dialog->show();
}
} // namespace suns
