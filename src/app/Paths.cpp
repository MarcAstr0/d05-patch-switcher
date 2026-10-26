#include "app/Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>

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

MigrateResult migrateDataDir(const QString &oldDir, const QString &newDir)
{
    if (QFileInfo::exists(newDir) || !QFileInfo(oldDir).isDir())
        return MigrateResult::NotNeeded;
    // Copy into a staging directory next to the target and rename it into place only when every
    // file is there: the final directory appearing is the completion signal.
    const QString staging = newDir + QStringLiteral(".migrating");
    if (QFileInfo::exists(staging))
        QDir(staging).removeRecursively();  // left behind by a crash or an earlier failure
    auto fail = [&] {
        QDir(staging).removeRecursively();
        return MigrateResult::Failed;
    };
    if (!QDir().mkpath(staging))
        return fail();
    const QFileInfoList files =
        QDir(oldDir).entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : files) {
        if (!fi.isFile())
            continue;
        const QString target = staging + QLatin1Char('/') + fi.fileName();
        if (!QFile::copy(fi.absoluteFilePath(), target))
            return fail();
        // A read-only source must not give read-only copies.
        const QFile::Permissions perms = QFile::permissions(target);
        if (!QFile::setPermissions(target, perms | QFile::ReadOwner | QFile::WriteOwner))
            return fail();
    }
    if (!QDir().rename(staging, newDir))
        return fail();
    return MigrateResult::Migrated;
}

MigrateResult migrateLegacyDataDir()
{
    if (!g_override.isEmpty())
        return MigrateResult::NotNeeded;  // --config-dir bypasses the migration
    return migrateDataDir(legacyDataDir(), dataDir());
}

bool useScratchDataDir()
{
    static QTemporaryDir *scratch = nullptr;  // lives until the process ends
    if (!scratch)
        scratch = new QTemporaryDir();
    if (!scratch->isValid())
        return false;
    setOverride(scratch->path());
    return true;
}

QString libraryPath() { return dataDir() + QStringLiteral("/library.json"); }
QString settingsPath() { return dataDir() + QStringLiteral("/settings.ini"); }
QString logoPath() { return dataDir() + QStringLiteral("/logo.png"); }

}  // namespace d05::Paths
