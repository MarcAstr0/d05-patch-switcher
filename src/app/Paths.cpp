#include "app/Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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

namespace {
// The platform's per-app data directory is <root>/marcastr0/<application name>; the application
// name is still the old one until the rename task, so both directories are derived from the
// parent (SPEC-2 3.5).
QString appDataParent()
{
    return QFileInfo(QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))).path();
}
}  // namespace

QString legacyDataDir() { return appDataParent() + QStringLiteral("/d05-patch-switcher"); }

QString dataDir()
{
    if (!g_override.isEmpty())
        return g_override;
    return appDataParent() + QStringLiteral("/boutique-patch-switcher");
}

bool migrateDataDir(const QString &oldDir, const QString &newDir)
{
    if (QFileInfo::exists(newDir) || !QFileInfo(oldDir).isDir())
        return false;
    if (!QDir().mkpath(newDir))
        return false;
    QStringList copied;
    bool ok = true;
    const QFileInfoList files = QDir(oldDir).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : files) {
        const QString target = newDir + QLatin1Char('/') + fi.fileName();
        if (QFile::copy(fi.absoluteFilePath(), target))
            copied << target;
        else {
            ok = false;
            break;
        }
    }
    if (!ok) {
        // Leave nothing half-done, so the next start tries again. Only the new directory is touched.
        for (const QString &t : copied)
            QFile::remove(t);
        QDir().rmdir(newDir);
    }
    return ok;
}

bool migrateLegacyDataDir()
{
    if (!g_override.isEmpty())
        return false;  // --config-dir bypasses the migration
    return migrateDataDir(legacyDataDir(), dataDir());
}

QString libraryPath() { return dataDir() + QStringLiteral("/library.json"); }
QString settingsPath() { return dataDir() + QStringLiteral("/settings.ini"); }
QString logoPath() { return dataDir() + QStringLiteral("/logo.png"); }

}  // namespace d05::Paths
