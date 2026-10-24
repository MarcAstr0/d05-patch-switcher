// Entry point. Milestone 1 grows this into the real application (docs/PLAN.md).

#include "core/PatchCatalog.h"
#include "midi/PortLister.h"
#include "ui/ScaffoldWindow.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("d05-patch-switcher"));
    QApplication::setApplicationDisplayName(QStringLiteral("D-05 Patch Switcher"));
    QApplication::setOrganizationName(QStringLiteral("marcastr0"));
    QApplication::setApplicationVersion(QStringLiteral(D05_VERSION));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption listPortsOpt(QStringLiteral("list-ports"),
                                          QStringLiteral("List MIDI ports and exit."));
    parser.addOption(listPortsOpt);
    parser.process(app);

    if (parser.isSet(listPortsOpt))
        return d05::listPorts();

    const d05::PatchCatalog catalog;
    d05::ScaffoldWindow window(catalog.isValid() ? catalog.presetCount() : -1);
    window.show();
    return app.exec();
}
