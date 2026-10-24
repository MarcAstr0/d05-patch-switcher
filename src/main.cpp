// Scaffold entry point. Milestone 1 replaces this with the real application
// (see docs/PLAN.md). It exists so the build, the resource embedding and the
// RtMidi link are proven on every platform from day one.

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QLabel>
#include <QMainWindow>
#include <QTextStream>

#include <RtMidi.h>

#include <cstdio>
#include <memory>

static int listPorts()
{
    try {
        RtMidiOut out;
        const unsigned n = out.getPortCount();
        std::printf("MIDI outputs: %u\n", n);
        for (unsigned i = 0; i < n; ++i)
            std::printf("  [%u] %s\n", i, out.getPortName(i).c_str());
        RtMidiIn in;
        const unsigned m = in.getPortCount();
        std::printf("MIDI inputs: %u\n", m);
        for (unsigned i = 0; i < m; ++i)
            std::printf("  [%u] %s\n", i, in.getPortName(i).c_str());
    } catch (const RtMidiError &e) {
        std::fprintf(stderr, "RtMidi error: %s\n", e.getMessage().c_str());
        return 1;
    }
    return 0;
}

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
        return listPorts();

    // Prove the embedded preset table is reachable.
    QFile presets(QStringLiteral(":/data/d05-presets.tsv"));
    int rows = -1; // minus the header
    if (presets.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&presets);
        while (!ts.readLine().isNull())
            ++rows;
    }

    QMainWindow window;
    auto *label = new QLabel(
        QStringLiteral("Scaffold build. %1 preset patches embedded. See docs/PLAN.md.").arg(rows));
    label->setMargin(24);
    window.setCentralWidget(label);
    window.setWindowTitle(QStringLiteral("D-05 Patch Switcher"));
    window.show();
    return app.exec();
}
