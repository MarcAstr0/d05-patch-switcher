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
// Copies the regular files of oldDir into newDir when newDir does not exist and oldDir does
// (SPEC-2 3.5). oldDir is never modified. Returns true when it copied. A failed copy removes
// what it made, so the next start retries.
bool migrateDataDir(const QString &oldDir, const QString &newDir);
// migrateDataDir(legacyDataDir(), dataDir()), except that it does nothing under --config-dir.
bool migrateLegacyDataDir();

// The directory holding library.json and settings.ini. Not created here.
QString dataDir();
QString libraryPath();
QString settingsPath();
// The optional owner-supplied wordmark replacement (SPEC FR-14): <data dir>/logo.png.
QString logoPath();

}  // namespace Paths
}  // namespace d05
