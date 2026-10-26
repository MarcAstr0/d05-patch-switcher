#include "core/PatchCatalog.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using d05::PatchAddress;
using d05::Model;
using d05::PatchCatalog;

static PatchAddress A(const char *s)
{
    return *PatchAddress::parse(QString::fromLatin1(s));
}

class TestPatchCatalog : public QObject
{
    Q_OBJECT
private slots:
    void presetsLoaded()
    {
        PatchCatalog c;
        QVERIFY(c.isValid());
        QCOMPARE(c.presetCount(), 384);
        for (int i = 0; i < PatchAddress::CatalogueSize; ++i) {
            const auto a = *PatchAddress::fromCatalogueIndex(i);
            if (a.isPreset()) {
                QVERIFY(!c.name(a).isEmpty());
                QVERIFY(QStringLiteral("WDS").contains(c.mode(a)));
            } else {
                QVERIFY(c.name(a).isEmpty());
                QVERIFY(c.mode(a).isNull());
            }
        }
    }

    void spotChecks()
    {
        PatchCatalog c;
        QCOMPARE(c.name(A("P1-11")), QStringLiteral("Fantasia"));
        QCOMPARE(c.displayName(A("P1-11")), QStringLiteral("Fantasia"));
        QCOMPARE(c.mode(A("P1-11")), QLatin1Char('D'));
        QCOMPARE(c.name(A("P6-88")), QStringLiteral("Big Waves"));
        QCOMPARE(c.mode(A("P2-57")), QLatin1Char('W'));
    }

    void missingFileIsInvalid()
    {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cannot open")));
        PatchCatalog c(QStringLiteral(":/nope.tsv"));
        QVERIFY(!c.isValid());
        QCOMPARE(c.presetCount(), 0);
    }

    void userOverlay()
    {
        PatchCatalog c;
        const auto u = A("U1-11");
        QCOMPARE(c.displayName(u), QStringLiteral("U1-11"));
        QVERIFY(c.name(u).isEmpty());
        QVERIFY(c.setUserName(u, QStringLiteral("  My Lead  ")));
        QCOMPARE(c.name(u), QStringLiteral("My Lead"));
        QCOMPARE(c.displayName(u), QStringLiteral("My Lead"));
        QVERIFY(c.hasUserName(u));
        QVERIFY(c.setUserName(u, QStringLiteral("")));
        QCOMPARE(c.displayName(u), QStringLiteral("U1-11"));
        QVERIFY(!c.hasUserName(u));
    }

    void userNameLimitAndControlChars()
    {
        PatchCatalog c;
        const auto u = A("U8-88");
        c.setUserName(u, QStringLiteral("0123456789ABCDEFGHIJKL"));
        QCOMPARE(c.name(u), QStringLiteral("0123456789ABCDEFGH"));
        c.setUserName(u, QStringLiteral("a\tb\nc"));
        QCOMPARE(c.name(u), QStringLiteral("a b c"));
    }

    void presetsAreReadOnly()
    {
        PatchCatalog c;
        QVERIFY(!c.setUserName(A("P1-11"), QStringLiteral("X")));
        QCOMPARE(c.name(A("P1-11")), QStringLiteral("Fantasia"));
    }

    void userNameBoundaries()
    {
        PatchCatalog c;
        const auto u = A("U2-22");
        c.setUserName(u, QStringLiteral("123456789012345678"));
        QCOMPARE(c.name(u), QStringLiteral("123456789012345678"));
        c.setUserName(u, QStringLiteral("1234567890123456789"));
        QCOMPARE(c.name(u), QStringLiteral("123456789012345678"));
        c.setUserName(u, QStringLiteral("12345678901234567 89"));
        QCOMPARE(c.name(u), QStringLiteral("12345678901234567"));
        c.setUserName(u, QStringLiteral(" \t\r\n "));
        QVERIFY(!c.hasUserName(u));
        c.setUserName(u, QStringLiteral("Warm\tPad"));
        QCOMPARE(c.name(u), QStringLiteral("Warm Pad"));
        c.setUserName(u, QStringLiteral("a   b")) ;
        QCOMPARE(c.name(u), QStringLiteral("a b"));
        c.setUserName(u, QStringLiteral("Caf\u00e9"));
        QCOMPARE(c.name(u), QStringLiteral("Caf"));
        c.setUserName(u, QStringLiteral("\u00e9\u4e2d"));
        QVERIFY(!c.hasUserName(u));
    }

