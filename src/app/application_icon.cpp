#include "application_icon.hpp"

#include <QResource>

// The reference keeps the generated resource object in the static UI library.
static void initializeSunsIcons()
{
    Q_INIT_RESOURCE(suns_icons);
}

namespace suns {
QIcon applicationIcon()
{
    static const QIcon icon = [] {
        initializeSunsIcons();
        QIcon result;
        for (const int size : {16, 24, 32, 48, 64, 128, 256})
            result.addFile(QString(":/suns/icons/suns-%1.png").arg(size), QSize(size, size));
        return result;
    }();
    return icon;
}
}
