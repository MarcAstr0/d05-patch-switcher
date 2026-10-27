// T8.4: follow across devices (FR-20) at the window level. Three enabled devices share one fake port
// on channels 3 (D-05), 4 (JU-06A) and 5 (JP-08), all following. Headless, temporary config dirs.
#include "app/AppSettings.h"
#include "app/Cli.h"
#include "app/Paths.h"
#include "app/Screenshot.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "midi/DeviceRouter.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiLink.h"
#include "midi/PortHub.h"
#include "ui/BankGrid.h"
#include "ui/DeviceSwitcher.h"
#include "ui/DisplayWidget.h"
#include "ui/EngineControl.h"
#include "ui/GroupBar.h"
#include "ui/LedDisplayWidget.h"
#include "ui/MainWindow.h"
#include "ui/PanelButton.h"
#include "ui/PatchDelegate.h"
#include "ui/PatchListModel.h"
#include "ui/PatchListView.h"
#include "ui/SettingsDialog.h"
#include "ui/Shortcuts.h"
#include "ui/Theme.h"

#include <QAction>
#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStatusBar>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace d05;

namespace {

QString S(const char *t) { return QString::fromLatin1(t); }

PatchAddress jp(const char *t) { return *PatchAddress::parse(S(t), Model::JP08); }
PatchAddress ju(const char *t) { return *PatchAddress::parse(S(t), Model::JU06A); }
PatchAddress dd(const char *t) { return *PatchAddress::parse(S(t)); }

AppSettings::DeviceValues dev(int channel, const char *port = "Hapax", bool follow = false, bool enabled = true)
{
    AppSettings::DeviceValues v;
    v.enabled = enabled;
    v.outputPort = S(port);
    v.channel = channel;
    if (follow) {
        v.inputPort = S(port);
        v.follow = true;
    }
    return v;
}

class PaintCounter : public QObject {
public:
    int paints = 0;
    bool eventFilter(QObject *, QEvent *e) override
    {
        if (e->type() == QEvent::Paint)
            ++paints;
        return false;
    }
};

// A window on a router over a hub of fake backends: one forwarder port for the three devices.
// D-05 on channel 3, JU-06A on 4, JP-08 on 5.
struct Rig {
    QTemporaryDir dir;
    QString iniPath;
    PatchCatalog catalog;
    std::unique_ptr<Library> library;
    QList<FakeMidiBackend *> fakes;
    QStringList outs{S("Hapax Out")};
    QStringList ins{S("Hapax In")};
    std::unique_ptr<PortHub> hub;
    std::unique_ptr<DeviceRouter> router;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<MainWindow> w;

    explicit Rig(bool d05On = true, bool jp08On = true, bool ju06aOn = true, bool follow = true)
    {
        Paths::setOverride(dir.path());
        iniPath = dir.filePath(QStringLiteral("settings.ini"));
        library = std::make_unique<Library>(catalog, dir.filePath(QStringLiteral("library.json")));
        hub = std::make_unique<PortHub>([this]() -> MidiBackend * {
            auto *f = new FakeMidiBackend;
            f->setOutputPorts(outs, false);
            f->setInputPorts(ins, false);
            fakes << f;
            return f;
        });
        router = std::make_unique<DeviceRouter>(hub.get());
        settings = std::make_unique<AppSettings>(iniPath);
        w = std::make_unique<MainWindow>(catalog, *library, *router);
        w->setSystemLookup(false);
        w->setServices(settings.get());
        put(Model::D05, dev(3, "Hapax", follow, d05On));
        put(Model::JP08, dev(5, "Hapax", follow, jp08On));
        put(Model::JU06A, dev(4, "Hapax", follow, ju06aOn));
    }
    void put(Model m, const AppSettings::DeviceValues &v)
    {
        w->applyDevice(m, v);
        settings->setDevice(m, v);
    }
    ~Rig()
    {
        w.reset();
        settings.reset();
        router.reset();
        hub.reset();
        library.reset();
        Paths::setOverride(QString());
    }
    bool show()
    {
        w->resize(960, 560);
        w->show();
        w->activateWindow();
        return QTest::qWaitForWindowActive(w.get());
    }
    // Every message any fake backend has sent.
    QList<QByteArray> sent() const
    {
        QList<QByteArray> all;
        for (const FakeMidiBackend *f : fakes)
            all += f->sentMessages();
        return all;
    }
    void clearSent()
    {
        for (FakeMidiBackend *f : fakes)
            f->clearSent();
    }
    // A message arriving on the open input.
    // Messages arriving on the open input; a status byte starts a new message, as RtMidi delivers them.
    void inject(std::initializer_list<int> v)
    {
        QList<QByteArray> msgs;
        for (int x : v) {
            if (x >= 0x80 || msgs.isEmpty())
                msgs.append(QByteArray());
            msgs.last().append(char(x));
        }
        for (FakeMidiBackend *f : fakes) {
            if (f->isInputOpen()) {
                for (const QByteArray &m : msgs)
                    f->injectReceived(m);
                break;
            }
        }
        QCoreApplication::processEvents();
    }
    EngineTracker *tracker() const { return router->ju06aTracker(); }
    // Selects `a` in the list as the user would, and presses Enter there.
    void choose(const PatchAddress &a)
    {
        w->list()->setCurrentIndex(w->model()->indexOf(a));
        w->list()->setFocus();
        QTest::keyClick(w->list(), Qt::Key_Return);
    }
    QString context() const { return w->ledDisplay()->context(); }
};


}  // namespace

