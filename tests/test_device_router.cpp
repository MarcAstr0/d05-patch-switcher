// DeviceRouter on fake backends: model-aware sends, shared ports, echo suppression per link,
// the JU-06A tracker, FR-20, the poll.
#include "midi/DeviceRouter.h"
#include "midi/FakeMidiBackend.h"
#include "midi/PortHub.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

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
PatchAddress addr(Model m, const char *text, int engine = 0)
{
    return *PatchAddress::parse(S(text), m, engine ? std::optional<int>(engine) : std::nullopt);
}

// One forwarder port for all three devices (the owner's Hapax), D-05 on 1, JP-08 on 3, JU-06A on 5.
struct Rig {
    QList<FakeMidiBackend *> fakes;
    QStringList outs{S("Hapax Out")};
    QStringList ins{S("Hapax In")};
    PortHub hub;
    DeviceRouter router;

    Rig()
        : hub([this] {
              auto *f = new FakeMidiBackend;
              f->setOutputPorts(outs, false);
              f->setInputPorts(ins, false);
              fakes << f;
              return f;
          })
        , router(&hub)
    {
    }
    FakeMidiBackend *fake() { return fakes.first(); }
    static DeviceRouter::DeviceConfig cfg(int ch, bool follow, const char *port = "Hapax")
    {
        DeviceRouter::DeviceConfig c;
        c.enabled = true;
        c.outputPort = S(port);
        c.inputPort = S(port);
        c.channel = ch;
        c.follow = follow;
        return c;
    }
    void configureAll(bool follow)
    {
        QVERIFY(router.configure(Model::D05, cfg(1, follow)));
        QVERIFY(router.configure(Model::JP08, cfg(3, follow)));
        QVERIFY(router.configure(Model::JU06A, cfg(5, follow)));
    }
    void inject(std::initializer_list<int> v)
    {
        fake()->injectReceived(bytes(v));
        QCoreApplication::processEvents();
    }
};
}  // namespace

class TestDeviceRouter : public QObject {
    Q_OBJECT
private slots:
    void sendsOneMessageForThe64PatchModelsAndThreeForTheD05()
    {
        Rig r;
        r.configureAll(false);
        QCOMPARE(r.hub.openOutputCount(), 1);  // all three share the one OS port
        QVERIFY(r.router.select(addr(Model::JP08, "4-5")));
        QCOMPARE(r.fake()->sentMessages(), QList<QByteArray>{bytes({0xC2, 28})});
        r.fake()->clearSent();
        r.router.setJu06aState(106, -1, -1);
        r.fake()->clearSent();
        QVERIFY(r.router.select(addr(Model::JU06A, "106:4-5")));
        QCOMPARE(r.fake()->sentMessages(), QList<QByteArray>{bytes({0xC4, 28})});
        r.fake()->clearSent();
        QVERIFY(r.router.select(*PatchAddress::parse(S("P3-45"))));
        QCOMPARE(r.fake()->sentMessages(),
                 (QList<QByteArray>{bytes({0xB0, 0, 87}), bytes({0xB0, 32, 10}), bytes({0xC0, 28})}));
    }

    void ju06aNeverSendsForTheOtherEngine()
    {
        Rig r;
        r.configureAll(false);
        r.router.setJu06aState(60, -1, -1);
        r.fake()->clearSent();
        // The link itself refuses a mismatch.
        QVERIFY(!r.router.link(Model::JU06A)->sendPatch(addr(Model::JU06A, "106:4-5")));
        QVERIFY(r.fake()->sentMessages().isEmpty());
        // Through the router the selection arms and sends nothing...
        QVERIFY(r.router.select(addr(Model::JU06A, "106:4-5")));
        QVERIFY(r.fake()->sentMessages().isEmpty());
        QVERIFY(r.router.ju06aTracker()->armed().has_value());
        // ...until the engine is toggled, when exactly one message goes out for the new engine.
        r.router.ju06aTracker()->toggleEngine();
        QCOMPARE(r.router.link(Model::JU06A)->engine(), 106);
        QCOMPARE(r.fake()->sentMessages(), QList<QByteArray>{bytes({0xC4, 28})});
    }

