#include "core/PatchAddress.h"

#include <QtTest>

using d05::PatchAddress;

class TestPatchAddress : public QObject {
    Q_OBJECT
private slots:
    void roundTripAll896()
    {
        for (int i = 0; i < PatchAddress::CatalogueSize; ++i) {
            const auto a = PatchAddress::fromCatalogueIndex(i);
            QVERIFY(a);
            QCOMPARE(a->catalogueIndex(), i);
            const auto p = PatchAddress::parse(a->toString());
            QVERIFY2(p, qPrintable(a->toString()));
            QCOMPARE(*p, *a);
            const QString s = a->toString();  // "P3-45"
            const QString compact = s.left(2) + s.mid(3);
            const auto c = PatchAddress::parse(compact);
            QVERIFY2(c, qPrintable(compact));
            QCOMPARE(*c, *a);
            if (a->isPreset()) {
                const auto n1 = PatchAddress::parse(s.mid(1));
                const auto n2 = PatchAddress::parse(compact.mid(1));
                QVERIFY(n1 && n2);
                QCOMPARE(*n1, *a);
                QCOMPARE(*n2, *a);
            }
            const auto b = PatchAddress::fromParts(a->group(), a->bank(), a->number());
            QVERIFY(b);
            QCOMPARE(*b, *a);
        }
        QVERIFY(!PatchAddress::fromCatalogueIndex(-1));
        QVERIFY(!PatchAddress::fromCatalogueIndex(896));
    }

    void catalogueOrder()
    {
        QCOMPARE(PatchAddress::fromCatalogueIndex(0)->toString(), QStringLiteral("U1-11"));
        QCOMPARE(PatchAddress::fromCatalogueIndex(511)->toString(), QStringLiteral("U8-88"));
        QCOMPARE(PatchAddress::fromCatalogueIndex(512)->toString(), QStringLiteral("P1-11"));
        QCOMPARE(PatchAddress::fromCatalogueIndex(895)->toString(), QStringLiteral("P6-88"));
    }

    void parseForms_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("canonical");
        QTest::newRow("canonical") << "P3-45" << "P3-45";
        QTest::newRow("lower") << "p3-45" << "P3-45";
        QTest::newRow("compact") << "P345" << "P3-45";
        QTest::newRow("compact lower") << "p345" << "P3-45";
        QTest::newRow("no letter dash") << "3-45" << "P3-45";
        QTest::newRow("no letter") << "345" << "P3-45";
        QTest::newRow("user") << "U3-45" << "U3-45";
        QTest::newRow("user compact") << "u345" << "U3-45";
        QTest::newRow("spaces") << "  P3-45 " << "P3-45";
        QTest::newRow("last") << "P6-88" << "P6-88";
        QTest::newRow("first user") << "U1-11" << "U1-11";
    }
    void parseForms()
    {
        QFETCH(QString, text);
        QFETCH(QString, canonical);
        const auto a = PatchAddress::parse(text);
        QVERIFY(a);
        QCOMPARE(a->toString(), canonical);
    }

    void rejects_data()
    {
        QTest::addColumn<QString>("text");
        for (const char *s : {"P1-19", "P1-90", "P7-11", "U9-11", "X1-11", "P0-11", "U0-11", "7-11",
                              "", "P", "P1-1", "P1-111", "P1--11", "P1_11", "12", "1234", "P1-1a",
                              "P١-11"})
            QTest::newRow(s) << QString::fromUtf8(s);
    }
    void rejects()
    {
        QFETCH(QString, text);
        QVERIFY(!PatchAddress::parse(text));
    }

    void rejectsBadParts()
    {
        QVERIFY(!PatchAddress::fromParts(14, 1, 1));
        QVERIFY(!PatchAddress::fromParts(-1, 1, 1));
        QVERIFY(!PatchAddress::fromParts(0, 0, 1));
        QVERIFY(!PatchAddress::fromParts(0, 9, 1));
        QVERIFY(!PatchAddress::fromParts(0, 1, 0));
        QVERIFY(!PatchAddress::fromParts(0, 1, 9));
    }

    // The five worked examples from docs/MIDI.md.
    void workedExamples_data()
    {
        QTest::addColumn<QString>("addr");
        QTest::addColumn<int>("lsb");
        QTest::addColumn<int>("program");
        QTest::newRow("P1-11") << "P1-11" << 8 << 0;
        QTest::newRow("P1-21") << "P1-21" << 8 << 8;
        QTest::newRow("P1-44") << "P1-44" << 8 << 27;
        QTest::newRow("P6-88") << "P6-88" << 13 << 63;
        QTest::newRow("P3-45") << "P3-45" << 10 << 28;
    }
    void workedExamples()
    {
        QFETCH(QString, addr);
        QFETCH(int, lsb);
        QFETCH(int, program);
        const auto t = PatchAddress::parse(addr)->midiTriplet();
        QCOMPARE(t.msb, 87);
        QCOMPARE(t.lsb, lsb);
        QCOMPARE(t.program, program);
    }

    void tripletInvariantsAll896()
    {
        for (int i = 0; i < PatchAddress::CatalogueSize; ++i) {
            const auto t = PatchAddress::fromCatalogueIndex(i)->midiTriplet();
            QCOMPARE(t.msb, 87);
            QCOMPARE(t.lsb, PatchAddress::fromCatalogueIndex(i)->group());
            QVERIFY(t.program >= 0 && t.program <= 63);
        }
        const auto m = PatchAddress::parse(QStringLiteral("U3-45"))->midiTriplet();
        QCOMPARE(m.lsb, 2);
        QCOMPARE(m.program, 28);
    }

    void accessors()
    {
        const auto u = *PatchAddress::parse(QStringLiteral("U3-45"));
        QVERIFY(u.isUser());
        QVERIFY(!u.isPreset());
        QCOMPARE(u.groupLabel(), QStringLiteral("U3"));
        QCOMPARE(u.groupNumber(), 3);
        const auto p = *PatchAddress::parse(QStringLiteral("P6-18"));
        QVERIFY(!p.isUser());
        QVERIFY(p.isPreset());
        QCOMPARE(p.groupLabel(), QStringLiteral("P6"));
        QCOMPARE(p.groupNumber(), 6);
    }

    void tripletBoundaries()
    {
        const auto u = PatchAddress::parse(QStringLiteral("U1-11"))->midiTriplet();
        QCOMPARE(u.lsb, 0);
        QCOMPARE(u.program, 0);
        const auto u8 = PatchAddress::parse(QStringLiteral("U8-88"))->midiTriplet();
        QCOMPARE(u8.lsb, 7);
        QCOMPARE(u8.program, 63);
    }
};

QTEST_APPLESS_MAIN(TestPatchAddress)
#include "test_patch_address.moc"
