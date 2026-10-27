#include "app/Screenshot.h"

#include "app/Paths.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "core/SearchQuery.h"
#include "midi/DeviceRouter.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiLink.h"
#include "midi/PortHub.h"
#include "ui/BankGrid.h"
#include "ui/DisplayWidget.h"
#include "ui/LedDisplayWidget.h"
#include "ui/MainWindow.h"
#include "ui/AboutDialog.h"
#include "ui/PanelButton.h"
#include "ui/ShortcutsDialog.h"
#include "ui/PatchListView.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QLineEdit>
#include <QDialog>
#include <QPainter>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTextStream>

#include <memory>

namespace d05 {

namespace {

void fillCannedLibrary(Library &lib)
{
    const auto addr = [](const char *t) { return *PatchAddress::parse(QString::fromLatin1(t)); };
    for (const char *t : {"P1-11", "P1-12", "P3-45", "U1-11", "P6-88"})
        lib.setFavorite(addr(t), true);
    lib.setUserName(addr("U1-11"), QStringLiteral("Warm Pad Mk2"));
    lib.setUserName(addr("U1-12"), QStringLiteral("Bass Test"));
    const QDateTime base(QDate(2026, 1, 1), QTime(12, 0), Qt::UTC);
    const char *recents[] = {"P2-33", "P1-12", "P3-45", "P1-11"};  // oldest first
    int i = 0;
    for (const char *t : recents)
        lib.recordSend(addr(t), base.addSecs(60 * i++));
    lib.recordSend(addr("P1-11"), base.addSecs(60 * i++));
    lib.flush();
}

}  // namespace

namespace {
// Restores the Paths override that was in force when it was created.
struct OverrideGuard {
    QString previous = Paths::overrideDir();
    ~OverrideGuard() { Paths::setOverride(previous); }
};
}  // namespace

namespace {

// Puts the D-05 skin back when a screenshot state has used another: the skin is process-wide and
// the tests render several states in one process.
struct SkinGuard {
    ~SkinGuard() { Theme::setSkin(QStringLiteral("d05")); }
};

// The skin a state renders with (DESIGN-2.md 12); every other state is the D-05.
QString skinForState(const QString &state)
{
    if (state == QLatin1String("jp08"))
        return QStringLiteral("jp08");
    if (state == QLatin1String("ju06a-60") || state == QLatin1String("flip-hint"))
        return QStringLiteral("ju06a-60");
    if (state == QLatin1String("ju06a-106") || state == QLatin1String("flip-prompt")
        || state == QLatin1String("settings-devices"))
        return QStringLiteral("ju06a-106");
    return QStringLiteral("d05");
}

// What the LED display shows in a skin state. The window still drives the D-05 only (the router
// reaches the UI in T8.2 and T8.3), so these scenes are set directly: the contents of the
// mockups in docs/design (DESIGN-2.md 10) and the context strings of DESIGN-2.md 4.2.
struct LedScene {
    QString readout;
    QString name;
    QString context;
    LedDisplayWidget::Tone tone = LedDisplayWidget::Tone::Secondary;
};

LedScene sceneForState(const QString &state)
{
#ifdef Q_OS_MACOS
    const QString key = QStringLiteral("CMD");
#else
    const QString key = QStringLiteral("CTRL");
#endif
    if (state == QLatin1String("jp08"))
        return {QStringLiteral("35"), QStringLiteral("Lo Brass"), Theme::skin().idleContext,
                LedDisplayWidget::Tone::Secondary};
    if (state == QLatin1String("ju06a-60"))
        return {QStringLiteral("4-5"), QStringLiteral("LD Silo Lead"), Theme::skin().idleContext,
                LedDisplayWidget::Tone::Secondary};
    if (state == QLatin1String("ju06a-106"))
        return {QStringLiteral("1-1"), QStringLiteral("Brass"), Theme::skin().idleContext,
                LedDisplayWidget::Tone::Secondary};
    if (state == QLatin1String("flip-prompt"))  // armed for the 60 engine while believing 106
        return {QStringLiteral("1-1"), QStringLiteral("Brass"), QStringLiteral("FLIP SWITCH TO 60"),
                LedDisplayWidget::Tone::Lit};
    if (state == QLatin1String("flip-hint"))  // hint for the 106 engine while believing 60
        return {QStringLiteral("4-5"), QStringLiteral("LD Silo Lead"),
                QStringLiteral("FLIPPED TO 106? %1+E").arg(key), LedDisplayWidget::Tone::Primary};
    return {};
}

}  // namespace

int runScreenshot(const CliOptions &opts)
{
    QTextStream err(stderr);
    const QString &state = opts.screenshotState;

    // The skin first, before any widget exists: they are built in it.
    const SkinGuard skinGuard;
    Theme::setSkin(skinForState(state));

    OverrideGuard guard;
    std::unique_ptr<QTemporaryDir> tmp;
    if (opts.configDir.isEmpty()) {
        tmp = std::make_unique<QTemporaryDir>();
        if (!tmp->isValid()) {
            err << "cannot create a temporary config directory; not touching the real one\n";
            return 1;
        }
        Paths::setOverride(tmp->path());
    }
    PatchCatalog catalog;
    Library library(catalog, Paths::libraryPath());
    if (tmp) {
        fillCannedLibrary(library);
    } else {
        library.load();
    }

    FakeMidiBackend backend;
    PortHub hub(&backend);
    DeviceRouter router(&hub);
    // "disconnected" asks for a port the fake does not have, as the mockup's "Hapax MIDI 1".
    const bool disconnected = state == QLatin1String("disconnected");
    const bool devices = state == QLatin1String("devices");
    const bool settingsDevices = state == QLatin1String("settings-devices");
    if (settingsDevices)  // ports the detection knows (docs/IDENTIFY.md rule 1)
        backend.setOutputPorts({QStringLiteral("JU-06A")});
    if (settingsDevices)
        backend.setInputPorts({QStringLiteral("JU-06A")});

    PatchAddress current = *PatchAddress::parse(QStringLiteral("P1-11"));
    if (state == QLatin1String("recents") && !library.recents().isEmpty())
        current = library.recents().first().address;

    MainWindow window(catalog, library, router);
    window.setSystemLookup(false);  // the fake backend's names are not ALSA clients
    // The D-05 as version 1 had it: one device, on the fake port ("firstrun" has none configured at
    // all: line 2 shows the OPEN SETTINGS hint; "disconnected" wants a port the fake does not have).
    {
        AppSettings::DeviceValues d05;
        d05.enabled = true;
        d05.channel = 1;
        if (state != QLatin1String("firstrun"))
            d05.outputPort = disconnected ? QStringLiteral("Hapax MIDI 1") : QStringLiteral("Fake D-05 In");
        window.applyDevice(Model::D05, d05);
    }
    if (devices) {
        // DESIGN-2.md 10, mockup-devices.png: the D-05 current, the JP-08 connected on the same port
        // (a forwarder: another channel), the JU-06A's port missing (red).
        AppSettings::DeviceValues jp08;
        jp08.enabled = true;
        jp08.channel = 5;
        jp08.outputPort = QStringLiteral("Fake D-05 In");
        window.applyDevice(Model::JP08, jp08);
        AppSettings::DeviceValues ju;
        ju.enabled = true;
        ju.channel = 4;
        ju.outputPort = QStringLiteral("Hapax MIDI 1");
        window.applyDevice(Model::JU06A, ju);
    }
    window.setCurrent(current);
    if (state == QLatin1String("search")) {
        window.searchField()->setText(QStringLiteral("brass"));
    } else if (state == QLatin1String("nomatch")) {
        window.searchField()->setText(QStringLiteral("zzzz"));
    } else if (state == QLatin1String("favorites")) {
        window.viewButton(PatchListModel::View::Favourites)->setChecked(true);
    } else if (state == QLatin1String("recents")) {
        window.viewButton(PatchListModel::View::Recents)->setChecked(true);
    } else if (disconnected) {
        window.list()->setFocus();  // the mockup's second frame has the list focused
    }

    if (state == QLatin1String("settings") || state == QLatin1String("settings-error") || settingsDevices) {
        // The dialog alone, as in docs/design/mockup-settings.png (the D-05 page: the fake output
        // open, no input, channel 1, resend-on-connect on; the error variant asks for a port the
        // fake does not have: "Port not found", TEST disabled) and docs/design/mockup-settings-devices.png
        // (the JU-06A page in the JUNO-106 skin: detected ports, engine information, the JP-08 off).
        SettingsDialog::State st;
        Model shown = Model::D05;
        if (settingsDevices) {
            shown = Model::JU06A;
            AppSettings::DeviceValues d05;
            d05.enabled = true;
            d05.channel = 3;
            d05.outputPort = QStringLiteral("JU-06A");
            AppSettings::DeviceValues ju;
            ju.enabled = true;
            ju.channel = 4;
            ju.outputPort = QStringLiteral("JU-06A");
            ju.inputPort = QStringLiteral("JU-06A");
            ju.follow = true;
            st.devices[int(Model::D05)] = d05;
            st.devices[int(Model::JU06A)] = ju;
            st.followSwitches = true;
            router.setJu06aState(106, 28, 0);  // 60: 4-5, 106: 1-1
        } else {
            AppSettings::DeviceValues d05;
            d05.enabled = true;
            d05.outputPort = state == QLatin1String("settings-error") ? QStringLiteral("Hapax MIDI 1")
                                                                      : QStringLiteral("Fake D-05 In");
            d05.resendOnConnect = true;
            st.devices[int(Model::D05)] = d05;
        }
        SettingsDialog dialog(router, st, shown, nullptr, false);
        dialog.show();
        dialog.activateWindow();
        QApplication::processEvents();
        const QPixmap shot = dialog.grab();
        if (!shot.save(opts.screenshotFile, "PNG")) {
            err << "cannot write " << opts.screenshotFile << Qt::endl;
            return 1;
        }
        library.flush();
        return Exit::Ok;
    }

    if (state == QLatin1String("shortcuts") || state == QLatin1String("about")) {
        // The dialog alone, like the settings states.
        std::unique_ptr<QDialog> dialog;
        if (state == QLatin1String("shortcuts"))
            dialog = std::make_unique<ShortcutsDialog>();
        else
            dialog = std::make_unique<AboutDialog>(QCoreApplication::applicationVersion());
        dialog->show();
        dialog->activateWindow();
        QApplication::processEvents();
        const QPixmap shot = dialog->grab();
        if (!shot.save(opts.screenshotFile, "PNG")) {
            err << "cannot write " << opts.screenshotFile << Qt::endl;
            return 1;
        }
        library.flush();
        return Exit::Ok;
    }

    if (Theme::skin().display == Theme::DisplayKind::Led) {  // the readout, name and context of the state
        const LedScene scene = sceneForState(state);
        window.ledDisplay()->setContent(scene.readout, scene.name, scene.context, scene.tone);
    }
    window.resize(opts.screenshotSize);
    window.show();
    window.activateWindow();
    QApplication::processEvents();
    if (state == QLatin1String("entry")) {
        // A pending grid entry "P3-4_": group P3, bank 4 pressed, awaiting the number.
        window.bankGrid()->setFocus(Qt::OtherFocusReason);
        window.bankGrid()->beginEntry(PatchAddress::parse(QStringLiteral("P3-11"))->group());
        window.bankGrid()->pressBank(4);
    } else if (state == QLatin1String("rename")) {
        // The inline editor on U1-11 with a partial name and its n/18 counter.
        window.list()->setFocus(Qt::OtherFocusReason);
        const int row = window.model()->rowOf(*PatchAddress::parse(QStringLiteral("U1-11")));
        window.list()->setCurrentIndex(window.model()->index(row, 0));
        window.renameHighlighted();
        QApplication::processEvents();
        if (auto *editor = window.list()->viewport()->findChild<QLineEdit *>()) {
            editor->setText(QStringLiteral("Warm Pa"));
            editor->setCursorPosition(editor->text().size());
        }
    } else if (state == QLatin1String("clearrecents")) {
        window.confirmClearRecents();
    }
    QApplication::processEvents();
    QPixmap shot = window.grab();
    if (QDialog *box = window.clearRecentsBox()) {
        // The box is its own top-level window: paint it over the window where it sits.
        const QPoint at = box->mapToGlobal(QPoint()) - window.mapToGlobal(QPoint());
        const QPixmap boxShot = box->grab();
        QPainter painter(&shot);
        painter.drawPixmap(at, boxShot);
    }
    if (!shot.save(opts.screenshotFile, "PNG")) {
        err << "cannot write " << opts.screenshotFile << Qt::endl;
        return 1;
    }
    library.flush();
    return Exit::Ok;
}

}  // namespace d05
