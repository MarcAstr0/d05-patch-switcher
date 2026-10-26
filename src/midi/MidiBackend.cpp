#include "midi/MidiBackend.h"

namespace d05 {

QString MidiBackend::ownClientName()
{
    return QStringLiteral("D-05 Patch Switcher");
}

bool MidiBackend::accepts(quint32 mask, const unsigned char *d, std::size_t n)
{
    if (!d || n < 2)
        return false;
    const unsigned status = d[0];
    if (status < 0x80 || status >= 0xF0)  // data byte, or system / realtime
        return false;
    const unsigned ch = status & 0x0Fu;
    switch (status & 0xF0u) {
    case 0xC0:
        return (mask >> ch) & 1u;
    case 0xB0:
        return n >= 3 && (d[1] == 0 || d[1] == 32) && ((mask >> (16 + ch)) & 1u);
    default:
        return false;
    }
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