    void clearAll()
    {
        PatchCatalog c;
        c.setUserName(A("U1-11"), QStringLiteral("x"));
        c.setUserName(A("U2-11"), QStringLiteral("y"));
        c.clearUserNames();
        QVERIFY(!c.hasUserName(A("U1-11")));
        QCOMPARE(c.displayName(A("U2-11")), QStringLiteral("U2-11"));
        QCOMPARE(c.presetCount(), 384);
    }

    void roundTripAndOrder()
    {
        for (int i = 0; i < PatchAddress::CatalogueSize; ++i) {
            const auto x = *PatchAddress::fromCatalogueIndex(i);
            if (i < PatchAddress::CatalogueSize - 1) {
                QVERIFY(PatchCatalog::next(x).catalogueIndex() == i + 1);
                QCOMPARE(PatchCatalog::prev(PatchCatalog::next(x)), x);
            }
            if (i > 0) {
                QVERIFY(PatchCatalog::prev(x).catalogueIndex() == i - 1);
                QCOMPARE(PatchCatalog::next(PatchCatalog::prev(x)), x);
            }
        }
    }

    // --- custom TSVs ---
    static QString writeTsv(QTemporaryDir &dir, const QByteArray &content)
    {
        const QString path = dir.filePath(QStringLiteral("t.tsv"));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return {};
        f.write(content);
        return path;
    }
    static const QByteArray hdr() { return QByteArray("group\tbank\tnumber\tname\tmode"); }

    void badHeader()
    {
        QTemporaryDir d;
        const QString p = writeTsv(d, QByteArray("g\tb\tn\tname\tmode\nP1\t1\t1\tFantasia\tD\n"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unexpected header")));
        PatchCatalog c(p);
        QCOMPARE(c.presetCount(), 1);
        QVERIFY(!c.isValid());
    }

    void crlfLineEndings()
    {
        QTemporaryDir d;
        const QString p = writeTsv(d, hdr() + "\r\nP1\t1\t1\tFantasia\tD\r\nP1\t1\t2\tMetal Harp\tD\r\n");
        PatchCatalog c(p);
        QCOMPARE(c.presetCount(), 2);
        QCOMPARE(c.name(A("P1-12")), QStringLiteral("Metal Harp"));
        QCOMPARE(c.mode(A("P1-12")), QLatin1Char('D'));
    }

    void skippedRows()
    {
        QTemporaryDir d;
        const QString p = writeTsv(d, hdr() + "\nP1\t1\t1\tFantasia\tD\n"
                                              "P1\t1\t2\tShort\n"
                                              "P7\t1\t1\tNope\tD\n"
                                              "U1\t1\t1\tNope\tD\n"
                                              "P1\t1\t3\t\tD\n"
                                              "P1\t1\t9\tBad\tD\n"
                                              "P1\t1\t4\tBadMode\tX\n"
                                              "P1\t1\t1\tDuplicate\tS\n");
        for (const char *re : {"too few columns", "not a preset group", "not a preset group",
                               "empty name", "out of range", "mode is not", "duplicate address"})
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QString::fromLatin1(re)));
        PatchCatalog c(p);
        QCOMPARE(c.presetCount(), 1);
        QVERIFY(!c.isValid());
        QCOMPARE(c.name(A("P1-11")), QStringLiteral("Fantasia"));  // first wins
        QCOMPARE(c.mode(A("P1-11")), QLatin1Char('D'));
        QVERIFY(c.name(A("U1-11")).isEmpty());
    }

