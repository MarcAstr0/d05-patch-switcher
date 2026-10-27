// Entry point. Arguments are parsed before any application object exists, so
// help, version and usage errors never touch a display. Headless runs
// (--list-ports, --identify, --send) use a QCoreApplication: no window, no library file.

#include "app/AppSettings.h"
#include "app/Cli.h"
#include "app/Paths.h"
#include "app/Screenshot.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "midi/DeviceRouter.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiBackend.h"
#include "midi/RtMidiBackend.h"
#include "midi/MidiLink.h"
#include "midi/PortHub.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QTextStream>

#include <cstdio>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

QStringList rawArguments(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    QStringList list;
    int n = 0;
    if (LPWSTR *w = CommandLineToArgvW(GetCommandLineW(), &n)) {
        for (int i = 0; i < n; ++i)
            list << QString::fromWCharArray(w[i]);
        LocalFree(w);
    }
    return list;
#else
    QStringList list;
    for (int i = 0; i < argc; ++i)
        list << QString::fromLocal8Bit(argv[i]);
    return list;
#endif
}

// The exe is a GUI-subsystem program on Windows. For each std stream that was
// not inherited (no handle: started from Explorer or a console, not redirected),
// hook up the parent console so help, --list-ports and --send output is visible.
// Streams redirected to a pipe or file (QProcess, `> out.txt`) are left alone.
// Exit codes still need `start /wait` in cmd.exe.
void attachConsole()
{
#ifdef Q_OS_WIN
    auto usable = [](DWORD which) {
        const HANDLE h = GetStdHandle(which);
        return h && h != INVALID_HANDLE_VALUE && GetFileType(h) != FILE_TYPE_UNKNOWN;
    };
    const bool needOut = !usable(STD_OUTPUT_HANDLE);
    const bool needErr = !usable(STD_ERROR_HANDLE);
    if (!needOut && !needErr)
        return;
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    if (needOut)
        std::freopen("CONOUT$", "w", stdout);
    if (needErr)
        std::freopen("CONOUT$", "w", stderr);
#endif
}

