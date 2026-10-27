// T3.4: the Settings dialog, auto-send, follow-the-panel, resend-on-connect and the
// disconnected display, on the fake MIDI backend, headless, with a temporary config dir.
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
#include "ui/DisplayWidget.h"
#include "ui/MainWindow.h"
#include "ui/PanelButton.h"
#include "ui/PatchListView.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace d05;

namespace {

PatchAddress addr(const char *t)
{
    return *PatchAddress::parse(QString::fromLatin1(t));
}

const QString kOut = QStringLiteral("Fake D-05 In");
const QString kIn = QStringLiteral("Fake D-05 Out");
// "Fake D-05 In" and "Boutique..." are ports the detection attributes to the D-05 (docs/IDENTIFY.md
// rule 1), so the D-05 page shows them marked (DESIGN-2.md 7.3).
const QString kDetected = QStringLiteral(" \u00B7 detected");

AppSettings::Values values(const QString &out, int channel = 1)
{
    AppSettings::Values v;
    v.outputPort = out;
    v.channel = channel;
    return v;
}

struct Rig {
    QTemporaryDir dir;
    QString iniPath;
    PatchCatalog catalog;
    std::unique_ptr<Library> library;
    FakeMidiBackend backend;
    std::unique_ptr<MidiLink> link;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<MainWindow> w;

    // Starts the way main.cpp does: window first, then the values take effect.
    explicit Rig(const AppSettings::Values &start = values(kOut))
    {
        Paths::setOverride(dir.path());
        iniPath = dir.filePath(QStringLiteral("settings.ini"));
        library = std::make_unique<Library>(catalog, dir.filePath(QStringLiteral("library.json")));
        link = std::make_unique<MidiLink>(&backend);
        settings = std::make_unique<AppSettings>(iniPath);
        w = std::make_unique<MainWindow>(catalog, *library, *link);
        w->setServices(settings.get());
        w->applySettings(start);
    }
    ~Rig()
    {
        w.reset();
        settings.reset();
        link.reset();
        library.reset();
        Paths::setOverride(QString());
    }

    bool show()
    {
        w->show();
        w->activateWindow();
        return QTest::qWaitForWindowActive(w.get());
    }
    const QList<QByteArray> &sent() const { return backend.sentMessages(); }
    AppSettings::Values saved() const { return AppSettings(iniPath).values(); }
    // Writes `v` as the saved settings (the dialog starts from these).
    void persist(const AppSettings::Values &v) { settings->setValues(v); }
    void dropAndReturn()
    {
        backend.setOutputPorts({});
        backend.setOutputPorts({kOut});
        link->pollNow();
    }

    SettingsDialog *openDialog()
    {
        w->openSettings();
        return w->settingsDialog();
    }
    // Moves the highlight like the keyboard does and lets queued events run.
    void key(QWidget *target, Qt::Key k) { QTest::keyClick(target, k); }
    void inject(std::initializer_list<int> bytes)
    {
        QByteArray b;
        for (int v : bytes)
            b.append(char(v));
        backend.injectReceived(b);
        QCoreApplication::processEvents();
    }
    void injectPatch(int lsb, int program)
    {
        inject({0xB0, 0x00, 87});
        inject({0xB0, 0x20, lsb});
        inject({0xC0, program});
    }
};

}  // namespace

