#include "core/SearchQuery.h"

#include <QStringList>

#include <algorithm>

namespace d05 {

namespace {

// Folds one UTF-16 unit: lowercase, first character of its canonical decomposition
// (so accents drop), combining marks vanish (returns a null QChar).
// apostrophes: JP-08 / JU-06A only, U+2019 compares equal to '.
QChar foldChar(QChar c, bool apostrophes = false)
{
    const char16_t u = c.unicode();
    if (apostrophes && u == 0x2019)
        return QLatin1Char('\'');
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

QString foldString(const QString &s, bool apostrophes = false)
{
    QString out;
    out.reserve(s.size());
    for (QChar c : s) {
        const QChar f = foldChar(c, apostrophes);
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

SearchQuery::SearchQuery(const QString &text, Model model)
{
    if (model == Model::D05) {
        *this = SearchQuery(text);
        return;
    }
    m_model = model;
    parseSmall(text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts), model == Model::JU06A);
}

void SearchQuery::parseSmall(const QStringList &parts, bool ju06a)
{
    for (const QString &raw : parts) {
        Token t;
        t.kind = Kind::Text;
        QString rest = raw;
        int engine = 0;
        if (ju06a) {
            if (raw == QLatin1String("60") || raw == QLatin1String("106")) {
                t.kind = Kind::Engine;
                t.engine = raw.toInt();
            } else if (raw.startsWith(QLatin1String("60:")) || raw.startsWith(QLatin1String("106:"))) {
                const int colon = raw.indexOf(QLatin1Char(':'));
                engine = raw.left(colon).toInt();
                rest = raw.mid(colon + 1);
            }
        }
        if (t.kind == Kind::Text) {
            // bank: d; address: dd or d-d. Digits 1..8 only.
            auto d = [&](int i) { return i < rest.size() ? digit(rest.at(i)) : -1; };
            auto ok = [](int v) { return v >= 1 && v <= 8; };
            if (rest.size() == 1 && ok(d(0))) {
                t.kind = Kind::Bank64;
                t.bank = d(0);
                t.engine = engine;
            } else if ((rest.size() == 2 && ok(d(0)) && ok(d(1)))
                       || (rest.size() == 3 && rest.at(1) == QLatin1Char('-') && ok(d(0)) && ok(d(2)))) {
                t.kind = Kind::Exact64;
                t.bank = d(0);
                t.number = d(rest.size() - 1);
                t.engine = engine;
            }
        }
        if (t.kind == Kind::Text) {
            t.folded = foldString(raw, true);
            if (t.folded.isEmpty())
                continue;
        }
        m_tokens.append(t);
    }
}

bool SearchQuery::matches(const PatchAddress &a, const QString &name) const
{
    return matches(a, name, nullptr);
}

bool SearchQuery::matches(const PatchAddress &a, const QString &name, QVector<Range> *ranges,
                          const QString &altName) const
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
        case Kind::Engine:
            if (a.model() != m_model || a.engine() != t.engine) return false;
            break;
        case Kind::Bank64:
            if (a.model() != m_model || a.bank() != t.bank || (t.engine && a.engine() != t.engine)) return false;
            break;
        case Kind::Exact64:
            if (a.model() != m_model || a.bank() != t.bank || a.number() != t.number
                || (t.engine && a.engine() != t.engine))
                return false;
            break;
        case Kind::Text:
            needText = true;
            break;
        }
    }
    if (!needText)
        return true;

    // A name folded once per call (non-ASCII names only; ASCII names are searched in place).
    const bool apos = m_model != Model::D05;
    struct Folded {
        const QString *orig = nullptr;
        bool ascii = true;
        QString folded;
        QVector<int> map;  // folded index -> original index
    };
    auto prepare = [apos](const QString &s) {
        Folded f;
        f.orig = &s;
        f.ascii = isAscii(s);
        if (!f.ascii) {
            f.folded.reserve(s.size());
            f.map.reserve(s.size());
            for (int i = 0; i < s.size(); ++i) {
                const QChar c = foldChar(s.at(i), apos);
                if (!c.isNull()) {
                    f.folded.append(c);
                    f.map.append(i);
                }
            }
        }
        return f;
    };
    const Folded fname = prepare(name);
    const Folded falt = altName.isEmpty() ? Folded() : prepare(altName);

    // Finds `t` in the name; fills start/length in original-string indices.
    auto find = [apos](const Folded &f, const Token &t, int *start, int *length) {
        const QString &s = *f.orig;
        if (f.ascii) {
            *start = s.indexOf(t.folded, 0, Qt::CaseInsensitive);
            *length = t.folded.size();
            return *start >= 0;
        }
        const int fi = f.folded.indexOf(t.folded);
        if (fi < 0)
            return false;
        *start = f.map.at(fi);
        int end = f.map.at(fi + t.folded.size() - 1) + 1;
        while (end < s.size() && foldChar(s.at(end), apos).isNull())
            ++end;  // keep trailing combining marks inside the range
        *length = end - *start;
        return true;
    };

    QVector<Range> found[2];  // Primary, Alternate
    for (const Token &t : m_tokens) {
        if (t.kind != Kind::Text)
            continue;
        int start = 0, length = 0;
        if (find(fname, t, &start, &length)) {
            if (ranges)
                found[0].append({start, length, NameIndex::Primary});
        } else if (!altName.isEmpty() && find(falt, t, &start, &length)) {
            if (ranges)
                found[1].append({start, length, NameIndex::Alternate});
        } else {
            return false;
        }
    }

    if (ranges) {
        for (QVector<Range> &group : found) {
            if (group.isEmpty())
                continue;
            std::sort(group.begin(), group.end(), [](const Range &x, const Range &y) { return x.start < y.start; });
            Range cur = group.first();
            for (int i = 1; i < group.size(); ++i) {
                const Range &r = group.at(i);
                if (r.start <= cur.start + cur.length)
                    cur.length = std::max(cur.length, r.start + r.length - cur.start);
                else {
                    ranges->append(cur);
                    cur = r;
                }
            }
            ranges->append(cur);
        }
    }
    return true;
}

}  // namespace d05
