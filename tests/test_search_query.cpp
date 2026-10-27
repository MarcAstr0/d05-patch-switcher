#include "core/PatchCatalog.h"
#include "core/SearchQuery.h"

#include <QTest>

using d05::PatchAddress;
using d05::PatchCatalog;
using d05::SearchQuery;
using Range = SearchQuery::Range;

static PatchAddress A(const char *s) { return *PatchAddress::parse(QString::fromLatin1(s)); }

class TestSearchQuery : public QObject
{
    Q_OBJECT
private slots:
    void grammar_data()
    {
        QTest::addColumn<QString>("query");
        QTest::addColumn<QString>("address");
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("expected");

        auto row = [](const char *q, const char *a, const char *n, bool e) {
            QTest::newRow(qPrintable(QStringLiteral("%1 | %2 | %3").arg(QString::fromLatin1(q), QString::fromLatin1(a), QString::fromLatin1(n))))
                << QString::fromLatin1(q) << QString::fromLatin1(a) << QString::fromLatin1(n) << e;
        };
        // group filter
        row("p1", "P1-11", "Fantasia", true);
        row("p1", "P2-11", "Fantasia", false);
        row("U3", "U3-52", "", true);
        row("U3", "P3-52", "x", false);
        row("P6", "P6-88", "Zed", true);
        // group + bank
        row("p3-4", "P3-45", "Foo", true);
        row("P34", "P3-45", "Foo", true);
        row("p3-4", "P3-55", "Foo", false);
        row("u7-2", "U7-23", "", true);
        // exact address, missing letter means P
        row("P3-45", "P3-45", "Foo", true);
        row("p345", "P3-45", "Foo", true);
        row("3-45", "P3-45", "Foo", true);
        row("345", "P3-45", "Foo", true);
        row("345", "U3-45", "Foo", false);
        row("u345", "U3-45", "Foo", true);
        row("p345", "P3-46", "Foo", false);
        // bare digits are text
        row("3", "P1-11", "Synth Lead 3", true);
        row("3", "P3-11", "Synth Lead", false);
        row("45", "P4-55", "Foo", false);
        row("45", "P1-11", "Room 45", true);
        row("34", "P3-45", "Foo", false);
        row("3-4", "P3-45", "Foo", false);
        // out of range shapes are text
        row("p7", "P6-11", "Foo", false);
        row("p7", "P1-11", "Studio p7", true);
        row("U9", "U8-11", "Foo", false);
        row("p3-9", "P3-11", "Foo", false);
        row("P1-19", "P1-11", "Foo", false);
        row("p-3", "P3-11", "Foo", false);
        // text: case and substring
        row("brass", "P1-35", "Slap Brass", true);
        row("BRASS", "P1-35", "Slap Brass", true);
        row("e-piano", "P1-86", "PCM E-Piano", true);
        row("p", "P1-11", "Warm Pad", true);
        // accents
        row("Cafe", "P2-22", "Venetian Cafe", true);
        // multi-token AND
        row("brass slap", "P1-35", "Slap Brass", true);
        row("brass slap", "P1-35", "Rich Brass", false);
        row("p1 brass", "P1-35", "Slap Brass", true);
        row("p1 brass", "P2-35", "Slap Brass", false);
        row("p3 4", "P3-45", "Room 4", true);
        row("p1  brass", "P1-35", "Slap Brass", true);  // runs of whitespace
        row("p7", "P1-11", "Zap p7", true);
        row("p7", "P6-88", "Plain", false);
        row("p7", "U3-45", "Plain", false);
        row("u9", "P1-11", "Zap u9", true);
        row("u9", "P6-88", "Plain", false);
        row("u9", "U3-45", "Plain", false);
        row("U9", "P1-11", "Zap U9", true);
        row("U9", "P6-88", "Plain", false);
        row("U9", "U3-45", "Plain", false);
        row("p3-9", "P1-11", "Zap p3-9", true);
        row("p3-9", "P6-88", "Plain", false);
        row("p3-9", "U3-45", "Plain", false);
        row("P1-19", "P1-11", "Zap P1-19", true);
        row("P1-19", "P6-88", "Plain", false);
        row("P1-19", "U3-45", "Plain", false);
        row("p-3", "P1-11", "Zap p-3", true);
        row("p-3", "P6-88", "Plain", false);
        row("p-3", "U3-45", "Plain", false);
        row("p-34", "P1-11", "Zap p-34", true);
        row("p-34", "P6-88", "Plain", false);
        row("p-34", "U3-45", "Plain", false);
        row("p3-", "P1-11", "Zap p3-", true);
        row("p3-", "P6-88", "Plain", false);
        row("p3-", "U3-45", "Plain", false);
        row("P34-", "P1-11", "Zap P34-", true);
        row("P34-", "P6-88", "Plain", false);
        row("P34-", "U3-45", "Plain", false);
        row("p0", "P1-11", "Zap p0", true);
        row("p0", "P6-88", "Plain", false);
        row("p0", "U3-45", "Plain", false);
        row("u0", "P1-11", "Zap u0", true);
        row("u0", "P6-88", "Plain", false);
        row("u0", "U3-45", "Plain", false);
        row("p3-0", "P1-11", "Zap p3-0", true);
        row("p3-0", "P6-88", "Plain", false);
        row("p3-0", "U3-45", "Plain", false);
        row("p3-45x", "P1-11", "Zap p3-45x", true);
        row("p3-45x", "P6-88", "Plain", false);
        row("p3-45x", "U3-45", "Plain", false);
        row("745", "P1-11", "Zap 745", true);
        row("745", "P6-88", "Plain", false);
        row("745", "U3-45", "Plain", false);
        row("945", "P1-11", "Zap 945", true);
        row("945", "P6-88", "Plain", false);
        row("945", "U3-45", "Plain", false);
        row("P1-19", "P1-19", "Plain", false);
        row("P1-19", "P1-11", "Plain", false);
        row("3-45", "U3-45", "", false);
        row("45", "U3-45", "", false);
        row("u", "U1-11", "", false);
        row("u3", "U3-45", "", true);
        row("u3", "U4-45", "", false);
        row("", "P1-11", "Anything", true);
        row("   ", "P1-11", "Anything", true);
    }