// Measurement aid for scripts/measure-footprint.sh: when D05_FIRST_PAINT_STAMP is set, prints
// the wall-clock time of the window's first paint to stderr, once. Not installed otherwise.
class FirstPaintStamp : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Paint) {
            QTextStream(stderr) << "first-paint-ms " << QDateTime::currentMSecsSinceEpoch() << "\n";
            watched->removeEventFilter(this);
        }
        return false;
    }
};

}  // namespace

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(QStringLiteral("d05-patch-switcher"));
    QCoreApplication::setOrganizationName(QStringLiteral("marcastr0"));
    QCoreApplication::setApplicationVersion(QStringLiteral(D05_VERSION));

    d05::CliOptions opts;
    QString message;
    int code = 0;
    const bool exitNow =
        d05::Cli::parse(rawArguments(argc, argv), &opts, &message, &code) == d05::Cli::ParseStatus::ExitNow;

    if (exitNow || opts.headless()) {
        attachConsole();
        QTextStream out(stdout);
        QTextStream err(stderr);
        if (exitNow) {
            (code == 0 ? out : err) << message;
            return code;
        }
        QCoreApplication app(argc, argv);
        if (!opts.configDir.isEmpty())
            d05::Paths::setOverride(opts.configDir);
        std::unique_ptr<d05::MidiBackend> backend;
        if (opts.fakeMidi) {
            auto fake = std::make_unique<d05::FakeMidiBackend>();
            fake->setPrintSent(true);
            backend = std::move(fake);
        } else {
            backend = std::make_unique<d05::RtMidiBackend>();
        }
        if (opts.listPorts)
            return d05::Cli::runListPorts(*backend, out, err);
        if (opts.identify)  // the fake backend's names are not ALSA clients: rule 1 only
            return d05::Cli::runIdentify(*backend, out, err, !opts.fakeMidi);
        // --send falls back to the saved port and channel when --port / --channel are absent.
        if (d05::Paths::migrateLegacyDataDir() == d05::Paths::MigrateResult::Failed) {
            err << "warning: could not copy the version 1 settings to the new data directory; "
                   "using none this run, will retry next start\n";
            d05::Paths::useScratchDataDir();
        }
        const d05::AppSettings saved;
        return d05::Cli::runSend(opts, *backend, out, err, &saved);
    }

    // Screenshots never need a screen: pick the offscreen platform unless the caller chose one.
    if (opts.wantsScreenshot() && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    // A blinking text caret in the focused search box wakes the event loop twice a second for as
    // long as the window is active (SPEC section 5: no timer under 2 s). Draw it steady.
    QApplication::setCursorFlashTime(0);
    QGuiApplication::setApplicationDisplayName(QStringLiteral("D-05 Patch Switcher"));
    QGuiApplication::setDesktopFileName(QStringLiteral("d05-patch-switcher"));
    const bool themeOk = d05::Theme::install(app);
    if (!themeOk)
        QTextStream(stderr) << "warning: a font or the style sheet could not be loaded from the resources\n";
    if (!opts.configDir.isEmpty())
        d05::Paths::setOverride(opts.configDir);
    if (opts.wantsScreenshot()) {
        const int rc = d05::runScreenshot(opts);
        return rc != 0 ? rc : (themeOk ? 0 : 1);
    }
    // The hub, the router and the library outlive the window (declared first, destroyed last); the
    // library flushes pending changes in its destructor. The hub makes one backend per OS port, as
    // many as the enabled devices need (SPEC-2 section 7).
    const bool fake = opts.fakeMidi;
    d05::PortHub hub([fake]() -> d05::MidiBackend * {
        try {
            if (fake) {
                auto *f = new d05::FakeMidiBackend;
                f->setPrintSent(true);  // FR-15: bytes to stderr
                return f;
            }
            return new d05::RtMidiBackend;
        } catch (...) {
            return nullptr;  // the hub reports it as a port error
        }
    });
    // First start after the rename: copy, never move. On failure this session uses a throwaway
    // directory so nothing blocks the retry on the next start.
    const bool migrationFailed =
        d05::Paths::migrateLegacyDataDir() == d05::Paths::MigrateResult::Failed;
    if (migrationFailed)
        d05::Paths::useScratchDataDir();
    d05::PatchCatalog catalog;
    d05::Library library(catalog, d05::Paths::libraryPath());
    const d05::Library::LoadResult loaded = library.load();
    d05::DeviceRouter router(&hub);
    d05::AppSettings settings;
    d05::MainWindow window(catalog, library, router);
    // The fake backend's ports are not the user's: keep them out of the real settings file, and its
    // names are not ALSA clients for the detection to look up.
    window.setPersistPorts(!(opts.fakeMidi && opts.configDir.isEmpty()));
    window.setSystemLookup(!opts.fakeMidi);
    // The patch each device last showed comes back without sending anything, so resend-on-connect
    // resends the right one. Then the saved settings take effect; --device, --port, --in-port and
    // --channel override them for this run only and are never written back. The window is not yet
    // given the settings, so nothing of this is saved.
    if (const auto last = settings.lastPatch())
        window.setCurrent(*last);
    for (const d05::Model m : {d05::Model::JP08, d05::Model::JU06A})
        if (const auto last = settings.lastPatch(m))
            window.setCurrent(*last);  // the JU-06A's own current patch comes with its engine state below
    const d05::Cli::Startup start = d05::Cli::startup(settings, opts);
    router.setJu06aState(start.engine, start.remembered60, start.remembered106);
    window.applyGlobal(start.autoSend, start.followSwitches);
    for (const d05::Model m : d05::allModels()) {
        window.applyDevice(m, start.devices[int(m)]);
        if (start.devices[int(m)].enabled && !settings.device(m).enabled)
            window.markRunOnly(m);  // turned on by a flag for this run: quitting writes nothing about it
    }
    window.switchDevice(start.current);
    window.setServices(&settings);
    window.restoreGeometry(settings.windowGeometry());
    if (loaded == d05::Library::LoadResult::Recovered)
        window.showMessage(QStringLiteral("Library file was damaged; started a fresh one"));
    else if (loaded == d05::Library::LoadResult::Unreadable || loaded == d05::Library::LoadResult::SidelineFailed)
        window.showMessage(QStringLiteral("Library file could not be read; changes will not be saved"));
    if (migrationFailed)
        window.showMessage(QStringLiteral("Could not copy your earlier library and settings; changes are not kept this run"));
    FirstPaintStamp paintStamp;
    if (qEnvironmentVariableIsSet("D05_FIRST_PAINT_STAMP"))
        window.installEventFilter(&paintStamp);
    window.show();
    return app.exec();
}