    void sameOutputPortAndChannelIsRefused()
    {
        Rig r;
        QVERIFY(r.router.configure(Model::JP08, Rig::cfg(3, false)));
        Model other = Model::D05;
        QVERIFY(!r.router.configure(Model::JU06A, Rig::cfg(3, false), &other));
        QCOMPARE(other, Model::JP08);
        QVERIFY(!r.router.isEnabled(Model::JU06A));  // nothing applied
        QVERIFY(!r.router.link(Model::JU06A)->isConnected());
        // Same port, other channel: fine. Another port, same channel: fine.
        QVERIFY(r.router.configure(Model::JU06A, Rig::cfg(5, false)));
        r.outs << S("JU-06A");
        r.fake()->setOutputPorts(r.outs, false);
        QVERIFY(r.router.configure(Model::D05, Rig::cfg(3, false, "JU-06A")));
        // The same device may re-apply its own settings; a disabled one never conflicts.
        QVERIFY(r.router.configure(Model::JP08, Rig::cfg(3, false)));
        auto off = Rig::cfg(3, false);
        off.enabled = false;
        QVERIFY(!r.router.conflict(Model::D05, off).has_value());
        // The query alone changes nothing.
        QCOMPARE(r.router.conflict(Model::JU06A, Rig::cfg(3, false)).value(), Model::JP08);
        // Inputs: shared on the same terms (only when follow is on in both).
        QVERIFY(r.router.conflict(Model::JU06A, Rig::cfg(3, true)).has_value());
    }

    void twoDevicesOnOnePortReceiveOnlyTheirOwnChannel()
    {
        Rig r;
        r.configureAll(true);
        QCOMPARE(r.hub.openInputCount(), 1);
        QCOMPARE(r.hub.backendCount(), 1);
        QSignalSpy jp(r.router.link(Model::JP08), &MidiLink::followReceived);
        QSignalSpy ju(r.router.link(Model::JU06A), &MidiLink::programReceived);
        QSignalSpy d5(r.router.link(Model::D05), &MidiLink::followReceived);
        r.inject({0xC2, 10});  // JP-08's channel
        QCOMPARE(jp.count(), 1);
        QCOMPARE(ju.count(), 0);
        QCOMPARE(d5.count(), 0);
        r.inject({0xC4, 11});  // JU-06A's
        QCOMPARE(jp.count(), 1);
        QCOMPARE(ju.count(), 1);
        QCOMPARE(ju.at(0).at(0).toInt(), 11);
        r.inject({0xC5, 12});  // nobody's
        r.inject({0xF8});
        QCOMPARE(jp.count() + ju.count() + d5.count(), 2);
        QCOMPARE(r.router.link(Model::JP08)->current().value(), addr(Model::JP08, "2-3"));
    }

    void programChange64To127IsIgnoredOnThe64PatchModels()
    {
        Rig r;
        r.configureAll(true);
        QSignalSpy jp(r.router.link(Model::JP08), &MidiLink::followReceived);
        QSignalSpy ju(r.router.link(Model::JU06A), &MidiLink::programReceived);
        r.inject({0xC2, 64});
        r.inject({0xC2, 127});
        r.inject({0xC4, 100});
        QCOMPARE(jp.count() + ju.count(), 0);
        r.inject({0xC2, 63});
        QCOMPARE(jp.count(), 1);
    }

    void echoSuppressionIsPerLink()
    {
        Rig r;
        r.configureAll(true);
        QSignalSpy jp(r.router.link(Model::JP08), &MidiLink::followReceived);
        QSignalSpy ju(r.router.link(Model::JU06A), &MidiLink::programReceived);
        QVERIFY(r.router.select(addr(Model::JP08, "2-3")));  // PC 10 on channel 3
        r.inject({0xC2, 10});                                  // the loop-back: suppressed
        QCOMPARE(jp.count(), 0);
        r.inject({0xC4, 10});  // the same number on the JU-06A's channel is a real event
        QCOMPARE(ju.count(), 1);
        // After the window the same message counts again.
        r.router.link(Model::JP08)->setEchoWindowMs(0);
        r.inject({0xC2, 10});
        QCOMPARE(jp.count(), 1);  // no longer an echo: a follow event (equal to current, still reported)
    }

