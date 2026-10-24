#include "app/Cli.h"
#include "midi/FakeMidiBackend.h"

#include <QTest>
#include <QTextStream>

using namespace d05;

namespace {
QStringList argv(std::initializer_list<const char *> v)
{
    QStringList l{QStringLiteral("d05-patch-switcher")};
    for (const char *s : v)
        l << QString::fromLatin1(s);
    return l;
}

struct Run {
    int code = -1;
    QString out, err;
};

Run send(const QStringList &args, FakeMidiBackend &fake)
{
    Run r;
    CliOptions o;
    QString msg;
    int code = 0;
    if (Cli::parse(args, &o, &msg, &code) == Cli::ParseStatus::ExitNow) {
        r.code = code;
        r.err = msg;
        return r;
    }
    QTextStream out(&r.out), err(&r.err);
    r.code = o.listPorts ? Cli::runListPorts(fake, out, err) : Cli::runSend(o, fake, out, err);
    return r;
}
}  // namespace

class TestCli : public QObject {
    Q_OBJECT
private slots:
    void parseOptions()
    {
        CliOptions o;
        QString msg;
        int code = 0;
        QCOMPARE(Cli::parse(argv({"--port", "Hapax", "--in-port", "Hapax", "--channel", "10", "--send", "P3-45",
                                  "--fake-midi", "--config-dir", "/tmp/x"}),
                            &o, &msg, &code),
                 Cli::ParseStatus::Run);
        QCOMPARE(o.port, QStringLiteral("Hapax"));
        QCOMPARE(o.inPort, QStringLiteral("Hapax"));
        QCOMPARE(o.channel, 10);
        QVERIFY(o.hasSend);
        QCOMPARE(o.sendAddress, QStringLiteral("P3-45"));
        QVERIFY(o.fakeMidi);
        QCOMPARE(o.configDir, QStringLiteral("/tmp/x"));
        QVERIFY(o.headless());
    }

    void screenshotOptions()
    {
        for (const QString &state : Cli::screenshotStates()) {
            CliOptions o;
            QString msg;
            int code = 0;
            QCOMPARE(Cli::parse(argv({"--screenshot", "a.png", "--screenshot-state", state.toLatin1().constData()}),
                                &o, &msg, &code),
                     Cli::ParseStatus::Run);
            QVERIFY(o.wantsScreenshot());
            QVERIFY(!o.headless());
            QCOMPARE(o.screenshotState, state);
        }
        CliOptions o;
        QString msg;
        int code = 0;
        QCOMPARE(Cli::parse(argv({"--screenshot", "a.png"}), &o, &msg, &code), Cli::ParseStatus::Run);
        QCOMPARE(o.screenshotState, QStringLiteral("default"));
        QCOMPARE(Cli::screenshotStates().size(), 14);
        QCOMPARE(o.screenshotSize, QSize(960, 560));
        QCOMPARE(Cli::parse(argv({"--screenshot", "a.png", "--screenshot-size", "800x480"}), &o, &msg, &code),
                 Cli::ParseStatus::Run);
        QCOMPARE(o.screenshotSize, QSize(800, 480));
        QCOMPARE(Cli::parse(argv({"--screenshot", "a.png", "--screenshot-size", "1300x700"}), &o, &msg, &code),
                 Cli::ParseStatus::Run);
        QCOMPARE(o.screenshotSize, QSize(1300, 700));
    }

    void usageErrors_data()
    {
        QTest::addColumn<QStringList>("args");
        QTest::newRow("channel 0") << argv({"--channel", "0"});
        QTest::newRow("channel 17") << argv({"--channel", "17"});
        QTest::newRow("channel text") << argv({"--channel", "x"});
        QTest::newRow("unknown") << argv({"--bogus"});
        QTest::newRow("positional") << argv({"stray"});
        QTest::newRow("screenshot bad state") << argv({"--screenshot", "a.png", "--screenshot-state", "bogus"});
        QTest::newRow("screenshot state alone") << argv({"--screenshot-state", "search"});
        QTest::newRow("screenshot with send") << argv({"--screenshot", "a.png", "--send", "P1-11"});
        QTest::newRow("size too small") << argv({"--screenshot", "a.png", "--screenshot-size", "799x480"});
        QTest::newRow("size too short") << argv({"--screenshot", "a.png", "--screenshot-size", "800x479"});
        QTest::newRow("size malformed") << argv({"--screenshot", "a.png", "--screenshot-size", "800"});
        QTest::newRow("size text") << argv({"--screenshot", "a.png", "--screenshot-size", "axb"});
        QTest::newRow("size alone") << argv({"--screenshot-size", "900x600"});
        QTest::newRow("missing value") << argv({"--port"});
    }
    void usageErrors()
    {
        QFETCH(QStringList, args);
        CliOptions o;
        QString msg;
        int code = 0;
        QCOMPARE(Cli::parse(args, &o, &msg, &code), Cli::ParseStatus::ExitNow);
        QCOMPARE(code, Exit::Usage);
        QVERIFY(!msg.isEmpty());
    }

