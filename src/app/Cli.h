#pragma once

#include "app/AppSettings.h"

#include <QString>
#include <QStringList>

class QTextStream;

namespace d05 {

class MidiBackend;

struct CliOptions {
    bool listPorts = false;
    bool fakeMidi = false;
    bool hasSend = false;
    QString sendAddress;
    QString port;
    QString inPort;
    int channel = 0;  // 0 = not given
    QString configDir;
    QString screenshotFile;   // --screenshot FILE.png (empty = none)
    QString screenshotState;  // default | search | favorites | recents | disconnected | settings

    // True when the run needs no window (--list-ports, --send).
    bool headless() const { return listPorts || hasSend; }
    // True when the run renders the window to a PNG offscreen and exits (needs a QApplication).
    bool wantsScreenshot() const { return !screenshotFile.isEmpty(); }
};

namespace Exit {
constexpr int Ok = 0;
constexpr int BadAddress = 1;
constexpr int PortNotFound = 2;
constexpr int MidiError = 3;
constexpr int Usage = 64;
}  // namespace Exit

namespace Cli {

// The values --screenshot-state accepts.
QStringList screenshotStates();

enum class ParseStatus { Run, ExitNow };

// Parses `args` (argv as a list, program name first). Needs no application
// object, so help, version and usage errors work with no display. ExitNow: print `*message` (stdout when `*exitCode` is 0, else
// stderr) and exit with `*exitCode` (help, version, usage errors).
ParseStatus parse(const QStringList &args, CliOptions *opts, QString *message, int *exitCode);

// Precedence: help/version/usage errors, then --list-ports (wins over --send), then --send.
// --list-ports: both lists on `out`. Exit::Ok or Exit::MidiError (text on `err`).
int runListPorts(MidiBackend &backend, QTextStream &out, QTextStream &err);

// The settings this run starts with (FR-15): the saved values, with --port, --in-port and
// --channel overriding them for this run only. With --fake-midi and no --port the output
// is the fake backend's "Fake D-05 In" (a saved real port cannot exist there). Nothing is
// written: overrides are never persisted.
AppSettings::Values startupValues(const AppSettings &saved, const CliOptions &opts);

// --send: opens the output port (exact, then case-insensitive substring match;
// with the fake backend and no --port, its first port), sends the patch, prints
// the three messages in hex on `out`. Returns an Exit code; diagnostics on `err`.
// When --port / --channel are absent the saved port and channel (`saved`, may be null)
// are used; the port falls back to the saved one except with --fake-midi.
int runSend(const CliOptions &opts, MidiBackend &backend, QTextStream &out, QTextStream &err,
            const AppSettings *saved = nullptr);

}  // namespace Cli
}  // namespace d05