    void grammar()
    {
        QFETCH(QString, query);
        QFETCH(QString, address);
        QFETCH(QString, name);
        QFETCH(bool, expected);
        SearchQuery q(query);
        QCOMPARE(q.matches(*PatchAddress::parse(address), name), expected);
    }

    void accents()
    {
        const QString cafeAcute = QStringLiteral("Venetian Café");
        const PatchAddress a = A("P2-22");
        QVERIFY(SearchQuery(QStringLiteral("Cafe")).matches(a, cafeAcute));
        QVERIFY(SearchQuery(QStringLiteral("café")).matches(a, cafeAcute));
        QVERIFY(SearchQuery(QStringLiteral("CAFÉ")).matches(a, QStringLiteral("Venetian Cafe")));
        // decomposed input (e + combining acute) in the query
        QVERIFY(SearchQuery(QStringLiteral("café")).matches(a, QStringLiteral("Venetian Cafe")));
        QVERIFY(SearchQuery(QStringLiteral("café")).matches(a, cafeAcute));
        QVERIFY(!SearchQuery(QStringLiteral("cafes")).matches(a, cafeAcute));
    }

    void rangesAscii()
    {
        QVector<Range> r;
        SearchQuery q(QStringLiteral("brass"));
        QVERIFY(q.matches(A("P1-35"), QStringLiteral("Slap Brass"), &r));
        QCOMPARE(r, (QVector<Range>{{5, 5}}));
    }

    void rangesMultiToken()
    {
        QVector<Range> r;
        SearchQuery q(QStringLiteral("p1 slap bra"));
        QVERIFY(q.matches(A("P1-35"), QStringLiteral("Slap Brass"), &r));
        // "slap" 0..4 and "bra" 5..8 do not touch (space between)
        QCOMPARE(r, (QVector<Range>{{0, 4}, {5, 3}}));
        // overlapping tokens merge
        SearchQuery q2(QStringLiteral("brass ass"));
        QVERIFY(q2.matches(A("P1-35"), QStringLiteral("Slap Brass"), &r));
        QCOMPARE(r, (QVector<Range>{{5, 5}}));
        // order of tokens does not matter; output is sorted
        SearchQuery q3(QStringLiteral("brass slap"));
        QVERIFY(q3.matches(A("P1-35"), QStringLiteral("Slap Brass"), &r));
        QCOMPARE(r, (QVector<Range>{{0, 4}, {5, 5}}));
    }

