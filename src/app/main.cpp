// Entry point. Headless paths (--list-ports, --send) run under a QCoreApplication
// and never create a window or touch the library file.

#include "app/Cli.h"
#include "app/Paths.h"
#include "core/PatchCatalog.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiBackend.h"
#include "midi/RtMidiBackend.h"
#include "ui/ScaffoldWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTextStream>

#include <cstdio>
#include <memory>

int main(int argc, char *argv[])
{
    std::unique_ptr<QCoreApplication> app;
    {
        QStringList raw;
        for (int i = 0; i < argc; ++i)
            raw << QString::fromLocal8Bit(argv[i]);
        if (d05::Cli::wantsHeadless(raw))
            app = std::make_unique<QCoreApplication>(argc, argv);
        else
            app = std::make_unique<QApplication>(argc, argv);
    }
    QCoreApplication::setApplicationName(QStringLiteral("d05-patch-switcher"));
    QCoreApplication::setOrganizationName(QStringLiteral("marcastr0"));
    QCoreApplication::setApplicationVersion(QStringLiteral(D05_VERSION));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("D-05 Patch Switcher"));

    QTextStream out(stdout);
    QTextStream err(stderr);

    d05::CliOptions opts;
    QString message;
    int code = 0;
    if (d05::Cli::parse(QCoreApplication::arguments(), &opts, &message, &code) == d05::Cli::ParseStatus::ExitNow) {
        (code == 0 ? out : err) << message;
        return code;
    }
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
    if (opts.hasSend)
        return d05::Cli::runSend(opts, *backend, out, err);

    const d05::PatchCatalog catalog;
    d05::ScaffoldWindow window(catalog.isValid() ? catalog.presetCount() : -1);
    window.show();
    return app->exec();
}
