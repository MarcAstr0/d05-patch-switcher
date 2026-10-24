// Entry point. Arguments are parsed before any application object exists, so
// help, version and usage errors never touch a display. Headless runs
// (--list-ports, --send) use a QCoreApplication: no window, no library file.

#include "app/Cli.h"
#include "app/Paths.h"
#include "app/Screenshot.h"
#include "core/PatchCatalog.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiBackend.h"
#include "midi/RtMidiBackend.h"
#include "ui/ScaffoldWindow.h"
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
        return d05::Cli::runSend(opts, *backend, out, err);
    }

    QApplication app(argc, argv);
    QGuiApplication::setApplicationDisplayName(QStringLiteral("D-05 Patch Switcher"));
    d05::Theme::install(app);
    if (!opts.configDir.isEmpty())
        d05::Paths::setOverride(opts.configDir);
    if (opts.wantsScreenshot())
        return d05::runScreenshot(opts);
    // M3 wires the MIDI backend (--fake-midi or RtMidi) into the main window; the
    // scaffold creates none, so no ALSA client appears at start.

    const d05::PatchCatalog catalog;
    d05::ScaffoldWindow window(catalog.isValid() ? catalog.presetCount() : -1);
    window.show();
    return app.exec();
}