    void ju06aFollowEventDrivesTheTracker()
    {
        Rig r;
        r.configureAll(true);
        EngineTracker *t = r.router.ju06aTracker();
        r.router.setJu06aState(60, 3, 20);
        QSignalSpy cur(t, &EngineTracker::currentChanged);
        r.inject({0xC4, 7});
        QCOMPARE(t->current().value(), addr(Model::JU06A, "60:1-8", 60));
        QCOMPARE(t->remembered(60), 7);
        QCOMPARE(cur.count(), 1);
        // Our own send is not fed to the tracker.
        QVERIFY(r.router.select(addr(Model::JU06A, "60:2-3", 60)));
        QCOMPARE(t->remembered(60), 10);
        r.inject({0xC4, 10});
        QCOMPARE(t->remembered(60), 10);
        QCOMPARE(t->hint(), 0);
        // A PC equal to the other engine's remembered program raises the hint (section 6).
        r.inject({0xC4, 20});
        QCOMPARE(t->hint(), 106);
        // Flip detection reflects an open input with follow on.
        QVERIFY(r.router.link(Model::JU06A)->inputConnected());
        t->escape();
        QVERIFY(r.router.select(addr(Model::JU06A, "106:1-1", 106)));
        QCOMPARE(t->prompt(), EngineTracker::Prompt::FlipSwitch);
        // Flip seen: the unit re-sends the armed engine's old patch (106 remembered 20).
        r.fake()->clearSent();
        r.inject({0xC4, 20});
        QCOMPARE(t->engine(), 106);
        QCOMPARE(r.fake()->sentMessages(), QList<QByteArray>{bytes({0xC4, 0})});
    }

    void flipPromptWithoutAnInputPath()
    {
        Rig r;
        r.configureAll(false);
        r.router.setJu06aState(60, 3, 20);
        QVERIFY(r.router.select(addr(Model::JU06A, "106:1-1", 106)));
        QCOMPARE(r.router.ju06aTracker()->prompt(), EngineTracker::Prompt::FlipThenCtrlE);
    }

    void followSwitchesDeviceOnAndOff()
    {
        Rig r;
        r.configureAll(true);
        QSignalSpy sw(&r.router, &DeviceRouter::currentDeviceChanged);
        QCOMPARE(r.router.currentDevice(), Model::D05);
        QVERIFY(r.router.followSwitchesDevice());
        r.fake()->clearSent();

        r.inject({0xC2, 10});  // JP-08 event, D-05 current
        QCOMPARE(r.router.currentDevice(), Model::JP08);
        QCOMPARE(sw.count(), 1);
        QCOMPARE(sw.at(0).at(0).value<Model>(), Model::JP08);
        QVERIFY(sw.at(0).at(1).toBool());
        QCOMPARE(r.router.link(Model::JP08)->current().value(), addr(Model::JP08, "2-3"));
        QVERIFY(r.fake()->sentMessages().isEmpty());  // nothing is sent on a switch

        r.inject({0xC2, 11});  // same device: no switch
        QCOMPARE(sw.count(), 1);

        // Off: the event is applied to the device, the window stays.
        r.router.setFollowSwitchesDevice(false);
        r.inject({0xC4, 7});
        QCOMPARE(r.router.currentDevice(), Model::JP08);
        QCOMPARE(sw.count(), 1);
        QCOMPARE(r.router.ju06aTracker()->remembered(60), 7);
        r.inject({0xC0, 87});  // D-05: lone PC after nothing, falls back to its group
        r.router.setFollowSwitchesDevice(true);

        // On: a device with follow off has no input by design and does not block (FR-20).
        auto d5 = Rig::cfg(1, false);
        QVERIFY(r.router.configure(Model::D05, d5));
        r.inject({0xC4, 8});
        QCOMPARE(r.router.currentDevice(), Model::JU06A);
        QCOMPARE(r.router.ju06aTracker()->remembered(60), 8);  // and the event was applied
        QCOMPARE(sw.count(), 2);
    }

