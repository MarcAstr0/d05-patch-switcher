// --fake-clock: the fake backend's clock thread goes through the input filter, so clock never
// reaches the main thread's event queue (SPEC-2 section 5: the callbacks drop realtime bytes
// before any allocation).
#include "app/Cli.h"
#include "midi/FakeMidiBackend.h"
#include "midi/PortHub.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

using namespace d05;

class TestFakeClock : public QObject {
    Q_OBJECT
private slots:
    void optionNeedsFakeMidi()
    {
        CliOptions o;
        QString msg;
        int code = 0;
        QCOMPARE(Cli::parse({QStringLiteral("app"), QStringLiteral("--fake-clock")}, &o, &msg, &code),
                 Cli::ParseStatus::ExitNow);
        QVERIFY(code != 0);
        QCOMPARE(Cli::parse({QStringLiteral("app"), QStringLiteral("--fake-midi"), QStringLiteral("--fake-clock")}, &o,
                            &msg, &code),
                 Cli::ParseStatus::Run);
        QVERIFY(o.fakeClock);
    }

    void threePortsEachWay()
    {
        FakeMidiBackend f;
        QCOMPARE(f.inputPortNames(nullptr).size(), 1);
        f.setFakeClock(true);
        QCOMPARE(f.inputPortNames(nullptr).size(), 3);
        QCOMPARE(f.outputPortNames(nullptr).size(), 3);
    }

    void clockNeverReachesTheMainThread()
    {
        FakeMidiBackend f;
        f.setFakeClock(true, 6000.0);  // 2400 pulses/s so the test is short
        QSignalSpy spy(&f, &MidiBackend::received);
        QString err;
        QVERIFY(f.openInput(QStringLiteral("Fake JP-08 Out"), &err));
        QTRY_VERIFY_WITH_TIMEOUT(f.clockTicks() > 50, 3000);
        QCoreApplication::processEvents();
        QCOMPARE(f.queuedEvents(), quint64(0));  // nothing was queued per tick
        QCOMPARE(spy.count(), 0);
        // A wanted message still crosses, so the counter is meaningful.
        f.injectReceived(QByteArray::fromHex("c003"));
        QCOMPARE(f.queuedEvents(), quint64(1));
        QTRY_COMPARE(spy.count(), 1);
        f.closeInput();
        const quint64 t = f.clockTicks();
        QTest::qWait(50);
        QCOMPARE(f.clockTicks(), t);  // the thread stopped with the port
    }

    void rateIsTwentyFourPerQuarterNote()
    {
        FakeMidiBackend f;
        f.setFakeClock(true, 120.0);
        QString err;
        QVERIFY(f.openInput(QStringLiteral("Fake D-05 Out"), &err));
        QTest::qWait(1000);
        f.closeInput();
        QVERIFY2(f.clockTicks() >= 44 && f.clockTicks() <= 50, qPrintable(QString::number(f.clockTicks())));
    }

    void hubRunsOneThreadPerPort()
    {
        QList<FakeMidiBackend *> made;
        PortHub hub([&made]() -> MidiBackend * {
            auto *f = new FakeMidiBackend;
            f->setFakeClock(true);
            made << f;
            return f;
        });
        QObject a, b, c;
        for (QObject *o : {&a, &b, &c})
            hub.attach(o, PortHub::Client{});
        QString err;
        QVERIFY(hub.acquireInput(&a, QStringLiteral("Fake D-05 Out"), 1, true, &err));
        QVERIFY(hub.acquireInput(&b, QStringLiteral("Fake JP-08 Out"), 1, false, &err));
        QVERIFY(hub.acquireInput(&c, QStringLiteral("Fake JU-06A Out"), 1, false, &err));
        QCOMPARE(hub.openInputCount(), 3);
        QTest::qWait(300);
        quint64 queued = 0, ticks = 0;
        for (auto *f : made) {
            queued += f->queuedEvents();
            ticks += f->clockTicks();
        }
        QVERIFY(ticks > 20);
        QCOMPARE(queued, quint64(0));
    }
};

QTEST_MAIN(TestFakeClock)
#include "test_fake_clock.moc"
