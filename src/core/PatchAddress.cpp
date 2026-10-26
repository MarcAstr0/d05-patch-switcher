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

std::optional<PatchAddress> PatchAddress::fromBankNumber(Model model, int bank, int number, int engine)
{
    if (bank < 1 || bank > 8 || number < 1 || number > 8)
        return std::nullopt;
    PatchAddress a(0, bank, number);
    a.m_model = model;
    switch (model) {
    case Model::D05:
        return std::nullopt;  // use fromParts
    case Model::JP08:
        return a;
    case Model::JU06A:
        if (engine != 60 && engine != 106)
            return std::nullopt;
        a.m_engine = engine;
        return a;
    }
    return std::nullopt;
}

std::optional<PatchAddress> PatchAddress::fromCatalogueIndex(Model model, int index)
{
    switch (model) {
    case Model::D05:
        return fromCatalogueIndex(index);
    case Model::JP08:
        if (index < 0 || index >= 64)
            return std::nullopt;
        return fromBankNumber(model, index / 8 + 1, index % 8 + 1);
    case Model::JU06A:
        if (index < 0 || index >= 128)
            return std::nullopt;
        return fromBankNumber(model, (index % 64) / 8 + 1, index % 8 + 1, index < 64 ? 60 : 106);
    }
    return std::nullopt;
}

std::optional<PatchAddress> PatchAddress::parse(const QString &text, Model model,
                                                std::optional<int> currentEngine)
{
    if (model == Model::D05)
        return parse(text);
    QString s = text.trimmed();
    int engine = 0;
    if (model == Model::JU06A) {
        const int colon = s.indexOf(QLatin1Char(':'));
        if (colon >= 0) {
            const QString e = s.left(colon);
            if (e == QLatin1String("60"))
                engine = 60;
            else if (e == QLatin1String("106"))
                engine = 106;
            else
                return std::nullopt;
            s = s.mid(colon + 1);
        } else if (currentEngine && (*currentEngine == 60 || *currentEngine == 106)) {
            engine = *currentEngine;
        } else {
            return std::nullopt;
        }
    }
    QString digits;
    if (s.size() == 3 && s[1] == QLatin1Char('-'))
        digits = s.left(1) + s.mid(2);
    else if (s.size() == 2)
        digits = s;
    else
        return std::nullopt;
    for (const QChar c : digits)
        if (c < QLatin1Char('1') || c > QLatin1Char('8'))
            return std::nullopt;
    return fromBankNumber(model, digits[0].unicode() - '0', digits[1].unicode() - '0', engine);
}

QList<QByteArray> PatchAddress::midiMessages(int channel, std::optional<int> deviceEngine) const
{
    if (channel < 1 || channel > 16)
        return {};
    const char ch = static_cast<char>(channel - 1);
    auto msg = [](int status, int a, int b = -1) {
        QByteArray m;
        m.append(static_cast<char>(status));
        m.append(static_cast<char>(a));
        if (b >= 0)
            m.append(static_cast<char>(b));
        return m;
    };
    switch (m_model) {
    case Model::D05:
        return {msg(0xB0 | ch, 0x00, BankSelectMsb), msg(0xB0 | ch, 0x20, m_group),
                msg(0xC0 | ch, program())};
    case Model::JP08:
        return {msg(0xC0 | ch, program())};
    case Model::JU06A:
        if (!deviceEngine || *deviceEngine != m_engine)
            return {};
        return {msg(0xC0 | ch, program())};
    }
    return {};
}

PatchAddress::MidiTriplet PatchAddress::midiTriplet() const
{
    return {BankSelectMsb, m_group, (m_bank - 1) * 8 + (m_number - 1)};
}

QString PatchAddress::groupLabel() const
{
    if (m_model != Model::D05)
        return QString();
    return QStringLiteral("%1%2").arg(isUser() ? QLatin1Char('U') : QLatin1Char('P')).arg(groupNumber());
}

QString PatchAddress::toString() const
{
    if (m_model == Model::JP08)
        return QStringLiteral("%1-%2").arg(m_bank).arg(m_number);
    if (m_model == Model::JU06A)
        return QStringLiteral("%1:%2-%3").arg(m_engine).arg(m_bank).arg(m_number);
    return QStringLiteral("%1-%2%3").arg(groupLabel()).arg(m_bank).arg(m_number);
}

}  // namespace d05