    void switchingClearsTheJu06aArm()
    {
        Rig r;
        r.configureAll(false);
        r.router.setCurrentDevice(Model::JU06A);
        r.router.setJu06aState(60, -1, -1);
        QVERIFY(r.router.select(addr(Model::JU06A, "106:1-1", 106)));
        QVERIFY(r.router.ju06aTracker()->armed().has_value());
        r.router.setCurrentDevice(Model::JP08);
        QVERIFY(!r.router.ju06aTracker()->armed().has_value());
        r.router.setCurrentDevice(Model::JP08);  // no change, no signal
    }

    void pollRunsOnlyWhileAnEnabledDeviceIsDisconnected()
    {
        Rig r;
        r.outs = {};  // the forwarder is unplugged
        QCOMPARE(r.router.pollIntervalMs(), 2000);
        QVERIFY(!r.router.isPolling());
        QVERIFY(r.router.configure(Model::JP08, Rig::cfg(3, false)));
        QVERIFY(r.router.isPolling());
        for (Model m : allModels())  // the links have no timers of their own
            QVERIFY(!r.router.link(m)->isPolling());
        // A second device that is also missing changes nothing.
        QVERIFY(r.router.configure(Model::JU06A, Rig::cfg(5, false)));
        QVERIFY(r.router.isPolling());
        r.router.pollNow();
        QVERIFY(r.router.isPolling());
        // Plug in: the next poll connects both and the timer stops.
        r.fake()->setOutputPorts({S("Hapax Out")}, false);
        r.router.pollNow();
        QVERIFY(r.router.link(Model::JP08)->isConnected());
        QVERIFY(r.router.link(Model::JU06A)->isConnected());
        QVERIFY(r.router.allConnected());
        QVERIFY(!r.router.isPolling());
        // Unplug and notice (window activation): it starts again.
        r.fake()->setOutputPorts({}, false);
        r.router.checkConnections();
        QVERIFY(r.router.isPolling());
        // A device with no port chosen needs no poll; neither does a disabled one.
        auto none = Rig::cfg(3, false);
        none.outputPort.clear();
        QVERIFY(r.router.configure(Model::JP08, none));
        none = Rig::cfg(5, false);
        none.enabled = false;
        QVERIFY(r.router.configure(Model::JU06A, none));
        QVERIFY(!r.router.isPolling());
    }

    void inputPollStartsAndStopsToo()
    {
        Rig r;
        r.ins = {};
        QVERIFY(r.router.configure(Model::JP08, Rig::cfg(3, true)));
        QVERIFY(r.router.link(Model::JP08)->isConnected());
        QVERIFY(r.router.isPolling());  // the output is up, the wanted input is not
        r.fake()->setInputPorts({S("Hapax In")}, false);
        r.router.pollNow();
        QVERIFY(r.router.link(Model::JP08)->inputConnected());
        QVERIFY(!r.router.isPolling());
    }

    // --- Review fixes -------------------------------------------------------------------------

    void followSwitchingIgnoresEnabledDevicesWithFollowOff()
    {
        Rig r;
        QVERIFY(r.router.configure(Model::D05, Rig::cfg(1, true)));
        QVERIFY(r.router.configure(Model::JP08, Rig::cfg(3, false)));  // no input by design
        r.router.setCurrentDevice(Model::JP08);
        QVERIFY(r.router.allInputsOpen());
        r.inject({0xB0, 0, 87});
        r.inject({0xB0, 32, 0});
        r.inject({0xC0, 5});
        QCOMPARE(r.router.currentDevice(), Model::D05);
        // A follow device whose input is down still blocks it.
        r.router.setCurrentDevice(Model::JP08);
        auto lost = Rig::cfg(5, true);
        lost.inputPort = S("Nowhere");
        QVERIFY(r.router.configure(Model::JU06A, lost));
        QVERIFY(!r.router.allInputsOpen());
        r.inject({0xB0, 0, 87});
        r.inject({0xB0, 32, 0});
        r.inject({0xC0, 6});
        QCOMPARE(r.router.currentDevice(), Model::JP08);  // blocked by the follow device that is blind
    }

