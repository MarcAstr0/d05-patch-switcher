#include "core/SearchQuery.h"

#include <QStringList>

#include <algorithm>

namespace d05 {

namespace {

// Folds one UTF-16 unit: lowercase, first character of its canonical decomposition
// (so accents drop), combining marks vanish (returns a null QChar).
QChar foldChar(QChar c)
{
    const char16_t u = c.unicode();
    if (u < 0x80)
        return (u >= 'A' && u <= 'Z') ? QChar(char16_t(u + 32)) : c;
    if (c.isSurrogate())
        return c;
    const QChar::Category cat = c.category();
    if (cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining || cat == QChar::Mark_Enclosing)
        return QChar();
    const QString d = QString(c).normalized(QString::NormalizationForm_D);
    return d.isEmpty() ? QChar() : d.at(0).toCaseFolded();
}

QString foldString(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (QChar c : s) {
        const QChar f = foldChar(c);
        if (!f.isNull())
            out.append(f);
    }
    return out;
}

bool isAscii(const QString &s)
{
    for (QChar c : s)
        if (c.unicode() >= 0x80)
            return false;
    return true;
}

int digit(QChar c) { return (c.unicode() >= '0' && c.unicode() <= '9') ? c.unicode() - '0' : -1; }

bool groupInRange(bool user, int g) { return g >= 1 && g <= (user ? 8 : 6); }

}  // namespace

SearchQuery::SearchQuery(const QString &text)
{
    const QStringList parts = text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &raw : parts) {
        Token t;
        t.kind = Kind::Text;

        // Shape: [letter] g [-] rest, where the '-' is only valid right after g.
        int i = 0;
        bool hasLetter = false;
        bool user = false;
        const QChar first = raw.at(0).toLower();
        if (first == QLatin1Char('u') || first == QLatin1Char('p')) {
            hasLetter = true;
            user = first == QLatin1Char('u');
            i = 1;
        }
        const int g = i < raw.size() ? digit(raw.at(i)) : -1;
        if (g >= 0) {
            ++i;
            bool dash = false;
            if (i < raw.size() && raw.at(i) == QLatin1Char('-')) {
                dash = true;
                ++i;
            }
            int rest[2] = {-1, -1};
            int nrest = 0;
            bool ok = true;
            for (; i < raw.size(); ++i) {
                const int d = digit(raw.at(i));
                if (d < 1 || d > 8 || nrest == 2) {
                    ok = false;
                    break;
                }
                rest[nrest++] = d;
            }
            if (ok && groupInRange(user, g)) {
                const int gi = user ? g - 1 : g + 7;
                if (nrest == 0 && !dash && hasLetter) {
                    t.kind = Kind::Group;
                    t.group = gi;
                } else if (nrest == 1 && hasLetter) {
                    t.kind = Kind::GroupBank;
                    t.group = gi;
                    t.bank = rest[0];
                } else if (nrest == 2) {
                    t.kind = Kind::Exact;
                    t.group = gi;
                    t.bank = rest[0];
                    t.number = rest[1];
                }
            }
        }
        if (t.kind == Kind::Text)
            t.folded = foldString(raw);
        if (t.kind == Kind::Text && t.folded.isEmpty())
            continue;  // token made only of combining marks
        m_tokens.append(t);
    }
}

bool SearchQuery::matches(const PatchAddress &a, const QString &name) const
{
    return matches(a, name, nullptr);
}

bool SearchQuery::matches(const PatchAddress &a, const QString &name, QVector<Range> *ranges) const
{
    if (ranges)
        ranges->clear();

    // Address tokens first: cheap integer tests, no string work.
    bool needText = false;
    for (const Token &t : m_tokens) {
        switch (t.kind) {
        case Kind::Group:
            if (a.group() != t.group) return false;
            break;
        case Kind::GroupBank:
            if (a.group() != t.group || a.bank() != t.bank) return false;
            break;
        case Kind::Exact:
            if (a.group() != t.group || a.bank() != t.bank || a.number() != t.number) return false;
            break;
        case Kind::Text:
            needText = true;
            break;
        }
    }
    if (!needText)
        return true;

    const bool ascii = isAscii(name);
    QString foldedName;
    QVector<int> map;  // folded index -> original index (non-ASCII names only)
    if (!ascii) {
        foldedName.reserve(name.size());
        map.reserve(name.size());
        for (int i = 0; i < name.size(); ++i) {
            const QChar f = foldChar(name.at(i));
            if (!f.isNull()) {
                foldedName.append(f);
                map.append(i);
            }
        }
    }

    QVector<Range> found;
    for (const Token &t : m_tokens) {
        if (t.kind != Kind::Text)
            continue;
        int start, length;
        if (ascii) {
            start = name.indexOf(t.folded, 0, Qt::CaseInsensitive);
            if (start < 0) return false;
            length = t.folded.size();
        } else {
            const int fi = foldedName.indexOf(t.folded);
            if (fi < 0) return false;
            start = map.at(fi);
            int end = map.at(fi + t.folded.size() - 1) + 1;
            while (end < name.size() && foldChar(name.at(end)).isNull())
                ++end;  // keep trailing combining marks inside the range
            length = end - start;
        }
        if (ranges)
            found.append({start, length});
    }

    if (ranges && !found.isEmpty()) {
        std::sort(found.begin(), found.end(), [](const Range &x, const Range &y) { return x.start < y.start; });
        Range cur = found.first();
        for (int i = 1; i < found.size(); ++i) {
            const Range &r = found.at(i);
            if (r.start <= cur.start + cur.length)
                cur.length = std::max(cur.length, r.start + r.length - cur.start);
            else {
                ranges->append(cur);
                cur = r;
            }
        }
        ranges->append(cur);
    }
    return true;
}

}  // namespace d05
