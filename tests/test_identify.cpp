#include "core/Identify.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <cstdio>
#include <unistd.h>

using namespace d05;
using namespace d05::Identify;
using C = Confidence;

Q_DECLARE_METATYPE(d05::Model)

class TestIdentify : public QObject {
    Q_OBJECT
private:
    static bool put(const QString &path, const QByteArray &data)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
    }

private slots:
    void rule1_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("hasModel");
        QTest::addColumn<int>("model");
        QTest::addColumn<int>("confidence");
        QTest::addColumn<bool>("firstGen");
        auto row = [](const char *tag, const char *name, bool has, Model m, C c, bool fg) {
            QTest::newRow(tag) << QString::fromLatin1(name) << has << int(m) << int(c) << fg;
        };
        row("d05 linux", "Boutique:Boutique MIDI 1 24:0", true, Model::D05, C::Likely, false);
        row("first gen", "Boutiq:Boutiq MIDI 1 28:0", true, Model::JP08, C::Ambiguous, true);
        row("ju06a", "JU-06A:JU-06A MIDI 1 28:0", true, Model::JU06A, C::Certain, false);
        row("ju06a lower", "ju-06a", true, Model::JU06A, C::Certain, false);
        row("jp08 named", "JP-08", true, Model::JP08, C::Certain, false);
        row("d05 named", "D-05 MIDI 1", true, Model::D05, C::Certain, false);
        row("upper", "BOUTIQUE MIDI", true, Model::D05, C::Likely, false);
        row("upper short", "BOUTIQ MIDI", true, Model::JP08, C::Ambiguous, true);
        row("longest first", "Boutique JU-06A", true, Model::JU06A, C::Certain, false);
        row("hapax", "HAPAX Puerto 1", false, Model::D05, C::None, false);
        row("fake", "Fake D-05 In", true, Model::D05, C::Certain, false);
        row("through", "Midi Through:Midi Through Port-0 14:0", false, Model::D05, C::None, false);
        row("empty", "", false, Model::D05, C::None, false);
    }
    void rule1()
    {
        QFETCH(QString, name);
        QFETCH(bool, hasModel);
        QFETCH(int, model);
        QFETCH(int, confidence);
        QFETCH(bool, firstGen);
        const Result r = byPortName(name);
        QCOMPARE(r.model.has_value(), hasModel);
        if (hasModel)
            QCOMPARE(int(*r.model), model);
        QCOMPARE(int(r.confidence), confidence);
        QCOMPARE(r.firstGeneration, firstGen);
    }

    void serialPrefixes_data()
    {
        QTest::addColumn<QString>("usbid");
        QTest::addColumn<QString>("serial");
        QTest::addColumn<bool>("hasModel");
        QTest::addColumn<int>("model");
        QTest::newRow("jp08") << "0582:01b5" << "BQJP2400123" << true << int(Model::JP08);
        QTest::newRow("ju06a") << "0582:023a" << "BQJU2400123" << true << int(Model::JU06A);
        QTest::newRow("d05") << "0582:01ff" << "BQ_D2400123" << true << int(Model::D05);
        QTest::newRow("newline") << "0582:01b5\n" << "BQJP2400123\n" << true << int(Model::JP08);
        QTest::newRow("other BQ") << "0582:01b5" << "BQJX2400123" << false << 0;
        QTest::newRow("other vendor") << "1234:01b5" << "BQJP2400123" << false << 0;
        QTest::newRow("no serial") << "0582:01b5" << "" << false << 0;
        QTest::newRow("no usbid") << "" << "BQJP1" << false << 0;
        QTest::newRow("jp08 id with ju serial") << "0582:01b5" << "BQJU2400123" << false << 0;
        QTest::newRow("ju06a id with jp serial") << "0582:023a" << "BQJP2400123" << false << 0;
        QTest::newRow("d05 id with jp serial") << "0582:01ff" << "BQJP2400123" << false << 0;
        QTest::newRow("unknown product id") << "0582:0001" << "BQJP2400123" << false << 0;
        QTest::newRow("upper case id") << "0582:01FF" << "BQ_D2400123" << true << int(Model::D05);
    }
    void serialPrefixes()
    {
        QFETCH(QString, usbid);
        QFETCH(QString, serial);
        QFETCH(bool, hasModel);
        QFETCH(int, model);
        const auto r = bySerial(usbid, serial);
        QCOMPARE(r.has_value(), hasModel);
        if (hasModel) {
            QCOMPARE(int(*r->model), model);
            QCOMPARE(r->confidence, C::Certain);
            QVERIFY(!r->firstGeneration);
        }
    }

    void rule2ReadsFakeRoots()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString proc = root.filePath(QStringLiteral("proc")), sys = root.filePath(QStringLiteral("sys"));
        QVERIFY(put(proc + QStringLiteral("/asound/card3/usbid"), "0582:01b5\n"));
        // device/ is a directory so that device/../serial is the file beside it.
        QVERIFY(QDir().mkpath(sys + QStringLiteral("/class/sound/card3/device")));
        QVERIFY(put(sys + QStringLiteral("/class/sound/card3/serial"), "BQJP24xx99\n"));
        const auto r = byCard(3, proc, sys);
        QVERIFY(r.has_value());
        QCOMPARE(*r->model, Model::JP08);
        QCOMPARE(r->confidence, C::Certain);
    }

    void rule2FailsSilentlyOnMissingFiles()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString proc = root.filePath(QStringLiteral("proc")), sys = root.filePath(QStringLiteral("sys"));
        QVERIFY(!byCard(0, proc, sys).has_value());  // nothing at all
        QVERIFY(put(proc + QStringLiteral("/asound/card0/usbid"), "0582:01ff\n"));
        QVERIFY(!byCard(0, proc, sys).has_value());  // serial missing
        QVERIFY(!byCard(-1, proc, sys).has_value());
        QVERIFY(!byCard(0, QStringLiteral("/nonexistent"), QStringLiteral("/nonexistent")).has_value());
    }

    void clientNumber()
    {
        QCOMPARE(alsaClientOf(QStringLiteral("Boutiq:Boutiq MIDI 1 28:0")).value_or(-1), 28);
        QCOMPARE(alsaClientOf(QStringLiteral("Boutique:Boutique MIDI 1 24:0")).value_or(-1), 24);
        QVERIFY(!alsaClientOf(QStringLiteral("Fake D-05 In")).has_value());
        QVERIFY(!alsaClientOf(QStringLiteral("HAPAX Puerto 1")).has_value());
        QVERIFY(!alsaClientOf(QString()).has_value());
    }

    void identifyPortsFallsBackToRule1()
    {
        // Client numbers that are almost certainly not USB cards: rule 2 must change nothing.
        const QStringList names{QStringLiteral("Boutiq:Boutiq MIDI 1 9999:0"), QStringLiteral("Fake D-05 In"),
                                QStringLiteral("HAPAX Puerto 1")};
        const QList<Result> off = identifyPorts(names, false);
        const QList<Result> on = identifyPorts(names, true);
        QCOMPARE(off.size(), 3);
        QCOMPARE(on, off);
        QCOMPARE(off.at(0).confidence, C::Ambiguous);
        QCOMPARE(off.at(1).confidence, C::Certain);
        QCOMPARE(off.at(2).confidence, C::None);
    }