    void failedSameEngineSendLeavesTheTrackerAsItWas()
    {
        Rig r;
        r.configureAll(false);
        r.router.setJu06aState(60, 3, 20);
        EngineTracker *t = r.router.ju06aTracker();
        r.fake()->setFailSend(true);
        QVERIFY(!r.router.select(addr(Model::JU06A, "60:2-1", 60)));
        QCOMPARE(t->remembered(60), 3);
        QCOMPARE(t->current().value(), addr(Model::JU06A, "60:1-4", 60));
        QVERIFY(!t->armed().has_value());
        QVERIFY(!r.router.link(Model::JU06A)->sendPatch(addr(Model::JU06A, "60:2-1", 60)));
    }

    void armThenToggleWithTheOutputDownStaysArmed()
    {
        Rig r;
        r.configureAll(false);
        r.router.setJu06aState(60, 3, 20);
        EngineTracker *t = r.router.ju06aTracker();
        QVERIFY(r.router.select(addr(Model::JU06A, "106:1-1", 106)));
        QVERIFY(t->armed().has_value());
        r.fake()->setOutputPorts({}, false);
        r.router.checkConnections();
        QVERIFY(!r.router.link(Model::JU06A)->isConnected());
        t->toggleEngine();
        QCOMPARE(t->engine(), 106);  // the toggle stands
        QCOMPARE(r.router.link(Model::JU06A)->engine(), 106);
        QVERIFY(t->armed().has_value());  // the armed send did not happen
        QCOMPARE(t->remembered(106), 20);
        QCOMPARE(t->current().value(), addr(Model::JU06A, "106:3-5", 106));
        QVERIFY(t->prompt() != EngineTracker::Prompt::None);
    }

    void armedJu06aConfirmingPcSendsAndSwitchesTheWindowWhenFollowSwitchesIsOn()
    {
        Rig r;
        r.configureAll(true);
        r.router.setJu06aState(60, 3, 20);
        EngineTracker *t = r.router.ju06aTracker();
        QCOMPARE(r.router.currentDevice(), Model::D05);
        QVERIFY(r.router.select(addr(Model::JU06A, "106:1-1", 106)));
        QVERIFY(t->armed().has_value());
        r.fake()->clearSent();
        r.inject({0xC4, 20});  // the flip re-sends engine 106's old patch
        QCOMPARE(r.router.currentDevice(), Model::JU06A);
        QCOMPARE(t->engine(), 106);
        QVERIFY(!t->armed().has_value());
        QCOMPARE(r.fake()->sentMessages(), QList<QByteArray>{bytes({0xC4, 0})});
    }

    void armedJu06aConfirmingPcSendsAndTheWindowStaysWhenFollowSwitchesIsOff()
    {
        Rig r;
        r.configureAll(true);
        r.router.setFollowSwitchesDevice(false);
        r.router.setJu06aState(60, 3, 20);
        EngineTracker *t = r.router.ju06aTracker();
        QVERIFY(r.router.select(addr(Model::JU06A, "106:1-1", 106)));
        r.fake()->clearSent();
        r.inject({0xC4, 20});
        QCOMPARE(r.router.currentDevice(), Model::D05);
        QCOMPARE(t->engine(), 106);
        QVERIFY(!t->armed().has_value());
        QCOMPARE(r.fake()->sentMessages(), QList<QByteArray>{bytes({0xC4, 0})});
    }

    void userSwitchStillClearsTheArmWhileFollowInducedSwitchDoesNot()
    {
        Rig r;
        r.configureAll(true);
        r.router.setJu06aState(60, 3, 20);
        EngineTracker *t = r.router.ju06aTracker();
        QVERIFY(r.router.select(addr(Model::JU06A, "106:1-1", 106)));
        r.inject({0xC2, 10});  // JP-08 event switches the window by follow
        QCOMPARE(r.router.currentDevice(), Model::JP08);
        QVERIFY(t->armed().has_value());
        r.router.setCurrentDevice(Model::JU06A);  // the user
        QVERIFY(!t->armed().has_value());
    }

