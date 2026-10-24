#include "core/PatchCatalog.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using d05::PatchAddress;
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
};

QTEST_GUILESS_MAIN(TestPatchCatalog)
#include "test_patch_catalog.moc"
