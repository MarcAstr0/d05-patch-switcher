#pragma once

#include "core/Model.h"
#include "core/PatchAddress.h"

#include <QString>
#include <QVector>

namespace d05 {

// The FR-3 search grammar: a query string parsed once into tokens, then evaluated
// per row. Whitespace-separated tokens are ANDed. Each token is a group filter
// (p1, U3), a group+bank filter (p3-4, P34), an exact address (P3-45, p345, 3-45,
// 345) or, otherwise, a case- and accent-insensitive substring of the name.
// Bare one- or two-digit tokens, and out-of-range address-shaped tokens (p7, U9,
// p3-9, P1-19), are text.
class SearchQuery {
public:
    // Match range inside the name string passed to matches(). Both values are in
    // UTF-16 code units (QString indices), so they can be fed straight to the UI.
    struct Range {
        int start = 0;
        int length = 0;
        bool operator==(const Range &o) const { return start == o.start && length == o.length; }
    };

    SearchQuery() = default;
    explicit SearchQuery(const QString &text);
    // Per-model grammar (SPEC-2 FR-3). D05 is the grammar above. JP-08: one digit 1-8 is a bank
    // filter, two digits 1-8 (with or without a dash) an exact address. JU-06A: the same, plus
    // `60` / `106` as an engine filter and `60:` / `106:` prefixes on the bank and address
    // forms; bare forms match both engines. Anything else, including digits with 0 or 9 and
    // three or more digits, is text. Address tokens never match addresses of another model.
    SearchQuery(const QString &text, Model model);

    // True for an empty or all-whitespace query (everything matches).
    bool isEmpty() const { return m_tokens.isEmpty(); }

    // `name` is the patch name, empty if none (pass PatchCatalog::name, not
    // displayName); ranges index into that string, which equals the painted text for
    // named rows. Text tokens match the name only, never the address text. Address
    // tokens test `address` only.
    bool matches(const PatchAddress &address, const QString &name) const;

    // As above; on a match, `ranges` is cleared and filled with the highlight ranges
    // of the text tokens: the first occurrence of each, sorted, overlaps and
    // touching ranges merged. Address tokens produce no ranges.
    bool matches(const PatchAddress &address, const QString &name, QVector<Range> *ranges,
                 const QString &altName = QString()) const;
    // altName (JP-08 / JU-06A: the factory name of a slot that has a user name in `name`) is
    // searched too; a text token found only there matches but produces no range.

private:
    enum class Kind { Group, GroupBank, Exact, Text, Engine, Bank64, Exact64 };
    struct Token {
        Kind kind = Kind::Text;
        int group = 0;
        int bank = 0;
        int number = 0;
        int engine = 0;  // 64-patch kinds: 60 / 106, 0 = both
        QString folded;  // Text only: lowercased, accents stripped
    };
    void parseSmall(const QStringList &parts, bool ju06a);
    Model m_model = Model::D05;
    QVector<Token> m_tokens;
};

}  // namespace d05