    void armOnTheCurrentEngineDoesNotHijackAPanelPress()
    {
        Rig r;
        r.configureAll(true);
        r.router.setJu06aState(60, 3, 20);
        EngineTracker *t = r.router.ju06aTracker();
        QVERIFY(r.router.select(addr(Model::JU06A, "106:1-1", 106)));  // armed
        r.fake()->setFailSend(true);
        t->toggleEngine();  // Ctrl+E: engine 106, the send fails, the arm is kept
        QCOMPARE(t->engine(), 106);
        QVERIFY(t->armed().has_value());
        r.fake()->setFailSend(false);
        r.fake()->clearSent();
        r.inject({0xC4, 20});  // a panel press, not a flip confirmation
        QVERIFY(r.fake()->sentMessages().isEmpty());
        QVERIFY(!t->armed().has_value());
        QCOMPARE(t->remembered(106), 20);
    }

    void armThenConfirmingPcWithAFailingSendStaysArmed()
    {
        Rig r;
        r.configureAll(true);
        r.router.setJu06aState(60, 3, 20);
        r.router.setCurrentDevice(Model::JU06A);  // a switch would clear the arm
        EngineTracker *t = r.router.ju06aTracker();
        QVERIFY(r.router.select(addr(Model::JU06A, "106:1-1", 106)));
        r.fake()->setFailSend(true);
        r.fake()->clearSent();
        r.inject({0xC4, 20});
        QCOMPARE(t->engine(), 106);
        QVERIFY(t->armed().has_value());
        QCOMPARE(t->remembered(106), 20);
        QCOMPARE(t->current().value(), addr(Model::JU06A, "106:3-5", 106));
        QVERIFY(r.fake()->sentMessages().isEmpty());
    }

    void disconnectedSelectionStillClearsTheArm()
    {
        Rig r;
        r.configureAll(false);
        r.router.setJu06aState(60, 3, 20);
        EngineTracker *t = r.router.ju06aTracker();
        r.fake()->setOutputPorts({}, false);
        r.router.checkConnections();
        QVERIFY(r.router.select(addr(Model::JU06A, "106:1-1", 106)));  // arms, needs no connection
        QVERIFY(t->armed().has_value());
        QVERIFY(!r.router.select(addr(Model::JU06A, "60:2-1", 60)));
        QVERIFY(!t->armed().has_value());
        QCOMPARE(t->remembered(60), 3);
        QCOMPARE(t->current().value(), addr(Model::JU06A, "60:1-4", 60));
    }

    void sharedOutputFailureInvalidatesEveryUser()
    {
        Rig r;
        r.configureAll(false);
        QCOMPARE(r.hub.openOutputCount(), 1);
        r.router.setJu06aState(60, -1, -1);
        r.fake()->setFailSend(true);
        QVERIFY(!r.router.select(addr(Model::JP08, "1-1")));
        r.fake()->setFailSend(false);
        QVERIFY(!r.router.link(Model::JP08)->isConnected());
        QVERIFY(!r.router.link(Model::JU06A)->isConnected());
        QVERIFY(!r.router.link(Model::D05)->isConnected());
        QCOMPARE(r.hub.openOutputCount(), 0);
        QVERIFY(r.router.isPolling());
        r.router.pollNow();
        QVERIFY(r.router.allConnected());
        QVERIFY(r.router.select(addr(Model::JU06A, "60:1-1", 60)));
    }

    void sharedOutputVanishDetectedByOneInvalidatesEveryUser()
    {
        Rig r;
        r.configureAll(false);
        r.fake()->setOutputPorts({}, false);  // unplugged, no notification
        r.router.link(Model::JP08)->checkConnection();
        QVERIFY(!r.router.link(Model::JU06A)->isConnected());
        QVERIFY(!r.router.link(Model::D05)->isConnected());
        QVERIFY(r.router.isPolling());
        r.fake()->setOutputPorts({S("Hapax Out")}, false);
        r.router.pollNow();
        QVERIFY(r.router.allConnected());
        QVERIFY(r.router.link(Model::D05)->isConnected());
    }

    void sharedInputVanishDetectedByOneInvalidatesEveryUser()
    {
        Rig r;
        r.configureAll(true);
        r.fake()->setInputPorts({}, false);
        r.router.link(Model::JP08)->checkConnection();
        QVERIFY(!r.router.link(Model::JU06A)->inputConnected());
        QVERIFY(!r.router.link(Model::D05)->inputConnected());
        r.fake()->setInputPorts({S("Hapax In")}, false);
        r.router.pollNow();
        QVERIFY(r.router.link(Model::JU06A)->inputConnected());
        QVERIFY(r.router.link(Model::D05)->inputConnected());
        r.inject({0xC2, 10});
        QCOMPARE(r.router.link(Model::JP08)->current().value(), addr(Model::JP08, "2-3"));
    }