    void rangesAddressOnlyAndFailure()
    {
        QVector<Range> r{{1, 1}};
        QVERIFY(SearchQuery(QStringLiteral("p345")).matches(A("P3-45"), QStringLiteral("Foo"), &r));
        QVERIFY(r.isEmpty());
        r = {{1, 1}};
        QVERIFY(!SearchQuery(QStringLiteral("zzz")).matches(A("P3-45"), QStringLiteral("Foo"), &r));
        QVERIFY(r.isEmpty());
    }

    void rangesAgainstOriginalString()
    {
        QVector<Range> r;
        const QString name = QStringLiteral("Venetian Café Bar");
        QVERIFY(SearchQuery(QStringLiteral("cafe")).matches(A("P2-22"), name, &r));
        QCOMPARE(r, (QVector<Range>{{9, 4}}));
        // decomposed name: the combining mark is inside the range
        const QString dec = QStringLiteral("Venetian Café Bar");
        QVERIFY(SearchQuery(QStringLiteral("cafe")).matches(A("P2-22"), dec, &r));
        QCOMPARE(r, (QVector<Range>{{9, 5}}));
    }

    void unicodeWhitespaceSeparates()
    {
        SearchQuery q(QStringLiteral("p1\u00a0brass\u2003slap"));
        QVERIFY(q.matches(A("P1-35"), QStringLiteral("Slap Brass")));
        QVERIFY(!q.matches(A("P2-35"), QStringLiteral("Slap Brass")));
        QVERIFY(!q.matches(A("P1-35"), QStringLiteral("Slap")));
    }

    void emptyQuery()
    {
        QVector<Range> r{{1, 1}};
        SearchQuery q(QStringLiteral("  "));
        QVERIFY(q.isEmpty());
        QVERIFY(q.matches(A("U1-11"), QString(), &r));
        QVERIFY(r.isEmpty());
        QVERIFY(SearchQuery().isEmpty());
    }

    void overFullCatalogue()
    {
        PatchCatalog c;
        auto count = [&](const QString &text) {
            SearchQuery q(text);
            int n = 0;
            for (int i = 0; i < PatchAddress::CatalogueSize; ++i) {
                const auto a = *PatchAddress::fromCatalogueIndex(i);
                if (q.matches(a, c.name(a)))
                    ++n;
            }
            return n;
        };
        QCOMPARE(count(QString()), 896);
        QCOMPARE(count(QStringLiteral("p1")), 64);
        QCOMPARE(count(QStringLiteral("u3")), 64);
        QCOMPARE(count(QStringLiteral("p3-4")), 8);
        QCOMPARE(count(QStringLiteral("345")), 1);
        QCOMPARE(count(QStringLiteral("u345")), 1);
        QCOMPARE(count(QStringLiteral("p7")), 0);
        {
            // unnamed U slots never match a text token: "u" hits only presets with a 'u' in the name
            int expected = 0;
            for (int i = 0; i < PatchAddress::CatalogueSize; ++i)
                if (c.name(*PatchAddress::fromCatalogueIndex(i)).contains(QLatin1Char('u'), Qt::CaseInsensitive))
                    ++expected;
            QCOMPARE(count(QStringLiteral("u")), expected);
        }
        {
            int expected = 0;
            for (int i = 0; i < PatchAddress::CatalogueSize; ++i)
                if (c.name(*PatchAddress::fromCatalogueIndex(i)).contains(QStringLiteral("45")))
                    ++expected;
            QCOMPARE(count(QStringLiteral("45")), expected);
        }
        QVERIFY(count(QStringLiteral("brass")) > 3);
        QVERIFY(count(QStringLiteral("p1 brass")) < count(QStringLiteral("brass")));
    }

