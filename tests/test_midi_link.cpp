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
        QCOMPARE(link.status(), QStringLiteral("Port not found"));
        QVERIFY(!link.isPolling());
        QVERIFY(!link.sendPatch(addr("P1-11")));
        QVERIFY(f.sentMessages().isEmpty());
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
