#pragma once

#include "app/AppSettings.h"
#include "core/Model.h"

#include <array>

#include <QSize>
#include <QString>
#include <QStringList>

class QTextStream;

namespace d05 {

class MidiBackend;

struct CliOptions {
    bool listPorts = false;
    bool fakeMidi = false;
    bool hasSend = false;
    bool identify = false;     // --identify
    Model device = Model::D05;  // --device; when absent (hasDevice false) the saved current device, else d05
    bool hasDevice = false;
    int engine = 0;            // --engine 60|106 (JU-06A only); 0 = not given
    QString sendAddress;
    QString port;
    QString inPort;
    int channel = 0;  // 0 = not given
    QString configDir;
    QString screenshotFile;   // --screenshot FILE.png (empty = none)
    QString screenshotState;  // see Cli::screenshotStates()
    QSize screenshotSize{960, 560};  // --screenshot-size WxH, at least 800x480 (main window only)

    // True when the run needs no window (--list-ports, --send).
    bool headless() const { return listPorts || identify || hasSend; }
    // True when the run renders the window to a PNG offscreen and exits (needs a QApplication).
    bool wantsScreenshot() const { return !screenshotFile.isEmpty(); }
};

namespace Exit {
constexpr int Ok = 0;
constexpr int BadAddress = 1;
constexpr int PortNotFound = 2;
constexpr int MidiError = 3;
constexpr int EngineMismatch = 5;  // JU-06A address of the other engine; nothing sent
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

// Precedence: help/version/usage errors, then --list-ports, then --identify, then --send.
// --list-ports: both lists on `out`. Exit::Ok or Exit::MidiError (text on `err`).
int runListPorts(MidiBackend &backend, QTextStream &out, QTextStream &err);

// --identify: every output and input port with the model docs/IDENTIFY.md detects and how
// sure it is. Opens and sends nothing. `systemLookup` false skips the Linux USB-serial rule
// (the fake backend's names are not ALSA clients). Exit::Ok or Exit::MidiError.
int runIdentify(MidiBackend &backend, QTextStream &out, QTextStream &err, bool systemLookup = true);

// What a GUI run starts with (FR-15, SPEC-2 3.4): the saved devices, with --port, --in-port and
// --channel overriding the device named by --device (default: the saved current device, else the
// D-05) for this run only. An override, or --device itself, turns that device on for the run; with
// --fake-midi and no --port its output is the fake backend's "Fake D-05 In". Nothing is written:
// overrides are never persisted. `current` is the device the window shows first: --device, else the
// saved current device when it is enabled, else the first enabled one, else the D-05.
struct Startup {
    std::array<AppSettings::DeviceValues, 3> devices;
    Model current = Model::D05;
    bool autoSend = false;
    bool followSwitches = true;
    // The JU-06A's believed engine (--engine, else the saved one) and its remembered programs
    // (-1 = unknown).
    int engine = 60;
    int remembered60 = -1;
    int remembered106 = -1;
};
Startup startup(const AppSettings &saved, const CliOptions &opts);

// The settings this run starts with (FR-15), the D-05 only (version 1 form of startup()): the saved values, with --port, --in-port and
// --channel overriding them for this run only. With --fake-midi and no --port the output
// is the fake backend's "Fake D-05 In" (a saved real port cannot exist there). Nothing is
// written: overrides are never persisted.
AppSettings::Values startupValues(const AppSettings &saved, const CliOptions &opts);

// --send: opens the output port (exact, then case-insensitive substring match;
// with the fake backend and no --port, its first port), sends the patch, prints
// the three messages in hex on `out`. Returns an Exit code; diagnostics on `err`.
// When --port / --channel are absent the saved port and channel (`saved`, may be null)
// are used; the port falls back to the saved one except with --fake-midi. The saved values
// are the device's own (--device, else the saved current device, else the D-05): its saved port,
// channel and, for the JU-06A, engine. A device with no saved channel needs --channel (exit 64; the
// D-05 and --fake-midi default to channel 1), and a JU-06A with no --engine and no saved engine
// exits 1 with a message (the unit cannot be asked).
// Address forms per model: P3-45 | 4-5 | 106:4-5 (bare 4-5 takes --engine). A JU-06A address of
// the other engine than --engine sends nothing and returns Exit::EngineMismatch.
int runSend(const CliOptions &opts, MidiBackend &backend, QTextStream &out, QTextStream &err,
            const AppSettings *saved = nullptr);

}  // namespace Cli
}  // namespace d05
