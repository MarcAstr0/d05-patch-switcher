#include "midi/MidiBackend.h"

namespace d05 {

QString MidiBackend::ownClientName()
{
    return QStringLiteral("D-05 Patch Switcher");
}

bool MidiBackend::isOwnPort(const QString &name)
{
    const int colon = name.indexOf(QLatin1Char(':'));
    if (colon < 0)
        return false;
    if (name.left(colon) == ownClientName())
        return true;
    // Port names passed to RtMidi's openPort (also what an older build under the default
    // "RtMidi Output Client" name looked like).
    const QString port = name.mid(colon + 1);
    return port.startsWith(QLatin1String("d05-patch-switcher out"))
           || port.startsWith(QLatin1String("d05-patch-switcher in"));
}

QStringList MidiBackend::withoutOwnPorts(const QStringList &names)
{
    QStringList out;
    for (const QString &n : names)
        if (!isOwnPort(n))
            out << n;
    return out;
}

}  // namespace d05
