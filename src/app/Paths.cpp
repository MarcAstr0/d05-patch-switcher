#include "app/Paths.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <functional>
#include <utility>

namespace d05::Paths {

namespace {
QString g_override;
std::function<void()> g_beforeRename;
int g_staleStagingSeconds = 600;
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
    // file is there: the final directory appearing is the completion signal. The staging name is
    // per process, so two first starts running at once do not trample each other.
    const QString prefix = QFileInfo(newDir).fileName() + QStringLiteral(".migrating");
    const QString staging =
        newDir + QStringLiteral(".migrating-") + QString::number(QCoreApplication::applicationPid());
    // Best effort: staging directories left behind by a crash or an earlier failure (the old
    // un-suffixed name, or another pid's directory that has not been touched for a while; a
    // recent one may belong to a process that is copying right now).
    const QFileInfo parent(newDir);
    const QDir parentDir(parent.absolutePath());
    const QFileInfoList leftovers = parentDir.entryInfoList(
        QStringList{prefix, prefix + QStringLiteral("-*")}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : leftovers) {
        if (fi.absoluteFilePath() == QFileInfo(staging).absoluteFilePath())
            continue;
        const bool legacyName = fi.fileName() == prefix;
        if (legacyName || fi.lastModified().secsTo(QDateTime::currentDateTime()) >= g_staleStagingSeconds)
            QDir(fi.absoluteFilePath()).removeRecursively();
    }
    if (QFileInfo::exists(staging))
        QDir(staging).removeRecursively();  // ours from an earlier run with a recycled pid
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
    if (g_beforeRename)
        g_beforeRename();
    if (!QDir().rename(staging, newDir)) {
        // Another process finished first: newDir exists, so the migration is done.
        const bool lost = QFileInfo::exists(newDir);
        QDir(staging).removeRecursively();
        return lost ? MigrateResult::NotNeeded : MigrateResult::Failed;
    }
    return MigrateResult::Migrated;
}

void setMigrationTestHooks(std::function<void()> beforeRename, int staleStagingSeconds)
{
    g_beforeRename = std::move(beforeRename);
    g_staleStagingSeconds = staleStagingSeconds;
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