#ifdef Q_OS_LINUX
    void rule2IsSilentWhenTheSequencerCannotBeOpened()
    {
        // No alsa configuration at all: snd_seq_open("default") fails, which alsa-lib would print.
        qputenv("ALSA_CONFIG_PATH", "/nonexistent/alsa.conf");
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile capture(dir.filePath(QStringLiteral("stderr.txt")));
        QVERIFY(capture.open(QIODevice::ReadWrite));
        fflush(stderr);
        const int saved = dup(2);
        QVERIFY(saved >= 0);
        QVERIFY(dup2(capture.handle(), 2) >= 0);
        const QList<Result> r = identifyPorts({QStringLiteral("Boutiq:Boutiq MIDI 1 9999:0")}, true);
        fflush(stderr);
        dup2(saved, 2);
        close(saved);
        qunsetenv("ALSA_CONFIG_PATH");
        QCOMPARE(r.size(), 1);
        QCOMPARE(r.at(0).confidence, C::Ambiguous);  // rule 1's answer stands
        capture.seek(0);
        QCOMPARE(capture.readAll(), QByteArray());
    }
#endif

    void confidenceWords()
    {
        QCOMPARE(confidenceText(C::Certain), QStringLiteral("certain"));
        QCOMPARE(confidenceText(C::Likely), QStringLiteral("likely"));
        QCOMPARE(confidenceText(C::Ambiguous), QStringLiteral("ambiguous"));
        QCOMPARE(confidenceText(C::None), QStringLiteral("none"));
    }
};

QTEST_GUILESS_MAIN(TestIdentify)
#include "test_identify.moc"