    void distinctFullPortNamesAreNotOneConflict()
    {
        // WinMM names have no ALSA " id:sub" suffix and carry a " <index>" that portKey strips, so
        // a saved key such as "Hapax:Hapax MIDI 1" is not a key there. Use each platform's own shapes.
        const bool winmm = MidiLink::portKey(S("X 1")) != S("X 1");
        const QString n1 = winmm ? S("Hapax MIDI 1") : S("Hapax:Hapax MIDI 1 24:0");
        const QString n2 = winmm ? S("Hapax MIDI 2") : S("Hapax:Hapax MIDI 1 28:0");
        const QString n3 = winmm ? S("Hapax MIDI 3") : S("Hapax:Hapax MIDI 1 30:0");
        const QString key = winmm ? S("Hapax MIDI") : S("Hapax:Hapax MIDI 1");
        QVERIFY(MidiLink::portKey(key) == key);
        Rig r;
        r.outs = {n1, n2};
        r.ins = r.outs;
        auto a = Rig::cfg(3, false, n1.toLatin1().constData());
        auto b = Rig::cfg(3, false, n2.toLatin1().constData());
        QVERIFY(r.router.configure(Model::JP08, a));
        QVERIFY(!r.router.conflict(Model::JU06A, b).has_value());
        QVERIFY(r.router.configure(Model::JU06A, b));
        // The same full name, or a saved key that names it, still conflicts.
        QVERIFY(r.router.conflict(Model::D05, Rig::cfg(3, false, n1.toLatin1().constData())).has_value());
        QVERIFY(r.router.conflict(Model::D05, Rig::cfg(3, false, key.toLatin1().constData())).has_value());
        a.follow = b.follow = true;
        QVERIFY(!r.router.conflict(Model::D05, Rig::cfg(3, true, n3.toLatin1().constData())).has_value());
    }

    // ------------------------------------------------------------------ T8.2 additions

    void findConflictOverAPendingSetReportsTheLaterDeviceOnly()
    {
        DeviceRouter::Configs set;
        set[0] = Rig::cfg(3, false);  // D-05
        set[1] = Rig::cfg(3, false);  // JP-08: same port and channel
        set[2] = Rig::cfg(4, false);
        const auto both = DeviceRouter::findConflict(set, Model::D05);
        QVERIFY(both && both->other == Model::JP08 && !both->input);
        // earlierOnly: the D-05 reports nothing, the JP-08 names the D-05.
        QVERIFY(!DeviceRouter::findConflict(set, Model::D05, true));
        const auto later = DeviceRouter::findConflict(set, Model::JP08, true);
        QVERIFY(later && later->other == Model::D05 && !later->input);
        QVERIFY(!DeviceRouter::findConflict(set, Model::JU06A, true));
        // Inputs: only when both follow.
        set[1] = Rig::cfg(3, true, "Other");
        set[0] = Rig::cfg(3, true);
        set[1].outputPort = S("Elsewhere");
        set[1].inputPort = set[0].inputPort;
        const auto input = DeviceRouter::findConflict(set, Model::JP08, true);
        QVERIFY(input && input->other == Model::D05 && input->input);
        set[1].follow = false;
        QVERIFY(!DeviceRouter::findConflict(set, Model::JP08, true));
        set[1].follow = true;
        set[1].enabled = false;  // a disabled device never conflicts, either way
        QVERIFY(!DeviceRouter::findConflict(set, Model::JP08, true));
        QVERIFY(!DeviceRouter::findConflict(set, Model::D05));
    }

