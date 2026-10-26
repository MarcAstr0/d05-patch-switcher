// Guards the JP-08 and JU-06A factory name tables (data/jp08-patches.tsv,
// data/ju06a-patches.tsv), extracted from Roland's sound-list PDFs by
// scripts/extract-boutique-lists.py. Read from the source tree.

#include <QFile>
#include <QSet>
#include <QTest>
#include <QTextStream>

class TestBoutiqueTables : public QObject
{
    Q_OBJECT

    struct Row { QString engine; int bank; int number; QString name; };

    static QList<Row> load(const QString &file, const QString &header, bool hasEngine)
    {
        QList<Row> rows;
        QFile f(QStringLiteral(D05_DATA_DIR "/") + file);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return rows;
        QTextStream ts(&f);
        if (ts.readLine() != header)
            return {};
        while (!ts.atEnd()) {
            const QString line = ts.readLine();
            if (line.isEmpty())
                continue;
            const QStringList c = line.split(QLatin1Char('\t'));
            if (hasEngine && c.size() == 4)
                rows.append({c[0], c[1].toInt(), c[2].toInt(), c[3]});
            else if (!hasEngine && c.size() == 3)
                rows.append({QString(), c[0].toInt(), c[1].toInt(), c[2]});
            else
                return {};
        }
        return rows;
    }

    static void checkShape(const QList<Row> &rows, int expected)
    {
        QCOMPARE(rows.size(), expected);
        QSet<QString> seen;
        for (const Row &r : rows) {
            QVERIFY(r.bank >= 1 && r.bank <= 8);
            QVERIFY(r.number >= 1 && r.number <= 8);
            QVERIFY(!r.name.isEmpty());
            QCOMPARE(r.name, r.name.trimmed());
            const QString key = r.engine + QLatin1Char(':') + QString::number(r.bank)
                                + QLatin1Char('-') + QString::number(r.number);
            QVERIFY2(!seen.contains(key), qPrintable(QStringLiteral("duplicate ") + key));
            seen.insert(key);
        }
    }

    static QString nameOf(const QList<Row> &rows, const QString &engine, int bank, int number)
    {
        for (const Row &r : rows)
            if (r.engine == engine && r.bank == bank && r.number == number)
                return r.name;
        return QStringLiteral("<missing>");
    }

private slots:
    void jp08()
    {
        const QList<Row> rows = load(QStringLiteral("jp08-patches.tsv"),
                                     QStringLiteral("bank\tnumber\tname"), false);
        checkShape(rows, 64);
        QCOMPARE(nameOf(rows, QString(), 1, 1), QStringLiteral("NEG Sync"));
        QCOMPARE(nameOf(rows, QString(), 4, 5), QStringLiteral("Sine Glass"));
        QCOMPARE(nameOf(rows, QString(), 8, 8), QStringLiteral("Space Boomerang"));
    }

    void ju06a()
    {
        const QList<Row> rows = load(QStringLiteral("ju06a-patches.tsv"),
                                     QStringLiteral("engine\tbank\tnumber\tname"), true);
        checkShape(rows, 128);
        int n60 = 0;
        for (const Row &r : rows) {
            QVERIFY(r.engine == QLatin1String("60") || r.engine == QLatin1String("106"));
            n60 += r.engine == QLatin1String("60");
        }
        QCOMPARE(n60, 64);
        const QString s60 = QStringLiteral("60"), s106 = QStringLiteral("106");
        QCOMPARE(nameOf(rows, s60, 1, 1), QStringLiteral("LD Echolocator"));
        QCOMPARE(nameOf(rows, s60, 8, 8), QStringLiteral("1982 Synth Drum"));
        QCOMPARE(nameOf(rows, s106, 1, 1), QStringLiteral("Brass"));
        QCOMPARE(nameOf(rows, s106, 1, 6), QStringLiteral("Brass & Strings"));
        QCOMPARE(nameOf(rows, s106, 4, 5), QStringLiteral("Reso Up"));
        QCOMPARE(nameOf(rows, s106, 8, 6), QString::fromUtf8("Louis\xE2\x80\x99 Weapon"));
        QCOMPARE(nameOf(rows, s106, 8, 8), QStringLiteral("Takeoff"));
    }
};

QTEST_GUILESS_MAIN(TestBoutiqueTables)
#include "test_boutique_tables.moc"