    void prevNext()
    {
        QCOMPARE(PatchCatalog::next(A("P1-88")), A("P2-11"));
        QCOMPARE(PatchCatalog::next(A("U8-88")), A("P1-11"));  // crosses U/P boundary
        QCOMPARE(PatchCatalog::prev(A("P1-11")), A("U8-88"));
        QCOMPARE(PatchCatalog::next(A("P6-88")), A("P6-88"));
        QCOMPARE(PatchCatalog::prev(A("U1-11")), A("U1-11"));
        QCOMPARE(PatchCatalog::next(A("U1-18")), A("U1-21"));
    }

    void jp08Table()
    {
        PatchCatalog c(Model::JP08);
        QVERIFY(c.isValid());
        QCOMPARE(c.presetCount(), 64);
        QCOMPARE(c.model(), Model::JP08);
        const auto a = *PatchAddress::fromBankNumber(Model::JP08, 1, 1);
        QCOMPARE(c.name(a), QStringLiteral("NEG Sync"));
        QCOMPARE(c.displayName(a), QStringLiteral("NEG Sync"));
        QVERIFY(c.mode(a).isNull());
        QVERIFY(c.tooltipName(a).isEmpty());
        for (int i = 0; i < 64; ++i)
            QVERIFY(!c.name(*PatchAddress::fromCatalogueIndex(Model::JP08, i)).isEmpty());
        // A D-05 address is foreign here.
        QVERIFY(c.name(A("P1-11")).isEmpty());
    }

    void jp08UserName()
    {
        PatchCatalog c(Model::JP08);
        const auto a = *PatchAddress::fromBankNumber(Model::JP08, 1, 1);
        QVERIFY(c.setUserName(a, QStringLiteral("  My Sync ")));
        QVERIFY(c.hasUserName(a));
        QCOMPARE(c.name(a), QStringLiteral("My Sync"));
        QCOMPARE(c.displayName(a), QStringLiteral("My Sync"));
        QCOMPARE(c.userName(a), QStringLiteral("My Sync"));
        QCOMPARE(c.factoryName(a), QStringLiteral("NEG Sync"));
        QCOMPARE(c.tooltipName(a), QStringLiteral("NEG Sync"));
        QVERIFY(c.hasFactoryName(a));
        QVERIFY(c.mode(a).isNull());
        QVERIFY(!c.setUserName(A("U1-11"), QStringLiteral("x")));
        QVERIFY(c.setUserName(a, QStringLiteral("0123456789012345678901")));
        QCOMPARE(c.userName(a).size(), 18);
        QVERIFY(c.setUserName(a, QString()));
        QVERIFY(!c.hasUserName(a));
        QCOMPARE(c.name(a), QStringLiteral("NEG Sync"));
        QVERIFY(c.tooltipName(a).isEmpty());
    }

    void ju06aTable()
    {
        PatchCatalog c(Model::JU06A);
        QVERIFY(c.isValid());
        QCOMPARE(c.presetCount(), 128);
        const auto a60 = *PatchAddress::fromBankNumber(Model::JU06A, 1, 1, 60);
        const auto a106 = *PatchAddress::fromBankNumber(Model::JU06A, 1, 1, 106);
        QCOMPARE(c.name(a60), QStringLiteral("LD Echolocator"));
        QCOMPARE(c.name(a106), QStringLiteral("Brass"));
        QVERIFY(c.mode(a60).isNull());
        QVERIFY(c.setUserName(a106, QStringLiteral("Mine")));
        QCOMPARE(c.displayName(a106), QStringLiteral("Mine"));
        QCOMPARE(c.tooltipName(a106), QStringLiteral("Brass"));
        QCOMPARE(c.name(a60), QStringLiteral("LD Echolocator"));  // other engine untouched
        QVERIFY(c.tooltipName(a60).isEmpty());
    }

