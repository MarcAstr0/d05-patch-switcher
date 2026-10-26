#include "core/PatchCatalog.h"

#include <QFile>
#include <QTextStream>

namespace d05 {

namespace {
QString resourcePath(Model m)
{
    switch (m) {
    case Model::JP08: return QStringLiteral(":/data/jp08-patches.tsv");
    case Model::JU06A: return QStringLiteral(":/data/ju06a-patches.tsv");
    case Model::D05: break;
    }
    return QStringLiteral(":/data/d05-presets.tsv");
}
}  // namespace

int PatchCatalog::expectedFactoryCount(Model m)
{
    return m == Model::D05 ? 6 * 64 : modelInfo(m).catalogueSize;
}

PatchCatalog::PatchCatalog(const QString &tsvPath)
{
    load(tsvPath);
}

PatchCatalog::PatchCatalog(Model model) : m_model(model)
{
    load(resourcePath(model));
}

PatchCatalog::PatchCatalog(Model model, const QString &tsvPath) : m_model(model)
{
    load(tsvPath);
}

void PatchCatalog::load(const QString &tsvPath)
{
    QFile f(tsvPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("PatchCatalog: cannot open %s", qPrintable(tsvPath));
        return;
    }
    QTextStream ts(&f);
    const QString header = ts.readLine();
    QString wantHeader;
    int cols = 5;
    switch (m_model) {
    case Model::D05: wantHeader = QStringLiteral("group\tbank\tnumber\tname\tmode"); break;
    case Model::JP08: wantHeader = QStringLiteral("bank\tnumber\tname"); cols = 3; break;
    case Model::JU06A: wantHeader = QStringLiteral("engine\tbank\tnumber\tname"); cols = 4; break;
    }
    if (header != wantHeader)
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
        if (c.size() < cols) {
            skip("too few columns");
            continue;
        }
        std::optional<PatchAddress> addr;
        QString nm;
        QChar md;
        if (m_model == Model::D05) {
            if (c[0].size() != 2 || c[0][0] != QLatin1Char('P') || c[0][1] < QLatin1Char('1')
                || c[0][1] > QLatin1Char('6')) {
                skip("not a preset group P1..P6");
                continue;
            }
            addr = PatchAddress::fromParts(7 + c[0][1].digitValue(), c[1].toInt(), c[2].toInt());
            nm = c[3];
            if (c[4].size() != 1 || !QStringLiteral("WDS").contains(c[4][0])) {
                skip("mode is not W, D or S");
                continue;
            }
            md = c[4][0];
        } else if (m_model == Model::JP08) {
            addr = PatchAddress::fromBankNumber(m_model, c[0].toInt(), c[1].toInt());
            nm = c[2];
        } else {
            const int engine = c[0].toInt();
            if (engine != 60 && engine != 106) {
                skip("engine is not 60 or 106");
                continue;
            }
            addr = PatchAddress::fromBankNumber(m_model, c[1].toInt(), c[2].toInt(), engine);
            nm = c[3];
        }
        if (!addr) {
            skip("bank or number out of range");
            continue;
        }
        if (nm.isEmpty()) {
            skip("empty name");
            continue;
        }
        const int i = addr->catalogueIndex();
        if (!m_presetNames[i].isEmpty()) {
            qWarning("PatchCatalog: line %d skipped: duplicate address %s", lineNo,
                     qPrintable(addr->toString()));
            continue;
        }
        m_presetNames[i] = nm;
        m_modes[i] = md;
        ++m_presetCount;
    }
}

QString PatchCatalog::factoryName(const PatchAddress &a) const
{
    if (a.model() != m_model)
        return {};
    if (m_model == Model::D05 && !a.isPreset())
        return {};
    return m_presetNames[a.catalogueIndex()];
}

QString PatchCatalog::userName(const PatchAddress &a) const
{
    return a.model() == m_model ? m_userNames.value(a.catalogueIndex()) : QString();
}

QString PatchCatalog::name(const PatchAddress &a) const
{
    const QString u = userName(a);
    return u.isEmpty() ? factoryName(a) : u;
}

QString PatchCatalog::tooltipName(const PatchAddress &a) const
{
    return userName(a).isEmpty() ? QString() : factoryName(a);
}

QChar PatchCatalog::mode(const PatchAddress &a) const
{
    return (m_model == Model::D05 && a.model() == Model::D05 && a.isPreset())
        ? m_modes[a.catalogueIndex()]
        : QChar();
}

QString PatchCatalog::displayName(const PatchAddress &a) const
{
    const QString n = name(a);
    return n.isEmpty() ? a.toString() : n;
}

bool PatchCatalog::setUserName(const PatchAddress &a, const QString &name)
{
    if (a.model() != m_model || (m_model == Model::D05 && !a.isUser()))
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

PatchAddress PatchCatalog::step(const PatchAddress &a, int delta)
{
    const int i = a.catalogueIndex();
    const int j = i + delta;
    if (a.model() == Model::JU06A && (j < 0 || j / 64 != i / 64))
        return a;  // never crosses engines
    return PatchAddress::fromCatalogueIndex(a.model(), j).value_or(a);
}

PatchAddress PatchCatalog::next(const PatchAddress &a)
{
    return step(a, 1);
}

PatchAddress PatchCatalog::prev(const PatchAddress &a)
{
    return step(a, -1);
}

}  // namespace d05
