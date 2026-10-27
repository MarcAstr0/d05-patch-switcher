// Runs the built binary with no display and no Qt platform plugin configured.
#include "app/AppSettings.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
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

Result run(const QStringList &args, bool offscreen = false)
{
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const char *k : {"QT_QPA_PLATFORM", "DISPLAY", "WAYLAND_DISPLAY"})
        env.remove(QString::fromLatin1(k));
    if (offscreen)
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
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

    // The real binary reads settings.ini from --config-dir: the saved channel applies to
    // --send, and --channel still beats it.
    void sendUsesTheSavedChannel()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QFile ini(tmp.filePath(QStringLiteral("settings.ini")));
        QVERIFY(ini.open(QIODevice::WriteOnly));
        ini.write("[midi]\nchannel=10\n");
        ini.close();
        Result r = run({QStringLiteral("--fake-midi"), QStringLiteral("--send"), QStringLiteral("P3-45"),
                        QStringLiteral("--config-dir"), tmp.path()});
        QVERIFY2(!r.crashed && r.code == 0, qPrintable(r.describe()));
        QVERIFY2(QString(r.out).replace(QStringLiteral("\r\n"), QStringLiteral("\n"))
                     == QStringLiteral("B9 00 57\nB9 20 0A\nC9 1C\n"),
                 qPrintable(r.describe()));
        r = run({QStringLiteral("--fake-midi"), QStringLiteral("--channel"), QStringLiteral("1"),
                 QStringLiteral("--send"), QStringLiteral("P3-45"), QStringLiteral("--config-dir"), tmp.path()});
        QVERIFY2(QString(r.out).startsWith(QStringLiteral("B0 00 57")), qPrintable(r.describe()));
        // Reading the settings did not rewrite them.
        QVERIFY(ini.open(QIODevice::ReadOnly));
        QCOMPARE(ini.readAll(), QByteArray("[midi]\nchannel=10\n"));
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
        QTest::newRow("engine with d05") << QStringList{QStringLiteral("--engine"), QStringLiteral("60")} << 64;
        QTest::newRow("identify fake") << QStringList{QStringLiteral("--fake-midi"), QStringLiteral("--identify")} << 0;
        QTest::newRow("jp08 fake") << QStringList{QStringLiteral("--fake-midi"), QStringLiteral("--device"), QStringLiteral("jp08"), QStringLiteral("--send"), QStringLiteral("4-5")} << 0;
        QTest::newRow("ju06a mismatch") << QStringList{QStringLiteral("--fake-midi"), QStringLiteral("--device"), QStringLiteral("ju06a"), QStringLiteral("--engine"), QStringLiteral("60"), QStringLiteral("--send"), QStringLiteral("106:4-5")} << 5;
        QTest::newRow("bad address") << QStringList{QStringLiteral("--fake-midi"), QStringLiteral("--send"), QStringLiteral("P1-19")} << 1;
    }
    void noDisplayExits()
    {
        QFETCH(QStringList, args);
        QFETCH(int, code);
        const Result r = run(args);
        QVERIFY2(!r.crashed && r.code == code, qPrintable(r.describe()));
    }

    // A GUI run whose --port substring resolves to the port another device already uses on that
    // channel is a usage error (64) before any window shows; it used to open the window (and hang here).
    void guiRunWithAConflictingPortSubstringExits64()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        {
            d05::AppSettings saved(tmp.filePath(QStringLiteral("settings.ini")));
            d05::AppSettings::DeviceValues d;
            d.enabled = true;
            d.outputPort = QStringLiteral("Fake D-05 In");
            d.channel = 3;
            saved.setDevice(d05::Model::D05, d);
        }
        const Result r = run({QStringLiteral("--fake-midi"), QStringLiteral("--device"), QStringLiteral("jp08"),
                              QStringLiteral("--port"), QStringLiteral("Fake"), QStringLiteral("--channel"),
                              QStringLiteral("3"), QStringLiteral("--config-dir"), tmp.path()},
                             true);
        QVERIFY2(!r.crashed && r.code == 64, qPrintable(r.describe()));
        QVERIFY2(r.err.contains(QStringLiteral("D-05")), qPrintable(r.describe()));
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
