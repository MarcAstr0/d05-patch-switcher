#include "app/Screenshot.h"

#include "app/Paths.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "core/SearchQuery.h"
#include "midi/FakeMidiBackend.h"
#include "ui/DisplayWidget.h"

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTextStream>

#include <memory>

namespace d05 {

namespace {

constexpr int kWidth = 960;
constexpr int kHeight = 560;

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

// A minimal stand-in for the main window: the display in its place and an empty
// band. T3.2 to T3.4 replace it with the real layout.
class PlaceholderWindow : public QMainWindow {
public:
    PlaceholderWindow(const QString &state, DisplayWidget **displayOut)
    {
        setWindowTitle(QStringLiteral("D-05 Patch Switcher"));
        auto *central = new QWidget(this);
        setCentralWidget(central);
        auto *display = new DisplayWidget(central);
        display->setPitch(DisplayWidget::pitchForWidth(kWidth));
        display->move(16, 46);
        const int bandTop = 46 + display->height() + 14;
        auto *band = new QFrame(central);
        band->setObjectName(QStringLiteral("band"));
        band->setGeometry(0, bandTop, kWidth, kHeight - bandTop);
        auto *label = new QLabel(QStringLiteral("Placeholder, state: %1").arg(state), band);
        label->move(16, 16);
        resize(kWidth, kHeight);
        *displayOut = display;
    }
};

}  // namespace

int runScreenshot(const CliOptions &opts)
{
    QTextStream err(stderr);
    const QString &state = opts.screenshotState;

    std::unique_ptr<QTemporaryDir> tmp;
    if (opts.configDir.isEmpty()) {
        tmp = std::make_unique<QTemporaryDir>();
        if (tmp->isValid())
            Paths::setOverride(tmp->path());
    }
    PatchCatalog catalog;
    Library library(catalog, Paths::libraryPath());
    if (tmp && tmp->isValid()) {
        fillCannedLibrary(library);
    } else {
        library.load();
    }

    FakeMidiBackend backend;
    QString error;
    const bool connected = state != QLatin1String("disconnected")
                           && backend.openOutput(QStringLiteral("Fake D-05 In"), &error);

    PatchAddress current = *PatchAddress::parse(QStringLiteral("P1-11"));
    if (state == QLatin1String("recents") && !library.recents().isEmpty())
        current = library.recents().first().address;

    DisplayWidget *display = nullptr;
    PlaceholderWindow window(state, &display);
    display->setPatch(current, catalog.name(current), catalog.mode(current));
    display->setConnected(connected);
    if (state == QLatin1String("search")) {
        const SearchQuery query(QStringLiteral("brass"));
        int n = 0;
        for (int i = 0; i < PatchAddress::CatalogueSize; ++i) {
            const PatchAddress a = *PatchAddress::fromCatalogueIndex(i);
            if (query.matches(a, catalog.name(a)))
                ++n;
        }
        display->setSearch(true, n);
    }
    // "settings" has no dialog yet (T3.4): the main window renders as the default state.

    window.show();
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
