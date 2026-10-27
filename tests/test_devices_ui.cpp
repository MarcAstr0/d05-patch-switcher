// T8.2: the device switcher, switching the window between devices, and the Settings dialog's device
// list and pages, on fake backends, headless, with temporary config directories.
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
#include "ui/DeviceListWidget.h"
#include "ui/DevicePage.h"
#include "ui/DeviceSwitcher.h"
#include "ui/DisplayWidget.h"
#include "ui/GroupBar.h"
#include "ui/LedDisplayWidget.h"
#include "ui/MainWindow.h"
#include "ui/PanelButton.h"
#include "ui/PatchListView.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace d05;

namespace {

QString S(const char *t) { return QString::fromLatin1(t); }

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

// A window on a router over a hub of fake backends: one forwarder port for the three devices (the
// owner's Hapax), D-05 on channel 1, JP-08 on 3, JU-06A on 5 unless told otherwise.
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

    explicit Rig(bool d05 = true, bool jp08 = true, bool ju06a = true, bool follow = false)
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
        put(Model::D05, dev(1, "Hapax", follow, d05));
        put(Model::JP08, dev(3, "Hapax", follow, jp08));
        put(Model::JU06A, dev(5, "Hapax", follow, ju06a));
    }
    // A device as a session has it: in effect and saved (the dialog starts from the saved values).
    void put(Model m, const AppSettings::DeviceValues &v)
    {
        w->applyDevice(m, v);
        settings->setDevice(m, v);
    }
    // The OS port lists, for the backends that exist and the ones the hub makes later.
    void setPorts(const QStringList &o, const QStringList &i)
    {
        outs = o;
        ins = i;
        for (FakeMidiBackend *f : fakes) {
            f->setOutputPorts(outs, false);
            f->setInputPorts(ins, false);
        }
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
    DeviceSwitcher *sw() const { return w->switcher(); }
    void clickTab(Model m)
    {
        QTest::mouseClick(sw(), Qt::LeftButton, Qt::NoModifier, sw()->tabRect(m).center());
    }
    FakeMidiBackend *fake() { return fakes.first(); }
    void inject(std::initializer_list<int> v)
    {
        QByteArray b;
        for (int x : v)
            b.append(char(x));
        fake()->injectReceived(b);
        QCoreApplication::processEvents();
    }
};

}  // namespace

