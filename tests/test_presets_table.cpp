// Guards the preset data file. It is the one piece of this project that cannot
// be regenerated from code, so the facts below come straight from the Roland
// D-05 Parameter Guide, pages 23-28.

#include <QFile>
#include <QSet>
#include <QTest>
#include <QTextStream>

class TestPresetsTable : public QObject
{
    Q_OBJECT

    struct Row { QString group; int bank; int number; QString name; QString mode; };
    QList<Row> m_rows;

private slots:
    void initTestCase()
    {
        QFile f(QStringLiteral(":/data/d05-presets.tsv"));
        QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "resource missing");
        QTextStream ts(&f);
        const QString header = ts.readLine();
        QCOMPARE(header, QStringLiteral("group\tbank\tnumber\tname\tmode"));
        while (!ts.atEnd()) {
            const QString line = ts.readLine();
            if (line.isEmpty())
                continue;
            const QStringList c = line.split(QLatin1Char('\t'));
            QCOMPARE(c.size(), 5);
            m_rows.append({c[0], c[1].toInt(), c[2].toInt(), c[3], c[4]});
        }
    }

    void hasAllPresets()
    {
        QCOMPARE(m_rows.size(), 6 * 8 * 8);
        QSet<QString> seen;
        for (const Row &r : m_rows) {
            QVERIFY(r.group.size() == 2 && r.group[0] == QLatin1Char('P'));
            QVERIFY(r.bank >= 1 && r.bank <= 8);
            QVERIFY(r.number >= 1 && r.number <= 8);
            QVERIFY(!r.name.isEmpty());
            QVERIFY(r.mode == QLatin1String("W") || r.mode == QLatin1String("D")
                    || r.mode == QLatin1String("S"));
            const QString key = r.group + QString::number(r.bank) + QString::number(r.number);
            QVERIFY2(!seen.contains(key), qPrintable(QStringLiteral("duplicate ") + key));
            seen.insert(key);
        }
    }

    void spotChecks_data()
    {
        QTest::addColumn<QString>("group");
        QTest::addColumn<int>("bank");
        QTest::addColumn<int>("number");
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("mode");
        QTest::newRow("P1-11") << "P1" << 1 << 1 << "Fantasia" << "D";
        QTest::newRow("P1-21") << "P1" << 2 << 1 << "DigitalNativeDance" << "D";
        QTest::newRow("P1-34") << "P1" << 3 << 4 << "PressureMe Strings" << "D"; // wrapped in the PDF
        QTest::newRow("P1-63") << "P1" << 6 << 3 << "E-Bass and E-Piano" << "S";
        QTest::newRow("P1-88") << "P1" << 8 << 8 << "PCM E-Piano" << "D";
        QTest::newRow("P2-11") << "P2" << 1 << 1 << "Neo Horizon" << "D";
        QTest::newRow("P2-57") << "P2" << 5 << 7 << "Mono Octabass" << "W";
        QTest::newRow("P3-48") << "P3" << 4 << 8 << "Whistling Soldiers" << "S";
        QTest::newRow("P4-52") << "P4" << 5 << 2 << "Drums Set 1" << "S";
        QTest::newRow("P5-24") << "P5" << 2 << 4 << "Strings Horn" << "D"; // wrapped in the PDF
        QTest::newRow("P6-58") << "P6" << 5 << 8 << "Zean- - -" << "D";
        QTest::newRow("P6-88") << "P6" << 8 << 8 << "Big Waves" << "D";
    }

    void spotChecks()
    {
        QFETCH(QString, group);
        QFETCH(int, bank);
        QFETCH(int, number);
        QFETCH(QString, name);
        QFETCH(QString, mode);
        for (const Row &r : m_rows) {
            if (r.group == group && r.bank == bank && r.number == number) {
                QCOMPARE(r.name, name);
                QCOMPARE(r.mode, mode);
                return;
            }
        }
        QFAIL("row not found");
    }
};

QTEST_GUILESS_MAIN(TestPresetsTable)
#include "test_presets_table.moc"
