#pragma once

#include <QString>

namespace d05 {

// Where per-user files live. The default is the app-data location
// (QStandardPaths::AppDataLocation; requires organization "marcastr0" and
// application "d05-patch-switcher" to be set on QCoreApplication first); `--config-dir` (and tests) override it.
namespace Paths {

// An empty string clears the override.
void setOverride(const QString &dir);
QString overrideDir();

// The directory holding library.json and settings.ini. Not created here.
QString dataDir();
QString libraryPath();
QString settingsPath();

}  // namespace Paths
}  // namespace d05
