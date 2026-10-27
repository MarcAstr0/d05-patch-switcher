// AppSettings (typed INI wrapper), the CLI start-up values, and --send with saved settings.
// No widgets; everything in a temporary directory.
#include "app/AppSettings.h"
#include "app/Cli.h"
#include "app/Paths.h"
#include "midi/FakeMidiBackend.h"

#include <QFile>
#include <QSettings>
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

    // ------------------------------------------------------------------ SPEC-2 3.4: per device

    void perDeviceDefaults()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        AppSettings s(path);
        for (Model m : allModels()) {
            QCOMPARE(s.device(m), AppSettings::DeviceValues());  // disabled, no ports, channel 1
            QVERIFY(!s.hasChannel(m));
            QVERIFY(!s.lastPatch(m));
        }
        QVERIFY(!s.currentDevice());
        QVERIFY(s.followSwitchesDevice());  // default true
        QCOMPARE(s.engine(), 60);
        QVERIFY(!s.hasEngine());
        QVERIFY(!s.remembered(60) && !s.remembered(106));
        QVERIFY(!QFile::exists(path));  // reading creates nothing
    }

    void perDeviceRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        AppSettings::DeviceValues jp;
        jp.enabled = true;
        jp.outputPort = QStringLiteral("Hapax Port A");
        jp.inputPort = QStringLiteral("Hapax Port B");
        jp.channel = 5;
        jp.follow = true;
        jp.resendOnConnect = true;
        AppSettings::DeviceValues ju;
        ju.enabled = true;
        ju.outputPort = QStringLiteral("JU-06A:JU-06A MIDI 1");
        ju.channel = 12;
        {
            AppSettings s(path);
            s.setDevice(Model::JP08, jp);
            s.setDevice(Model::JU06A, ju);
            s.setLastPatch(Model::JP08, *PatchAddress::parse(QStringLiteral("4-5"), Model::JP08));
            s.setLastPatch(*PatchAddress::parse(QStringLiteral("106:8-8"), Model::JU06A));  // by the address's model
            s.setEngine(106);
            s.setRemembered(60, 28);
            s.setRemembered(106, 0);
            s.setCurrentDevice(Model::JU06A);
            s.setFollowSwitchesDevice(false);
            s.setAutoSend(true);
        }
        const AppSettings again(path);
        QCOMPARE(again.device(Model::JP08), jp);
        QCOMPARE(again.device(Model::JU06A), ju);
        QVERIFY(!again.device(Model::D05).enabled);
        QVERIFY(again.hasChannel(Model::JP08));
        QCOMPARE(again.lastPatch(Model::JP08)->toString(), QStringLiteral("4-5"));
        QCOMPARE(again.lastPatch(Model::JU06A)->toString(), QStringLiteral("106:8-8"));
        QVERIFY(!again.lastPatch(Model::D05));
        QCOMPARE(again.engine(), 106);
        QVERIFY(again.hasEngine());
        QCOMPARE(again.remembered(60).value(), 28);
        QCOMPARE(again.remembered(106).value(), 0);
        QCOMPARE(again.currentDevice().value(), Model::JU06A);
        QVERIFY(!again.followSwitchesDevice());
        QVERIFY(again.autoSend());
        // The keys are the ones SPEC-2 3.4 names.
        QSettings raw(path, QSettings::IniFormat);
        for (const char *k : {"devices/jp08/enabled", "devices/jp08/outputPort", "devices/jp08/inputPort",
                              "devices/jp08/channel", "devices/jp08/follow", "devices/jp08/resendOnConnect",
                              "devices/jp08/lastPatch", "devices/ju06a/engine", "devices/ju06a/remembered60",
                              "devices/ju06a/remembered106", "currentDevice", "autoSend", "followSwitchesDevice"})
            QVERIFY2(raw.contains(QString::fromLatin1(k)), k);
        // An unknown remembered program clears the key.
        AppSettings s(path);
        s.setRemembered(60, std::nullopt);
        QVERIFY(!AppSettings(path).remembered(60));
        QCOMPARE(AppSettings(path).remembered(106).value(), 0);
    }

    void aVersion1FileReadsAsTheD05EnabledWithoutWritingAnything()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        {
            QSettings v1(path, QSettings::IniFormat);
            v1.setValue(QStringLiteral("midi/outputPort"), QStringLiteral("Hapax Port A"));
            v1.setValue(QStringLiteral("midi/inputPort"), QStringLiteral("Hapax Port B"));
            v1.setValue(QStringLiteral("midi/channel"), 3);
            v1.setValue(QStringLiteral("behaviour/autoSend"), true);
            v1.setValue(QStringLiteral("behaviour/follow"), true);
            v1.setValue(QStringLiteral("behaviour/resendOnConnect"), true);
            v1.setValue(QStringLiteral("session/lastPatch"), QStringLiteral("P3-45"));
        }
        const qint64 size = QFileInfo(path).size();
        AppSettings s(path);
        const AppSettings::DeviceValues d = s.device(Model::D05);
        QVERIFY(d.enabled);
        QCOMPARE(d.outputPort, QStringLiteral("Hapax Port A"));
        QCOMPARE(d.inputPort, QStringLiteral("Hapax Port B"));
        QCOMPARE(d.channel, 3);
        QVERIFY(d.follow && d.resendOnConnect);
        QVERIFY(s.autoSend());
        QCOMPARE(s.lastPatch(Model::D05)->toString(), QStringLiteral("P3-45"));
        QVERIFY(s.hasChannel(Model::D05));
        QVERIFY(!s.device(Model::JP08).enabled);
        QCOMPARE(QFileInfo(path).size(), size);  // reading wrote nothing
        QVERIFY(!QSettings(path, QSettings::IniFormat).contains(QStringLiteral("devices/d05/enabled")));
    }

    void aFreshInstallStaysWithoutDevicesWhateverVersion2Writes()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        {
            AppSettings s(path);
            s.setWindowGeometry(QByteArray("geometry"));
            s.setAutoSend(true);
        }
        AppSettings again(path);
        QVERIFY(!again.device(Model::D05).enabled);
        again.setCurrentDevice(Model::JP08);  // a store() after the reload
        QVERIFY(!AppSettings(path).device(Model::D05).enabled);
        QVERIFY(!QSettings(path, QSettings::IniFormat).contains(QStringLiteral("devices/d05/enabled")));
    }

    void theFirstWriteMigratesVersion1KeysOnceAndKeepsThem()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        {
            QSettings v1(path, QSettings::IniFormat);
            v1.setValue(QStringLiteral("midi/outputPort"), QStringLiteral("Hapax Port A"));
            v1.setValue(QStringLiteral("midi/channel"), 3);
            v1.setValue(QStringLiteral("behaviour/autoSend"), true);
            v1.setValue(QStringLiteral("session/lastPatch"), QStringLiteral("P3-45"));
        }
        {
            AppSettings s(path);
            s.setCurrentDevice(Model::D05);  // any write
        }
        QSettings raw(path, QSettings::IniFormat);
        QCOMPARE(raw.value(QStringLiteral("devices/d05/enabled")).toBool(), true);
        QCOMPARE(raw.value(QStringLiteral("devices/d05/outputPort")).toString(), QStringLiteral("Hapax Port A"));
        QCOMPARE(raw.value(QStringLiteral("devices/d05/channel")).toInt(), 3);
        QCOMPARE(raw.value(QStringLiteral("devices/d05/lastPatch")).toString(), QStringLiteral("P3-45"));
        QCOMPARE(raw.value(QStringLiteral("autoSend")).toBool(), true);
        // The old keys are still there (a downgrade reads them).
        QCOMPARE(raw.value(QStringLiteral("midi/outputPort")).toString(), QStringLiteral("Hapax Port A"));
        QCOMPARE(raw.value(QStringLiteral("midi/channel")).toInt(), 3);
        QCOMPARE(raw.value(QStringLiteral("session/lastPatch")).toString(), QStringLiteral("P3-45"));
        // Later writes win over the old keys and the migration does not run again.
        AppSettings s(path);
        s.setChannel(7);
        QCOMPARE(AppSettings(path).device(Model::D05).channel, 7);
        QCOMPARE(QSettings(path, QSettings::IniFormat).value(QStringLiteral("midi/channel")).toInt(), 3);
        // A user who turned the D-05 off keeps it off: migration never re-enables.
        AppSettings::DeviceValues off = s.device(Model::D05);
        off.enabled = false;
        s.setDevice(Model::D05, off);
        QVERIFY(!AppSettings(path).device(Model::D05).enabled);
    }

    void aFreshInstallMigratesNothingAndEnablesNothing()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        AppSettings s(path);
        s.setCurrentDevice(Model::JP08);
        QVERIFY(!AppSettings(path).device(Model::D05).enabled);
        QVERIFY(!QSettings(path, QSettings::IniFormat).contains(QStringLiteral("devices/d05/enabled")));
    }

    void startupAppliesOverridesToTheNamedDevicesOnly()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        AppSettings saved(path);
        AppSettings::DeviceValues d05;
        d05.enabled = true;
        d05.outputPort = QStringLiteral("Hapax Port A");
        d05.channel = 3;
        saved.setDevice(Model::D05, d05);
        AppSettings::DeviceValues jp;
        jp.enabled = true;
        jp.outputPort = QStringLiteral("Hapax Port A");
        jp.channel = 5;
        saved.setDevice(Model::JP08, jp);
        saved.setCurrentDevice(Model::JP08);
        saved.setEngine(106);
        const qint64 size = QFileInfo(path).size();

        // No options: the saved current device is the one the window shows.
        Cli::Startup st = Cli::startup(saved, parsed({}));
        QCOMPARE(st.current, Model::JP08);
        QCOMPARE(st.devices[int(Model::D05)], d05);
        QCOMPARE(st.devices[int(Model::JP08)], jp);
        QVERIFY(!st.devices[int(Model::JU06A)].enabled);
        QVERIFY(st.followSwitches);
        QCOMPARE(st.engine, 106);
        // --channel / --port with no --device apply to the saved current device.
        st = Cli::startup(saved, parsed({"--channel", "9", "--port", "Other"}));
        QCOMPARE(st.devices[int(Model::JP08)].channel, 9);
        QCOMPARE(st.devices[int(Model::JP08)].outputPort, QStringLiteral("Other"));
        QCOMPARE(st.devices[int(Model::D05)], d05);
        // --device names the device, turns it on for the run, and shows it.
        st = Cli::startup(saved, parsed({"--device", "ju06a", "--channel", "4", "--engine", "60"}));
        QCOMPARE(st.current, Model::JU06A);
        QVERIFY(st.devices[int(Model::JU06A)].enabled);
        QCOMPARE(st.devices[int(Model::JU06A)].channel, 4);
        QCOMPARE(st.engine, 60);
        QCOMPARE(st.devices[int(Model::JP08)], jp);
        // --fake-midi with no --port: the fake's output on the target device.
        st = Cli::startup(saved, parsed({"--fake-midi", "--device", "d05"}));
        QCOMPARE(st.devices[int(Model::D05)].outputPort, QStringLiteral("Fake D-05 In"));
        // The saved current device is disabled: the first enabled one is shown.
        AppSettings::DeviceValues off = jp;
        off.enabled = false;
        saved.setDevice(Model::JP08, off);
        QCOMPARE(Cli::startup(saved, parsed({})).current, Model::D05);
        // Nothing enabled at all: the D-05 skin (first run).
        AppSettings fresh(dir.filePath(QStringLiteral("other.ini")));
        QCOMPARE(Cli::startup(fresh, parsed({})).current, Model::D05);
        QVERIFY(!Cli::startup(fresh, parsed({})).devices[int(Model::D05)].enabled);
        Q_UNUSED(size);
    }

    void sendFallsBackToTheSavedDevicePortChannelAndEngine()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.ini"));
        AppSettings saved(path);
        AppSettings::DeviceValues jp;
        jp.enabled = true;
        jp.outputPort = QStringLiteral("Fake D-05 In");
        jp.channel = 5;
        saved.setDevice(Model::JP08, jp);
        AppSettings::DeviceValues ju = jp;
        ju.channel = 4;
        saved.setDevice(Model::JU06A, ju);
        saved.setEngine(106);
        FakeMidiBackend fake;
        QString out, err;
        QTextStream so(&out), se(&err);
        // --device jp08 alone: the saved port and channel (5 -> C4).
        QCOMPARE(Cli::runSend(parsed({"--device", "jp08", "--send", "4-5"}), fake, so, se, &saved), Exit::Ok);
        QCOMPARE(out, QStringLiteral("C4 1C\n"));
        QCOMPARE(fake.sentMessages(), QList<QByteArray>{QByteArray::fromHex("c41c")});
        // --device ju06a and no --engine: the saved engine (106) and channel 4 (-> C3).
        out.clear();
        fake.clearSent();
        QCOMPARE(Cli::runSend(parsed({"--device", "ju06a", "--send", "4-5"}), fake, so, se, &saved), Exit::Ok);
        QCOMPARE(out, QStringLiteral("C3 1C\n"));
        out.clear();
        fake.clearSent();
        // The saved engine is a belief like any other: a 60 address against 106 is a mismatch (5).
        QCOMPARE(Cli::runSend(parsed({"--device", "ju06a", "--send", "60:4-5"}), fake, so, se, &saved), Exit::EngineMismatch);
        QVERIFY(fake.sentMessages().isEmpty());
        // --engine wins over the saved one, and --channel over the saved channel.
        QCOMPARE(Cli::runSend(parsed({"--device", "ju06a", "--engine", "60", "--channel", "2", "--send", "60:1-1"}), fake,
                              so, se, &saved),
                 Exit::Ok);
        QCOMPARE(fake.sentMessages(), QList<QByteArray>{QByteArray::fromHex("c100")});
        // No --device: the saved current device.
        fake.clearSent();
        saved.setCurrentDevice(Model::JP08);
        QCOMPARE(Cli::runSend(parsed({"--send", "88"}), fake, so, se, &saved), Exit::Ok);
        QCOMPARE(fake.sentMessages(), QList<QByteArray>{QByteArray::fromHex("c43f")});
        // The D-05 address then no longer parses for the current device: exit 1, nothing sent.
        fake.clearSent();
        QCOMPARE(Cli::runSend(parsed({"--send", "P1-11"}), fake, so, se, &saved), Exit::BadAddress);
        QVERIFY(fake.sentMessages().isEmpty());
        // --device d05 is explicit and does not need the saved device.
        saved.setDevice(Model::D05, [] {
            AppSettings::DeviceValues d;
            d.outputPort = QStringLiteral("Fake D-05 In");
            d.channel = 2;
            return d;
        }());
        QCOMPARE(Cli::runSend(parsed({"--device", "d05", "--send", "P1-11"}), fake, so, se, &saved), Exit::Ok);
    }

    void sendWithoutASavedChannelOrEngineIsRefused()
    {
        QTemporaryDir dir;
        AppSettings saved(dir.filePath(QStringLiteral("settings.ini")));
        AppSettings::DeviceValues jp;
        jp.outputPort = QStringLiteral("Fake D-05 In");
        saved.setDevice(Model::JP08, jp);  // a port, but never a chosen channel: the default 1 is saved too
        QSettings(saved.path(), QSettings::IniFormat).remove(QStringLiteral("devices/jp08/channel"));
        AppSettings noChannel(saved.path());
        FakeMidiBackend fake;
        QString out, err;
        QTextStream so(&out), se(&err);
        QCOMPARE(Cli::runSend(parsed({"--device", "jp08", "--send", "4-5"}), fake, so, se, &noChannel), Exit::Usage);
        QVERIFY(err.contains(QStringLiteral("--channel")));
        QVERIFY(fake.sentMessages().isEmpty());
        // The JU-06A with a channel but no saved engine: exit 1, as before.
        AppSettings::DeviceValues ju = jp;
        ju.channel = 4;
        saved.setDevice(Model::JU06A, ju);
        AppSettings withChannel(saved.path());
        QCOMPARE(Cli::runSend(parsed({"--device", "ju06a", "--send", "4-5"}), fake, so, se, &withChannel), Exit::BadAddress);
        QVERIFY(fake.sentMessages().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestAppSettings)
#include "test_app_settings.moc"
