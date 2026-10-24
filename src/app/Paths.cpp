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
    // AppDataLocation needs organization and application names (main.cpp sets them
    // before any Paths call): <data>/marcastr0/d05-patch-switcher.
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
}

QString libraryPath() { return dataDir() + QStringLiteral("/library.json"); }
QString settingsPath() { return dataDir() + QStringLiteral("/settings.ini"); }
QString logoPath() { return dataDir() + QStringLiteral("/logo.png"); }

}  // namespace d05::Paths
