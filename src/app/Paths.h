#pragma once

#include <QString>

namespace d05 {

// Where per-user files live. The default is the app-data location
// (<GenericData>/marcastr0/d05-patch-switcher); `--config-dir` (and tests) override it.
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