    void smallModels()
    {
        using d05::Model;
        auto P = [](const char *t, Model m) { return *PatchAddress::parse(QString::fromLatin1(t), m); };
        const SearchQuery jp(QStringLiteral("4 lead"), Model::JP08);
        QVERIFY(jp.matches(P("4-1", Model::JP08), QStringLiteral("Big Saw Lead")));
        QVERIFY(!jp.matches(P("5-1", Model::JP08), QStringLiteral("Big Saw Lead")));
        // address tokens never match another model's addresses
        QVERIFY(!SearchQuery(QStringLiteral("4"), Model::JP08).matches(P("106:4-1", Model::JU06A), QString()));
        QVERIFY(!SearchQuery(QStringLiteral("4"), Model::JU06A).matches(P("4-1", Model::JP08), QString()));
        // 60 / 106 are text on the JP-08
        QVERIFY(!SearchQuery(QStringLiteral("106"), Model::JP08).matches(P("1-1", Model::JP08), QStringLiteral("x")));
        QVERIFY(SearchQuery(QStringLiteral("106"), Model::JP08).matches(P("1-1", Model::JP08), QStringLiteral("Mk 106")));
        // engine filter and prefix on the JU-06A
        const SearchQuery e(QStringLiteral("106:45"), Model::JU06A);
        QVERIFY(e.matches(P("106:4-5", Model::JU06A), QString()));
        QVERIFY(!e.matches(P("60:4-5", Model::JU06A), QString()));
        QVERIFY(SearchQuery(QStringLiteral("45"), Model::JU06A).matches(P("60:4-5", Model::JU06A), QString()));
        // alternate (factory) name is searched; its ranges say they index it (T8.3: they used to be dropped)
        const SearchQuery t(QStringLiteral("takeoff zebra"), Model::JU06A);
        QVector<Range> r;
        QVERIFY(t.matches(P("106:8-8", Model::JU06A), QStringLiteral("Zebra Lead"), &r, QStringLiteral("Takeoff")));
        QCOMPARE(r, (QVector<Range>{{0, 5}, {0, 7, SearchQuery::NameIndex::Alternate}}));
        QVERIFY(!t.matches(P("106:8-8", Model::JU06A), QStringLiteral("Zebra Lead"), &r));
        // typographic apostrophe folds to ' on both sides (JP-08 / JU-06A only)
        const QString louis = QString::fromUtf8("Louis\xe2\x80\x99 Weapon");
        for (const char *q : {"louis'", "louis\xe2\x80\x99", "s' w"}) {
            QVector<Range> lr;
            const SearchQuery sq(QString::fromUtf8(q), Model::JU06A);
            QVERIFY(sq.matches(P("106:8-6", Model::JU06A), louis, &lr));
            QVERIFY(!lr.isEmpty());
            QCOMPARE(lr.first().start, q[0] == 's' ? 4 : 0);
        }
        {
            QVector<Range> lr;
            QVERIFY(SearchQuery(QString::fromUtf8("louis\xe2\x80\x99"), Model::JU06A)
                        .matches(P("106:8-6", Model::JU06A), louis, &lr));
            QCOMPARE(lr, (QVector<Range>{{0, 6}}));
            QVERIFY(SearchQuery(QString::fromUtf8("weapon"), Model::JP08)
                        .matches(P("1-1", Model::JP08), louis, &lr));
            QCOMPARE(lr, (QVector<Range>{{7, 6}}));
            // straight-quote name, curly query; and the factory (alt) name path
            QVERIFY(SearchQuery(QString::fromUtf8("louis\xe2\x80\x99"), Model::JP08)
                        .matches(P("1-1", Model::JP08), QStringLiteral("Louis' X")));
            QVERIFY(SearchQuery(QStringLiteral("louis'"), Model::JU06A)
                        .matches(P("106:8-6", Model::JU06A), QStringLiteral("Mine"), nullptr, louis));
        }
        // the D-05 does not fold apostrophes
        QVERIFY(!SearchQuery(QStringLiteral("louis'"), Model::D05).matches(A("P3-45"), louis));
        // D-05 constructor unchanged
        QVERIFY(SearchQuery(QStringLiteral("p345"), Model::D05).matches(A("P3-45"), QStringLiteral("Foo")));
    }
};

QTEST_APPLESS_MAIN(TestSearchQuery)
#include "test_search_query.moc"