class TestDevicesUi : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Theme::install(*qobject_cast<QApplication *>(qApp)); }
    void init() { Theme::setSkin(QStringLiteral("d05")); }
    void cleanup() { Theme::setSkin(QStringLiteral("d05")); }

    // ------------------------------------------------------------------ the switcher

    void noDeviceIsTheWordmarkAlone()
    {
        Rig r(false, false, false);
        QCOMPARE(r.sw()->enabledCount(), 0);
        QVERIFY(!r.sw()->hasTabs());
        QVERIFY(r.sw()->testAttribute(Qt::WA_TransparentForMouseEvents));
        QCOMPARE(r.w->currentDevice(), Model::D05);
        QCOMPARE(Theme::skin().id, QStringLiteral("d05"));
        // First run: line 2 of the LCD says where Settings is (version 1's hint).
        QVERIFY(r.w->display()->line2().startsWith(QStringLiteral("OPEN SETTINGS")));
    }

    void oneDeviceIsTheNameplate()
    {
        Rig r(true, false, false);
        QCOMPARE(r.sw()->enabledCount(), 1);
        QVERIFY(!r.sw()->hasTabs());
        QCOMPARE(r.w->nameplate()->geometry(), QRect(16, 10, 360, 28));
    }

    void oneDeviceThatIsNotTheD05ShowsItsSkin()
    {
        Rig r(false, true, false);
        QCOMPARE(r.w->currentDevice(), Model::JP08);  // the only enabled device
        QCOMPARE(Theme::skin().id, QStringLiteral("jp08"));
        QVERIFY(r.w->ledDisplay()->isVisible() || r.w->ledDisplay()->isVisibleTo(r.w.get()));
        QVERIFY(!r.w->display()->isVisibleTo(r.w.get()));
    }

    void tabsFollowTheEnabledDevicesInFixedOrder()
    {
        Rig r(true, false, true);
        QVERIFY(r.sw()->hasTabs());
        QCOMPARE(r.sw()->enabledCount(), 2);
        QVERIFY(!r.sw()->tabRect(Model::D05).isEmpty());
        QVERIFY(r.sw()->tabRect(Model::JP08).isEmpty());
        QVERIFY(r.sw()->tabRect(Model::JU06A).left() > r.sw()->tabRect(Model::D05).right());
        QCOMPARE(r.w->nameplate()->width(), 960 - 32);  // the tabs need the row
    }

    void tabsMarkTheConnectionAndTheCurrentDevice()
    {
        Rig r;
        r.w->applyDevice(Model::JU06A, dev(5, "Nowhere"));  // wanted, not there
        QVERIFY(r.sw()->hasTabs());
        QCOMPARE(r.sw()->current(), Model::D05);
        QVERIFY(r.sw()->tabLit(Model::D05));
        QVERIFY(!r.sw()->tabLit(Model::JP08));
        QCOMPARE(r.sw()->nameTone(Model::D05), DeviceSwitcher::NameTone::Primary);
        QCOMPARE(r.sw()->nameTone(Model::JP08), DeviceSwitcher::NameTone::Secondary);
        QCOMPARE(r.sw()->nameTone(Model::JU06A), DeviceSwitcher::NameTone::Red);
        QCOMPARE(r.sw()->tabToolTip(Model::D05), QStringLiteral("D-05: Hapax Out, channel 1 (Ctrl+Tab)"));
        QCOMPARE(r.sw()->tabToolTip(Model::JP08), QStringLiteral("JP-08: Hapax Out, channel 3 (Ctrl+Tab)"));
        QCOMPARE(r.sw()->tabToolTip(Model::JU06A), QStringLiteral("JU-06A: Nowhere, channel 5 (Ctrl+Tab)"));
        // A device with no output port at all is red too, and its tooltip says so.
        r.w->applyDevice(Model::JU06A, dev(5, ""));
        QCOMPARE(r.sw()->nameTone(Model::JU06A), DeviceSwitcher::NameTone::Red);
        QCOMPARE(r.sw()->tabToolTip(Model::JU06A), QStringLiteral("JU-06A: no MIDI output (Ctrl+Tab)"));
        // The port comes back: the poll connects it and the red goes.
        r.w->applyDevice(Model::JU06A, dev(5, "Hapax"));
        QCOMPARE(r.sw()->nameTone(Model::JU06A), DeviceSwitcher::NameTone::Secondary);
    }

    void hoverLightsANonCurrentTabAndRepaintsOnlyOnAChange()
    {
        Rig r;
        QVERIFY(r.show());
        QTest::qWait(30);
        PaintCounter c;
        r.sw()->installEventFilter(&c);
        const QPoint jp = r.sw()->tabRect(Model::JP08).center();
        QTest::mouseMove(r.sw(), jp);
        QTest::qWait(30);
        QVERIFY(r.sw()->hovered(Model::JP08));
        QCOMPARE(r.sw()->nameTone(Model::JP08), DeviceSwitcher::NameTone::Primary);
        const int afterEnter = c.paints;
        QVERIFY(afterEnter >= 1);
        QTest::mouseMove(r.sw(), jp + QPoint(2, 0));  // still on the tab: nothing changes
        QTest::qWait(30);
        QCOMPARE(c.paints, afterEnter);
        // The current tab never hovers.
        QTest::mouseMove(r.sw(), r.sw()->tabRect(Model::D05).center());
        QTest::qWait(30);
        QVERIFY(!r.sw()->hovered(Model::D05));
        r.sw()->removeEventFilter(&c);
    }

    // ------------------------------------------------------------------ switching

    void aClickSwitchesSkinAndSwitcherAndSendsNothing()
    {
        Rig r;
        QVERIFY(r.show());
        r.fake()->clearSent();
        QSignalSpy spy(r.router.get(), &DeviceRouter::currentDeviceChanged);
        r.clickTab(Model::JP08);
        QCOMPARE(r.w->currentDevice(), Model::JP08);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(1).toBool(), false);  // a user switch
        QCOMPARE(Theme::skin().id, QStringLiteral("jp08"));
        QVERIFY(r.w->ledDisplay()->isVisible());
        QVERIFY(!r.w->display()->isVisible());
        QCOMPARE(r.sw()->current(), Model::JP08);
        QVERIFY(r.sw()->tabLit(Model::JP08));
        QVERIFY(r.fake()->sentMessages().isEmpty());
        QCOMPARE(AppSettings(r.iniPath).currentDevice().value(), Model::JP08);  // remembered
        if (const QString dir = qEnvironmentVariable("D05_T82_SHOT_DIR"); !dir.isEmpty()) {
            QTest::qWait(50);
            r.w->grab().save(dir + QStringLiteral("/switched-jp08.png"));
        }
        r.clickTab(Model::JU06A);
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-60"));
        r.clickTab(Model::D05);
        QCOMPARE(Theme::skin().id, QStringLiteral("d05"));
        QVERIFY(r.w->display()->isVisible());
        QVERIFY(r.fake()->sentMessages().isEmpty());
    }

    void theCurrentTabIgnoresClicks()
    {
        Rig r;
        QVERIFY(r.show());
        QSignalSpy spy(r.router.get(), &DeviceRouter::currentDeviceChanged);
        r.clickTab(Model::D05);
        QCOMPARE(spy.count(), 0);
        // A click that starts on one tab and ends on another switches to neither.
        QTest::mousePress(r.sw(), Qt::LeftButton, Qt::NoModifier, r.sw()->tabRect(Model::JP08).center());
        QTest::mouseRelease(r.sw(), Qt::LeftButton, Qt::NoModifier, r.sw()->tabRect(Model::JU06A).center());
        QCOMPARE(spy.count(), 0);
    }

    void aSwitchHidesTheOldDevicesContentAndShowsItBack()
    {
        Rig r;
        QVERIFY(r.show());
        QVERIFY(r.w->list()->isVisible());
        QVERIFY(r.w->bankGrid()->isVisible());
        QVERIFY(r.w->searchField()->isVisible());
        r.clickTab(Model::JP08);
        // Nothing of the D-05's catalogue is on screen under the JP-08 skin (T8.3 fills it per model).
        for (QWidget *w : std::initializer_list<QWidget *>{r.w->list(), r.w->bankGrid(), r.w->searchField(), r.w->groupBar(),
                                                           r.w->prevButton(), r.w->nextButton()})
            QVERIFY2(!w->isVisible(), w->metaObject()->className());
        QVERIFY(r.w->autoSendButton()->isVisible());
        QVERIFY(r.w->systemButton()->isVisible());
        // The LED display says what it knows: dashes and the idle line.
        QCOMPARE(r.w->ledDisplay()->readout(), QStringLiteral("--"));
        QCOMPARE(r.w->ledDisplay()->context(), Theme::skin().idleContext);
        // The keys that would act on the hidden list do nothing.
        r.fake()->clearSent();
        QTest::keyClick(r.w.get(), Qt::Key_PageDown);
        QTest::keyClick(r.w.get(), Qt::Key_Return);
        QVERIFY(r.fake()->sentMessages().isEmpty());
        r.clickTab(Model::D05);
        QVERIFY(r.w->list()->isVisible());
        QVERIFY(r.w->bankGrid()->isVisible());
        QVERIFY(r.w->searchField()->isVisible());
        QVERIFY(r.w->groupBar()->isVisible());
    }

    void theLedDisplayShowsTheDevicesConnectionLine()
    {
        Rig r;
        r.w->applyDevice(Model::JU06A, dev(5, "Nowhere"));
        QVERIFY(r.show());
        r.clickTab(Model::JU06A);
        QCOMPARE(r.w->ledDisplay()->context(), QStringLiteral("NO MIDI OUTPUT"));
        QCOMPARE(r.w->ledDisplay()->tone(), LedDisplayWidget::Tone::Red);
        r.w->applyDevice(Model::JU06A, dev(5, ""));
        QVERIFY(r.w->ledDisplay()->context().startsWith(QStringLiteral("OPEN SETTINGS")));
        r.w->applyDevice(Model::JU06A, dev(5, "Hapax"));
        QCOMPARE(r.w->ledDisplay()->context(), Theme::skin().idleContext);
    }

    void theStatusBarFollowsTheCurrentDevice()
    {
        Rig r;
        QVERIFY(r.show());
        r.clickTab(Model::JP08);
        // The status strip shows the JP-08's channel 3 (the strip's labels have no object names).
        bool found = false;
        for (const QLabel *l : r.w->statusBar()->findChildren<QLabel *>())
            found = found || (l->isVisible() && l->text() == QStringLiteral("3"));
        QVERIFY(found);
    }

    void aDeviceSwitchPaintsTheWindowOnce()
    {
        Rig r;
        QVERIFY(r.show());
        QTest::qWait(80);
        const auto switchTo = [&](Model to, bool led) {
            PaintCounter win, plate, leds, lcd, status;
            r.w->installEventFilter(&win);
            r.sw()->installEventFilter(&plate);
            r.w->ledDisplay()->installEventFilter(&leds);
            r.w->display()->installEventFilter(&lcd);
            r.w->statusBar()->installEventFilter(&status);
            r.clickTab(to);
            QTest::qWait(100);
            r.w->removeEventFilter(&win);
            r.sw()->removeEventFilter(&plate);
            r.w->ledDisplay()->removeEventFilter(&leds);
            r.w->display()->removeEventFilter(&lcd);
            r.w->statusBar()->removeEventFilter(&status);
            QCOMPARE(win.paints, 1);
            QCOMPARE(plate.paints, 1);
            QCOMPARE(led ? leds.paints : lcd.paints, 1);
            QCOMPARE(led ? lcd.paints : leds.paints, 0);  // the old display is not painted on the way out
            QCOMPARE(status.paints, 1);
        };
        switchTo(Model::JP08, true);
        switchTo(Model::JU06A, true);
        switchTo(Model::D05, false);
    }

    void aSwitchAddsNoTimer()
    {
        Rig r;
        QVERIFY(r.show());
        const bool polling = r.router->isPolling();
        r.clickTab(Model::JP08);
        r.clickTab(Model::D05);
        QCOMPARE(r.router->isPolling(), polling);  // every enabled device is connected: no poll
        QVERIFY(!r.router->isPolling());
    }

    void controlTabCyclesTheEnabledDevices()
    {
        Rig r(true, true, true);
        QVERIFY(r.show());
        QTest::keyClick(r.w.get(), Qt::Key_Tab, Qt::ControlModifier);
        QCOMPARE(r.w->currentDevice(), Model::JP08);
        QTest::keyClick(r.w.get(), Qt::Key_Tab, Qt::ControlModifier);
        QCOMPARE(r.w->currentDevice(), Model::JU06A);
        QTest::keyClick(r.w.get(), Qt::Key_Tab, Qt::ControlModifier);
        QCOMPARE(r.w->currentDevice(), Model::D05);
        QTest::keyClick(r.w.get(), Qt::Key_Tab, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(r.w->currentDevice(), Model::JU06A);
        QTest::keyClick(r.w.get(), Qt::Key_Backtab, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(r.w->currentDevice(), Model::JP08);
    }

    void cyclingSkipsDisabledDevicesAndIsNothingForOne()
    {
        Rig r(true, false, true);
        r.w->cycleDevice(1);
        QCOMPARE(r.w->currentDevice(), Model::JU06A);
        r.w->cycleDevice(1);
        QCOMPARE(r.w->currentDevice(), Model::D05);
        r.w->cycleDevice(-1);
        QCOMPARE(r.w->currentDevice(), Model::JU06A);
        Rig one(false, true, false);
        QSignalSpy spy(one.router.get(), &DeviceRouter::currentDeviceChanged);
        one.w->cycleDevice(1);
        one.w->cycleDevice(-1);
        QCOMPARE(spy.count(), 0);
        // A disabled device cannot be switched to.
        one.w->switchDevice(Model::D05);
        QCOMPARE(one.w->currentDevice(), Model::JP08);
    }

    void aUserSwitchClearsAnArmAFollowSwitchDoesNot()
    {
        Rig r(true, true, true, true);  // follow on everywhere: the inputs are open
        QVERIFY(r.router->allInputsOpen());
        r.router->setJu06aState(106, -1, -1);
        r.w->switchDevice(Model::JU06A);
        // Arm: select an address of the other engine (nothing is sent).
        QVERIFY(r.router->select(*PatchAddress::parse(S("60:4-5"), Model::JU06A)));
        QVERIFY(r.router->ju06aTracker()->armed().has_value());
        // A follow event from the JP-08 (channel 3) switches the window to it and clears nothing.
        r.inject({0xC2, 10});
        QCOMPARE(r.w->currentDevice(), Model::JP08);
        QCOMPARE(Theme::skin().id, QStringLiteral("jp08"));
        QCOMPARE(r.sw()->current(), Model::JP08);
        QVERIFY(r.router->ju06aTracker()->armed().has_value());
        // A switch the user makes to the JU-06A (or away from it) clears the arm.
        r.w->switchDevice(Model::JU06A);
        QVERIFY(!r.router->ju06aTracker()->armed().has_value());
    }

    void theJu06aSkinFollowsItsBelievedEngine()
    {
        Rig r;
        r.router->setJu06aState(106, -1, -1);
        r.w->switchDevice(Model::JU06A);
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-106"));
        r.router->ju06aTracker()->toggleEngine();
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-60"));
        r.w->switchDevice(Model::D05);
        r.router->ju06aTracker()->toggleEngine();  // not the current device: no skin change
        QCOMPARE(Theme::skin().id, QStringLiteral("d05"));
    }

    void turningTheCurrentDeviceOffMovesTheWindowToAnother()
    {
        Rig r(true, true, false);
        r.w->applyDevice(Model::D05, dev(1, "Hapax", false, false));  // the current device off
        QCOMPARE(r.w->currentDevice(), Model::JP08);
        QCOMPARE(Theme::skin().id, QStringLiteral("jp08"));
        r.w->applyDevice(Model::JP08, dev(3, "Hapax", false, false));  // none left: the D-05 skin
        QCOMPARE(r.w->currentDevice(), Model::D05);
        QCOMPARE(Theme::skin().id, QStringLiteral("d05"));
        QCOMPARE(r.sw()->enabledCount(), 0);
    }

    // ------------------------------------------------------------------ Settings

    void theDialogListsThreeDevicesAndShowsTheCurrentDevicesPage()
    {
        Rig r(true, false, true);
        r.w->switchDevice(Model::JU06A);
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        QVERIFY(d);
        QCOMPARE(d->size(), QSize(760, 610));
        QCOMPARE(d->shown(), Model::JU06A);
        QCOMPARE(d->deviceList()->current(), Model::JU06A);
        QVERIFY(d->deviceList()->row(Model::D05).enabled);
        QVERIFY(!d->deviceList()->row(Model::JP08).enabled);
        QVERIFY(d->deviceList()->row(Model::JU06A).enabled);
        QCOMPARE(d->deviceList()->row(Model::JP08).text, QStringLiteral("Off"));
        QCOMPARE(d->deviceList()->row(Model::D05).text, QStringLiteral("Connected"));
        QVERIFY(d->page(Model::JU06A)->isVisibleTo(d));
        QVERIFY(!d->page(Model::D05)->isVisibleTo(d));
        // The dialog does not resize between pages (DESIGN-2.md 7.1).
        d->setShown(Model::JP08);
        QCOMPARE(d->size(), QSize(760, 610));
        QVERIFY(d->page(Model::JP08)->isVisibleTo(d));
        QVERIFY(!d->page(Model::JU06A)->isVisibleTo(d));
        d->reject();
    }

    void theListSelectsAndTogglesByMouseAndKeyboard()
    {
        Rig r;
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        DeviceListWidget *list = d->deviceList();
        QVERIFY(QTest::qWaitForWindowExposed(d));
        // A click right of the checkbox selects, a click on it toggles.
        QTest::mouseClick(list, Qt::LeftButton, Qt::NoModifier, DeviceListWidget::rowRect(Model::JP08).center() + QPoint(30, 0));
        QCOMPARE(d->shown(), Model::JP08);
        QVERIFY(list->row(Model::JP08).enabled);
        QTest::mouseClick(list, Qt::LeftButton, Qt::NoModifier, DeviceListWidget::toggleRect(Model::JP08).center());
        QVERIFY(!list->row(Model::JP08).enabled);
        QCOMPARE(list->row(Model::JP08).text, QStringLiteral("Off"));
        QTest::mouseClick(list, Qt::LeftButton, Qt::NoModifier, DeviceListWidget::toggleRect(Model::JP08).center());
        QVERIFY(list->row(Model::JP08).enabled);
        // The keyboard: Up / Down move, Space toggles.
        list->setFocus();
        QTest::keyClick(list, Qt::Key_Down);
        QCOMPARE(d->shown(), Model::JU06A);
        QTest::keyClick(list, Qt::Key_Down);  // the last row stays
        QCOMPARE(d->shown(), Model::JU06A);
        QTest::keyClick(list, Qt::Key_Space);
        QVERIFY(!list->row(Model::JU06A).enabled);
        QTest::keyClick(list, Qt::Key_Up);
        QCOMPARE(d->shown(), Model::JP08);
        d->reject();
        // Cancel changed nothing.
        QVERIFY(r.router->isEnabled(Model::JU06A));
    }

    void oneEditAppliesNothingUntilOk()
    {
        Rig r;
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        d->setShown(Model::JP08);
        d->channelSpin()->setValue(9);
        d->deviceList()->setRow(Model::JU06A, DeviceListWidget::Row());  // (display only)
        QCOMPARE(r.router->config(Model::JP08).channel, 3);
        d->reject();
        QCOMPARE(r.router->config(Model::JP08).channel, 3);
        QCOMPARE(AppSettings(r.iniPath).device(Model::JP08).channel, 3);
    }

    void okAppliesAndPersistsEveryDevice()
    {
        Rig r(true, false, false);
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        // The JP-08 on: page, channel, resend; the JU-06A on with follow.
        QTest::mouseClick(d->deviceList(), Qt::LeftButton, Qt::NoModifier,
                          DeviceListWidget::toggleRect(Model::JP08).center());
        QVERIFY(d->deviceList()->row(Model::JP08).enabled);
        d->setShown(Model::JP08);
        d->channelSpin()->setValue(5);
        d->resendCheck()->setChecked(true);
        d->setShown(Model::JU06A);
        QTest::mouseClick(d->deviceList(), Qt::LeftButton, Qt::NoModifier,
                          DeviceListWidget::toggleRect(Model::JU06A).center());
        d->channelSpin()->setValue(4);
        d->inputCombo()->setCurrentIndex(1);
        d->followCheck()->setChecked(true);
        d->autoSendCheck()->setChecked(true);
        d->followSwitchesCheck()->setChecked(false);
        QVERIFY(d->okButton()->isEnabled());
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());

        QVERIFY(r.router->isEnabled(Model::JP08));
        QVERIFY(r.router->isEnabled(Model::JU06A));
        QCOMPARE(r.router->config(Model::JP08).channel, 5);
        QVERIFY(r.router->config(Model::JP08).resendOnConnect);
        QCOMPARE(r.router->config(Model::JU06A).channel, 4);
        QVERIFY(r.router->config(Model::JU06A).follow);
        QVERIFY(r.router->link(Model::JU06A)->inputConnected());
        QVERIFY(r.w->autoSend());
        QVERIFY(!r.router->followSwitchesDevice());
        QCOMPARE(r.sw()->enabledCount(), 3);

        const AppSettings saved(r.iniPath);
        QVERIFY(saved.device(Model::D05).enabled);
        QVERIFY(saved.device(Model::JP08).enabled);
        QCOMPARE(saved.device(Model::JP08).channel, 5);
        QVERIFY(saved.device(Model::JP08).resendOnConnect);
        QVERIFY(saved.device(Model::JU06A).enabled);
        QVERIFY(saved.device(Model::JU06A).follow);
        QCOMPARE(saved.device(Model::JU06A).inputPort, QStringLiteral("Hapax In"));
        QVERIFY(saved.autoSend());
        QVERIFY(!saved.followSwitchesDevice());
    }

    void turningTheCurrentDeviceOffInSettingsMovesOn()
    {
        Rig r(true, true, false);
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        QTest::mouseClick(d->deviceList(), Qt::LeftButton, Qt::NoModifier,
                          DeviceListWidget::toggleRect(Model::D05).center());
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());
        QVERIFY(!r.router->isEnabled(Model::D05));
        QVERIFY(!r.router->link(Model::D05)->isConnected());  // a disabled device has its ports closed
        QCOMPARE(r.w->currentDevice(), Model::JP08);
        QCOMPARE(Theme::skin().id, QStringLiteral("jp08"));
        QVERIFY(!AppSettings(r.iniPath).device(Model::D05).enabled);
    }

    void cancelRevertsATestOnEveryDevice()
    {
        Rig r(true, true, false);
        r.setPorts({S("Hapax Out"), S("Other Out")}, r.ins);
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        d->setShown(Model::JP08);
        d->channelSpin()->setValue(7);
        d->outputCombo()->setCurrentIndex(d->outputCombo()->findData(S("Other Out")));
        QVERIFY(d->testButton()->isEnabled());
        r.fake()->clearSent();
        QTest::mouseClick(d->testButton(), Qt::LeftButton);
        // TEST sends the device's patch (the first of its catalogue when it has none) on the pending channel.
        QCOMPARE(r.router->config(Model::JP08).channel, 7);
        QCOMPARE(r.router->link(Model::JP08)->connectedPortKey(), S("Other Out"));
        QVERIFY(d->testPressed(Model::JP08));
        QVERIFY(!d->testPressed(Model::D05));
        d->reject();
        QTRY_VERIFY(!r.w->settingsDialog());
        QCOMPARE(r.router->config(Model::JP08).channel, 3);
        QCOMPARE(r.router->link(Model::JP08)->connectedPortKey(), S("Hapax Out"));
        QVERIFY(r.router->isEnabled(Model::D05));
        QCOMPARE(r.router->config(Model::D05).channel, 1);
    }

    void testIsOffForADisabledDeviceAndForAConflict()
    {
        Rig r(true, false, false);
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        d->setShown(Model::JP08);
        QVERIFY(!d->testButton()->isEnabled());  // not enabled
        QTest::mouseClick(d->deviceList(), Qt::LeftButton, Qt::NoModifier,
                          DeviceListWidget::toggleRect(Model::JP08).center());
        d->channelSpin()->setValue(2);
        QVERIFY(d->testButton()->isEnabled());
        d->channelSpin()->setValue(1);  // the D-05's channel on the same port
        QVERIFY(!d->testButton()->isEnabled());
        d->reject();
    }

    void aSharedPortAndChannelIsNamedInPlaceAndOkIsOff()
    {
        Rig r(true, false, false);
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        QVERIFY(QTest::qWaitForWindowExposed(d));
        d->setShown(Model::JP08);
        QTest::mouseClick(d->deviceList(), Qt::LeftButton, Qt::NoModifier,
                          DeviceListWidget::toggleRect(Model::JP08).center());
        d->channelSpin()->setValue(1);  // the D-05's channel, on the same port
        QVERIFY(d->hasConflict());
        QVERIFY(!d->okButton()->isEnabled());
        QCOMPARE(d->page(Model::JP08)->conflictText(), QStringLiteral("Channel 1 on Hapax Out is used by the D-05."));
        QCOMPARE(d->page(Model::JP08)->conflictLabel()->text(), d->page(Model::JP08)->conflictText());
        QVERIFY(d->page(Model::D05)->conflictText().isEmpty());  // only the later device says it
        QCOMPARE(d->page(Model::JP08)->conflictLabel()->objectName(), QStringLiteral("channelConflictText"));
        // OK does nothing while the conflict stands.
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QVERIFY(r.w->settingsDialog());
        // Another channel and the text is gone, OK is back.
        d->channelSpin()->setValue(2);
        QVERIFY(!d->hasConflict());
        QVERIFY(d->page(Model::JP08)->conflictText().isEmpty());
        QVERIFY(d->okButton()->isEnabled());
        // The conflict needs both enabled: turning the D-05 off clears it too.
        d->channelSpin()->setValue(1);
        QVERIFY(d->hasConflict());
        QTest::mouseClick(d->deviceList(), Qt::LeftButton, Qt::NoModifier,
                          DeviceListWidget::toggleRect(Model::D05).center());
        QVERIFY(!d->hasConflict());
        QVERIFY(d->okButton()->isEnabled());
        d->reject();
    }

    void aSharedInputAndChannelIsRefusedOnlyWhenBothFollow()
    {
        Rig r(true, true, false);
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        d->setShown(Model::JP08);
        d->channelSpin()->setValue(1);  // the D-05's channel on the same output port
        QVERIFY(d->hasConflict());
        d->channelSpin()->setValue(2);
        QVERIFY(!d->hasConflict());
        // The same input and channel with follow on in both.
        d->inputCombo()->setCurrentIndex(1);
        d->followCheck()->setChecked(true);
        d->channelSpin()->setValue(1);
        d->setShown(Model::D05);
        d->inputCombo()->setCurrentIndex(1);
        d->followCheck()->setChecked(true);
        // The D-05 and the JP-08 now share the output port and the channel (1), and the input too.
        QVERIFY(d->hasConflict());
        d->setShown(Model::JP08);
        d->channelSpin()->setValue(2);
        d->followCheck()->setChecked(true);
        QVERIFY(!d->hasConflict());
        d->setShown(Model::D05);
        d->channelSpin()->setValue(2);  // now both on channel 2 with the same output
        QVERIFY(d->hasConflict());
        QVERIFY(d->page(Model::JP08)->conflictText().contains(QStringLiteral("is used by the D-05")));
        d->reject();
    }

    void anInputConflictSaysInput()
    {
        // Two devices on different output ports but the same input and channel, both following.
        Rig r(false, false, false);
        r.setPorts({S("Out A"), S("Out B")}, {S("In X")});
        AppSettings::DeviceValues d05 = dev(4, "Out A", false, true);
        d05.inputPort = S("In X");
        d05.follow = true;
        r.put(Model::D05, d05);
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        d->setShown(Model::JP08);
        QTest::mouseClick(d->deviceList(), Qt::LeftButton, Qt::NoModifier,
                          DeviceListWidget::toggleRect(Model::JP08).center());
        d->outputCombo()->setCurrentIndex(d->outputCombo()->findData(S("Out B")));
        d->channelSpin()->setValue(4);
        d->inputCombo()->setCurrentIndex(d->inputCombo()->findData(S("In X")));
        d->followCheck()->setChecked(true);
        QVERIFY(d->hasConflict());
        QCOMPARE(d->page(Model::JP08)->conflictText(), QStringLiteral("Channel 4 on In X (input) is used by the D-05."));
        QVERIFY(!d->okButton()->isEnabled());
        d->followCheck()->setChecked(false);  // follow off: the inputs do not collide
        QVERIFY(!d->hasConflict());
        d->reject();
    }

    void swappingChannelsBetweenTwoDevicesIsNotRefused()
    {
        Rig r(true, true, false);  // D-05 channel 1, JP-08 channel 3, same port
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        d->channelSpin()->setValue(3);  // D-05 -> 3
        d->setShown(Model::JP08);
        d->channelSpin()->setValue(1);  // JP-08 -> 1
        QVERIFY(!d->hasConflict());
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());
        QCOMPARE(r.router->config(Model::D05).channel, 3);
        QCOMPARE(r.router->config(Model::JP08).channel, 1);
        QCOMPARE(AppSettings(r.iniPath).device(Model::JP08).channel, 1);
    }

    void detectedPortsAreMarkedAndUseSelectsTheOne()
    {
        Rig r(true, true, true);
        r.setPorts({S("Boutiq:Boutiq MIDI 1 28:0"), S("Boutique:Boutique MIDI 1 24:0"), S("JU-06A:JU-06A MIDI 1 32:0"),
                    S("HAPAX Puerto 1")},
                   {S("JU-06A:JU-06A MIDI 1 32:0"), S("HAPAX Puerto 1")});
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        const QString dot = QStringLiteral(" · detected");
        // The D-05 page marks the Boutique port, the JP-08 page the Boutiq one (a candidate), the JU-06A its own.
        QCOMPARE(d->page(Model::D05)->detectedOutputs(), QStringList{S("Boutique:Boutique MIDI 1 24:0")});
        QCOMPARE(d->page(Model::JP08)->detectedOutputs(), QStringList{S("Boutiq:Boutiq MIDI 1 28:0")});
        QCOMPARE(d->page(Model::JU06A)->detectedOutputs(), QStringList{S("JU-06A:JU-06A MIDI 1 32:0")});
        QCOMPARE(d->page(Model::JU06A)->detectedInputs(), QStringList{S("JU-06A:JU-06A MIDI 1 32:0")});
        QVERIFY(d->page(Model::D05)->detectedInputs().isEmpty());
        // The marked text, with the stored value the plain OS name.
        QComboBox *out = d->page(Model::JU06A)->outputCombo();
        const int i = out->findText(S("JU-06A:JU-06A MIDI 1") + dot);
        QVERIFY(i >= 0);
        QCOMPARE(out->itemData(i).toString(), S("JU-06A:JU-06A MIDI 1 32:0"));
        QCOMPARE(out->findText(S("HAPAX Puerto 1")) >= 0, true);  // unmarked
        // USE is on when exactly one port is detected and the combo does not hold it.
        QVERIFY(d->page(Model::D05)->useOutputButton()->isEnabled() == (d->page(Model::D05)->outputCombo()->currentData().toString()
                                                                       != S("Boutique:Boutique MIDI 1 24:0")));
        d->page(Model::JU06A)->outputCombo()->setCurrentIndex(out->findData(S("HAPAX Puerto 1")));
        QPushButton *use = d->page(Model::JU06A)->useOutputButton();
        QVERIFY(use->isEnabled());
        QCOMPARE(use->toolTip(), QStringLiteral("Use JU-06A:JU-06A MIDI 1"));
        QTest::mouseClick(use, Qt::LeftButton);
        QCOMPARE(out->currentData().toString(), S("JU-06A:JU-06A MIDI 1 32:0"));
        QVERIFY(!use->isEnabled());  // already used
        // The input USE too, and it saves the key.
        QComboBox *in = d->page(Model::JU06A)->inputCombo();
        in->setCurrentIndex(in->findData(S("HAPAX Puerto 1")));
        QVERIFY(d->page(Model::JU06A)->useInputButton()->isEnabled());
        QTest::mouseClick(d->page(Model::JU06A)->useInputButton(), Qt::LeftButton);
        QCOMPARE(d->deviceValues(Model::JU06A).inputPort, S("JU-06A:JU-06A MIDI 1"));
        d->reject();
    }

    void useIsOffWithTwoDetectedPortsOrNone()
    {
        Rig r;
        r.setPorts({S("Boutique:Boutique MIDI 1 24:0"), S("Boutique:Boutique MIDI 1 32:0"), S("Other 1")}, r.ins);
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        QCOMPARE(d->page(Model::D05)->detectedOutputs().size(), 2);
        QVERIFY(!d->page(Model::D05)->useOutputButton()->isEnabled());
        QVERIFY(d->page(Model::JU06A)->detectedOutputs().isEmpty());
        QVERIFY(!d->page(Model::JU06A)->useOutputButton()->isEnabled());
        d->reject();
    }

    void detectionRunsOnlyWhenThePortListIsRefreshed()
    {
        Rig r;
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        QVERIFY(d->page(Model::JU06A)->detectedOutputs().isEmpty());
        r.setPorts({S("Hapax Out"), S("JU-06A")}, r.ins);
        QTest::qWait(2300);  // longer than the poll: nothing re-lists by itself
        QVERIFY(d->page(Model::JU06A)->detectedOutputs().isEmpty());
        QTest::mouseClick(d->refreshButton(), Qt::LeftButton);
        QCOMPARE(d->page(Model::JU06A)->detectedOutputs(), QStringList{S("JU-06A")});
        d->reject();
    }

    void theJu06aPageShowsTheEngineStateReadOnly()
    {
        Rig r;
        r.router->setJu06aState(106, 28, 0);  // 60: 4-5, 106: 1-1
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        DevicePage *p = d->page(Model::JU06A);
        const auto labels = p->findChildren<QLabel *>();
        const auto textOf = [&](const char *role) {
            QStringList out;
            for (const QLabel *l : labels)
                if (l->property("role").toString() == QLatin1String(role))
                    out << l->text();
            return out;
        };
        QCOMPARE(textOf("engineValue"), (QStringList{S("106"), S("4-5"), S("1-1")}));
        QVERIFY(textOf("engineUnknown").isEmpty());
        QVERIFY(!d->page(Model::D05)->findChild<QLabel *>(QStringLiteral("x")));  // only the JU-06A has the rows
        d->reject();
        QTRY_VERIFY(!r.w->settingsDialog());
        r.router->setJu06aState(60, -1, 0);
        r.w->openSettings();
        d = r.w->settingsDialog();
        const auto labels2 = d->page(Model::JU06A)->findChildren<QLabel *>();
        QStringList unknown;
        for (const QLabel *l : labels2)
            if (l->property("role").toString() == QLatin1String("engineUnknown"))
                unknown << l->text();
        QCOMPARE(unknown, QStringList{S("unknown")});
        d->reject();
    }

    void theNotesAreTheModelsOwn()
    {
        Rig r;
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        const auto note = [&](Model m) {
            QStringList notes;
            for (const QLabel *l : d->page(m)->findChildren<QLabel *>())
                if (l->property("role").toString() == QLatin1String("note"))
                    notes << l->text();
            return notes.join(QLatin1Char('|'));
        };
        QVERIFY(note(Model::D05).startsWith(QStringLiteral("The D-05 changes patch only when SYSTEM > MIDI > Prog.C is ON.")));
        QVERIFY(note(Model::JP08).startsWith(QStringLiteral("Set the JP-08's MIDI channel with [MANUAL] + [2]")));
        QVERIFY(note(Model::JU06A).startsWith(QStringLiteral("Set the JU-06A's MIDI channel: hold ARPEGGIO")));
        d->reject();
    }

    void tabOrderRunsThroughTheListThePageAndTheGlobals()
    {
        Rig r;
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        QVERIFY(d->deviceList()->focusPolicy() != Qt::NoFocus);
        QWidget *w = d->deviceList();
        QStringList names;
        for (int i = 0; i < 12 && w; ++i) {
            w = w->nextInFocusChain();
            names << w->objectName();
        }
        QVERIFY2(names.contains(QStringLiteral("outputCombo")), qPrintable(names.join(QLatin1Char(','))));
        d->reject();
    }

    // ------------------------------------------------------------------ the screenshots

    void deviceStatesRender()
    {
        for (const char *state : {"devices", "settings-devices"}) {
            QTemporaryDir out;
            QVERIFY(out.isValid());
            CliOptions o;
            o.screenshotFile = out.filePath(QStringLiteral("shot.png"));
            o.screenshotState = S(state);
            o.configDir = out.path();
            QCOMPARE(runScreenshot(o), 0);
            QImage img;
            QVERIFY(img.load(o.screenshotFile));
            const qreal dpr = qApp->devicePixelRatio();
            if (S(state) == QLatin1String("devices")) {
                QCOMPARE(img.width(), int(960 * dpr));
                QCOMPARE(img.height(), int(560 * dpr));
            } else {
                QCOMPARE(img.width(), int(760 * dpr));
                QCOMPARE(img.height(), int(610 * dpr));
            }
            Paths::setOverride(QString());
        }
    }

    void everyPageRenders()
    {
        Rig r;
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        QVERIFY(QTest::qWaitForWindowExposed(d));
        const QString dir = qEnvironmentVariable("D05_T82_SHOT_DIR");
        for (Model m : allModels()) {
            d->setShown(m);
            QApplication::processEvents();
            const QImage img = d->grab().toImage();
            QVERIFY(!img.isNull());
            QCOMPARE(img.size(), QSize(int(760 * img.devicePixelRatio()), int(610 * img.devicePixelRatio())));
            if (!dir.isEmpty())
                img.save(dir + QLatin1Char('/') + S("page-") + modelInfo(m).id + S(".png"));
        }
        d->reject();
    }
};

QTEST_MAIN(TestDevicesUi)
#include "test_devices_ui.moc"
