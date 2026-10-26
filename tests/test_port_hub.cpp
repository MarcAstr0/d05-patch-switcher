// PortHub on fake backends: one open per OS port, reference counting, demultiplexing by channel,
// the input filter that runs before any allocation.
#include "midi/FakeMidiBackend.h"
#include "midi/PortHub.h"

#include <QCoreApplication>
#include <QTest>

#include <memory>

using namespace d05;

namespace {
QByteArray bytes(std::initializer_list<int> v)
{
    QByteArray b;
    for (int x : v)
        b.append(char(x));
    return b;
}
QString S(const char *t) { return QString::fromLatin1(t); }

struct Sink : QObject {
    QList<QByteArray> got;
    int outLost = 0;
    int inLost = 0;
};

void attach(PortHub &hub, Sink &s)
{
    PortHub::Client c;
    c.received = [&s](const QByteArray &m) { s.got << m; };
    c.outputLost = [&s] { ++s.outLost; };
    c.inputLost = [&s] { ++s.inLost; };
    hub.attach(&s, c);
}
}  // namespace

class TestPortHub : public QObject {
    Q_OBJECT
private slots:
    void acceptFilterTable()
    {
        const quint32 pc3 = 1u << 2;            // PC on channel 3
        const quint32 cc3 = 1u << (16 + 2);     // Bank Select on channel 3
        auto acc = [](quint32 mask, std::initializer_list<int> v) {
            const QByteArray b = bytes(v);
            return MidiBackend::accepts(mask, reinterpret_cast<const unsigned char *>(b.constData()),
                                        size_t(b.size()));
        };
        QVERIFY(acc(pc3, {0xC2, 5}));
        QVERIFY(!acc(pc3, {0xC3, 5}));                // other channel
        QVERIFY(!acc(pc3, {0xB2, 0, 87}));            // CC needs its own bit
        QVERIFY(acc(pc3 | cc3, {0xB2, 0, 87}));
        QVERIFY(acc(pc3 | cc3, {0xB2, 32, 3}));
        QVERIFY(!acc(pc3 | cc3, {0xB2, 7, 100}));     // other CCs
        QVERIFY(!acc(pc3 | cc3, {0xB2, 0}));          // truncated
        QVERIFY(!acc(MidiBackend::AcceptAll, {0xF8}));  // realtime, every kind
        QVERIFY(!acc(MidiBackend::AcceptAll, {0xFE}));
        QVERIFY(!acc(MidiBackend::AcceptAll, {0xF0, 0x7E, 0xF7}));
        QVERIFY(!acc(MidiBackend::AcceptAll, {0x92, 60, 100}));  // notes
        QVERIFY(!acc(MidiBackend::AcceptAll, {0x05, 5}));        // data byte first
        QVERIFY(!acc(0, {0xC2, 5}));
        QVERIFY(!MidiBackend::accepts(MidiBackend::AcceptAll, nullptr, 0));
    }

    void demultiplexesByChannelOnOneSharedPort()
    {
        QList<FakeMidiBackend *> fakes;
        PortHub hub([&fakes] {
            auto *f = new FakeMidiBackend;
            fakes << f;
            return f;
        });
        Sink a, b;
        attach(hub, a);
        attach(hub, b);
        QString err;
        QVERIFY(hub.acquireOutput(&a, S("Fake D-05 In"), &err));
        QVERIFY(hub.acquireOutput(&b, S("Fake D-05 In"), &err));
        QVERIFY(hub.acquireInput(&a, S("Fake D-05 Out"), 3, false, &err));
        QVERIFY(hub.acquireInput(&b, S("Fake D-05 Out"), 5, false, &err));
        // One OS port each way, one backend, one input thread.
        QCOMPARE(hub.openInputCount(), 1);
        QCOMPARE(hub.openOutputCount(), 1);
        QCOMPARE(hub.backendCount(), 1);
        FakeMidiBackend *f = fakes.first();

        f->injectReceived(bytes({0xC2, 10}));  // channel 3
        f->injectReceived(bytes({0xC4, 20}));  // channel 5
        f->injectReceived(bytes({0xC3, 30}));  // channel 4: nobody
        f->injectReceived(bytes({0xB2, 0, 87}));  // CC: nobody wants Bank Select
        f->injectReceived(bytes({0xF8}));
        QCoreApplication::processEvents();
        QCOMPARE(a.got, QList<QByteArray>{bytes({0xC2, 10})});
        QCOMPARE(b.got, QList<QByteArray>{bytes({0xC4, 20})});

        // Both send through the same output.
        QVERIFY(hub.sendOn(&a, bytes({0xC2, 1}), &err));
        QVERIFY(hub.sendOn(&b, bytes({0xC4, 2}), &err));
        QCOMPARE(f->sentMessages().size(), 2);
    }

