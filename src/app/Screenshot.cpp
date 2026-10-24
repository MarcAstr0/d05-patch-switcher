#include "app/Screenshot.h"

#include "app/Paths.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "core/SearchQuery.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiLink.h"
#include "ui/DisplayWidget.h"
#include "ui/MainWindow.h"
#include "ui/PanelButton.h"
#include "ui/PatchListView.h"
#include "ui/SettingsDialog.h"

#include <QApplication>
#include <QLineEdit>
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

int runScreenshot(const CliOptions &opts)
{
    QTextStream err(stderr);
    const QString &state = opts.screenshotState;

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
    MidiLink link(&backend);
    link.setChannel(1);
    // "disconnected" asks for a port the fake does not have, as the mockup's "Hapax MIDI 1".
    const bool disconnected = state == QLatin1String("disconnected");
    link.setOutputPort(disconnected ? QStringLiteral("Hapax MIDI 1") : QStringLiteral("Fake D-05 In"));

    PatchAddress current = *PatchAddress::parse(QStringLiteral("P1-11"));
    if (state == QLatin1String("recents") && !library.recents().isEmpty())
        current = library.recents().first().address;

    MainWindow window(catalog, library, link);
    window.setCurrent(current);
    if (state == QLatin1String("search")) {
        window.searchField()->setText(QStringLiteral("brass"));
    } else if (state == QLatin1String("favorites")) {
        window.viewButton(PatchListModel::View::Favourites)->setChecked(true);
    } else if (state == QLatin1String("recents")) {
        window.viewButton(PatchListModel::View::Recents)->setChecked(true);
    } else if (disconnected) {
        window.list()->setFocus();  // the mockup's second frame has the list focused
    }

    if (state == QLatin1String("settings")) {
        // The dialog alone, as in docs/design/mockup-settings.png: the fake output open, no
        // input, channel 1, resend-on-connect on.
        AppSettings::Values values;
        values.outputPort = QStringLiteral("Fake D-05 In");
        values.resendOnConnect = true;
        SettingsDialog dialog(link, values);
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

    window.show();
    window.activateWindow();
    QApplication::processEvents();
    const QPixmap shot = window.grab();
    if (!shot.save(opts.screenshotFile, "PNG")) {
        err << "cannot write " << opts.screenshotFile << Qt::endl;
        return 1;
    }
    library.flush();
    return Exit::Ok;
}

}  // namespace d05