class TestSettingsUi : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Theme::install(*qobject_cast<QApplication *>(qApp)); }

    // ------------------------------------------------------------------ dialog

    void dialogMatchesTheDesign()
    {
        Rig r;
        SettingsDialog *d = r.openDialog();
        QVERIFY(d);
        QCOMPARE(d->windowTitle(), QStringLiteral("Settings"));
        // DESIGN-2.md 7: 760 x 610, the page at content x 160 (parent coordinates of the page's own widgets).
        QCOMPARE(d->size(), QSize(760, 610));
        QVERIFY(QTest::qWaitForWindowExposed(d));  // the style sheet is applied by now
        QCOMPARE(d->currentPage()->pos(), QPoint(180, 20));
        QCOMPARE(d->outputCombo()->geometry(), QRect(112, 20, 260, 26));
        QCOMPARE(d->refreshButton()->geometry(), QRect(460, 20, 100, 26));
        QCOMPARE(d->channelSpin()->geometry(), QRect(112, 54, 72, 26));
        QCOMPARE(d->inputCombo()->geometry(), QRect(112, 144, 260, 26));
        QCOMPARE(d->testButton()->geometry(), QRect(20, 562, 100, 28));
        QCOMPARE(d->okButton()->geometry(), QRect(640, 562, 100, 28));
        QCOMPARE(d->cancelButton()->geometry(), QRect(532, 562, 100, 28));
        QCOMPARE(d->channelSpin()->minimum(), 1);
        QCOMPARE(d->channelSpin()->maximum(), 16);
        QVERIFY(d->okButton()->isDefault());
        QVERIFY(!d->cancelButton()->isDefault());
        QVERIFY(d->isModal());
        QCOMPARE(d->inputCombo()->itemText(0), QStringLiteral("None"));
        QCOMPARE(d->connectionText(), QStringLiteral("Connected to Fake D-05 In"));
        QCOMPARE(d->connectionState(), QStringLiteral("ok"));
        d->reject();
    }

    void dialogHasInitialFocusOnTheOutputCombo()
    {
        Rig r;
        QVERIFY(r.show());
        SettingsDialog *d = r.openDialog();
        QVERIFY(QTest::qWaitForWindowExposed(d));
        // The dialog's own focus child (the offscreen platform may leave the main window active).
        QCOMPARE(d->focusWidget(), static_cast<QWidget *>(d->outputCombo()));
        d->reject();
    }

    void dialogShowsTheValuesInEffect()
    {
        AppSettings::Values v = values(kOut, 5);
        v.inputPort = kIn;
        v.follow = true;
        v.autoSend = true;
        v.resendOnConnect = true;
        Rig r(v);
        r.persist(v);
        SettingsDialog *d = r.openDialog();
        QCOMPARE(d->values(), v);
        QCOMPARE(d->channelSpin()->value(), 5);
        QVERIFY(d->followCheck()->isChecked());
        QVERIFY(d->followCheck()->isEnabled());
        QVERIFY(d->autoSendCheck()->isChecked());
        QVERIFY(d->resendCheck()->isChecked());
        d->reject();
    }

    void followIsDisabledWithoutAnInputPort()
    {
        Rig r;
        SettingsDialog *d = r.openDialog();
        QCOMPARE(d->inputCombo()->currentData().toString(), QString());
        QVERIFY(!d->followCheck()->isEnabled());
        d->inputCombo()->setCurrentIndex(1);  // Fake D-05 Out
        QVERIFY(d->followCheck()->isEnabled());
        d->inputCombo()->setCurrentIndex(0);  // None
        QVERIFY(!d->followCheck()->isEnabled());
        d->reject();
    }

    void okAppliesAndPersists()
    {
        Rig r;
        SettingsDialog *d = r.openDialog();
        d->channelSpin()->setValue(5);
        d->inputCombo()->setCurrentIndex(1);
        d->followCheck()->setChecked(true);
        d->autoSendCheck()->setChecked(true);
        d->resendCheck()->setChecked(true);
        QVERIFY(!QFile::exists(r.iniPath));  // nothing is written before OK
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());

        QCOMPARE(r.link->channel(), 5);
        QVERIFY(r.link->followEnabled());
        QVERIFY(r.w->autoSend());
        QVERIFY(r.w->resendOnConnect());
        QVERIFY(r.w->autoSendButton()->lit());
        QVERIFY(r.backend.isInputOpen());
        const AppSettings::Values saved = r.saved();
        QCOMPARE(saved.channel, 5);
        QCOMPARE(saved.outputPort, kOut);
        QCOMPARE(saved.inputPort, kIn);
        QVERIFY(saved.follow && saved.autoSend && saved.resendOnConnect);
        QVERIFY(r.sent().isEmpty());  // same port, nothing reconnected, nothing sent
    }

    void cancelRevertsEverything()
    {
        Rig r;
        r.backend.setOutputPorts({kOut, QStringLiteral("Other Out")}, false);
        SettingsDialog *d = r.openDialog();
        d->channelSpin()->setValue(9);
        d->outputCombo()->setCurrentIndex(d->outputCombo()->findText(QStringLiteral("Other Out")));
        d->autoSendCheck()->setChecked(true);
        d->inputCombo()->setCurrentIndex(1);
        d->followCheck()->setChecked(true);
        QTest::mouseClick(d->testButton(), Qt::LeftButton);  // the one thing that touches the live link
        QCOMPARE(r.link->channel(), 9);
        QCOMPARE(r.link->connectedPortKey(), QStringLiteral("Other Out"));
        QTest::mouseClick(d->cancelButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());

        QCOMPARE(r.link->channel(), 1);
        QCOMPARE(r.link->connectedPortKey(), kOut);
        QVERIFY(r.link->isConnected());
        QVERIFY(!r.w->autoSend());
        QVERIFY(!r.link->followEnabled());
        QVERIFY(!r.backend.isInputOpen());
        // Nothing but the remembered patch (TEST sent it) reached the file.
        QCOMPARE(r.saved(), AppSettings::Values{});
        QCOMPARE(r.sent().size(), 3);  // the TEST only
    }

    void escapeIsCancel()
    {
        Rig r;
        SettingsDialog *d = r.openDialog();
        d->channelSpin()->setValue(12);
        QTest::keyClick(d, Qt::Key_Escape);
        QTRY_VERIFY(!r.w->settingsDialog());
        QCOMPARE(r.link->channel(), 1);
        QVERIFY(!QFile::exists(r.iniPath));
    }

    void cancelWithoutTestChangesNothing()
    {
        Rig r;
        r.backend.setOutputPorts({kOut, QStringLiteral("Other Out")}, false);
        SettingsDialog *d = r.openDialog();
        d->outputCombo()->setCurrentIndex(d->outputCombo()->findText(QStringLiteral("Other Out")));
        QCOMPARE(d->connectionText(), QStringLiteral("Connected to Other Out"));  // preview only
        QCOMPARE(r.link->connectedPortKey(), kOut);                                // the link was not touched
        QCOMPARE(r.backend.openOutputName(), kOut);
        d->reject();
        QCOMPARE(r.link->connectedPortKey(), kOut);
    }

    void testSendsExactlyOnceOnThePendingChannel()
    {
        Rig r;
        r.w->setCurrent(addr("P3-45"));
        SettingsDialog *d = r.openDialog();
        QVERIFY(d->testButton()->isEnabled());
        d->channelSpin()->setValue(7);
        QTest::mouseClick(d->testButton(), Qt::LeftButton);
        QCOMPARE(r.sent(), MidiLink::patchMessages(addr("P3-45"), 7));
        QVERIFY(d->testPressed());
        d->reject();
        QCOMPARE(r.sent().size(), 3);
    }

    void testIsDisabledWhileThePortIsMissing()
    {
        Rig r(values(QStringLiteral("Not There")));
        r.persist(values(QStringLiteral("Not There")));
        SettingsDialog *d = r.openDialog();
        QCOMPARE(d->connectionText(), QStringLiteral("Port not found"));
        QCOMPARE(d->connectionState(), QStringLiteral("error"));
        QVERIFY(!d->testButton()->isEnabled());
        // The configured port stays visible so OK does not forget it.
        QCOMPARE(d->outputCombo()->currentText(), QStringLiteral("Not There"));
        QCOMPARE(d->values().outputPort, QStringLiteral("Not There"));
        d->reject();
        QVERIFY(r.sent().isEmpty());
    }

    void connectionLineReportsAListingError()
    {
        Rig r;
        r.backend.setListError(QStringLiteral("boom"));
        SettingsDialog *d = r.openDialog();
        QCOMPARE(d->connectionText(), QStringLiteral("Error: boom"));
        QCOMPARE(d->connectionState(), QStringLiteral("error"));
        QVERIFY(!d->testButton()->isEnabled());
        r.backend.setListError(QString());
        QTest::mouseClick(d->refreshButton(), Qt::LeftButton);
        QCOMPARE(d->connectionText(), QStringLiteral("Connected to Fake D-05 In"));
        QVERIFY(d->testButton()->isEnabled());
        d->reject();
    }

    void connectionLineReportsAnOpenError()
    {
        Rig r(values(kOut));
        r.persist(values(kOut));
        r.backend.setFailOpen(true);
        r.link->setOutputPort(kOut);  // reconnect: the open now fails
        QVERIFY(!r.link->isConnected());
        SettingsDialog *d = r.openDialog();
        QCOMPARE(d->connectionText(), QStringLiteral("Error: open failed"));
        d->reject();
    }

    void refreshPicksUpNewPortsAndKeepsTheSelection()
    {
        Rig r;
        SettingsDialog *d = r.openDialog();
        QCOMPARE(d->outputCombo()->count(), 1);
        r.backend.setOutputPorts({QStringLiteral("Aardvark"), kOut}, false);
        QTest::mouseClick(d->refreshButton(), Qt::LeftButton);
        QCOMPARE(d->outputCombo()->count(), 2);
        QCOMPARE(d->outputCombo()->currentData().toString(), kOut);
        QCOMPARE(d->outputCombo()->currentText(), kOut + kDetected);
        d->reject();
    }

    void portsAreOfferedAndSavedAsKeys()
    {
        Rig r;
        r.backend.setOutputPorts({QStringLiteral("Boutique:Boutique MIDI 1 24:0"), kOut}, false);
        SettingsDialog *d = r.openDialog();
        const int i = d->outputCombo()->findText(QStringLiteral("Boutique:Boutique MIDI 1") + kDetected);
        QVERIFY(i >= 0);
        d->outputCombo()->setCurrentIndex(i);
        QCOMPARE(d->values().outputPort, QStringLiteral("Boutique:Boutique MIDI 1"));
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());
        QCOMPARE(r.saved().outputPort, QStringLiteral("Boutique:Boutique MIDI 1"));
        QVERIFY(r.link->isConnected());
        QCOMPARE(r.link->connectedPortName(), QStringLiteral("Boutique:Boutique MIDI 1 24:0"));
    }

    void settingsActionOpensTheDialog()
    {
        Rig r;
        QVERIFY(r.show());
        QVERIFY(!r.w->settingsDialog());
        r.w->action(QStringLiteral("actionSettings"))->trigger();
        QVERIFY(r.w->settingsDialog());
        r.w->action(QStringLiteral("actionSettings"))->trigger();  // not a second one
        QCOMPARE(r.w->findChildren<SettingsDialog *>().size(), 1);
        r.w->settingsDialog()->reject();
        QTRY_VERIFY(!r.w->settingsDialog());
    }

    void withoutServicesTheActionSaysSo()
    {
        QTemporaryDir dir;
        Paths::setOverride(dir.path());
        PatchCatalog catalog;
        Library lib(catalog, dir.filePath(QStringLiteral("library.json")));
        FakeMidiBackend backend;
        MidiLink link(&backend);
        MainWindow w(catalog, lib, link);
        w.openSettings();
        QVERIFY(!w.settingsDialog());
        QVERIFY(w.message().contains(QStringLiteral("not available")));
        Paths::setOverride(QString());
    }

    // Two identical devices differ only by their ALSA ids: the combo keeps their full names, and
    // the saved value, TEST and OK all address the selected one.
    void identicalDevicesAreToldApartByFullName()
    {
        const QString a = QStringLiteral("Boutique:Boutique MIDI 1 24:0");
        const QString b = QStringLiteral("Boutique:Boutique MIDI 1 32:0");
        const QString other = QStringLiteral("Other:Port 1 20:0");
        Rig r(values(QString()));
        r.backend.setOutputPorts({a, b, other}, false);
        SettingsDialog *d = r.openDialog();
        QVERIFY(d);
        QComboBox *c = d->outputCombo();
        QCOMPARE(c->count(), 3);
        QCOMPARE(c->itemText(0), a + kDetected);  // ambiguous: full names
        QCOMPARE(c->itemText(1), b + kDetected);
        QCOMPARE(c->itemText(2), QStringLiteral("Other:Port 1"));  // unique: stripped key
        c->setCurrentIndex(1);
        QCOMPARE(d->values().outputPort, b);
        c->setCurrentIndex(2);
        QCOMPARE(d->values().outputPort, QStringLiteral("Other:Port 1"));
        // TEST opens the selected device, not the first with the same key.
        c->setCurrentIndex(1);
        QVERIFY(d->testButton()->isEnabled());
        QTest::mouseClick(d->testButton(), Qt::LeftButton);
        QCOMPARE(r.backend.openOutputName(), b);
        QCOMPARE(r.sent().size(), 3);
        // Cancel puts back what was open before the dialog (nothing).
        QTest::mouseClick(d->cancelButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());
        QVERIFY(!r.link->isConnected());
        // OK saves the full name and opens that device.
        d = r.openDialog();
        d->outputCombo()->setCurrentIndex(1);
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QCOMPARE(r.settings->outputPort(), b);
        QCOMPARE(r.backend.openOutputName(), b);
        // Reopened, the dialog selects the same device; switching to the other one reconnects.
        QTRY_VERIFY(!r.w->settingsDialog());
        d = r.openDialog();
        QCOMPARE(d->outputCombo()->currentIndex(), 1);
        d->outputCombo()->setCurrentIndex(0);
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QCOMPARE(r.settings->outputPort(), a);
        QCOMPARE(r.backend.openOutputName(), a);
    }

    // --fake-midi without --config-dir: OK saves the other values but never the fake ports.
    void fakePortsAreNotPersisted()
    {
        Rig r(values(QString()));
        r.persist(values(QStringLiteral("Real Synth")));  // what the real settings file holds
        r.w->setPersistPorts(false);
        SettingsDialog *d = r.openDialog();
        QVERIFY(d);
        const int i = d->outputCombo()->findData(kOut);
        QVERIFY(i >= 0);
        d->outputCombo()->setCurrentIndex(i);
        d->channelSpin()->setValue(7);
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(r.link->isConnected());  // the run itself uses the fake port
        const AppSettings::Values saved = r.saved();
        QCOMPARE(saved.channel, 7);                              // other settings persist
        QCOMPARE(saved.outputPort, QStringLiteral("Real Synth"));  // the port does not
        QVERIFY(saved.inputPort.isEmpty());
    }

    void inputErrorElidesWithBodyMetrics()
    {
        Rig r;
        r.backend.setListError(QStringLiteral("a very long input enumeration failure text"));
        SettingsDialog *d = r.openDialog();
        QVERIFY(d);
        QLabel *l = d->findChild<QLabel *>(QStringLiteral("inputStatusText"));
        QVERIFY(l);
        QVERIFY2(l->text().endsWith(QChar(0x2026)), qPrintable(l->text()));
        QCOMPARE(l->font(), Theme::font(Theme::Role::Body));
        QVERIFY(QFontMetrics(Theme::font(Theme::Role::Body)).horizontalAdvance(l->text()) <= 100);
        QVERIFY(l->styleSheet().isEmpty());
        QVERIFY(l->toolTip().contains(QStringLiteral("very long")));
    }

    void settingsScreenshotRenders()
    {
        QTemporaryDir out;
        QVERIFY(out.isValid());
        CliOptions o;
        o.screenshotFile = out.filePath(QStringLiteral("settings.png"));
        o.screenshotState = QStringLiteral("settings");
        o.configDir = out.path();
        QCOMPARE(runScreenshot(o), 0);
        QImage img;
        QVERIFY(img.load(o.screenshotFile));
        const qreal dpr = qApp->devicePixelRatio();
        QCOMPARE(img.width(), int(760 * dpr));
        QCOMPARE(img.height(), int(610 * dpr));
        Paths::setOverride(QString());
    }

    void dialogButtonsUsePanelLabelFontAndRefreshIs26High()
    {
        FakeMidiBackend backend;
        PortHub hub(&backend);
        DeviceRouter router(&hub);
        SettingsDialog d(router, SettingsDialog::State(), Model::D05, nullptr, false);
        d.show();
        QApplication::processEvents();
        const QFont want = Theme::font(Theme::Role::PanelLabel);
        for (QPushButton *b : {d.refreshButton(), d.testButton(), d.cancelButton(), d.okButton()})
            QCOMPARE(b->font().letterSpacing(), want.letterSpacing());
        QVERIFY(want.letterSpacing() != 0);
        // Painted height of REFRESH, unfocused and focused: 26 device-independent px.
        const auto painted = [&](QPushButton *b) {
            const QImage img = d.grab().toImage();
            const qreal dpr = img.devicePixelRatio();
            const QRect r(b->mapTo(&d, QPoint()), b->size());  // the page's widgets are in page coordinates
            const int x = int((r.x() + r.width() / 2) * dpr);
            const QColor bg = img.pixelColor(x, int((r.y() - 6) * dpr));
            int first = -1, last = -1;
            for (int y = int((r.y() - 6) * dpr); y < int((r.bottom() + 6) * dpr); ++y) {
                if (img.pixelColor(x, y) != bg) {
                    if (first < 0)
                        first = y;
                    last = y;
                }
            }
            return int(qRound((last - first + 1) / dpr));
        };
        d.okButton()->setFocus();
        QApplication::processEvents();
        QCOMPARE(painted(d.refreshButton()), 26);
        d.refreshButton()->setFocus();
        QApplication::processEvents();
        QCOMPARE(painted(d.refreshButton()), 26);
    }

    // ------------------------------------------------------------------ resend on connect

    void resendOnConnectSendsOnceWhenThePortReturns()
    {
        AppSettings::Values v = values(kOut);
        v.resendOnConnect = true;
        Rig r(v);
        QCOMPARE(r.sent().size(), 3);  // the start-up connect: the current patch, once
        QCOMPARE(r.sent(), MidiLink::patchMessages(addr("P1-11"), 1));
        r.w->setCurrent(addr("P2-33"));
        r.backend.clearSent();

        r.backend.setOutputPorts({});  // unplugged
        QVERIFY(!r.link->isConnected());
        QVERIFY(r.link->isPolling());
        QVERIFY(r.sent().isEmpty());
        r.backend.setOutputPorts({kOut});  // plugged back in
        r.link->pollNow();
        QVERIFY(r.link->isConnected());
        QCOMPARE(r.sent(), MidiLink::patchMessages(addr("P2-33"), 1));
        r.link->pollNow();  // connected now: nothing more
        QCOMPARE(r.sent().size(), 3);
        QVERIFY(!r.link->isPolling());
    }

    void noResendWhenTheOptionIsOff()
    {
        Rig r;
        r.backend.setOutputPorts({});
        r.backend.setOutputPorts({kOut});
        r.link->pollNow();
        QVERIFY(r.link->isConnected());
        QVERIFY(r.sent().isEmpty());
    }

    void resendUsesTheRestoredPatchAtStartup()
    {
        QTemporaryDir dir;
        Paths::setOverride(dir.path());
        PatchCatalog catalog;
        Library lib(catalog, dir.filePath(QStringLiteral("library.json")));
        FakeMidiBackend backend;
        MidiLink link(&backend);
        AppSettings settings(dir.filePath(QStringLiteral("settings.ini")));
        MainWindow w(catalog, lib, link);
        w.setServices(&settings);
        w.setCurrent(addr("U4-72"));  // as main.cpp restores the last patch: no send
        QVERIFY(backend.sentMessages().isEmpty());
        AppSettings::Values v = values(kOut);
        v.resendOnConnect = true;
        w.applySettings(v);
        QCOMPARE(backend.sentMessages(), MidiLink::patchMessages(addr("U4-72"), 1));
        Paths::setOverride(QString());
    }

    void okWithAChangedPortResendsOnce()
    {
        Rig r;
        r.backend.setOutputPorts({kOut, QStringLiteral("Other Out")}, false);
        SettingsDialog *d = r.openDialog();
        d->resendCheck()->setChecked(true);
        d->outputCombo()->setCurrentIndex(d->outputCombo()->findText(QStringLiteral("Other Out")));
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());
        QCOMPARE(r.link->connectedPortKey(), QStringLiteral("Other Out"));
        QCOMPARE(r.sent(), MidiLink::patchMessages(addr("P1-11"), 1));
    }

    void testThenOkDoesNotSendTwice()
    {
        Rig r;
        r.backend.setOutputPorts({kOut, QStringLiteral("Other Out")}, false);
        SettingsDialog *d = r.openDialog();
        d->resendCheck()->setChecked(true);
        d->outputCombo()->setCurrentIndex(d->outputCombo()->findText(QStringLiteral("Other Out")));
        QTest::mouseClick(d->testButton(), Qt::LeftButton);
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());
        QCOMPARE(r.sent().size(), 3);
    }

    void okWithTheSamePortDoesNotResend()
    {
        Rig r;
        SettingsDialog *d = r.openDialog();
        d->resendCheck()->setChecked(true);
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());
        QVERIFY(r.sent().isEmpty());
        QVERIFY(r.w->resendOnConnect());
    }

    // ------------------------------------------------------------------ disconnected display

    void displayShowsNoMidiOutputWhileDisconnected()
    {
        Rig r(values(QStringLiteral("Not There")));
        QVERIFY(!r.link->isConnected());
        QVERIFY(r.w->display()->line2().startsWith(QStringLiteral("NO MIDI OUTPUT")));
        // The patch stays on line 1; a send that fails changes nothing.
        QVERIFY(r.w->display()->line1().startsWith(QStringLiteral("P1-11:Fantasia")));
        r.backend.setOutputPorts({QStringLiteral("Not There")}, false);
        r.link->pollNow();
        QVERIFY(r.link->isConnected());
        QVERIFY(!r.w->display()->line2().startsWith(QStringLiteral("NO MIDI OUTPUT")));
    }

    void displayShowsNoMidiOutputWhenThePortVanishes()
    {
        Rig r;
        QVERIFY(!r.w->display()->line2().startsWith(QStringLiteral("NO MIDI OUTPUT")));
        r.backend.setOutputPorts({});
        QVERIFY(r.w->display()->line2().startsWith(QStringLiteral("NO MIDI OUTPUT")));
    }

    void startupWithNoPortIsDisconnected()
    {
        // No port was ever configured: the first-run hint stands in for NO MIDI OUTPUT (T5.1).
        Rig r(AppSettings::Values{});
        QVERIFY(r.w->display()->line2().startsWith(QStringLiteral("OPEN SETTINGS: ")));
        QVERIFY(!r.link->isConnected());
        QVERIFY(r.sent().isEmpty());
    }

    // ------------------------------------------------------------------ auto-send

    void autoSendSendsWhenTheKeyboardMovesTheHighlight()
    {
        AppSettings::Values v = values(kOut);
        v.autoSend = true;
        Rig r(v);
        QVERIFY(r.show());
        r.w->list()->setFocus();
        QVERIFY(r.sent().isEmpty());  // start-up sent nothing
        r.key(r.w->list(), Qt::Key_Down);
        QCOMPARE(r.sent(), MidiLink::patchMessages(addr("P1-12"), 1));
        QCOMPARE(r.w->current(), addr("P1-12"));
        r.backend.clearSent();
        r.key(r.w->list(), Qt::Key_Down);
        QCOMPARE(r.sent(), MidiLink::patchMessages(addr("P1-13"), 1));
        r.backend.clearSent();
        r.key(r.w->list(), Qt::Key_End);
        QCOMPARE(r.sent(), MidiLink::patchMessages(addr("P6-88"), 1));
        r.backend.clearSent();
        r.key(r.w->list(), Qt::Key_Down);  // already at the end: no move, no send
        QVERIFY(r.sent().isEmpty());
    }

    void autoSendOffSendsNothingOnHighlight()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->list()->setFocus();
        r.key(r.w->list(), Qt::Key_Down);
        r.key(r.w->list(), Qt::Key_Down);
        r.key(r.w->list(), Qt::Key_End);
        QVERIFY(r.sent().isEmpty());
    }

    void autoSendWorksFromTheSearchField()
    {
        AppSettings::Values v = values(kOut);
        v.autoSend = true;
        Rig r(v);
        QVERIFY(r.show());
        QTest::keyClicks(r.w->searchField(), QStringLiteral("brass"));  // typing never sends
        QVERIFY(r.sent().isEmpty());
        QVERIFY(r.w->model()->rowCount() > 2);
        const PatchAddress first = *r.w->highlighted();
        r.key(r.w->searchField(), Qt::Key_Down);
        QVERIFY(*r.w->highlighted() != first);
        QCOMPARE(r.sent(), MidiLink::patchMessages(*r.w->highlighted(), 1));
    }

    void autoSendNeverFiresOnSearchTyping()
    {
        AppSettings::Values v = values(kOut);
        v.autoSend = true;
        Rig r(v);
        QVERIFY(r.show());
        QTest::keyClicks(r.w->searchField(), QStringLiteral("p3-4"));
        QTest::keyClick(r.w->searchField(), Qt::Key_Backspace);
        r.w->searchField()->clear();
        QVERIFY(r.sent().isEmpty());
    }

    void autoSendNeverFiresOnViewSwitches()
    {
        AppSettings::Values v = values(kOut);
        v.autoSend = true;
        Rig r(v);
        QVERIFY(r.show());
        r.w->viewButton(PatchListModel::View::Favourites)->click();
        r.w->viewButton(PatchListModel::View::Recents)->click();
        r.w->viewButton(PatchListModel::View::All)->click();
        r.w->action(QStringLiteral("actionViewFavourites"))->trigger();
        r.w->action(QStringLiteral("actionViewAll"))->trigger();
        QVERIFY(r.sent().isEmpty());
    }

    void aMouseClickSendsOnceEvenWithAutoSend()
    {
        AppSettings::Values v = values(kOut);
        v.autoSend = true;
        Rig r(v);
        QVERIFY(r.show());
        const QModelIndex i = r.w->model()->index(5);
        QTest::mouseClick(r.w->list()->viewport(), Qt::LeftButton, Qt::NoModifier, r.w->list()->visualRect(i).center());
        QCOMPARE(r.sent().size(), 3);
    }

    void aHeldKeySendsOnlyItsFirstAndLastStep()
    {
        AppSettings::Values v = values(kOut);
        v.autoSend = true;
        Rig r(v);
        QVERIFY(r.show());
        r.w->list()->setFocus();
        PatchListView *list = r.w->list();
        // Press, three auto-repeats, release: what a held arrow key delivers.
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(list, &press);
        QCOMPARE(r.sent().size(), 3);  // the first step
        for (int i = 0; i < 3; ++i) {
            QKeyEvent repeat(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier, QString(), true);
            QApplication::sendEvent(list, &repeat);
        }
        QCOMPARE(r.sent().size(), 3);  // the repeats stream nothing
        QCOMPARE(*r.w->highlighted(), addr("P1-15"));
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(list, &release);
        QCOMPARE(r.sent().size(), 6);  // the final row on release
        QCOMPARE(r.sent().mid(3), MidiLink::patchMessages(addr("P1-15"), 1));
    }

    void autoSendWhileDisconnectedSendsNothingAndSaysSo()
    {
        AppSettings::Values v = values(QStringLiteral("Not There"));
        v.autoSend = true;
        Rig r(v);
        QVERIFY(r.show());
        r.w->list()->setFocus();
        r.key(r.w->list(), Qt::Key_Down);
        QVERIFY(r.sent().isEmpty());
        QVERIFY(r.w->message().startsWith(QStringLiteral("Not sent")));
        QCOMPARE(r.w->current(), addr("P1-11"));
    }

    void autoSendToggleButtonAndShortcutPersist()
    {
        Rig r;
        QVERIFY(r.show());
        PanelButton *b = r.w->autoSendButton();
        QVERIFY(!b->lit());
        QVERIFY(!QFile::exists(r.iniPath));
        QTest::mouseClick(b, Qt::LeftButton, Qt::NoModifier, QPoint(26, 16));
        QVERIFY(r.w->autoSend());
        QVERIFY(b->lit());
        QVERIFY(r.saved().autoSend);

        r.w->action(QStringLiteral("actionAutoSend"))->trigger();  // Ctrl+Shift+A
        QVERIFY(!r.w->autoSend());
        QVERIFY(!b->lit());
        QVERIFY(!b->isChecked());
        QVERIFY(!r.saved().autoSend);

        r.w->action(QStringLiteral("actionAutoSend"))->trigger();
        QVERIFY(r.w->autoSend() && b->lit() && b->isChecked());
        QVERIFY(r.saved().autoSend);
        QCOMPARE(r.w->action(QStringLiteral("actionAutoSend"))->shortcut(),
                 QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
        QVERIFY(r.sent().isEmpty());  // toggling sends nothing
    }

    void autoSendShortcutIsWindowWide()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->searchField()->setFocus();
        QTest::keyClick(r.w.get(), Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(r.w->autoSend());
    }

    void savedAutoSendComesBackAtStart()
    {
        AppSettings::Values v = values(kOut);
        v.autoSend = true;
        Rig r(v);
        QVERIFY(r.w->autoSend());
        QVERIFY(r.w->autoSendButton()->lit());
        QVERIFY(r.sent().isEmpty());
    }

    // ------------------------------------------------------------------ follow

    static AppSettings::Values followValues(bool autoSend = false)
    {
        AppSettings::Values v = values(kOut);
        v.inputPort = kIn;
        v.follow = true;
        v.autoSend = autoSend;
        return v;
    }

    void followMovesTheCurrentPatchWithoutSending()
    {
        Rig r(followValues());
        QVERIFY(r.backend.isInputOpen());
        QVERIFY(r.show());
        r.injectPatch(10, 28);  // P3-45
        QCOMPARE(r.w->current(), addr("P3-45"));
        QVERIFY(r.w->display()->line1().startsWith(QStringLiteral("P3-45")));
        QCOMPARE(r.w->highlighted().value(), addr("P3-45"));
        QVERIFY(r.sent().isEmpty());
        QVERIFY(r.w->display()->activityActive());   // the LED blinks
        QVERIFY(!r.w->display()->sentActive());      // but line 2 does not claim SENT
        QVERIFY(!r.w->display()->line2().startsWith(QStringLiteral("SENT")));
        QVERIFY(!r.w->display()->line2().startsWith(QStringLiteral("NO MIDI OUTPUT")));
        QVERIFY(r.library->recents().isEmpty());  // a follow is not a send
    }

    void followNeverTriggersAutoSend()
    {
        Rig r(followValues(true));
        QVERIFY(r.show());
        QVERIFY(r.w->autoSend());
        r.injectPatch(10, 28);
        r.injectPatch(0, 0);  // U1-11
        r.injectPatch(13, 63);  // P6-88
        QCOMPARE(r.w->current(), addr("P6-88"));
        QVERIFY(r.sent().isEmpty());
        // ... and a keyboard move afterwards still does.
        r.w->list()->setFocus();
        r.key(r.w->list(), Qt::Key_Up);
        QCOMPARE(r.sent(), MidiLink::patchMessages(addr("P6-87"), 1));
    }

    void followAlsoWorksOnTheSavedChannel()
    {
        AppSettings::Values v = followValues();
        v.channel = 3;
        Rig r(v);
        r.inject({0xB0, 0x00, 87});  // wrong channel: ignored
        r.inject({0xB0, 0x20, 10});
        r.inject({0xC0, 28});
        QCOMPARE(r.w->current(), addr("P1-11"));
        r.inject({0xB2, 0x00, 87});
        r.inject({0xB2, 0x20, 10});
        r.inject({0xC2, 28});
        QCOMPARE(r.w->current(), addr("P3-45"));
    }

    void midiClockAndRealtimeBytesChangeNothing()
    {
        Rig r(followValues(true));
        QVERIFY(r.show());
        const QString line1 = r.w->display()->line1();
        const QString line2 = r.w->display()->line2();
        const PatchAddress cur = r.w->current();
        for (int i = 0; i < 24; ++i)
            r.inject({0xF8});  // timing clock, continuously on the D-05's USB port
        r.inject({0xFE});      // active sensing
        r.inject({0xFA});
        r.inject({0xFB});
        r.inject({0xFC});
        r.inject({0xFF});
        r.inject({0xF8, 0x00});  // even if a backend delivered it with trailing bytes
        QCOMPARE(r.w->current(), cur);
        QCOMPARE(r.w->display()->line1(), line1);
        QCOMPARE(r.w->display()->line2(), line2);
        QVERIFY(!r.w->display()->activityActive());
        QVERIFY(r.sent().isEmpty());
        // Clock between the three messages of a triplet does not break it either.
        r.inject({0xB0, 0x00, 87});
        r.inject({0xF8});
        r.inject({0xB0, 0x20, 11});
        r.inject({0xF8});
        r.inject({0xC0, 0});
        QCOMPARE(r.w->current(), addr("P4-11"));
        QVERIFY(r.sent().isEmpty());
    }

    void followOffIgnoresIncomingPatches()
    {
        AppSettings::Values v = followValues();
        v.follow = false;
        Rig r(v);
        QVERIFY(!r.backend.isInputOpen());
        r.injectPatch(10, 28);
        QCOMPARE(r.w->current(), addr("P1-11"));
    }

    void ourOwnSendIsNotFollowedBack()
    {
        Rig r(followValues());
        QVERIFY(r.show());
        r.w->setCurrent(addr("P1-11"));
        QVERIFY(r.link->sendPatch(addr("P3-45")));
        r.backend.clearSent();
        r.injectPatch(10, 28);  // the loop-back of our own triplet within 300 ms
        QCOMPARE(r.w->current(), addr("P3-45"));
        QVERIFY(r.sent().isEmpty());
        QVERIFY(!r.w->display()->activityActive() || r.w->display()->sentActive());
    }

    // ------------------------------------------------------------------ start-up writes nothing

    void startupWithCliOverridesWritesNothing()
    {
        QTemporaryDir dir;
        Paths::setOverride(dir.path());
        const QString ini = dir.filePath(QStringLiteral("settings.ini"));
        {
            AppSettings saved(ini);
            AppSettings::Values v = values(QStringLiteral("Saved Port"), 4);
            saved.setValues(v);
        }
        const QByteArray before = [&] {
            QFile f(ini);
            f.open(QIODevice::ReadOnly);
            return f.readAll();
        }();
        {
            PatchCatalog catalog;
            Library lib(catalog, dir.filePath(QStringLiteral("library.json")));
            FakeMidiBackend backend;
            MidiLink link(&backend);
            AppSettings settings(ini);
            MainWindow w(catalog, lib, link);
            w.setServices(&settings);
            CliOptions o;
            o.port = QStringLiteral("Fake");
            o.channel = 11;
            w.applySettings(Cli::startupValues(settings, o));
            QCOMPARE(link.channel(), 11);
            QVERIFY(link.isConnected());
            w.show();
            QVERIFY(QTest::qWaitForWindowActive(&w));
            QCOMPARE(AppSettings(ini).values().channel, 4);  // still the saved one
            QCOMPARE(AppSettings(ini).values().outputPort, QStringLiteral("Saved Port"));
            QFile f(ini);
            QVERIFY(f.open(QIODevice::ReadOnly));
            QCOMPARE(f.readAll(), before);  // byte for byte, while the window is up
        }
        Paths::setOverride(QString());
    }

    void lastPatchIsRemembered()
    {
        Rig r;
        QVERIFY(r.show());
        QVERIFY(r.link->sendPatch(addr("P2-33")));
        QVERIFY(r.w->current() == addr("P2-33"));
        QVERIFY(!AppSettings(r.iniPath).lastPatch());  // not written per send
        r.w->close();
        QVERIFY(AppSettings(r.iniPath).lastPatch());
        QCOMPARE(AppSettings(r.iniPath).lastPatch().value(), addr("P2-33"));
    }

    void lastPatchIsWrittenOnDestruction()
    {
        QTemporaryDir dir;
        Paths::setOverride(dir.path());
        const QString ini = dir.filePath(QStringLiteral("settings.ini"));
        {
            PatchCatalog catalog;
            Library lib(catalog, dir.filePath(QStringLiteral("library.json")));
            FakeMidiBackend backend;
            MidiLink link(&backend);
            AppSettings settings(ini);
            MainWindow w(catalog, lib, link);
            w.setServices(&settings);
            w.applySettings(values(kOut));
            w.show();
            QVERIFY(QTest::qWaitForWindowActive(&w));
            QVERIFY(link.sendPatch(addr("U3-45")));
            QVERIFY(!AppSettings(ini).lastPatch());
        }
        QCOMPARE(AppSettings(ini).lastPatch().value(), addr("U3-45"));
        Paths::setOverride(QString());
    }

    void followedPatchIsNotWrittenPerChange()
    {
        Rig r(followValues());
        QVERIFY(r.show());
        r.injectPatch(10, 28);
        QVERIFY(!QFile::exists(r.iniPath));
    }

    void aFailedSettingsWriteIsReportedOnce()
    {
        QTemporaryDir dir;
        QFile blocker(dir.filePath(QStringLiteral("afile")));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        Rig r;
        AppSettings bad(dir.filePath(QStringLiteral("afile/settings.ini")));  // a file where a directory is needed
        r.w->setServices(&bad);
        QVERIFY(r.w->message().isEmpty());
        r.w->setAutoSend(true);
        QVERIFY(bad.writeFailed());
        QCOMPARE(r.w->message(), QStringLiteral("Settings could not be saved"));
        r.w->showMessage(QStringLiteral("something else"));
        r.w->setAutoSend(false);
        QCOMPARE(r.w->message(), QStringLiteral("something else"));  // not repeated
        r.w->setServices(nullptr);
    }

    // ------------------------------------------------------------------ fix round

    void staleHeldKeyDoesNotSendOnALaterRelease()
    {
        AppSettings::Values v = values(kOut);
        v.autoSend = true;
        Rig r(v);
        QVERIFY(r.show());
        PatchListView *list = r.w->list();
        list->setFocus();
        const auto press = [&](Qt::Key k, bool repeat) {
            QKeyEvent e(QEvent::KeyPress, k, Qt::NoModifier, QString(), repeat);
            QApplication::sendEvent(list, &e);
        };
        const auto release = [&](Qt::Key k) {
            QKeyEvent e(QEvent::KeyRelease, k, Qt::NoModifier);
            QApplication::sendEvent(list, &e);
        };
        // Hold Down, lose focus mid-hold, a different key is released later: nothing sent.
        press(Qt::Key_Down, false);
        press(Qt::Key_Down, true);
        press(Qt::Key_Down, true);
        r.backend.clearSent();
        r.w->searchField()->setFocus();
        list->setFocus();
        release(Qt::Key_Return);
        release(Qt::Key_Shift);
        release(Qt::Key_Down);
        QVERIFY(r.sent().isEmpty());
        // Tap Shift mid-hold: the hold is over; releasing Down sends nothing.
        press(Qt::Key_Down, false);
        press(Qt::Key_Down, true);
        r.backend.clearSent();
        press(Qt::Key_Shift, false);
        release(Qt::Key_Shift);
        release(Qt::Key_Down);
        QVERIFY(r.sent().isEmpty());
        // A normal hold still sends its last row on the release of the same key.
        press(Qt::Key_Down, false);
        press(Qt::Key_Down, true);
        r.backend.clearSent();
        release(Qt::Key_Down);
        QCOMPARE(r.sent().size(), 3);
    }

    void staleHeldKeyInTheSearchFieldDoesNotDoubleSend()
    {
        AppSettings::Values v = values(kOut);
        v.autoSend = true;
        Rig r(v);
        QVERIFY(r.show());
        QLineEdit *f = r.w->searchField();
        f->setFocus();
        QKeyEvent first(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(f, &first);
        QKeyEvent rep(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier, QString(), true);
        QApplication::sendEvent(f, &rep);
        r.w->list()->setFocus();  // focus leaves mid-hold
        f->setFocus();
        r.backend.clearSent();
        QKeyEvent rel(QEvent::KeyRelease, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(f, &rel);
        QVERIFY(r.sent().isEmpty());
        QTest::keyClick(f, Qt::Key_Down);  // a plain tap: exactly one send
        QCOMPARE(r.sent().size(), 3);
        // Another key's release never sends either.
        QApplication::sendEvent(f, &rep);
        r.backend.clearSent();
        QKeyEvent other(QEvent::KeyPress, Qt::Key_Shift, Qt::NoModifier);
        QApplication::sendEvent(f, &other);
        QApplication::sendEvent(f, &rel);
        QVERIFY(r.sent().isEmpty());
    }

    void settingsOkNeverPersistsCliOverrides()
    {
        Rig r(values(kOut, 9));  // this run: --channel 9
        AppSettings::Values saved = values(kOut, 4);
        r.persist(saved);
        QCOMPARE(r.link->channel(), 9);
        SettingsDialog *d = r.openDialog();
        QCOMPARE(d->channelSpin()->value(), 4);  // the dialog shows the saved channel
        d->resendCheck()->setChecked(true);
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());
        const AppSettings::Values after = r.saved();
        QCOMPARE(after.channel, 4);
        QCOMPARE(after.outputPort, kOut);
        QVERIFY(after.resendOnConnect);
    }

    void settingsOkDoesNotKeepAnOverridePort()
    {
        Rig r(AppSettings::Values{});
        r.backend.setOutputPorts({kOut, QStringLiteral("Saved Out")}, false);
        r.w->applySettings(values(kOut));  // this run: --port "Fake D-05 In"
        r.persist(values(QStringLiteral("Saved Out"), 2));
        SettingsDialog *d = r.openDialog();
        QCOMPARE(d->outputCombo()->currentText(), QStringLiteral("Saved Out"));
        d->autoSendCheck()->setChecked(true);
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());
        QCOMPARE(r.saved().outputPort, QStringLiteral("Saved Out"));
        QCOMPARE(r.saved().channel, 2);
    }

    void aFailedResendIsNotRetriedOnEveryReconnect()
    {
        AppSettings::Values v = values(kOut);
        v.resendOnConnect = true;
        Rig r(v);
        QCOMPARE(r.sent().size(), 3);
        r.backend.clearSent();
        r.backend.setFailSend(true);
        r.backend.setOutputPorts({});
        r.backend.setOutputPorts({kOut});
        r.link->pollNow();  // reconnects; the resend fails and drops the port again
        QVERIFY(!r.link->isConnected());
        r.backend.setFailSend(false);
        r.link->pollNow();  // reconnects: blocked, so no second attempt
        QVERIFY(r.link->isConnected());
        QVERIFY(r.sent().isEmpty());
        r.link->pollNow();
        r.dropAndReturn();
        QVERIFY(r.sent().isEmpty());
        // A send that works lifts the block.
        r.w->step(1);
        QCOMPARE(r.sent().size(), 3);
        r.backend.clearSent();
        r.dropAndReturn();
        QCOMPARE(r.sent().size(), 3);
    }

    void changingSettingsLiftsAFailedResendBlock()
    {
        AppSettings::Values v = values(kOut);
        v.resendOnConnect = true;
        Rig r(v);
        r.backend.clearSent();
        r.backend.setFailSend(true);
        r.dropAndReturn();  // resend fails
        r.backend.setFailSend(false);
        r.dropAndReturn();
        QVERIFY(r.sent().isEmpty());
        r.w->applySettings(v);  // settings applied again: the block is gone
        r.dropAndReturn();
        QCOMPARE(r.sent().size(), 3);
    }

    void resendAndTestAreNotRecentsEntries()
    {
        AppSettings::Values v = values(kOut);
        v.resendOnConnect = true;
        Rig r(v);
        QCOMPARE(r.sent().size(), 3);  // the start-up resend
        QVERIFY(r.library->recents().isEmpty());
        QVERIFY(r.w->display()->sentActive());  // it still flashes SENT
        r.dropAndReturn();
        QVERIFY(r.library->recents().isEmpty());
        SettingsDialog *d = r.openDialog();
        QTest::mouseClick(d->testButton(), Qt::LeftButton);
        QVERIFY(r.library->recents().isEmpty());
        QVERIFY(r.w->display()->sentActive());
        d->reject();
        // A real selection still is one.
        r.w->step(1);
        QCOMPARE(r.library->recents().size(), 1);
    }

    void dialogFollowsTheLinkWhileOpen()
    {
        Rig r;
        r.persist(values(kOut));
        SettingsDialog *d = r.openDialog();
        QVERIFY(d->testButton()->isEnabled());
        r.backend.setFailSend(true);
        QTest::mouseClick(d->testButton(), Qt::LeftButton);
        QCOMPARE(d->connectionText(), QStringLiteral("Error: send failed"));
        QCOMPARE(d->connectionState(), QStringLiteral("error"));
        QVERIFY(!d->testButton()->isEnabled());
        d->reject();
    }

    void linkErrorIsAttributedWithTheLinksMatching()
    {
        Rig r(AppSettings::Values{});
        r.backend.setOutputPorts({QStringLiteral("Boutique:Boutique MIDI 1 24:0")}, false);
        r.backend.setFailOpen(true);
        r.w->applySettings(values(QStringLiteral("bout")));  // --port bout
        QVERIFY(!r.link->isConnected());
        r.persist(values(QStringLiteral("Boutique:Boutique MIDI 1")));
        SettingsDialog *d = r.openDialog();
        QCOMPARE(d->connectionText(), QStringLiteral("Error: open failed"));
        d->reject();
    }

    void cancelStillProcessesAConnectThatHappenedWhileOpen()
    {
        AppSettings::Values v = values(kOut);
        v.resendOnConnect = true;
        Rig r(v);
        r.backend.clearSent();
        SettingsDialog *d = r.openDialog();
        r.dropAndReturn();  // reconnects while the dialog is open
        QVERIFY(r.sent().isEmpty());  // decided when it closes
        d->resendCheck()->setChecked(false);  // unchecking in the dialog does not matter on Cancel
        QTest::mouseClick(d->cancelButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());
        QCOMPARE(r.sent(), MidiLink::patchMessages(addr("P1-11"), 1));
        // With the option off there is nothing to resend.
        Rig off;
        SettingsDialog *d2 = off.openDialog();
        off.dropAndReturn();
        d2->reject();
        QVERIFY(off.sent().isEmpty());
    }

    void cancelAfterATestPortSwitchRestoresAndResends()
    {
        AppSettings::Values v = values(kOut);
        v.resendOnConnect = true;
        Rig r(v);
        r.backend.setOutputPorts({kOut, QStringLiteral("Other Out")}, false);
        r.backend.clearSent();
        SettingsDialog *d = r.openDialog();
        d->outputCombo()->setCurrentIndex(d->outputCombo()->findText(QStringLiteral("Other Out")));
        QTest::mouseClick(d->testButton(), Qt::LeftButton);
        QCOMPARE(r.sent().size(), 3);
        QTest::mouseClick(d->cancelButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());
        QCOMPARE(r.link->connectedPortKey(), kOut);
        QCOMPARE(r.sent().size(), 6);  // the TEST, then the restored port gets the patch
    }

    void aReconnectAfterTestVoidsTheOkSuppression()
    {
        Rig r;
        SettingsDialog *d = r.openDialog();
        d->resendCheck()->setChecked(true);
        QTest::mouseClick(d->testButton(), Qt::LeftButton);
        QCOMPARE(r.sent().size(), 3);
        r.dropAndReturn();  // the synth was power-cycled after the TEST
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(!r.w->settingsDialog());
        QCOMPARE(r.sent().size(), 6);
    }

    void applyingTheSameInputDoesNotReopenIt()
    {
        Rig r(followValues());
        r.inject({0xB0, 0x20, 10});  // half a triplet: LSB for P3
        r.w->applySettings(followValues());
        r.inject({0xC0, 28});  // reopening the input would have dropped the LSB
        QCOMPARE(r.w->current(), addr("P3-45"));
    }

    // ------------------------------------------------------------------ R3

    static QLineEdit *editorOf(Rig &r)
    {
        for (QLineEdit *e : r.w->list()->viewport()->findChildren<QLineEdit *>(QStringLiteral("nameEditor"))) {
            if (!e->isHidden())
                return e;
        }
        return nullptr;
    }

    void autoSendInRecentsDoesNotPingPong()
    {
        AppSettings::Values v = values(kOut);
        Rig r(v);
        QVERIFY(r.show());
        QVERIFY(r.link->sendPatch(addr("P1-13")));
        QVERIFY(r.link->sendPatch(addr("P1-12")));
        QVERIFY(r.link->sendPatch(addr("P1-11")));  // recents: P1-11, P1-12, P1-13
        r.w->viewButton(PatchListModel::View::Recents)->click();
        QCOMPARE(r.w->model()->rowCount(), 3);
        r.w->setAutoSend(true);
        r.backend.clearSent();
        r.w->list()->setFocus();
        QCOMPARE(*r.w->highlighted(), addr("P1-11"));
        QList<PatchAddress> visited{*r.w->highlighted()};
        for (int i = 0; i < 4; ++i) {
            r.key(r.w->list(), Qt::Key_Down);
            visited << *r.w->highlighted();
        }
        QCOMPARE(visited, (QList<PatchAddress>{addr("P1-11"), addr("P1-12"), addr("P1-13"), addr("P1-13"), addr("P1-13")}));
        QCOMPARE(r.sent().size(), 6);  // two sends: P1-12, P1-13; the two further Downs have no row to go to
        // The rows kept their order; the counts moved in place.
        QCOMPARE(r.w->model()->row(0).address, addr("P1-11"));
        QCOMPARE(r.w->model()->row(1).address, addr("P1-12"));
        QCOMPARE(r.w->model()->row(2).address, addr("P1-13"));
        QCOMPARE(r.w->model()->row(1).countText, QStringLiteral("×2"));
        QCOMPARE(r.w->model()->row(2).countText, QStringLiteral("×2"));
        QCOMPARE(r.w->model()->row(0).countText, QStringLiteral("×1"));
        // Entering the view again refreshes the order (most recent first).
        r.w->viewButton(PatchListModel::View::All)->click();
        r.w->viewButton(PatchListModel::View::Recents)->click();
        QCOMPARE(r.w->model()->row(0).address, addr("P1-13"));
        QCOMPARE(r.w->model()->row(1).address, addr("P1-12"));
        QCOMPARE(r.w->model()->row(2).address, addr("P1-11"));
    }

    void clearingRecentsStillEmptiesTheRecentsView()
    {
        Rig r;
        QVERIFY(r.show());
        QVERIFY(r.link->sendPatch(addr("P1-13")));
        r.w->viewButton(PatchListModel::View::Recents)->click();
        QCOMPARE(r.w->model()->rowCount(), 1);
        r.library->clearRecents();
        QCOMPARE(r.w->model()->rowCount(), 0);
    }

    void aQuietSendLeavesTheRenameEditorAndHighlightAlone()
    {
        AppSettings::Values v = values(kOut);
        v.resendOnConnect = true;
        Rig r(v);
        QVERIFY(r.show());
        r.w->list()->setFocus();
        const QModelIndex u = r.w->model()->indexOf(addr("U1-14"));
        r.w->list()->setCurrentIndex(u);
        r.w->renameHighlighted();
        QLineEdit *e = editorOf(r);
        QVERIFY(e);
        e->setText(QStringLiteral("Half a nam"));
        r.backend.clearSent();
        r.dropAndReturn();  // poll reconnect: resend-on-connect fires
        QCOMPARE(r.sent().size(), 3);
        QCOMPARE(editorOf(r), e);  // still open
        QCOMPARE(e->text(), QStringLiteral("Half a nam"));
        QCOMPARE(*r.w->highlighted(), addr("U1-14"));  // the list did not jump to the current patch
        QCOMPARE(r.w->current(), addr("P1-11"));       // the display shows what was resent
        // TEST is quiet too.
        SettingsDialog *d = r.openDialog();
        QTest::mouseClick(d->testButton(), Qt::LeftButton);
        QCOMPARE(editorOf(r), e);
        QCOMPARE(*r.w->highlighted(), addr("U1-14"));
        d->reject();
        // A real selection still commits the editor.
        r.w->step(1);
        QVERIFY(!editorOf(r));
        QCOMPARE(r.catalog.name(addr("U1-14")), QStringLiteral("Half a nam"));
    }

    void aFollowCommitsTheRenameEditor()
    {
        Rig r(followValues());
        QVERIFY(r.show());
        r.w->list()->setFocus();
        r.w->list()->setCurrentIndex(r.w->model()->indexOf(addr("U1-14")));
        r.w->renameHighlighted();
        QLineEdit *e = editorOf(r);
        QVERIFY(e);
        e->setText(QStringLiteral("Followed"));
        r.injectPatch(10, 28);
        QVERIFY(!editorOf(r));
        QCOMPARE(r.catalog.name(addr("U1-14")), QStringLiteral("Followed"));
    }

    void closingSavesARenameInProgress()
    {
        {
            Rig r;
            QVERIFY(r.show());
            r.w->list()->setFocus();
            r.w->list()->setCurrentIndex(r.w->model()->indexOf(addr("U2-21")));
            r.w->renameHighlighted();
            QLineEdit *e = editorOf(r);
            QVERIFY(e);
            e->setText(QStringLiteral("Saved On Close"));
            QVERIFY(r.w->close());
            QCOMPARE(r.catalog.name(addr("U2-21")), QStringLiteral("Saved On Close"));
            r.library->flush();
            PatchCatalog c2;
            Library l2(c2, r.dir.filePath(QStringLiteral("library.json")));
            l2.load();
            QCOMPARE(c2.name(addr("U2-21")), QStringLiteral("Saved On Close"));
        }
    }

    void closingAppliesACommitThatIsStillQueued()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->list()->setFocus();
        r.w->list()->setCurrentIndex(r.w->model()->indexOf(addr("U2-22")));
        r.w->renameHighlighted();
        QLineEdit *e = editorOf(r);
        QVERIFY(e);
        e->setText(QStringLiteral("Queued"));
        QTest::keyClick(e, Qt::Key_Return);  // the commit is queued: no event loop turn yet
        QVERIFY(r.w->close());
        QCOMPARE(r.catalog.name(addr("U2-22")), QStringLiteral("Queued"));
    }

    void quitActionSavesARenameInProgress()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->list()->setFocus();
        r.w->list()->setCurrentIndex(r.w->model()->indexOf(addr("U2-23")));
        r.w->renameHighlighted();
        QLineEdit *e = editorOf(r);
        QVERIFY(e);
        e->setText(QStringLiteral("Quit Name"));
        r.w->action(QStringLiteral("actionQuit"))->trigger();
        QCOMPARE(r.catalog.name(addr("U2-23")), QStringLiteral("Quit Name"));
    }

    static QStringList visibleStatusTexts(Rig &r)
    {
        QStringList texts;
        for (const QLabel *l : r.w->statusBar()->findChildren<QLabel *>())
            if (l->isVisibleTo(r.w.get()))
                texts << l->text();
        return texts;
    }

    void statusBarShowsFollowOnlyWhileTheInputIsOpen()
    {
        Rig r(followValues());
        QVERIFY(r.show());
        QVERIFY(visibleStatusTexts(r).contains(QStringLiteral("FOLLOW")));
        QVERIFY(!visibleStatusTexts(r).contains(QStringLiteral("NOT FOUND")));
        r.backend.setInputPorts({});  // the input vanishes
        QVERIFY(!visibleStatusTexts(r).contains(QStringLiteral("FOLLOW")));
        QVERIFY(visibleStatusTexts(r).contains(QStringLiteral("NOT FOUND")));
        r.backend.setInputPorts({kIn});
        r.link->pollNow();
        QVERIFY(visibleStatusTexts(r).contains(QStringLiteral("FOLLOW")));
        QVERIFY(!visibleStatusTexts(r).contains(QStringLiteral("NOT FOUND")));
        // An input that fails to open is reported as well.
        r.backend.setFailOpen(true);
        r.w->applySettings(values(kOut));  // follow off, input None
        r.w->applySettings(followValues());
        QVERIFY(!visibleStatusTexts(r).contains(QStringLiteral("FOLLOW")));
        QVERIFY(visibleStatusTexts(r).contains(QStringLiteral("NOT FOUND")));
        QVERIFY(r.w->message().startsWith(QStringLiteral("Input: Error")));
    }

    void dialogShowsInputProblems()
    {
        Rig r;
        AppSettings::Values v = followValues();
        r.persist(v);
        SettingsDialog *d = r.openDialog();
        QCOMPARE(d->inputProblem(), QString());
        // Listing fails: the error shows on the input row.
        r.backend.setListError(QStringLiteral("boom"));
        QTest::mouseClick(d->refreshButton(), Qt::LeftButton);
        QCOMPARE(d->inputProblem(), QStringLiteral("Error: boom"));
        r.backend.setListError(QString());
        QTest::mouseClick(d->refreshButton(), Qt::LeftButton);
        QCOMPARE(d->inputProblem(), QString());
        // The saved input is not listed any more.
        r.backend.setInputPorts({QStringLiteral("Other In")}, false);
        QTest::mouseClick(d->refreshButton(), Qt::LeftButton);
        QCOMPARE(d->inputProblem(), QStringLiteral("Port not found"));
        d->inputCombo()->setCurrentIndex(0);  // None: nothing to report
        QCOMPARE(d->inputProblem(), QString());
        d->reject();
    }

    void dialogShowsAnInputOpenErrorFromTheLink()
    {
        Rig r;
        r.persist(followValues());
        r.backend.setFailOpen(true);
        r.w->applySettings(followValues());  // the input is wanted and fails to open
        QVERIFY(!r.link->inputConnected());
        SettingsDialog *d = r.openDialog();
        QCOMPARE(d->inputProblem(), QStringLiteral("Error: open failed"));
        r.backend.setFailOpen(false);
        d->reject();
    }

    void followIsOffInTheValuesWhenTheInputIsNone()
    {
        Rig r;
        SettingsDialog *d = r.openDialog();
        d->followCheck()->setChecked(true);  // disabled, but a stale tick must not count
        QVERIFY(!d->followCheck()->isEnabled());
        QVERIFY(!d->values().follow);
        d->inputCombo()->setCurrentIndex(1);
        QVERIFY(d->values().follow);
        d->inputCombo()->setCurrentIndex(0);
        QVERIFY(!d->values().follow);
        d->reject();
    }

    void geometryIsSavedOnClose()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->resize(1000, 600);
        QVERIFY(r.w->close());
        QVERIFY(!AppSettings(r.iniPath).windowGeometry().isEmpty());
    }
};

QTEST_MAIN(TestSettingsUi)
#include "test_settings_ui.moc"