// Bytes a panel change sends: the D-05 triplet on channel 3 (U1-23), a lone PC on channel 5 (JP-08
// 2-3, program 10) and on channel 4 (JU-06A 4-5, program 28; 106's remembered 48 is 7-1).
#define D05_TRIPLET 0xB2, 0x00, 0x57, 0xB2, 0x20, 0x00, 0xC2, 0x0A

class TestFollowDevices : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QApplication::setCursorFlashTime(0);
        Theme::install(*qobject_cast<QApplication *>(qApp));
    }
    void init() { Theme::setSkin(QStringLiteral("d05")); }
    void cleanup() { Theme::setSkin(QStringLiteral("d05")); }

    void followEventsSwitchTheWindowAndSendNothing()
    {
        Rig r;
        r.router->setJu06aState(60, -1, -1);
        QVERIFY(r.show());
        QVERIFY(r.router->allInputsOpen());
        QCOMPARE(r.w->currentDevice(), Model::D05);
        r.clearSent();

        r.inject({0xC4, 10});  // JP-08 channel 5
        QCOMPARE(r.w->currentDevice(), Model::JP08);
        QCOMPARE(Theme::skin().id, QStringLiteral("jp08"));
        QCOMPARE(r.w->switcher()->current(), Model::JP08);
        QCOMPARE(r.w->currentPatch().value(), jp("2-3"));
        QCOMPARE(r.w->model()->current(), jp("2-3"));
        QCOMPARE(r.w->ledDisplay()->readout(), S("23"));

        r.inject({0xC3, 28});  // JU-06A channel 4, engine 60
        QCOMPARE(r.w->currentDevice(), Model::JU06A);
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-60"));
        QCOMPARE(r.w->currentPatch().value(), ju("60:4-5"));
        QCOMPARE(r.w->ledDisplay()->readout(), S("4-5"));

        r.inject({D05_TRIPLET});
        QCOMPARE(r.w->currentDevice(), Model::D05);
        QCOMPARE(Theme::skin().id, QStringLiteral("d05"));
        QCOMPARE(r.w->currentPatch().value(), dd("U1-23"));

        QVERIFY(r.sent().isEmpty());  // a switch by follow sends nothing
        // Following is not selecting: Recents stay empty on all three.
        for (Model m : allModels())
            QVERIFY(r.library->recents(m).isEmpty());
    }

    void aFollowSwitchPaintsEachWidgetOnce()
    {
        Rig r;
        QVERIFY(r.show());
        QTest::qWait(80);
        for (int i = 0; i < 100; ++i) {
            const bool toJp = i % 2 == 0;
            PaintCounter win, plate, leds, lcd, status;
            r.w->installEventFilter(&win);
            r.w->switcher()->installEventFilter(&plate);
            r.w->ledDisplay()->installEventFilter(&leds);
            r.w->display()->installEventFilter(&lcd);
            r.w->statusBar()->installEventFilter(&status);
            if (toJp)
                r.inject({0xC4, i % 64});
            else
                r.inject({D05_TRIPLET});
            QTest::qWait(30);
            r.w->removeEventFilter(&win);
            r.w->switcher()->removeEventFilter(&plate);
            r.w->ledDisplay()->removeEventFilter(&leds);
            r.w->display()->removeEventFilter(&lcd);
            r.w->statusBar()->removeEventFilter(&status);
            QCOMPARE(r.w->currentDevice(), toJp ? Model::JP08 : Model::D05);
            QCOMPARE(win.paints, 1);
            QCOMPARE(plate.paints, 1);
            QCOMPARE(toJp ? leds.paints : lcd.paints, 1);
            QCOMPARE(toJp ? lcd.paints : leds.paints, 0);
            QCOMPARE(status.paints, 1);
        }
    }

    void withFollowSwitchingOffTheStateUpdatesAndTheWindowStays()
    {
        Rig r;
        r.router->setJu06aState(60, -1, -1);
        r.w->applyGlobal(false, false);
        QVERIFY(!r.router->followSwitchesDevice());
        QVERIFY(r.show());
        r.clearSent();
        r.inject({0xC4, 10});
        r.inject({0xC3, 28});
        QCOMPARE(r.w->currentDevice(), Model::D05);
        QCOMPARE(Theme::skin().id, QStringLiteral("d05"));
        QCOMPARE(r.router->link(Model::JP08)->current().value(), jp("2-3"));
        QCOMPARE(r.tracker()->current().value(), ju("60:4-5"));
        for (Model m : allModels())
            QVERIFY(r.library->recents(m).isEmpty());
        // Switching to it later shows the followed patch.
        r.w->switchDevice(Model::JP08);
        QCOMPARE(r.w->currentPatch().value(), jp("2-3"));
        QCOMPARE(r.w->model()->current(), jp("2-3"));
        QCOMPARE(r.w->ledDisplay()->readout(), S("23"));
        r.w->switchDevice(Model::JU06A);
        QCOMPARE(r.w->currentPatch().value(), ju("60:4-5"));
        QCOMPARE(r.w->ledDisplay()->readout(), S("4-5"));
        // The D-05 too.
        r.inject({D05_TRIPLET});
        QCOMPARE(r.w->currentDevice(), Model::JU06A);
        r.w->switchDevice(Model::D05);
        QCOMPARE(r.w->currentPatch().value(), dd("U1-23"));
        QVERIFY(r.sent().isEmpty());
    }

    void aFlipHintRaisedWhileAnotherDeviceIsCurrentSurvivesTheSwitch()
    {
        for (bool followSwitches : {false, true}) {
            Theme::setSkin(QStringLiteral("d05"));
            Rig r;
            r.router->setJu06aState(60, 28, 48);
            r.w->applyGlobal(false, followSwitches);
            QVERIFY(r.show());
            QCOMPARE(r.w->currentDevice(), Model::D05);
            r.inject({0xC3, 48});  // 106's remembered program, not armed: a hint
            QCOMPARE(r.tracker()->hint(), 106);
            QCOMPARE(r.tracker()->engine(), 60);  // never a silent switch
            QCOMPARE(r.w->currentDevice(), followSwitches ? Model::JU06A : Model::D05);
            if (!followSwitches)
                r.w->switchDevice(Model::JU06A);  // a user switch keeps the hint
            QCOMPARE(r.tracker()->hint(), 106);
            QVERIFY(r.context().startsWith(S("FLIPPED")));
            QVERIFY(r.w->engineControl()->armedLens(106));
            // Ctrl+E answers it.
            QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
            QCOMPARE(r.tracker()->engine(), 106);
            QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-106"));
        }
    }

    void anArmedFlipConfirmationAnswersWhileTheD05IsCurrent()
    {
        for (bool followSwitches : {true, false}) {
            Theme::setSkin(QStringLiteral("d05"));  // the skin is global: the previous round left its own
            Rig r;
            r.router->setJu06aState(60, 28, 48);
            r.w->applyGlobal(false, followSwitches);
            QVERIFY(r.show());
            r.router->setCurrentDevice(Model::D05);
            QVERIFY(r.router->select(ju("106:1-1")));  // arms, sends nothing
            QCOMPARE(r.tracker()->armed().value(), ju("106:1-1"));
            r.clearSent();
            r.inject({0xC3, 48});  // the unit flipped and re-sent 106's remembered patch
            QCOMPARE(r.sent(), (QList<QByteArray>{QByteArray::fromHex("c300")}));
            QCOMPARE(r.tracker()->engine(), 106);
            QVERIFY(!r.tracker()->armed());
            QCOMPARE(r.tracker()->current().value(), ju("106:1-1"));
            if (followSwitches) {
                QCOMPARE(r.w->currentDevice(), Model::JU06A);  // after the send
                QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-106"));
                QCOMPARE(r.w->currentPatch().value(), ju("106:1-1"));
            } else {
                QCOMPARE(r.w->currentDevice(), Model::D05);
                QCOMPARE(Theme::skin().id, QStringLiteral("d05"));
                r.w->switchDevice(Model::JU06A);
                QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-106"));
                QCOMPARE(r.w->currentPatch().value(), ju("106:1-1"));
                QCOMPARE(r.w->ledDisplay()->readout(), S("1-1"));
                QVERIFY(!r.context().contains(S("FLIP")));
            }
        }
    }

    void aUserSwitchClearsThePromptAFollowSwitchKeepsTheArm()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        r.w->switchDevice(Model::JU06A);
        r.choose(ju("106:1-1"));
        QCOMPARE(r.context(), S("FLIP SWITCH TO 106"));
        // A follow-induced switch away keeps the arm.
        r.inject({0xC4, 10});
        QCOMPARE(r.w->currentDevice(), Model::JP08);
        QCOMPARE(r.tracker()->armed().value(), ju("106:1-1"));
        QVERIFY(r.w->engineControl()->armedLens(106));
        // The user switching clears it, and the prompt is gone on return.
        r.w->switchDevice(Model::D05);
        QVERIFY(!r.tracker()->armed());
        r.w->switchDevice(Model::JU06A);
        QVERIFY(!r.tracker()->armed());
        QVERIFY(!r.w->model()->armed());
        QVERIFY(!r.context().contains(S("FLIP SWITCH")));
        QVERIFY(!r.w->engineControl()->armedLens(106));
    }

    // T8.4 reviewer: switching to the device that is already current is no switch.
    void switchingToTheCurrentDeviceLeavesAnArmIntact()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        r.w->switchDevice(Model::JU06A);
        r.choose(ju("106:1-1"));
        QCOMPARE(r.tracker()->armed().value(), ju("106:1-1"));
        r.router->setCurrentDevice(Model::JU06A);
        QCOMPARE(r.tracker()->armed().value(), ju("106:1-1"));
        r.w->switchDevice(Model::JU06A);
        QCOMPARE(r.tracker()->armed().value(), ju("106:1-1"));
        QCOMPARE(r.context(), S("FLIP SWITCH TO 106"));
        r.w->switchDevice(Model::D05);  // a real switch ends it
        QVERIFY(!r.tracker()->armed());
    }

    // T8.4 reviewer: a user switch between two other devices keeps the hint (only an arm ends).
    void aUserSwitchBetweenTwoOtherDevicesKeepsTheHint()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        r.w->applyGlobal(false, false);
        QVERIFY(r.show());
        r.inject({0xC3, 48});
        QCOMPARE(r.tracker()->hint(), 106);
        r.w->switchDevice(Model::JP08);
        QCOMPARE(r.tracker()->hint(), 106);
        r.w->switchDevice(Model::D05);
        QCOMPARE(r.tracker()->hint(), 106);
        r.w->switchDevice(Model::JU06A);
        QCOMPARE(r.tracker()->hint(), 106);
        QVERIFY(r.context().startsWith(S("FLIPPED")));
    }

    // An event of a device that is not shown applies its state and nothing else to the shown one.
    void aNonCurrentDevicesEventLeavesTheShownDevicesEditorAndDisplayAlone()
    {
        for (bool followSwitches : {false, true}) {
            Theme::setSkin(QStringLiteral("d05"));
            Rig r;
            r.router->setJu06aState(60, -1, -1);
            r.w->applyGlobal(false, followSwitches);
            QVERIFY(r.show());
            QCOMPARE(r.w->currentDevice(), Model::D05);
            QVERIFY(!r.w->display()->activityActive());
            const auto editor = [&r]() -> QLineEdit * {
                for (QLineEdit *e : r.w->list()->viewport()->findChildren<QLineEdit *>(QStringLiteral("nameEditor")))
                    if (!e->isHidden())
                        return e;
                return nullptr;
            };
            r.w->list()->setCurrentIndex(r.w->model()->indexOf(dd("U2-34")));
            r.w->list()->setFocus();
            QTest::keyClick(r.w.get(), Qt::Key_F2);
            QVERIFY(editor());
            editor()->setText(S("Pending"));
            r.inject({0xC4, 10});  // JP-08 2-3
            QCOMPARE(r.router->link(Model::JP08)->current().value(), jp("2-3"));
            if (!followSwitches) {
                QCOMPARE(r.w->currentDevice(), Model::D05);
                QVERIFY(editor());
                QCOMPARE(editor()->text(), S("Pending"));
                QVERIFY(!r.w->display()->activityActive());
                QVERIFY(r.catalog.userName(dd("U2-34")).isEmpty());
                // The JU-06A's program is the same.
                r.inject({0xC3, 28});
                QVERIFY(editor());
                QVERIFY(!r.w->display()->activityActive());
            } else {
                QCOMPARE(r.w->currentDevice(), Model::JP08);
                QVERIFY(!editor());
                QCOMPARE(r.catalog.userName(dd("U2-34")), S("Pending"));  // finishing the edit is right here
                QVERIFY(r.w->display()->activityActive());
            }
        }
    }

    void echoSuppressionIsPerLinkOnASharedPort()
    {
        Rig r;
        QVERIFY(r.show());
        QCOMPARE(r.w->currentDevice(), Model::D05);
        r.clearSent();
        // The Hapax echoes what we send: a send to the JP-08 comes back as the same bytes.
        QVERIFY(r.router->select(jp("2-3")));
        QCOMPARE(r.sent(), (QList<QByteArray>{QByteArray::fromHex("c40a")}));
        r.inject({0xC4, 10});
        QCOMPARE(r.w->currentDevice(), Model::D05);  // not a JP-08 follow
        // A send to the D-05 and a JP-08 panel change straight after it: the panel change is not
        // an echo (the JP-08 link has its own window), and the D-05's echo is still dropped.
        r.clearSent();
        QVERIFY(r.router->select(dd("U1-12")));
        QCOMPARE(r.sent().size(), 3);
        r.inject({0xC4, 11});  // JP-08 panel change
        QCOMPARE(r.w->currentDevice(), Model::JP08);
        QCOMPARE(r.w->currentPatch().value(), jp("2-4"));
        r.router->setCurrentDevice(Model::JU06A);
        r.inject({0xB2, 0x00, 0x57, 0xB2, 0x20, 0x00, 0xC2, 0x01});  // the echo of the D-05 send
        QCOMPARE(r.w->currentDevice(), Model::JU06A);
    }

    void aD05TripletInterleavedWithAJp08PcStillCompletes()
    {
        Rig r;
        QVERIFY(r.show());
        r.router->setCurrentDevice(Model::JU06A);  // neither D-05 nor JP-08 is current
        r.clearSent();
        r.inject({0xB2, 0x00, 0x57});
        r.inject({0xC4, 10});  // the JP-08 PC lands between the D-05's CCs
        QCOMPARE(r.w->currentDevice(), Model::JP08);
        r.inject({0xB2, 0x20, 0x00});
        r.inject({0xC2, 0x0A});
        QCOMPARE(r.w->currentDevice(), Model::D05);
        QCOMPARE(r.w->currentPatch().value(), dd("U1-23"));
        QCOMPARE(r.router->link(Model::JP08)->current().value(), jp("2-3"));
        QVERIFY(r.sent().isEmpty());
    }
};

QTEST_MAIN(TestFollowDevices)
#include "test_follow_devices.moc"
