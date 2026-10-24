#include "core/PatchAddress.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiLink.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

using namespace d05;

namespace {
PatchAddress addr(const char *text)
{
    return *PatchAddress::parse(QString::fromLatin1(text));
}
QByteArray bytes(std::initializer_list<int> v)
{
    QByteArray b;
    for (int x : v)
        b.append(char(x));
    return b;
}
// Injects a full panel triplet and lets the queued delivery run.
void inject(FakeMidiBackend &f, int ch, int lsb, int pc)
{
    f.injectReceived(bytes({0xB0 | (ch - 1), 0, 87}));
    f.injectReceived(bytes({0xB0 | (ch - 1), 32, lsb}));
    f.injectReceived(bytes({0xC0 | (ch - 1), pc}));
    QCoreApplication::processEvents();
}
}  // namespace

class TestMidiLink : public QObject {
    Q_OBJECT
private slots:
    void defaults()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        QCOMPARE(link.pollIntervalMs(), 2000);
        QVERIFY(!link.isConnected());
        QCOMPARE(link.status(), QStringLiteral("No port selected"));
        QVERIFY(!link.isPolling());
        QVERIFY(!link.sendPatch(addr("P1-11")));
        QVERIFY(f.sentMessages().isEmpty());
    }

    // RtMidi 6.0.0 WinMM appends " <port index>" to every port name (RtMidi.cpp,
    // MidiInWinMM::getPortName line 2977 and MidiOutWinMM::getPortName line 3047,
    // "ensure unique portnames"); the index changes as devices come and go.
    void winMMIndexIsStripped()
    {
        QCOMPARE(MidiLink::stripWinMMIndex(QStringLiteral("Hapax MIDI 1 3")), QStringLiteral("Hapax MIDI 1"));
        QCOMPARE(MidiLink::stripWinMMIndex(QStringLiteral("Boutique 0")), QStringLiteral("Boutique"));
        QCOMPARE(MidiLink::stripWinMMIndex(QStringLiteral("USB MIDI Device 12")), QStringLiteral("USB MIDI Device"));
        QCOMPARE(MidiLink::stripWinMMIndex(QStringLiteral("Launchpad")), QStringLiteral("Launchpad"));  // not WinMM form
        QCOMPARE(MidiLink::stripWinMMIndex(QStringLiteral("Port2")), QStringLiteral("Port2"));          // no space, no index
#if defined(Q_OS_WIN) || defined(D05_WINMM_PORT_KEYS)
        QCOMPARE(MidiLink::portKey(QStringLiteral("Hapax MIDI 1 3")), QStringLiteral("Hapax MIDI 1"));
        QCOMPARE(MidiLink::matchPort({QStringLiteral("Hapax MIDI 1 3")}, QStringLiteral("Hapax MIDI 1")),
                 QStringLiteral("Hapax MIDI 1 3"));  // a saved key that ends in digits still finds its port
#else
        // Elsewhere only the ALSA "client:port" suffix is a varying part.
        QCOMPARE(MidiLink::portKey(QStringLiteral("Hapax MIDI 1")), QStringLiteral("Hapax MIDI 1"));
#endif
    }

    void inputProblemsAreSignalled()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        QSignalSpy st(&link, &MidiLink::inputStatusChanged);
        link.setInputPort(QStringLiteral("Fake D-05 Out"));
        QVERIFY(st.isEmpty());  // follow off: nothing wanted, no status
        QVERIFY(!link.inputConnected());
        link.setFollowEnabled(true);
        QVERIFY(link.inputConnected());
        QCOMPARE(st.last().at(0).toString(), QStringLiteral("Connected to Fake D-05 Out"));
        // The port vanishes.
        f.setInputPorts({});
        QVERIFY(!link.inputConnected());
        QCOMPARE(st.last().at(0).toString(), QStringLiteral("Port not found"));
        QCOMPARE(link.inputStatus(), QStringLiteral("Port not found"));
        // Back again: the poll reconnects it.
        f.setInputPorts({QStringLiteral("Fake D-05 Out")});
        link.pollNow();
        QVERIFY(link.inputConnected());
        QCOMPARE(st.last().at(0).toString(), QStringLiteral("Connected to Fake D-05 Out"));
        // An open failure.
        link.setFollowEnabled(false);
        QVERIFY(st.last().at(0).toString().isEmpty());
        f.setFailOpen(true);
        link.setFollowEnabled(true);
        QVERIFY(!link.inputConnected());
        QCOMPARE(st.last().at(0).toString(), QStringLiteral("Error: open failed"));
        // A listing failure.
        f.setFailOpen(false);
        f.setListError(QStringLiteral("boom"));
        link.setFollowEnabled(false);
        link.setFollowEnabled(true);
        QCOMPARE(st.last().at(0).toString(), QStringLiteral("Error: boom"));
    }

    void listsPortsThroughTheLink()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        QString err = QStringLiteral("stale");
        QCOMPARE(link.outputPortNames(&err), QStringList{QStringLiteral("Fake D-05 In")});
        QVERIFY(err.isEmpty());
        QCOMPARE(link.inputPortNames(&err), QStringList{QStringLiteral("Fake D-05 Out")});
        QVERIFY(err.isEmpty());
        f.setOutputPorts({QStringLiteral("A"), QStringLiteral("B")}, false);
        QCOMPARE(link.outputPortNames(nullptr).size(), 2);  // null err is fine
        f.setListError(QStringLiteral("boom"));
        QVERIFY(link.outputPortNames(&err).isEmpty());
        QCOMPARE(err, QStringLiteral("boom"));
        QVERIFY(link.inputPortNames(&err).isEmpty());
        QCOMPARE(err, QStringLiteral("boom"));
        // Listing opens nothing and changes no state.
        QVERIFY(!link.isConnected());
        QCOMPARE(link.status(), QStringLiteral("No port selected"));
    }

    void ownPortsAreNeverListedOrMatched()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        const QString own1 = QStringLiteral("D-05 Patch Switcher:d05-patch-switcher out 128:0");
        const QString own2 = QStringLiteral("RtMidi Output Client:d05-patch-switcher out 128:0");  // older naming
        const QString own3 = QStringLiteral("D-05 Patch Switcher:whatever 130:0");                // another instance
        const QString real = QStringLiteral("Boutique:Boutique MIDI 1 24:0");
        f.setOutputPorts({own1, real, own3}, false);
        f.setInputPorts({own2, real, own1}, false);
        QCOMPARE(link.outputPortNames(nullptr), QStringList{real});
        QCOMPARE(link.inputPortNames(nullptr), QStringList{real});
        // Asking for our own port by name finds nothing.
        link.setInputPort(QStringLiteral("d05-patch-switcher"));
        QVERIFY(!link.inputConnected());
        link.setOutputPort(QStringLiteral("D-05 Patch Switcher"));
        QVERIFY(!link.isConnected());
        link.setOutputPort(QStringLiteral("Boutique"));
        QVERIFY(link.isConnected());
        QVERIFY(!MidiBackend::isOwnPort(QStringLiteral("HAPAX Puerto 3")));
        QVERIFY(!MidiBackend::isOwnPort(real));
    }

    void connectBySubstring()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        QSignalSpy st(&link, &MidiLink::statusChanged);
        link.setOutputPort(QStringLiteral("fake d-05"));
        QVERIFY(link.isConnected());
        QCOMPARE(link.status(), QStringLiteral("Connected to Fake D-05 In"));
        QCOMPARE(st.count(), 1);
        QVERIFY(!link.isPolling());
    }

    void bytesOnChannels_data()
    {
        QTest::addColumn<int>("ch");
        QTest::addColumn<QByteArray>("b0");
        QTest::addColumn<QByteArray>("b1");
        QTest::addColumn<QByteArray>("b2");
        QTest::newRow("ch1") << 1 << bytes({0xB0, 0, 87}) << bytes({0xB0, 32, 10}) << bytes({0xC0, 28});
        QTest::newRow("ch10") << 10 << bytes({0xB9, 0, 87}) << bytes({0xB9, 32, 10}) << bytes({0xC9, 28});
        QTest::newRow("ch16") << 16 << bytes({0xBF, 0, 87}) << bytes({0xBF, 32, 10}) << bytes({0xCF, 28});
    }
    void bytesOnChannels()
    {
        QFETCH(int, ch);
        QFETCH(QByteArray, b0);
        QFETCH(QByteArray, b1);
        QFETCH(QByteArray, b2);
        FakeMidiBackend f;
        MidiLink link(&f);
        link.setOutputPort(QStringLiteral("Fake"));
        link.setChannel(ch);
        QSignalSpy sent(&link, &MidiLink::sent);
        QVERIFY(link.sendPatch(addr("P3-45")));
        QCOMPARE(f.sentMessages(), (QList<QByteArray>{b0, b1, b2}));
        QCOMPARE(sent.count(), 1);
        QCOMPARE(link.current().value(), addr("P3-45"));
    }

    void extremeAddresses()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        link.setOutputPort(QStringLiteral("Fake"));
        QVERIFY(link.sendPatch(addr("U1-11")));
        QVERIFY(link.sendPatch(addr("P6-88")));
        QCOMPARE(f.sentMessages().at(1), bytes({0xB0, 32, 0}));
        QCOMPARE(f.sentMessages().at(2), bytes({0xC0, 0}));
        QCOMPARE(f.sentMessages().at(4), bytes({0xB0, 32, 13}));
        QCOMPARE(f.sentMessages().at(5), bytes({0xC0, 63}));
    }

    void missingPortPollsAndReconnects()
    {
        FakeMidiBackend f;
        f.setOutputPorts({});
        MidiLink link(&f);
        link.setOutputPort(QStringLiteral("Fake D-05"));
        QVERIFY(!link.isConnected());
        QCOMPARE(link.status(), QStringLiteral("Port not found"));
        QVERIFY(link.isPolling());
        QCOMPARE(link.pollIntervalMs(), 2000);
        link.pollNow();
        QVERIFY(link.isPolling());
        f.setOutputPorts({QStringLiteral("Fake D-05 In")});
        QSignalSpy conn(&link, &MidiLink::connectedChanged);
        link.pollNow();
        QVERIFY(link.isConnected());
        QVERIFY(!link.isPolling());
        QCOMPARE(conn.count(), 1);
    }

    void vanishedPortFlipsStatusAndReconnects()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        link.setOutputPort(QStringLiteral("Fake D-05"));
        QVERIFY(link.isConnected());
        QVERIFY(!link.isPolling());
        f.setOutputPorts({});
        QVERIFY(!link.isConnected());
        QCOMPARE(link.status(), QStringLiteral("Port not found"));
        QVERIFY(link.isPolling());
        QVERIFY(!link.sendPatch(addr("P1-11")));
        f.setOutputPorts({QStringLiteral("Fake D-05 In")});
        link.pollNow();
        QVERIFY(link.isConnected());
        QVERIFY(!link.isPolling());
        QVERIFY(link.sendPatch(addr("P1-11")));
    }

    void followUpdatesCurrentWithoutSending()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        link.setOutputPort(QStringLiteral("Fake"));
        link.setInputPort(QStringLiteral("Fake"));
        link.setFollowEnabled(true);
        QVERIFY(f.isInputOpen());
        link.setChannel(3);
        QSignalSpy cur(&link, &MidiLink::currentChanged);
        inject(f, 3, 10, 28);
        QCOMPARE(cur.count(), 1);
        QCOMPARE(link.current().value(), addr("P3-45"));
        QVERIFY(f.sentMessages().isEmpty());
        // wrong channel is ignored
        inject(f, 4, 0, 0);
        QCOMPARE(cur.count(), 1);
    }

    void followOffIgnoresInput()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        link.setInputPort(QStringLiteral("Fake"));
        QVERIFY(!f.isInputOpen());
        QSignalSpy cur(&link, &MidiLink::currentChanged);
        inject(f, 1, 8, 0);
        QCOMPARE(cur.count(), 0);
    }

    void echoOfOwnSendIsIgnored()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        qint64 now = 1000;
        link.setClock([&now] { return now; });
        link.setOutputPort(QStringLiteral("Fake"));
        link.setInputPort(QStringLiteral("Fake"));
        link.setFollowEnabled(true);
        QVERIFY(link.sendPatch(addr("P3-45")));
        link.setCurrent(addr("U1-11"));  // so an accepted echo would be visible
        QSignalSpy cur(&link, &MidiLink::currentChanged);
        now += 299;
        inject(f, 1, 10, 28);
        QCOMPARE(cur.count(), 0);
        QCOMPARE(link.current().value(), addr("U1-11"));
        now += 2;  // 301 ms after the send
        inject(f, 1, 10, 28);
        QCOMPARE(cur.count(), 1);
        QCOMPARE(link.current().value(), addr("P3-45"));
    }

    void receivedArrivesOnMainThread()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        link.setInputPort(QStringLiteral("Fake"));
        link.setFollowEnabled(true);
        QThread *deliveredOn = nullptr;
        connect(&link, &MidiLink::currentChanged, this, [&deliveredOn](const PatchAddress &) {
            deliveredOn = QThread::currentThread();
        }, Qt::DirectConnection);
        QSignalSpy cur(&link, &MidiLink::currentChanged);
        QThread *worker = QThread::create([&f] {
            f.injectReceived(bytes({0xB0, 0, 87}));
            f.injectReceived(bytes({0xB0, 32, 9}));
            f.injectReceived(bytes({0xC0, 63}));
        });
        worker->start();
        worker->wait();
        delete worker;
        QVERIFY(cur.wait(1000));
        QCOMPARE(deliveredOn, QCoreApplication::instance()->thread());
        QCOMPARE(link.current().value(), addr("P2-88"));
    }

    void inputLossReconnects()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        link.setInputPort(QStringLiteral("Fake"));
        link.setFollowEnabled(true);
        QVERIFY(f.isInputOpen());
        QVERIFY(!link.isPolling());
        f.setInputPorts({});
        QVERIFY(!f.isInputOpen());
        QCOMPARE(link.inputStatus(), QStringLiteral("Port not found"));
        QVERIFY(link.isPolling());
        f.setInputPorts({QStringLiteral("Fake D-05 Out")});
        link.pollNow();
        QVERIFY(f.isInputOpen());
        QVERIFY(!link.isPolling());
    }

    void checkConnectionDetectsSilentLoss()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        link.setOutputPort(QStringLiteral("Fake"));
        link.setInputPort(QStringLiteral("Fake"));
        link.setFollowEnabled(true);
        f.setOutputPorts({}, false);
        f.setInputPorts({}, false);
        link.checkConnection();
        QVERIFY(!link.isConnected());
        QCOMPARE(link.status(), QStringLiteral("Port not found"));
        QCOMPARE(link.inputStatus(), QStringLiteral("Port not found"));
        QVERIFY(link.isPolling());
    }

    void silentOutputRemovalThenSendDrops()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        link.setOutputPort(QStringLiteral("Fake"));
        f.setOutputPorts({}, false);
        QVERIFY(link.isConnected());  // nobody told us
        QSignalSpy sent(&link, &MidiLink::sent);
        QVERIFY(!link.sendPatch(addr("P1-11")));
        QCOMPARE(sent.count(), 0);
        QVERIFY(f.sentMessages().isEmpty());
        QCOMPARE(link.status(), QStringLiteral("Port not found"));
        QVERIFY(link.isPolling());
    }

    void portKeyStripsAlsaIds()
    {
        QCOMPARE(MidiLink::portKey(QStringLiteral("Hapax:Hapax MIDI 1 24:0")),
                 QStringLiteral("Hapax:Hapax MIDI 1"));
        QCOMPARE(MidiLink::portKey(QStringLiteral("Fake D-05 In")), QStringLiteral("Fake D-05 In"));
#if !(defined(Q_OS_WIN) || defined(D05_WINMM_PORT_KEYS))
        QCOMPARE(MidiLink::portKey(QStringLiteral("Port 3")), QStringLiteral("Port 3"));  // WinMM would strip it
#endif

        FakeMidiBackend f;
        f.setOutputPorts({QStringLiteral("Hapax:Hapax MIDI 1 28:0")});
        MidiLink link(&f);
        link.setOutputPort(QStringLiteral("Hapax:Hapax MIDI 1 24:0"));
        QVERIFY(link.isConnected());
        QCOMPARE(link.connectedPortName(), QStringLiteral("Hapax:Hapax MIDI 1 28:0"));
        QCOMPARE(link.connectedPortKey(), QStringLiteral("Hapax:Hapax MIDI 1"));
        QCOMPARE(link.status(), QStringLiteral("Connected to Hapax:Hapax MIDI 1"));
    }

    void enumerationErrorIsReported()
    {
        FakeMidiBackend f;
        f.setListError(QStringLiteral("boom"));
        MidiLink link(&f);
        link.setOutputPort(QStringLiteral("Fake"));
        QCOMPARE(link.status(), QStringLiteral("Error: boom"));
        QVERIFY(link.isPolling());
        f.setListError(QString());
        link.pollNow();
        QVERIFY(link.isConnected());
        f.setListError(QStringLiteral("bad"));
        QVERIFY(!link.sendPatch(addr("P1-11")));  // checkConnection drops on error
        QCOMPARE(link.status(), QStringLiteral("Error: bad"));
    }

    void echoCoversEverySendInWindow()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        qint64 now = 0;
        link.setClock([&now] { return now; });
        link.setOutputPort(QStringLiteral("Fake"));
        link.setInputPort(QStringLiteral("Fake"));
        link.setFollowEnabled(true);
        QVERIFY(link.sendPatch(addr("P1-11")));  // A at 0
        now = 100;
        QVERIFY(link.sendPatch(addr("P2-22")));  // B at 100
        now = 150;
        QSignalSpy cur(&link, &MidiLink::currentChanged);
        inject(f, 1, 8, 0);  // echo of A
        QCOMPARE(cur.count(), 0);
        QCOMPARE(link.current().value(), addr("P2-22"));
    }

    void staleBankStateIsCleared()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        link.setInputPort(QStringLiteral("Fake"));
        link.setFollowEnabled(true);
        QSignalSpy cur(&link, &MidiLink::currentChanged);
        // foreign MSB, then PC: ignored and consumed
        f.injectReceived(bytes({0xB0, 0, 5}));
        f.injectReceived(bytes({0xB0, 32, 3}));
        f.injectReceived(bytes({0xC0, 1}));
        QCoreApplication::processEvents();
        QCOMPARE(cur.count(), 0);
        inject(f, 1, 10, 28);
        QCOMPARE(cur.count(), 1);
        QCOMPARE(link.current().value(), addr("P3-45"));
        // bare PC after a consumed triplet uses the current group (P3)
        f.injectReceived(bytes({0xC0, 0}));
        QCoreApplication::processEvents();
        QCOMPARE(link.current().value(), addr("P3-11"));
    }

    void malformedInputIgnored()
    {
        FakeMidiBackend f;
        MidiLink link(&f);
        link.setInputPort(QStringLiteral("Fake"));
        link.setFollowEnabled(true);
        QSignalSpy cur(&link, &MidiLink::currentChanged);
        inject(f, 1, 14, 0);  // lsb >= 14
        QCOMPARE(cur.count(), 0);
        f.injectReceived(bytes({0xB0, 32}));  // CC with missing data byte
        f.injectReceived(bytes({0xC0, 5}));   // bare PC, no current patch
        QCoreApplication::processEvents();
        QCOMPARE(cur.count(), 0);
        QVERIFY(!link.current().has_value());
    }

    void openFailureIsStatusNotCrash()
    {
        FakeMidiBackend f;
        f.setFailOpen(true);
        MidiLink link(&f);
        link.setOutputPort(QStringLiteral("Fake"));
        QVERIFY(link.status().startsWith(QStringLiteral("Error: ")));
        QVERIFY(link.isPolling());
    }
};

QTEST_MAIN(TestMidiLink)
#include "test_midi_link.moc"