    void helpAllWinsOverSend()
    {
        CliOptions o;
        QString msg;
        int code = -1;
        QCOMPARE(Cli::parse(argv({"--send", "P1-11", "--help-all"}), &o, &msg, &code), Cli::ParseStatus::ExitNow);
        QCOMPARE(code, 0);
        QVERIFY(msg.contains(QStringLiteral("--send")));
    }

    void spellings()
    {
        CliOptions o;
        QString msg;
        int code = 0;
        QCOMPARE(Cli::parse(argv({"--send=P2-22"}), &o, &msg, &code), Cli::ParseStatus::Run);
        QCOMPARE(o.sendAddress, QStringLiteral("P2-22"));
        QVERIFY(o.headless());
    }

    void portMatchingLikeTheGui()
    {
        FakeMidiBackend f;
        QCOMPARE(send(argv({"--port", "Fake D-05 In", "--send", "P1-11"}), f).code, 0);
        QCOMPARE(send(argv({"--port", "FAKE", "--send", "P1-11"}), f).code, 0);
    }

    void sendBytes()
    {
        FakeMidiBackend f;
        const Run r = send(argv({"--fake-midi", "--channel", "10", "--send", "P3-45"}), f);
        QCOMPARE(r.code, 0);
        QCOMPARE(r.out, QStringLiteral("B9 00 57\nB9 20 0A\nC9 1C\n"));
        QCOMPARE(f.sentMessages().size(), 3);
    }

    void sendDefaultsToChannel1AndSubstringPort()
    {
        FakeMidiBackend f;
        const Run r = send(argv({"--port", "fake d-05", "--send", "p1-11"}), f);
        QCOMPARE(r.code, 0);
        QCOMPARE(r.out, QStringLiteral("B0 00 57\nB0 20 08\nC0 00\n"));
    }

    void badAddressIsExit1()
    {
        FakeMidiBackend f;
        const Run r = send(argv({"--fake-midi", "--send", "P1-19"}), f);
        QCOMPARE(r.code, Exit::BadAddress);
        QVERIFY(f.sentMessages().isEmpty());
    }

    void missingPortIsExit2()
    {
        FakeMidiBackend f;
        QCOMPARE(send(argv({"--port", "nonesuch", "--send", "P1-11"}), f).code, Exit::PortNotFound);
        QCOMPARE(send(argv({"--send", "P1-11"}), f).code, Exit::PortNotFound);  // no port, not fake
        QVERIFY(f.sentMessages().isEmpty());
    }

    void midiErrorIsExit3()
    {
        FakeMidiBackend f;
        f.setFailOpen(true);
        QCOMPARE(send(argv({"--fake-midi", "--send", "P1-11"}), f).code, Exit::MidiError);
        FakeMidiBackend g;
        g.setListError(QStringLiteral("boom"));
        QCOMPARE(send(argv({"--fake-midi", "--send", "P1-11"}), g).code, Exit::MidiError);
    }

    void listPortsWinsOverSend()
    {
        FakeMidiBackend f;
        const Run r = send(argv({"--fake-midi", "--list-ports", "--send", "P1-11"}), f);
        QCOMPARE(r.code, 0);
        QVERIFY(r.out.contains(QStringLiteral("MIDI outputs")));
        QVERIFY(f.sentMessages().isEmpty());
    }

    void listPorts()
    {
        FakeMidiBackend f;
        const Run r = send(argv({"--fake-midi", "--list-ports"}), f);
        QCOMPARE(r.code, 0);
        QVERIFY(r.out.contains(QStringLiteral("[0] Fake D-05 In")));
        QVERIFY(r.out.contains(QStringLiteral("[0] Fake D-05 Out")));
    }
};

QTEST_GUILESS_MAIN(TestCli)
#include "test_cli.moc"
