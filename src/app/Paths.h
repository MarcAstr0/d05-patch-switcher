#pragma once

#include <QString>

namespace d05 {

// Where per-user files live. The default is the app-data location
// (marcastr0/boutique-patch-switcher under the platform root of
// QStandardPaths::AppDataLocation; requires organization "marcastr0" and an application name
// to be set on QCoreApplication first); `--config-dir` (and tests) override it.
namespace Paths {

// An empty string clears the override.
void setOverride(const QString &dir);
QString overrideDir();

// The version 1 data directory (marcastr0/d05-patch-switcher), only ever read.
QString legacyDataDir();
// NotNeeded: the new directory exists or there is no old one. Migrated: copied. Failed: the
// copy did not complete; nothing was created under the final name.
enum class MigrateResult { NotNeeded, Migrated, Failed };
// Copies every top-level regular file of oldDir (hidden ones too) into newDir when newDir does
// not exist and oldDir does (SPEC-2 3.5). The copy goes to "<newDir>.migrating" and is renamed to
// newDir only once complete, so newDir existing means done; a stale staging directory is removed
// first. oldDir is never modified.
MigrateResult migrateDataDir(const QString &oldDir, const QString &newDir);
// migrateDataDir(legacyDataDir(), dataDir()), except that it does nothing under --config-dir.
MigrateResult migrateLegacyDataDir();
// Rule after a Failed migration: the session runs against a throwaway directory (this sets the
// override to it), so no save can create the final directory and block the retry on the next
// start. The caller tells the user that changes are not kept. False if no such directory exists.
bool useScratchDataDir();

// The directory holding library.json and settings.ini. Not created here.
QString dataDir();
QString libraryPath();
QString settingsPath();
// The optional owner-supplied wordmark replacement (SPEC FR-14): <data dir>/logo.png.
QString logoPath();

}  // namespace Paths
}  // namespace d05
