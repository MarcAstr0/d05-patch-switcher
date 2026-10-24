// Runs the built binary with no display and no Qt platform plugin configured.
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

#ifndef D05_BINARY
#error D05_BINARY must be defined
#endif

namespace {
struct Result {
    int code = -1;
    QString out, err;
    bool crashed = false;
};

Result run(const QStringList &args)
{
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const char *k : {"QT_QPA_PLATFORM", "DISPLAY", "WAYLAND_DISPLAY"})
        env.remove(QString::fromLatin1(k));
    p.setProcessEnvironment(env);
    p.start(QStringLiteral(D05_BINARY), args);
    Result r;
    if (!p.waitForFinished(15000)) {
        p.kill();
        p.waitForFinished();
        return r;
    }
    r.crashed = p.exitStatus() != QProcess::NormalExit;
    r.code = p.exitCode();
    r.out = QString::fromLocal8Bit(p.readAllStandardOutput());
    r.err = QString::fromLocal8Bit(p.readAllStandardError());
    return r;
}
}  // namespace

class TestCliProcess : public QObject {
    Q_OBJECT
private slots:
    void sendFake()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Result r = run({QStringLiteral("--fake-midi"), QStringLiteral("--send"), QStringLiteral("P3-45"),
                              QStringLiteral("--config-dir"), tmp.path()});
        QVERIFY(!r.crashed);
        QCOMPARE(r.code, 0);
        QCOMPARE(r.out, QStringLiteral("B0 00 57\nB0 20 0A\nC0 1C\n"));
        QVERIFY(QDir(tmp.path()).isEmpty());
    }

    void noDisplayExits_data()
    {
        QTest::addColumn<QStringList>("args");
        QTest::addColumn<int>("code");
        QTest::newRow("help") << QStringList{QStringLiteral("--help")} << 0;
        QTest::newRow("help-all") << QStringList{QStringLiteral("--help-all")} << 0;
        QTest::newRow("help-all+send") << QStringList{QStringLiteral("--send"), QStringLiteral("P1-11"), QStringLiteral("--help-all")} << 0;
        QTest::newRow("version") << QStringList{QStringLiteral("--version")} << 0;
        QTest::newRow("channel 0") << QStringList{QStringLiteral("--channel"), QStringLiteral("0")} << 64;
        QTest::newRow("bogus") << QStringList{QStringLiteral("--bogus")} << 64;
        QTest::newRow("send=") << QStringList{QStringLiteral("--fake-midi"), QStringLiteral("--send=P1-11")} << 0;
        QTest::newRow("bad address") << QStringList{QStringLiteral("--fake-midi"), QStringLiteral("--send"), QStringLiteral("P1-19")} << 1;
    }
    void noDisplayExits()
    {
        QFETCH(QStringList, args);
        QFETCH(int, code);
        const Result r = run(args);
        QVERIFY2(!r.crashed, qPrintable(r.err));
        QCOMPARE(r.code, code);
    }

    void listPortsBeatsSend()
    {
        const Result r = run({QStringLiteral("--fake-midi"), QStringLiteral("--list-ports"), QStringLiteral("--send"),
                              QStringLiteral("P1-11")});
        QCOMPARE(r.code, 0);
        QVERIFY(r.out.contains(QStringLiteral("MIDI outputs")));
        QVERIFY(!r.out.contains(QStringLiteral("B0 00 57")));
    }
};

QTEST_GUILESS_MAIN(TestCliProcess)
#include "test_cli_process.moc"