    void modelPrevNext()
    {
        const auto jp = [](int b, int n) { return *PatchAddress::fromBankNumber(Model::JP08, b, n); };
        QCOMPARE(PatchCatalog::next(jp(1, 8)), jp(2, 1));
        QCOMPARE(PatchCatalog::prev(jp(2, 1)), jp(1, 8));
        QCOMPARE(PatchCatalog::next(jp(8, 8)), jp(8, 8));
        QCOMPARE(PatchCatalog::prev(jp(1, 1)), jp(1, 1));
        const auto ju = [](int e, int b, int n) { return *PatchAddress::fromBankNumber(Model::JU06A, b, n, e); };
        QCOMPARE(PatchCatalog::next(ju(60, 1, 8)), ju(60, 2, 1));
        QCOMPARE(PatchCatalog::next(ju(60, 8, 8)), ju(60, 8, 8));
        QCOMPARE(PatchCatalog::prev(ju(106, 1, 1)), ju(106, 1, 1));
        QCOMPARE(PatchCatalog::next(ju(106, 8, 8)), ju(106, 8, 8));
        QCOMPARE(PatchCatalog::prev(ju(106, 2, 1)), ju(106, 1, 8));
        QCOMPARE(PatchCatalog::prev(ju(60, 1, 1)), ju(60, 1, 1));
    }

    void ju06aSpecialCharacterNames()
    {
        PatchCatalog c(Model::JU06A);
        const auto a = *PatchAddress::parse(QStringLiteral("106:8-6"), Model::JU06A);
        QCOMPARE(c.name(a), QString::fromUtf8("Louis\xE2\x80\x99 Weapon"));
        QCOMPARE(c.name(a).at(5).unicode(), char16_t(0x2019));
        const auto b = *PatchAddress::parse(QStringLiteral("106:1-6"), Model::JU06A);
        QCOMPARE(c.name(b), QStringLiteral("Brass & Strings"));
    }

