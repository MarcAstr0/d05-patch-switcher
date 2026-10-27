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
#include <QScrollBar>
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
    // The other two devices (DESIGN-2.md 10): a favourite and a renamed slot each. The library makes
    // their catalogues when asked.
    lib.catalog(Model::JP08);
    lib.catalog(Model::JU06A);
    const auto jp = [](const char *t) { return *PatchAddress::parse(QString::fromLatin1(t), Model::JP08); };
    const auto ju = [](const char *t) { return *PatchAddress::parse(QString::fromLatin1(t), Model::JU06A); };
    lib.setFavorite(jp("3-8"), true);
    lib.setUserName(jp("3-4"), QStringLiteral("CN Strings Lift"));  // factory: Mellow Strings
    lib.setFavorite(ju("106:5-6"), true);
    lib.setFavorite(ju("106:7-3"), true);
    lib.setUserName(ju("106:5-2"), QStringLiteral("CN Sub Drone"));  // factory: Junoa Bass
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

// The skin a dialog state renders with (the dialogs take the skin that is current when they open);
// the main window picks its own from its device, like the application. Every other state is the D-05.
QString skinForState(const QString &state)
{
    return state == QLatin1String("settings-devices") ? QStringLiteral("ju06a-106") : QStringLiteral("d05");
}

}  // namespace

int runScreenshot(const CliOptions &opts)
{
    QTextStream err(stderr);
    const QString &state = opts.screenshotState;

    // The skin first, before any widget exists: they are built in it. The main window changes it with
    // its device, as in the application.
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
    // The JP-08 and JU-06A scenes (DESIGN-2.md 10, mockup-jp08.png and mockup-ju06a-*.png): one device
    // enabled, its output and input open on a fake port, everything driven through the router.
    const bool jp08 = state == QLatin1String("jp08");
    const bool ju06a = state == QLatin1String("ju06a-60") || state == QLatin1String("ju06a-106")
                       || state == QLatin1String("flip-prompt") || state == QLatin1String("flip-hint");
    if (settingsDevices || ju06a) {  // ports the detection knows (docs/IDENTIFY.md rule 1)
        backend.setOutputPorts({QStringLiteral("JU-06A")});
        backend.setInputPorts({QStringLiteral("JU-06A")});
    }
    if (jp08) {
        backend.setOutputPorts({QStringLiteral("Hapax MIDI 1")});
        backend.setInputPorts({QStringLiteral("Hapax MIDI 1")});
    }

    PatchAddress current = *PatchAddress::parse(QStringLiteral("P1-11"));
    if (state == QLatin1String("recents") && !library.recents().isEmpty())
        current = library.recents().first().address;

    MainWindow window(catalog, library, router);
    window.setSystemLookup(false);  // the fake backend's names are not ALSA clients
    // The D-05 as version 1 had it: one device, on the fake port ("firstrun" has none configured at
    // all: line 2 shows the OPEN SETTINGS hint; "disconnected" wants a port the fake does not have).
    if (!jp08 && !ju06a) {
        AppSettings::DeviceValues d05;
        d05.enabled = true;
        d05.channel = 1;
        if (state != QLatin1String("firstrun"))
            d05.outputPort = disconnected ? QStringLiteral("Hapax MIDI 1") : QStringLiteral("Fake D-05 In");
        window.applyDevice(Model::D05, d05);
    }
    // The 64-patch scenes enable their device (it becomes the current one, and the window re-skins
    // itself) and put the app in the state the mockup shows.
    AppSettings::DeviceValues scene;
    scene.enabled = true;
    scene.follow = true;
    if (jp08) {
        scene.channel = 5;
        scene.outputPort = scene.inputPort = QStringLiteral("Hapax MIDI 1");
        window.applyDevice(Model::JP08, scene);
        window.setCurrent(*PatchAddress::parse(QStringLiteral("3-5"), Model::JP08));
    } else if (ju06a) {
        scene.channel = 4;
        scene.outputPort = scene.inputPort = QStringLiteral("JU-06A");
        // 60 remembers 4-5, 106 remembers 7-1 (ju06a-60), or the other way round for the 106 scenes.
        if (state == QLatin1String("ju06a-60"))
            router.setJu06aState(60, 28, 48);
        else if (state == QLatin1String("flip-hint"))
            router.setJu06aState(60, 0, 28);  // 60: 1-1, 106: 4-5
        else
            router.setJu06aState(106, 28, 0);  // 60: 4-5, 106: 1-1
        window.applyDevice(Model::JU06A, scene);
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
    if (!jp08 && !ju06a)
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

    window.resize(opts.screenshotSize);
    window.show();
    window.activateWindow();
    QApplication::processEvents();
    // What the 64-patch scenes still need is the user's own doing: a search, the armed selection, the
    // highlight and the focus, and the panel's own flip (a Program Change on the input).
    const auto highlight = [&](const char *text, Model m, bool nearTop) {
        const QModelIndex i = window.model()->indexOf(*PatchAddress::parse(QString::fromLatin1(text), m));
        if (!i.isValid())
            return;
        window.list()->setCurrentIndex(i);
        window.list()->scrollTo(i, nearTop ? QAbstractItemView::PositionAtTop : QAbstractItemView::EnsureVisible);
    };
    if (jp08) {
        window.list()->setFocus();
        highlight("3-4", Model::JP08, true);  // the renamed slot, above the current 3-5
        window.list()->verticalScrollBar()->setValue(window.list()->verticalScrollBar()->value() - 5);  // per item
    } else if (state == QLatin1String("ju06a-60")) {
        window.searchField()->setText(QStringLiteral("pad"));
        window.searchField()->setFocus();
    } else if (state == QLatin1String("ju06a-106") || state == QLatin1String("flip-prompt")) {
        // On 106, armed for 60:3-6 (BS Ouch Bass): the prompt, the armed row and the 60 button's ring.
        router.select(*PatchAddress::parse(QStringLiteral("60:3-6"), Model::JU06A));
        if (state == QLatin1String("ju06a-106"))
            window.searchField()->setText(QStringLiteral("bass"));
        window.list()->setFocus();
        QApplication::processEvents();
        highlight("60:3-6", Model::JU06A, false);
    } else if (state == QLatin1String("flip-hint")) {
        // Believing 60 with 106 remembering 4-5: the unit's own flip re-sends 4-5, which the app
        // takes as a panel press on 60 and hints that the switch may now be on 106.
        backend.injectReceived(QByteArray::fromHex("c31c"));  // Program Change 28 on channel 4
        QApplication::processEvents();
    }
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
