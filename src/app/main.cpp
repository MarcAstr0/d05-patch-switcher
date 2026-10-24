// Entry point. Arguments are parsed before any application object exists, so
// help, version and usage errors never touch a display. Headless runs
// (--list-ports, --send) use a QCoreApplication: no window, no library file.

#include "app/AppSettings.h"
#include "app/Cli.h"
#include "app/Paths.h"
#include "app/Screenshot.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiBackend.h"
#include "midi/RtMidiBackend.h"
#include "midi/MidiLink.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCoreApplication>
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
        // --send falls back to the saved port and channel when --port / --channel are absent.
        const d05::AppSettings saved;
        return d05::Cli::runSend(opts, *backend, out, err, &saved);
    }

    // Screenshots never need a screen: pick the offscreen platform unless the caller chose one.
    if (opts.wantsScreenshot() && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QGuiApplication::setApplicationDisplayName(QStringLiteral("D-05 Patch Switcher"));
    const bool themeOk = d05::Theme::install(app);
    if (!themeOk)
        QTextStream(stderr) << "warning: a font or the style sheet could not be loaded from the resources\n";
    if (!opts.configDir.isEmpty())
        d05::Paths::setOverride(opts.configDir);
    if (opts.wantsScreenshot()) {
        const int rc = d05::runScreenshot(opts);
        return rc != 0 ? rc : (themeOk ? 0 : 1);
    }
    // Backend, link and library outlive the window (declared first, destroyed last);
    // the library flushes pending changes in its destructor.
    std::unique_ptr<d05::MidiBackend> backend;
    if (opts.fakeMidi) {
        auto fake = std::make_unique<d05::FakeMidiBackend>();
        fake->setPrintSent(true);  // FR-15: bytes to stderr
        backend = std::move(fake);
    } else
        backend = std::make_unique<d05::RtMidiBackend>();
    d05::PatchCatalog catalog;
    d05::Library library(catalog, d05::Paths::libraryPath());
    const d05::Library::LoadResult loaded = library.load();
    d05::MidiLink link(backend.get());
    d05::AppSettings settings;
    // The window connects to the link first, so a port error at startup shows its text.
    d05::MainWindow window(catalog, library, link);
    window.setServices(&settings, backend.get());
    // The patch the app last showed comes back without sending anything, so resend-on-connect
    // resends the right one. Then the saved settings take effect; --port, --in-port and
    // --channel override them for this run only and are never written back.
    if (const auto last = settings.lastPatch())
        window.setCurrent(*last);
    window.applySettings(d05::Cli::startupValues(settings, opts));
    window.restoreGeometry(settings.windowGeometry());
    if (loaded == d05::Library::LoadResult::Recovered)
        window.showMessage(QStringLiteral("Library file was damaged; started a fresh one"));
    else if (loaded == d05::Library::LoadResult::Unreadable || loaded == d05::Library::LoadResult::SidelineFailed)
        window.showMessage(QStringLiteral("Library file could not be read; changes will not be saved"));
    window.show();
    return app.exec();
}