    void jp08BadRows()
    {
        QTemporaryDir d;
        const QString p = writeTsv(d, QByteArray("bank\tnumber\tname\n"
                                                 "1\t1\tNEG Sync\n"
                                                 "0\t1\tZero\n"
                                                 "9\t1\tNine\n"
                                                 "1\t0\tZeroNum\n"
                                                 "1\t9\tNineNum\n"
                                                 "1\n"
                                                 "1\t2\n"
                                                 "1\t3\t\n"
                                                 "1\t1\tDuplicate\n"));
        for (const char *re : {"out of range", "out of range", "out of range", "out of range",
                               "too few columns", "too few columns", "empty name", "duplicate address"})
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QString::fromLatin1(re)));
        PatchCatalog c(Model::JP08, p);
        QCOMPARE(c.presetCount(), 1);
        QVERIFY(!c.isValid());
        QCOMPARE(c.name(*PatchAddress::fromBankNumber(Model::JP08, 1, 1)), QStringLiteral("NEG Sync"));
        QVERIFY(c.name(*PatchAddress::fromBankNumber(Model::JP08, 1, 2)).isEmpty());
    }

    void ju06aBadRows()
    {
        QTemporaryDir d;
        const QString p = writeTsv(d, QByteArray("engine\tbank\tnumber\tname\n"
                                                 "60\t1\t1\tLD Echolocator\n"
                                                 "70\t1\t2\tWrongEngine\n"
                                                 "106\t0\t1\tBank0\n"
                                                 "106\t9\t1\tBank9\n"
                                                 "106\t1\n"
                                                 "106\t1\t2\n"
                                                 "60\t1\t1\tDuplicate\n"
                                                 "106\t1\t1\tBrass\n"));
        for (const char *re : {"engine is not", "out of range", "out of range", "too few columns",
                               "too few columns", "duplicate address"})
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QString::fromLatin1(re)));
        PatchCatalog c(Model::JU06A, p);
        QCOMPARE(c.presetCount(), 2);  // 60:1-1 and 106:1-1; the same bank/number in another engine is not a duplicate
        QVERIFY(!c.isValid());
        QCOMPARE(c.name(*PatchAddress::parse(QStringLiteral("60:1-1"), Model::JU06A)), QStringLiteral("LD Echolocator"));
        QCOMPARE(c.name(*PatchAddress::parse(QStringLiteral("106:1-1"), Model::JU06A)), QStringLiteral("Brass"));
    }

    void smallModelBadHeader()
    {
        QTemporaryDir d;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unexpected header")));
        // A D-05 header with JU-06A rows: the rows are read by the JU-06A layout regardless.
        PatchCatalog j(Model::JP08, writeTsv(d, QByteArray("b\tn\tname\n1\t1\tNEG Sync\n")));
        QCOMPARE(j.presetCount(), 1);
        QVERIFY(!j.isValid());
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unexpected header")));
        PatchCatalog u(Model::JU06A, writeTsv(d, QByteArray("bank\tnumber\tname\n60\t1\t1\tX\n")));
        QVERIFY(!u.isValid());
    }

    void foreignAddresses()
    {
        const auto jp = *PatchAddress::parse(QStringLiteral("1-1"), Model::JP08);
        const auto ju60 = *PatchAddress::parse(QStringLiteral("60:1-1"), Model::JU06A);
        const auto d05 = A("U1-11");
        QCOMPARE(jp.catalogueIndex(), ju60.catalogueIndex());
        QCOMPARE(jp.catalogueIndex(), d05.catalogueIndex());
        QVERIFY(jp != ju60);

        PatchCatalog j(Model::JP08);
        PatchCatalog u(Model::JU06A);
        PatchCatalog d;
        QVERIFY(j.name(ju60).isEmpty());
        QVERIFY(j.factoryName(ju60).isEmpty());
        QVERIFY(u.name(jp).isEmpty());
        QVERIFY(u.factoryName(jp).isEmpty());
        QVERIFY(u.displayName(jp) == jp.toString());
        QVERIFY(d.name(jp).isEmpty());
        QVERIFY(j.name(d05).isEmpty());

        QVERIFY(!u.setUserName(jp, QStringLiteral("Foreign")));
        QVERIFY(!u.hasUserName(jp));
        QVERIFY(!u.hasUserName(ju60));
        QCOMPARE(u.name(ju60), QStringLiteral("LD Echolocator"));
        QVERIFY(!j.setUserName(ju60, QStringLiteral("Foreign")));
        QVERIFY(!j.hasUserName(ju60));
        QVERIFY(!j.hasUserName(jp));
        QCOMPARE(j.name(jp), QStringLiteral("NEG Sync"));
        QVERIFY(!d.setUserName(jp, QStringLiteral("Foreign")));
        QVERIFY(!d.setUserName(ju60, QStringLiteral("Foreign")));
        QVERIFY(!d.hasUserName(d05));
        QVERIFY(d.name(d05).isEmpty());
        // And a stored user name is not visible through a foreign address.
        QVERIFY(j.setUserName(jp, QStringLiteral("Mine")));
        QVERIFY(j.name(ju60).isEmpty());
        QVERIFY(j.userName(ju60).isEmpty());
        QVERIFY(j.tooltipName(ju60).isEmpty());
        QVERIFY(j.mode(d05).isNull());
    }

    void d05AccessorsUnchanged()
    {
        PatchCatalog c;
        QCOMPARE(c.model(), Model::D05);
        QCOMPARE(c.factoryName(A("P1-11")), QStringLiteral("Fantasia"));
        QVERIFY(c.tooltipName(A("P1-11")).isEmpty());
        QVERIFY(c.setUserName(A("U1-11"), QStringLiteral("Lead")));
        QVERIFY(c.factoryName(A("U1-11")).isEmpty());
        QVERIFY(c.tooltipName(A("U1-11")).isEmpty());
        QVERIFY(!c.setUserName(A("P1-11"), QStringLiteral("x")));
        PatchCatalog d(Model::D05);
        QVERIFY(d.isValid());
    }
};

QTEST_GUILESS_MAIN(TestPatchCatalog)
#include "test_patch_catalog.moc"