    void referenceCounting()
    {
        FakeMidiBackend f;
        PortHub hub(&f);
        Sink a, b;
        attach(hub, a);
        attach(hub, b);
        QString err;
        QVERIFY(hub.acquireOutput(&a, S("Fake D-05 In"), &err));
        QVERIFY(hub.acquireOutput(&b, S("Fake D-05 In"), &err));
        hub.releaseOutput(&a);
        QVERIFY(f.isOutputOpen());
        QVERIFY(hub.isOutputOpen(&b));
        QVERIFY(!hub.isOutputOpen(&a));
        hub.releaseOutput(&b);
        QVERIFY(!f.isOutputOpen());
        QCOMPARE(hub.openOutputCount(), 0);
        // Detach releases too.
        QVERIFY(hub.acquireInput(&a, S("Fake D-05 Out"), 1, false, &err));
        hub.detach(&a);
        QVERIFY(!f.isInputOpen());
    }

    void maskFollowsTheRegisteredLinks()
    {
        FakeMidiBackend f;
        PortHub hub(&f);
        Sink a, b;
        attach(hub, a);
        attach(hub, b);
        QString err;
        QCOMPARE(f.acceptMask(), 0u);
        QVERIFY(hub.acquireInput(&a, S("Fake D-05 Out"), 3, true, &err));
        QCOMPARE(f.acceptMask(), (1u << 2) | (1u << 18));
        QVERIFY(hub.acquireInput(&b, S("Fake D-05 Out"), 5, false, &err));
        QCOMPARE(f.acceptMask(), (1u << 2) | (1u << 18) | (1u << 4));
        hub.setInputChannel(&b, 6);
        QCOMPARE(f.acceptMask(), (1u << 2) | (1u << 18) | (1u << 5));
        hub.releaseInput(&a);
        QCOMPARE(f.acceptMask(), 1u << 5);
        hub.releaseInput(&b);
        QCOMPARE(f.acceptMask(), 0u);
    }

    void differentPortsNeedTheirOwnBackendInFactoryMode()
    {
        QList<FakeMidiBackend *> fakes;
        PortHub hub([&fakes] {
            auto *f = new FakeMidiBackend;
            f->setOutputPorts({S("A in"), S("B in")}, false);
            f->setInputPorts({S("A out"), S("B out")}, false);
            fakes << f;
            return f;
        });
        Sink a, b;
        attach(hub, a);
        attach(hub, b);
        QString err;
        QVERIFY(hub.acquireInput(&a, S("A out"), 1, false, &err));
        QVERIFY(hub.acquireInput(&b, S("B out"), 1, false, &err));
        QCOMPARE(hub.openInputCount(), 2);
        QVERIFY(hub.acquireOutput(&a, S("A in"), &err));
        QVERIFY(hub.acquireOutput(&b, S("B in"), &err));
        QCOMPARE(hub.openOutputCount(), 2);
        QCOMPARE(hub.backendCount(), 2);  // reused for outputs
        QVERIFY(!hub.acquireInput(&a, S("nope"), 1, false, &err));
        QVERIFY(!err.isEmpty());
    }

