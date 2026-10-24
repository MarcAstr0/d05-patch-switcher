#pragma once

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

    // True when the run needs no window (--list-ports, --send).
    bool headless() const { return listPorts || hasSend; }
};

namespace Exit {
constexpr int Ok = 0;
constexpr int BadAddress = 1;
constexpr int PortNotFound = 2;
constexpr int MidiError = 3;
constexpr int Usage = 64;
}  // namespace Exit

namespace Cli {

// True if `args` (argv as a list, program name first) asks for a headless run.
// Cheap pre-scan so main() can pick QCoreApplication before parsing.
bool wantsHeadless(const QStringList &args);

enum class ParseStatus { Run, ExitNow };

// Parses `args`. ExitNow: print `*message` (stdout when `*exitCode` is 0, else
// stderr) and exit with `*exitCode` (help, version, usage errors).
ParseStatus parse(const QStringList &args, CliOptions *opts, QString *message, int *exitCode);

// --list-ports: both lists on `out`. Exit::Ok or Exit::MidiError (text on `err`).
int runListPorts(MidiBackend &backend, QTextStream &out, QTextStream &err);

// --send: opens the output port (exact, then case-insensitive substring match;
// with the fake backend and no --port, its first port), sends the patch, prints
// the three messages in hex on `out`. Returns an Exit code; diagnostics on `err`.
int runSend(const CliOptions &opts, MidiBackend &backend, QTextStream &out, QTextStream &err);

}  // namespace Cli
}  // namespace d05
