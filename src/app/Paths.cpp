#include "app/Paths.h"

#include <QDir>
#include <QStandardPaths>

namespace d05::Paths {

namespace {
QString g_override;
}

void setOverride(const QString &dir)
{
    g_override = dir.isEmpty() ? QString() : QDir::cleanPath(QDir(dir).absolutePath());
}

QString overrideDir() { return g_override; }

QString dataDir()
{
    if (!g_override.isEmpty())
        return g_override;
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty())
        base = QDir::homePath();
    return QDir::cleanPath(base + QStringLiteral("/marcastr0/d05-patch-switcher"));
}

QString libraryPath() { return dataDir() + QStringLiteral("/library.json"); }
QString settingsPath() { return dataDir() + QStringLiteral("/settings.ini"); }

}  // namespace d05::Paths