    void fixedModeServesOnePortPerDirection()
    {
        FakeMidiBackend f;
        f.setOutputPorts({S("A in"), S("B in")}, false);
        PortHub hub(&f);
        Sink a, b;
        attach(hub, a);
        attach(hub, b);
        QString err;
        QVERIFY(hub.acquireOutput(&a, S("A in"), &err));
        QVERIFY(!hub.acquireOutput(&b, S("B in"), &err));
        QCOMPARE(err, S("no free MIDI output"));
    }

    void lostPortReachesEveryUser()
    {
        FakeMidiBackend f;
        PortHub hub(&f);
        Sink a, b;
        attach(hub, a);
        attach(hub, b);
        QString err;
        QVERIFY(hub.acquireOutput(&a, S("Fake D-05 In"), &err));
        QVERIFY(hub.acquireOutput(&b, S("Fake D-05 In"), &err));
        QVERIFY(hub.acquireInput(&a, S("Fake D-05 Out"), 1, false, &err));
        QVERIFY(hub.acquireInput(&b, S("Fake D-05 Out"), 2, false, &err));
        f.setOutputPorts({});
        QCOMPARE(a.outLost, 1);
        QCOMPARE(b.outLost, 1);
        QVERIFY(!hub.isOutputOpen(&a));
        f.setInputPorts({});
        QCOMPARE(a.inLost, 1);
        QCOMPARE(b.inLost, 1);
        QVERIFY(!hub.isInputOpen(&b));
    }

    void staleQueuedMessageIsNotDeliveredAfterTheBackendIsReused()
    {
        FakeMidiBackend f;
        f.setInputPorts({S("X"), S("Y")}, false);
        PortHub hub(&f);
        Sink a, b;
        attach(hub, a);
        attach(hub, b);
        QString err;
        QVERIFY(hub.acquireInput(&a, S("X"), 1, false, &err));
        f.injectReceived(bytes({0xC0, 5}));  // queued from port X, not delivered yet
        hub.releaseInput(&a);
        QVERIFY(hub.acquireInput(&b, S("Y"), 1, false, &err));  // the same backend, now port Y
        QCoreApplication::processEvents();
        QVERIFY(b.got.isEmpty());
        QVERIFY(a.got.isEmpty());
        // A message from port Y itself still arrives.
        f.injectReceived(bytes({0xC0, 6}));
        QCoreApplication::processEvents();
        QCOMPARE(b.got, QList<QByteArray>{bytes({0xC0, 6})});
    }

    void linkDetachedFromInsideItsOwnCallbackDoesNotDestroyTheRunningFunctor()
    {
        FakeMidiBackend f;
        PortHub hub(&f);
        Sink a, b;
        auto alive = std::make_shared<int>(1);
        std::weak_ptr<int> weak = alive;
        bool stillAlive = false;
        PortHub::Client c;
        c.received = [&, alive](const QByteArray &) {
            hub.detach(&a);  // what ~MidiLink does when the callback deletes its own link
            stillAlive = !weak.expired();  // the captured state of this very functor
        };
        hub.attach(&a, std::move(c));
        attach(hub, b);
        alive.reset();
        QString err;
        QVERIFY(hub.acquireInput(&a, S("Fake D-05 Out"), 1, false, &err));
        QVERIFY(hub.acquireInput(&b, S("Fake D-05 Out"), 1, false, &err));
        f.injectReceived(bytes({0xC0, 5}));
        QCoreApplication::processEvents();
        QVERIFY(stillAlive);
        QVERIFY(weak.expired());  // swept once the dispatch finished
        QVERIFY(!hub.isInputOpen(&a));
        QCOMPARE(b.got.size(), 1);  // the other link in the same dispatch still got it
        // The identity can be attached again.
        attach(hub, a);
        QVERIFY(hub.acquireInput(&a, S("Fake D-05 Out"), 1, false, &err));
    }
};

QTEST_MAIN(TestPortHub)
#include "test_port_hub.moc"
