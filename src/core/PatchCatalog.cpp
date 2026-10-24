#include "core/PatchCatalog.h"

#include <QFile>
#include <QTextStream>

namespace d05 {

PatchCatalog::PatchCatalog(const QString &tsvPath)
{
    QFile f(tsvPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("PatchCatalog: cannot open %s", qPrintable(tsvPath));
        return;
    }
    QTextStream ts(&f);
    const QString header = ts.readLine();
    if (header != QStringLiteral("group\tbank\tnumber\tname\tmode"))
        qWarning("PatchCatalog: unexpected header in %s", qPrintable(tsvPath));
    int lineNo = 1;
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        ++lineNo;
        if (line.isEmpty())
            continue;
        const auto skip = [&](const char *why) {
            qWarning("PatchCatalog: line %d skipped: %s", lineNo, why);
        };
        const QStringList c = line.split(QLatin1Char('\t'));
        if (c.size() < 5) {
            skip("too few columns");
            continue;
        }
        if (c[0].size() != 2 || c[0][0] != QLatin1Char('P') || c[0][1] < QLatin1Char('1')
            || c[0][1] > QLatin1Char('6')) {
            skip("not a preset group P1..P6");
            continue;
        }
        const auto addr = PatchAddress::fromParts(7 + c[0][1].digitValue(), c[1].toInt(), c[2].toInt());
        if (!addr) {
            skip("bank or number out of range");
            continue;
        }
        if (c[3].isEmpty()) {
            skip("empty name");
            continue;
        }
        if (c[4].size() != 1 || !QStringLiteral("WDS").contains(c[4][0])) {
            skip("mode is not W, D or S");
            continue;
        }
        const int i = addr->catalogueIndex();
        if (!m_presetNames[i].isEmpty()) {
            qWarning("PatchCatalog: line %d skipped: duplicate address %s", lineNo,
                     qPrintable(addr->toString()));
            continue;
        }
        m_presetNames[i] = c[3];
        m_modes[i] = c[4][0];
        ++m_presetCount;
    }
}

QString PatchCatalog::name(const PatchAddress &a) const
{
    if (a.isPreset())
        return m_presetNames[a.catalogueIndex()];
    return m_userNames.value(a.catalogueIndex());
}

QChar PatchCatalog::mode(const PatchAddress &a) const
{
    return a.isPreset() ? m_modes[a.catalogueIndex()] : QChar();
}

QString PatchCatalog::displayName(const PatchAddress &a) const
{
    const QString n = name(a);
    return n.isEmpty() ? a.toString() : n;
}

bool PatchCatalog::setUserName(const PatchAddress &a, const QString &name)
{
    if (!a.isUser())
        return false;
    QString clean;
    for (const QChar ch : name) {
        const char16_t u = ch.unicode();
        if (u == u'\t' || u == u'\r' || u == u'\n' || u == u'\v' || u == u'\f')
            clean.append(QLatin1Char(' '));
        else if (u >= 0x20 && u <= 0x7E)
            clean.append(ch);
    }
    clean = clean.simplified().left(MaxUserNameLength).trimmed();
    if (clean.isEmpty())
        m_userNames.remove(a.catalogueIndex());
    else
        m_userNames.insert(a.catalogueIndex(), clean);
    return true;
}

PatchAddress PatchCatalog::next(const PatchAddress &a)
{
    return PatchAddress::fromCatalogueIndex(a.catalogueIndex() + 1).value_or(a);
}

PatchAddress PatchCatalog::prev(const PatchAddress &a)
{
    return PatchAddress::fromCatalogueIndex(a.catalogueIndex() - 1).value_or(a);
}

}  // namespace d05
