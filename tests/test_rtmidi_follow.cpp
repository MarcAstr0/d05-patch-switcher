// The real input path on Linux: RtMidiBackend input on ALSA "Midi Through", MidiLink with
// follow on, a separate RtMidiOut on the same port sending CC0 / CC32 / PC with MIDI clock
// bytes mixed in. RtMidi's callback runs on its own thread; the test asserts the result
// arrives on the main thread (queued) with the right address and that nothing is sent.
// Exits 77 (skipped) when `Midi Through` is not there.
#include "midi/MidiLink.h"
#include "midi/MidiBackend.h"
#include "midi/RtMidiBackend.h"

#include <RtMidi.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

#include <vector>

using namespace d05;

namespace {

int throughIndexOut(RtMidiOut &out)
{
    for (unsigned i = 0; i < out.getPortCount(); ++i) {
        if (QString::fromStdString(out.getPortName(i)).contains(QStringLiteral("Midi Through")))
            return int(i);
    }
    return -1;
}

void sendRaw(RtMidiOut &out, std::initializer_list<unsigned char> bytes)
{
    std::vector<unsigned char> m(bytes);
    out.sendMessage(&m);
}

}  // namespace

class TestRtMidiFollow : public QObject {
    Q_OBJECT
private slots:
    void tripletWithClockArrivesOnTheMainThread()
    {
        RtMidiBackend backend;
        MidiLink link(&backend);
        link.setChannel(1);
        QSignalSpy changed(&link, &MidiLink::currentChanged);
        QSignalSpy sent(&link, &MidiLink::sent);
        bool onMain = true;
        connect(&link, &MidiLink::currentChanged, this,
                [&onMain](const PatchAddress &) { onMain = onMain && QThread::currentThread() == qApp->thread(); });

        link.setInputPort(QStringLiteral("Midi Through"));
        link.setFollowEnabled(true);
        QVERIFY2(backend.isInputOpen(), qPrintable(link.inputStatus()));

        RtMidiOut out;
        const int idx = throughIndexOut(out);
        QVERIFY(idx >= 0);
        out.openPort(unsigned(idx), "d05 test sender");
        QVERIFY(out.isPortOpen());

        // P3-45 on channel 1: CC0 87, CC32 10, PC 28; clock bytes between the messages.
        sendRaw(out, {0xF8});
        sendRaw(out, {0xB0, 0x00, 87});
        sendRaw(out, {0xF8});
        sendRaw(out, {0xB0, 0x20, 10});
        sendRaw(out, {0xF8});
        sendRaw(out, {0xC0, 28});
        sendRaw(out, {0xF8});
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 5000);
        QCOMPARE(changed.at(0).at(0).value<PatchAddress>(), *PatchAddress::parse(QStringLiteral("P3-45")));
        QVERIFY(onMain);
        QCOMPARE(link.current().value(), *PatchAddress::parse(QStringLiteral("P3-45")));

        // Another channel and bare clock: nothing more.
        sendRaw(out, {0xB1, 0x00, 87});
        sendRaw(out, {0xB1, 0x20, 8});
        sendRaw(out, {0xC1, 0});
        for (int i = 0; i < 10; ++i)
            sendRaw(out, {0xF8});
        QTest::qWait(300);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(sent.count(), 0);  // following never sends
        QVERIFY(!link.isConnected());  // and no output was ever opened
    }

    // The app's own ALSA ports never appear in its lists (they would let the input feed the
    // app's own sends back into follow mode).
    void ownPortsAreNotListed()
    {
        RtMidiBackend backend;
        QString err;
        const QStringList outs0 = backend.outputPortNames(&err);
        QVERIFY(!outs0.isEmpty());
        QString e1, e2;
        QVERIFY2(backend.openOutput(outs0.first(), &e1), qPrintable(e1));
        QStringList ins = backend.inputPortNames(&err);
        QString through;
        for (const QString &n : ins)
            if (n.contains(QStringLiteral("Midi Through")))
                through = n;
        QVERIFY(!through.isEmpty());
        QVERIFY2(backend.openInput(through, &e2), qPrintable(e2));

        // Proof the test bites: a plain RtMidi client does see our ports under our client name.
        bool rawSeesOurs = false;
        RtMidiIn rawIn;
        RtMidiOut rawOut;
        for (unsigned i = 0; i < rawIn.getPortCount(); ++i)
            rawSeesOurs = rawSeesOurs || QString::fromStdString(rawIn.getPortName(i)).startsWith(MidiBackend::ownClientName());
        for (unsigned i = 0; i < rawOut.getPortCount(); ++i)
            rawSeesOurs = rawSeesOurs || QString::fromStdString(rawOut.getPortName(i)).startsWith(MidiBackend::ownClientName());
        QVERIFY(rawSeesOurs);

        for (const QStringList &list : {backend.outputPortNames(&err), backend.inputPortNames(&err)}) {
            QVERIFY(!list.isEmpty());
            for (const QString &n : list) {
                QVERIFY2(!MidiBackend::isOwnPort(n), qPrintable(n));
                QVERIFY2(!n.contains(QStringLiteral("d05-patch-switcher")), qPrintable(n));
            }
        }
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        RtMidiOut out;
        if (throughIndexOut(out) < 0) {
            qInfo("ALSA 'Midi Through' not found: skipping");
            return 77;
        }
    } catch (const RtMidiError &) {
        return 77;
    }
    TestRtMidiFollow t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_rtmidi_follow.moc"