    void configureAllAppliesASwapAndRefusesAConflict()
    {
        Rig r;
        r.configureAll(false);  // D-05 1, JP-08 3, JU-06A 5, one port
        DeviceRouter::Configs swap;
        for (Model m : allModels())
            swap[int(m)] = r.router.config(m);
        std::swap(swap[0].channel, swap[1].channel);  // D-05 3, JP-08 1: one at a time each would collide
        Model a = Model::JU06A, b = Model::JU06A;
        QVERIFY(!r.router.configure(Model::D05, swap[0], &a));  // (what configureAll avoids)
        QVERIFY(r.router.configureAll(swap, &a, &b));
        QCOMPARE(r.router.config(Model::D05).channel, 3);
        QCOMPARE(r.router.config(Model::JP08).channel, 1);
        QCOMPARE(r.router.link(Model::D05)->channel(), 3);
        QCOMPARE(r.router.link(Model::JP08)->channel(), 1);
        // A set with a conflict changes nothing and names the pair.
        DeviceRouter::Configs bad = swap;
        bad[2].channel = 3;  // the JU-06A on the D-05's channel and port
        QVERIFY(!r.router.configureAll(bad, &a, &b));
        QCOMPARE(a, Model::D05);
        QCOMPARE(b, Model::JU06A);
        QCOMPARE(r.router.config(Model::JU06A).channel, 5);
        // Turning a device off in the set closes its ports.
        DeviceRouter::Configs off = swap;
        off[1].enabled = false;
        QVERIFY(r.router.configureAll(off));
        QVERIFY(!r.router.isEnabled(Model::JP08));
        QVERIFY(!r.router.link(Model::JP08)->isConnected());
        QVERIFY(r.router.link(Model::D05)->isConnected());  // untouched
    }

    void reapplyingAnUnchangedConfigurationReopensNothing()
    {
        Rig r;
        r.configureAll(true);
        FakeMidiBackend *f = r.fake();
        const int in = r.hub.openInputCount(), out = r.hub.openOutputCount();
        QSignalSpy status(r.router.link(Model::JP08), &MidiLink::statusChanged);
        QSignalSpy inputStatus(r.router.link(Model::JP08), &MidiLink::inputStatusChanged);
        DeviceRouter::Configs same;
        for (Model m : allModels())
            same[int(m)] = r.router.config(m);
        QVERIFY(r.router.configureAll(same));
        QVERIFY(r.router.configure(Model::JP08, same[1]));
        QCOMPARE(r.hub.openInputCount(), in);
        QCOMPARE(r.hub.openOutputCount(), out);
        QCOMPARE(status.count(), 0);
        QCOMPARE(inputStatus.count(), 0);
        QVERIFY(r.router.link(Model::JP08)->isConnected() && r.router.link(Model::JP08)->inputConnected());
        Q_UNUSED(f);
        // A port that is not open is tried again, as the poll would.
        Rig m;
        auto missing = Rig::cfg(3, false, "Nowhere");
        QVERIFY(m.router.configure(Model::JP08, missing));
        QVERIFY(!m.router.link(Model::JP08)->isConnected());
        m.outs << S("Nowhere");
        for (FakeMidiBackend *b : m.fakes)
            b->setOutputPorts(m.outs, false);
        QVERIFY(m.router.configure(Model::JP08, missing));
        QVERIFY(m.router.link(Model::JP08)->isConnected());
    }

    void anAdoptedD05LinkIsEnabledAndPollsItself()
    {
        FakeMidiBackend backend;
        MidiLink link(&backend);
        link.setChannel(7);
        link.setOutputPort(S("Fake D-05 In"));
        PortHub hub([]() -> MidiBackend * { return nullptr; });
        DeviceRouter router(&hub, nullptr, &link);
        QVERIFY(router.link(Model::D05) == &link);
        QVERIFY(router.isEnabled(Model::D05));
        QCOMPARE(router.config(Model::D05).channel, 7);
        QCOMPARE(router.config(Model::D05).outputPort, S("Fake D-05 In"));
        QVERIFY(!router.isEnabled(Model::JP08) && !router.isEnabled(Model::JU06A));
        // A missing port: the link's own timer polls, the router starts no second one.
        link.setOutputPort(S("Nowhere"));
        QVERIFY(link.isPolling());
        QVERIFY(!router.isPolling());
        QVERIFY(!router.allConnected());
    }
};

QTEST_MAIN(TestDeviceRouter)
#include "test_device_router.moc"
