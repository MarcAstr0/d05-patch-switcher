#include "core/PatchAddress.h"

namespace d05 {

std::optional<PatchAddress> PatchAddress::fromParts(int group, int bank, int number)
{
    if (group < 0 || group >= GroupCount || bank < 1 || bank > 8 || number < 1 || number > 8)
        return std::nullopt;
    return PatchAddress(group, bank, number);
}

std::optional<PatchAddress> PatchAddress::fromCatalogueIndex(int index)
{
    if (index < 0 || index >= CatalogueSize)
        return std::nullopt;
    return PatchAddress(index / 64, (index % 64) / 8 + 1, index % 8 + 1);
}

std::optional<PatchAddress> PatchAddress::parse(const QString &text)
{
    const QString s = text.trimmed();
    int pos = 0;
    bool user = false;
    if (!s.isEmpty() && (s[0] == QLatin1Char('U') || s[0] == QLatin1Char('u'))) {
        user = true;
        pos = 1;
    } else if (!s.isEmpty() && (s[0] == QLatin1Char('P') || s[0] == QLatin1Char('p'))) {
        pos = 1;
    }
    // Remaining: <g>[-]<b><n>, ASCII digits only.
    const QString rest = s.mid(pos);
    QString digits;
    if (rest.size() == 4 && rest[1] == QLatin1Char('-'))
        digits = rest.left(1) + rest.mid(2);
    else if (rest.size() == 3)
        digits = rest;
    else
        return std::nullopt;
    int v[3];
    for (int i = 0; i < 3; ++i) {
        const QChar c = digits[i];
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            return std::nullopt;
        v[i] = c.unicode() - '0';
    }
    const int g = v[0];
    if (g < 1 || g > (user ? 8 : 6))
        return std::nullopt;
    return fromParts(user ? g - 1 : g + 7, v[1], v[2]);
}

PatchAddress::MidiTriplet PatchAddress::midiTriplet() const
{
    return {BankSelectMsb, m_group, (m_bank - 1) * 8 + (m_number - 1)};
}

QString PatchAddress::groupLabel() const
{
    return QStringLiteral("%1%2").arg(isUser() ? QLatin1Char('U') : QLatin1Char('P')).arg(groupNumber());
}

QString PatchAddress::toString() const
{
    return QStringLiteral("%1-%2%3").arg(groupLabel()).arg(m_bank).arg(m_number);
}

}  // namespace d05
