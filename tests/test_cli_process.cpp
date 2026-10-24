// Runs the built binary with no display and no Qt platform plugin configured.
#include <QDir>
#include <QFileInfo>
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
    bool started = true;
    QString errorString;
    // For failure messages.
    QString describe() const
    {
        return QStringLiteral("started=%1 crashed=%2 exit=%3 error=\"%4\"\nstdout:\n%5\nstderr:\n%6")
            .arg(started).arg(crashed).arg(code).arg(errorString, out, err);
    }
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
    if (!p.waitForStarted(10000)) {
        r.started = false;
        r.errorString = QStringLiteral("failed to start ") + QStringLiteral(D05_BINARY) + QStringLiteral(": ")
                        + p.errorString();
        return r;
    }
    if (!p.waitForFinished(15000)) {
        p.kill();
        p.waitForFinished();
        r.errorString = QStringLiteral("timeout: ") + p.errorString();
        r.out = QString::fromLocal8Bit(p.readAllStandardOutput());
        r.err = QString::fromLocal8Bit(p.readAllStandardError());
        return r;
    }
    r.errorString = p.errorString();
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
        QVERIFY2(!r.crashed && r.code == 0, qPrintable(r.describe()));
        QVERIFY2(QString(r.out).replace(QStringLiteral("\r\n"), QStringLiteral("\n")) == QStringLiteral("B0 00 57\nB0 20 0A\nC0 1C\n"),
                 qPrintable(r.describe()));
        QVERIFY(QDir(tmp.path()).isEmpty());
    }

    void screenshotWithNoDisplayEnvironment()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString png = tmp.filePath(QStringLiteral("shot.png"));
        // run() already unsets QT_QPA_PLATFORM, DISPLAY and WAYLAND_DISPLAY.
        const Result r = run({QStringLiteral("--screenshot"), png});
        QVERIFY2(!r.crashed && r.code == 0, qPrintable(r.describe()));
        QVERIFY2(QFileInfo(png).size() > 0, qPrintable(r.describe()));
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
        QVERIFY2(!r.crashed && r.code == code, qPrintable(r.describe()));
    }

    void listPortsBeatsSend()
    {
        const Result r = run({QStringLiteral("--fake-midi"), QStringLiteral("--list-ports"), QStringLiteral("--send"),
                              QStringLiteral("P1-11")});
        QVERIFY2(r.code == 0, qPrintable(r.describe()));
        QVERIFY2(r.out.contains(QStringLiteral("MIDI outputs")), qPrintable(r.describe()));
        QVERIFY2(!r.out.contains(QStringLiteral("B0 00 57")), qPrintable(r.describe()));
    }
};

QTEST_GUILESS_MAIN(TestCliProcess)
#include "test_cli_process.moc"
