#pragma once

#include "core/Model.h"

#include <QString>
#include <QStringList>

#include <optional>

namespace d05 {

// Which Boutique is on a MIDI port (docs/IDENTIFY.md, SPEC-2 FR-18). Pure lookups: nothing here
// opens a port, sends, or runs on a timer, and nothing writes to stderr.
namespace Identify {

enum class Confidence { None, Ambiguous, Likely, Certain };

struct Result {
    std::optional<Model> model;  // the best candidate; empty when Confidence::None
    Confidence confidence = Confidence::None;
    bool firstGeneration = false;  // "Boutiq" only: JP-08, JU-06 or JX-03 (JP-08 is the candidate)

    bool operator==(const Result &o) const
    {
        return model == o.model && confidence == o.confidence && firstGeneration == o.firstGeneration;
    }
};

// Rule 1, by port name (all platforms): case-insensitive, longest match first.
Result byPortName(const QString &portName);

// Rule 2, the pure part: the card's usbid line ("0582:01b5") and the USB serial. A Roland serial
// whose prefix is BQJP, BQJU or BQ_D gives Certain; anything else (other vendor, other BQ.. prefix,
// empty text) gives an empty optional so the caller keeps rule 1's answer.
std::optional<Result> bySerial(const QString &usbId, const QString &serial);

// Rule 2 with the files read from `procRoot` ("/proc") and `sysRoot` ("/sys"): usbid at
// procRoot/asound/card<N>/usbid, serial at sysRoot/class/sound/card<N>/device/../serial. Any file
// missing or unreadable gives an empty optional. Injectable so tests use a temp directory.
std::optional<Result> byCard(int card, const QString &procRoot, const QString &sysRoot);

// The ALSA client number in a Linux RtMidi port name ("Boutiq:Boutiq MIDI 1 28:0" gives 28);
// empty when the name has no trailing "client:port".
std::optional<int> alsaClientOf(const QString &portName);

// Rules 1 and 2 for each name. Rule 2 runs on Linux only (and only when `useSystemLookup`), reads
// /proc and /sys and asks ALSA for the client's card; every failure falls back to rule 1 silently.
QList<Result> identifyPorts(const QStringList &portNames, bool useSystemLookup = true);

// "certain", "likely", "ambiguous", "none".
QString confidenceText(Confidence c);

}  // namespace Identify
}  // namespace d05
