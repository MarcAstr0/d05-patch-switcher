#include "core/Model.h"
#include "core/PatchAddress.h"

#include <QtTest>

using namespace d05;

class TestModel : public QObject {
    Q_OBJECT
private slots:
    void info()
    {
        QCOMPARE(modelInfo(Model::D05).catalogueSize, 896);
        QCOMPARE(modelInfo(Model::JP08).catalogueSize, 64);
        QCOMPARE(modelInfo(Model::JU06A).catalogueSize, 128);
        QCOMPARE(modelInfo(Model::JU06A).name, QStringLiteral("JU-06A"));
        QCOMPARE(modelFromId(QStringLiteral("jp08")).value(), Model::JP08);
        QVERIFY(!modelFromId(QStringLiteral("x")));
    }
    void roundTrip()
    {
        for (Model m : allModels()) {
            const int n = modelInfo(m).catalogueSize;
            for (int i = 0; i < n; ++i) {
                const auto a = PatchAddress::fromCatalogueIndex(m, i);
                QVERIFY(a);
                QCOMPARE(a->catalogueIndex(), i);
                const auto b = PatchAddress::parse(a->toString(), m);
                QVERIFY2(b, qPrintable(a->toString()));
                QCOMPARE(*b, *a);
            }
            QVERIFY(!PatchAddress::fromCatalogueIndex(m, n));
            QVERIFY(!PatchAddress::fromCatalogueIndex(m, -1));
        }
    }
    void parseTable()
    {
        const auto jp = [](const char *t) { return PatchAddress::parse(QString::fromLatin1(t), Model::JP08); };
        QCOMPARE(jp("45")->toString(), QStringLiteral("4-5"));
        QCOMPARE(jp(" 4-5 ")->catalogueIndex(), 28);
        QCOMPARE(PatchAddress::parse(QStringLiteral("106:45"), Model::JU06A)->toString(), QStringLiteral("106:4-5"));
        QCOMPARE(PatchAddress::parse(QStringLiteral("60:4-5"), Model::JU06A)->catalogueIndex(), 28);
        QCOMPARE(PatchAddress::parse(QStringLiteral("106:1-1"), Model::JU06A)->catalogueIndex(), 64);
        QCOMPARE(PatchAddress::parse(QStringLiteral("45"), Model::JU06A, 106)->toString(), QStringLiteral("106:4-5"));
        QCOMPARE(PatchAddress::parse(QStringLiteral("4-5"), Model::JU06A, 60)->toString(), QStringLiteral("60:4-5"));
        QVERIFY(!PatchAddress::parse(QStringLiteral("45"), Model::JU06A));
        QVERIFY(!PatchAddress::parse(QStringLiteral("45"), Model::JU06A, 7));
        QCOMPARE(PatchAddress::parse(QStringLiteral("P3-45"), Model::D05)->toString(), QStringLiteral("P3-45"));
        for (const char *bad : {"0-1", "9-9", "4-9", "P1-11", "", "4--5", "4-", "445"})
            QVERIFY2(!jp(bad), bad);
        for (const char *bad : {"107:1-1", "60-45", "0-1", "9-9", "4-9", "60:", ":45", "60:0-1", "61:11"})
            QVERIFY2(!PatchAddress::parse(QString::fromLatin1(bad), Model::JU06A, 60), bad);
    }
    void messages()
    {
        const auto jp = *PatchAddress::parse(QStringLiteral("4-5"), Model::JP08);
        QCOMPARE(jp.midiMessages(5), (QList<QByteArray>{QByteArray::fromHex("c41c")}));
        const auto ju = *PatchAddress::parse(QStringLiteral("60:1-6"), Model::JU06A);
        QCOMPARE(ju.midiMessages(12, 60), (QList<QByteArray>{QByteArray::fromHex("cb05")}));
        QVERIFY(ju.midiMessages(12, 106).isEmpty());
        QVERIFY(ju.midiMessages(12).isEmpty());
        QVERIFY(ju.midiMessages(0, 60).isEmpty());
        const auto d = *PatchAddress::parse(QStringLiteral("P3-45"), Model::D05);
        QCOMPARE(d.midiMessages(1),
                 (QList<QByteArray>{QByteArray::fromHex("b00057"), QByteArray::fromHex("b0200a"),
                                    QByteArray::fromHex("c01c")}));
        const auto t = d.midiTriplet();
        QCOMPARE(t.lsb, 10);
        QVERIFY(!(jp == *PatchAddress::fromCatalogueIndex(Model::JU06A, 28)));
    }
    void d05Channel16()
    {
        const auto u = *PatchAddress::parse(QStringLiteral("U3-45"), Model::D05);
        QCOMPARE(u.midiMessages(16),
                 (QList<QByteArray>{QByteArray::fromHex("bf0057"), QByteArray::fromHex("bf2002"),
                                    QByteArray::fromHex("cf1c")}));
        const auto p = *PatchAddress::parse(QStringLiteral("P3-45"), Model::D05);
        QCOMPARE(p.midiMessages(16),
                 (QList<QByteArray>{QByteArray::fromHex("bf0057"), QByteArray::fromHex("bf200a"),
                                    QByteArray::fromHex("cf1c")}));
        for (const PatchAddress &a : {u, p}) {
            const auto t = a.midiTriplet();
            const QList<QByteArray> m = a.midiMessages(16);
            QCOMPARE(m[0], (QByteArray{"\xbf\x00", 2} + char(t.msb)));
            QCOMPARE(m[1], (QByteArray{"\xbf\x20", 2} + char(t.lsb)));
            QCOMPARE(m[2], (QByteArray{"\xcf", 1} + char(t.program)));
        }
    }
    void deviceEngineAndEdges()
    {
        const auto jp = *PatchAddress::parse(QStringLiteral("4-5"), Model::JP08);
        const QList<QByteArray> want{QByteArray::fromHex("c41c")};
        QCOMPARE(jp.midiMessages(5, 60), want);
        QCOMPARE(jp.midiMessages(5, 106), want);
        QCOMPARE(jp.midiMessages(5, std::nullopt), want);
        const auto ju = *PatchAddress::parse(QStringLiteral("106:8-8"), Model::JU06A);
        QCOMPARE(ju.midiMessages(16, 106), (QList<QByteArray>{QByteArray::fromHex("cf3f")}));
        QVERIFY(!PatchAddress::parse(QStringLiteral("P7-11"), Model::D05));
        QVERIFY(!PatchAddress::parse(QStringLiteral("U9-11"), Model::D05));
        for (const char *t : {"45", "4-5"}) {
            QVERIFY(!PatchAddress::parse(QString::fromLatin1(t), Model::JU06A, std::nullopt));
            QVERIFY(!PatchAddress::parse(QString::fromLatin1(t), Model::JU06A, 61));
        }
        QCOMPARE(jp.groupNumber(), 0);
        QVERIFY(jp.groupLabel().isEmpty());
        QCOMPARE(ju.groupNumber(), 0);
        QVERIFY(ju.groupLabel().isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestModel)
#include "test_model.moc"
