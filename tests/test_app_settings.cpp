// AppSettings (typed INI wrapper), the CLI start-up values, and --send with saved settings.
// No widgets; everything in a temporary directory.
#include "app/AppSettings.h"
#include "app/Cli.h"
#include "app/Paths.h"
#include "midi/FakeMidiBackend.h"

#include <QFile>
#include <QTemporaryDir>
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

CliOptions parsed(std::initializer_list<const char *> v)
{
    CliOptions o;
    QString msg;
    int code = 0;
    if (Cli::parse(argv(v), &o, &msg, &code) != Cli::ParseStatus::Run)
        qFatal("test arguments did not parse: %s", qPrintable(msg));
    return o;
}

}  // namespace

class TestAppSettings : public QObject {
    Q_OBJECT
private slots:
    void defaults()
    {
        QTemporaryDir dir;
        AppSettings s(dir.filePath(QStringLiteral("settings.ini")));
        const AppSettings::Values v = s.values();
        QCOMPARE(v.outputPort, QString());
        QCOMPARE(v.inputPort, QString());
        QCOMPARE(v.channel, 1);
        QVERIFY(!v.autoSend);
        QVERIFY(!v.follow);
        QVERIFY(!v.resendOnConnect);
        QVERIFY(!s.lastPatch());
        QVERIFY(s.windowGeometry().isEmpty());
        // Reading creates nothing.
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("settings.ini"))));
    }

    void roundTripThroughTheFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        AppSettings::Values v;
        v.outputPort = QStringLiteral("Hapax Port A");
        v.inputPort = QStringLiteral("Hapax Port B");
        v.channel = 3;
        v.autoSend = true;
        v.follow = true;
        v.resendOnConnect = true;
        {
            AppSettings s(path);
            s.setValues(v);
            s.setLastPatch(*PatchAddress::parse(QStringLiteral("P3-45")));
            s.setWindowGeometry(QByteArray("\x01\x02geometry"));
        }
        // A second object reads what the first wrote.
        AppSettings again(path);
        QCOMPARE(again.values(), v);
        QCOMPARE(again.lastPatch()->toString(), QStringLiteral("P3-45"));
        QCOMPARE(again.windowGeometry(), QByteArray("\x01\x02geometry"));
    }

    void individualSettersWriteThrough()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        AppSettings s(path);
        s.setChannel(16);
        s.setAutoSend(true);
        AppSettings other(path);
        QCOMPARE(other.channel(), 16);
        QVERIFY(other.autoSend());
        s.setChannel(99);  // clamped
        QCOMPARE(AppSettings(path).channel(), 16);
        s.setChannel(-4);
        QCOMPARE(AppSettings(path).channel(), 1);
    }

    void portsAreSavedAsKeys()
    {
        QTemporaryDir dir;
        AppSettings s(dir.filePath(QStringLiteral("settings.ini")));
        s.setOutputPort(QStringLiteral("Boutique:Boutique MIDI 1 24:0"));
        s.setInputPort(QStringLiteral("Boutique:Boutique MIDI 1 24:0"));
        QCOMPARE(s.outputPort(), QStringLiteral("Boutique:Boutique MIDI 1"));
        QCOMPARE(s.inputPort(), QStringLiteral("Boutique:Boutique MIDI 1"));
        // Values already hold keys (the dialog strips OS names); they are stored as given.
        AppSettings::Values v;
        v.outputPort = QStringLiteral("Midi Through:Midi Through Port-0");
        s.setValues(v);
        QCOMPARE(s.outputPort(), QStringLiteral("Midi Through:Midi Through Port-0"));
    }

    void garbageInTheFileFallsBackToDefaults()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[midi]\nchannel=banana\n[session]\nlastPatch=P9-99\n[behaviour]\nautoSend=maybe\n");
        f.close();
        AppSettings s(path);
        QCOMPARE(s.channel(), 1);
        QVERIFY(!s.lastPatch());
        QVERIFY(!s.autoSend());
    }

    void createsAMissingConfigDirectory()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("deep/er/settings.ini"));
        AppSettings s(path);
        s.setChannel(4);
        QVERIFY(QFile::exists(path));
        QCOMPARE(AppSettings(path).channel(), 4);
    }

    void reportsAFailedWrite()
    {
        QTemporaryDir dir;
        QFile blocker(dir.filePath(QStringLiteral("afile")));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        AppSettings bad(dir.filePath(QStringLiteral("afile/settings.ini")));
        QVERIFY(!bad.writeFailed());
        bad.setChannel(5);
        QVERIFY(bad.writeFailed());
        AppSettings good(dir.filePath(QStringLiteral("settings.ini")));
        good.setChannel(5);
        QVERIFY(!good.writeFailed());
    }

    void defaultPathHonoursConfigDir()
    {
        QTemporaryDir dir;
        Paths::setOverride(dir.path());
        {
            AppSettings s;
            QCOMPARE(QFileInfo(s.path()).absoluteFilePath(), QFileInfo(dir.filePath(QStringLiteral("settings.ini"))).absoluteFilePath());
        }
        Paths::setOverride(QString());
    }

    // -------------------------------------------------------------- CLI overrides

    void cliOverridesApplyToThisRunOnly()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        AppSettings saved(path);
        AppSettings::Values v;
        v.outputPort = QStringLiteral("Hapax Port A");
        v.inputPort = QStringLiteral("Hapax Port B");
        v.channel = 3;
        v.autoSend = true;
        saved.setValues(v);

        const AppSettings::Values none = Cli::startupValues(saved, parsed({}));
        QCOMPARE(none, v);

        const AppSettings::Values over =
            Cli::startupValues(saved, parsed({"--port", "Boutique", "--in-port", "Other", "--channel", "10"}));
        QCOMPARE(over.outputPort, QStringLiteral("Boutique"));
        QCOMPARE(over.inputPort, QStringLiteral("Other"));
        QCOMPARE(over.channel, 10);
        QVERIFY(over.autoSend);  // untouched settings carry over

        // Nothing was written: a fresh reader still sees the saved values.
        QCOMPARE(AppSettings(path).values(), v);
        QCOMPARE(saved.values(), v);
    }

    void cliOverridesNeverCreateTheFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        AppSettings saved(path);
        const AppSettings::Values over = Cli::startupValues(saved, parsed({"--port", "X", "--channel", "7"}));
        QCOMPARE(over.channel, 7);
        QVERIFY(!QFile::exists(path));
    }

    void fakeMidiUsesTheFakePort()
    {
        QTemporaryDir dir;
        AppSettings saved(dir.filePath(QStringLiteral("settings.ini")));
        saved.setOutputPort(QStringLiteral("Hapax Port A"));
        QCOMPARE(Cli::startupValues(saved, parsed({"--fake-midi"})).outputPort, QStringLiteral("Fake D-05 In"));
        QCOMPARE(Cli::startupValues(saved, parsed({"--fake-midi", "--port", "Fake"})).outputPort, QStringLiteral("Fake"));
    }

    // -------------------------------------------------------------- --send with saved settings

    void sendUsesSavedPortAndChannel()
    {
        QTemporaryDir dir;
        AppSettings saved(dir.filePath(QStringLiteral("settings.ini")));
        saved.setOutputPort(QStringLiteral("Fake D-05 In 20:0"));  // stored as the key
        saved.setChannel(10);
        FakeMidiBackend fake;
        QString out, err;
        QTextStream so(&out), se(&err);
        QCOMPARE(Cli::runSend(parsed({"--send", "P3-45"}), fake, so, se, &saved), Exit::Ok);
        QCOMPARE(out, QStringLiteral("B9 00 57\nB9 20 0A\nC9 1C\n"));
        QCOMPARE(fake.sentMessages().size(), 3);
    }

    void sendFlagsBeatSavedValues()
    {
        QTemporaryDir dir;
        AppSettings saved(dir.filePath(QStringLiteral("settings.ini")));
        saved.setOutputPort(QStringLiteral("Not There"));
        saved.setChannel(10);
        FakeMidiBackend fake;
        QString out, err;
        QTextStream so(&out), se(&err);
        QCOMPARE(Cli::runSend(parsed({"--port", "Fake", "--channel", "2", "--send", "P1-11"}), fake, so, se, &saved),
                 Exit::Ok);
        QCOMPARE(out, QStringLiteral("B1 00 57\nB1 20 08\nC1 00\n"));
        QVERIFY(err.isEmpty());
    }

    void sendWithNothingSavedIsStillExit2()
    {
        QTemporaryDir dir;
        AppSettings saved(dir.filePath(QStringLiteral("settings.ini")));
        FakeMidiBackend fake;
        QString out, err;
        QTextStream so(&out), se(&err);
        QCOMPARE(Cli::runSend(parsed({"--send", "P1-11"}), fake, so, se, &saved), Exit::PortNotFound);
        QVERIFY(fake.sentMessages().isEmpty());
    }

    void sendWithSavedPortThatIsGoneIsExit2()
    {
        QTemporaryDir dir;
        AppSettings saved(dir.filePath(QStringLiteral("settings.ini")));
        saved.setOutputPort(QStringLiteral("Hapax Port A"));
        FakeMidiBackend fake;
        QString out, err;
        QTextStream so(&out), se(&err);
        QCOMPARE(Cli::runSend(parsed({"--send", "P1-11"}), fake, so, se, &saved), Exit::PortNotFound);
        QVERIFY(err.contains(QStringLiteral("Hapax Port A")));
    }

    void sendFakeMidiIgnoresASavedRealPort()
    {
        QTemporaryDir dir;
        AppSettings saved(dir.filePath(QStringLiteral("settings.ini")));
        saved.setOutputPort(QStringLiteral("Hapax Port A"));
        FakeMidiBackend fake;
        QString out, err;
        QTextStream so(&out), se(&err);
        QCOMPARE(Cli::runSend(parsed({"--fake-midi", "--send", "P1-11"}), fake, so, se, &saved), Exit::Ok);
    }

    void badAddressStaysExit1WithSavedSettings()
    {
        QTemporaryDir dir;
        AppSettings saved(dir.filePath(QStringLiteral("settings.ini")));
        saved.setOutputPort(QStringLiteral("Fake D-05 In"));
        FakeMidiBackend fake;
        QString out, err;
        QTextStream so(&out), se(&err);
        QCOMPARE(Cli::runSend(parsed({"--send", "P1-19"}), fake, so, se, &saved), Exit::BadAddress);
    }
};

QTEST_GUILESS_MAIN(TestAppSettings)
#include "test_app_settings.moc"
