#include "app/Cli.h"

#include "core/PatchAddress.h"
#include "midi/MidiBackend.h"
#include "midi/MidiLink.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

namespace d05::Cli {

namespace {

QString hex(const QByteArray &b)
{
    return QString::fromLatin1(b.toHex(' ').toUpper());
}

}  // namespace

ParseStatus parse(const QStringList &args, CliOptions *opts, QString *message, int *exitCode)
{
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Select patches on a Roland Boutique D-05 over MIDI."));
    const QCommandLineOption help = p.addHelpOption();
    const QCommandLineOption version = p.addVersionOption();
    const QCommandLineOption listPorts(QStringLiteral("list-ports"),
                                       QStringLiteral("Print MIDI inputs and outputs, exit 0."));
    const QCommandLineOption port(QStringLiteral("port"),
                                  QStringLiteral("Output port (substring match, first hit); overrides the saved one."),
                                  QStringLiteral("name"));
    const QCommandLineOption inPort(QStringLiteral("in-port"),
                                    QStringLiteral("Input port, same rules."), QStringLiteral("name"));
    const QCommandLineOption channel(QStringLiteral("channel"), QStringLiteral("MIDI channel 1-16."),
                                     QStringLiteral("n"));
    const QCommandLineOption send(QStringLiteral("send"),
                                  QStringLiteral("Headless: send ADDR (e.g. P3-45), print the bytes in hex, exit "
                                                 "(0 sent, 1 bad address, 2 port not found, 3 MIDI error)."),
                                  QStringLiteral("addr"));
    const QCommandLineOption fake(QStringLiteral("fake-midi"),
                                  QStringLiteral("Use the fake backend (ports \"Fake D-05 In/Out\"); bytes go to stderr."));
    const QCommandLineOption configDir(QStringLiteral("config-dir"),
                                       QStringLiteral("Directory for library.json and settings."),
                                       QStringLiteral("dir"));
    const QCommandLineOption shot(QStringLiteral("screenshot"),
                                  QStringLiteral("Not available yet (milestone 3)."), QStringLiteral("file"));
    const QCommandLineOption shotState(QStringLiteral("screenshot-state"),
                                       QStringLiteral("Not available yet (milestone 3)."), QStringLiteral("state"));
    p.addOptions({listPorts, port, inPort, channel, send, fake, configDir, shot, shotState});

    auto fail = [&](const QString &text) {
        *message = QCoreApplication::applicationName() + QStringLiteral(": ") + text + QLatin1Char('\n');
        *exitCode = Exit::Usage;
        return ParseStatus::ExitNow;
    };

    // A help or version request wins over unrelated parse errors, but not over unknown options.
    const bool parsed = p.parse(args);
    const bool infoRequested = p.isSet(help) || p.isSet(QStringLiteral("help-all")) || p.isSet(version);
    if (!parsed && !(infoRequested && p.unknownOptionNames().isEmpty()))
        return fail(p.errorText());
    if (p.isSet(help) || p.isSet(QStringLiteral("help-all"))) {
        *message = p.helpText().replace(QLatin1String("<executable_name>"), QCoreApplication::applicationName());
        *exitCode = Exit::Ok;
        return ParseStatus::ExitNow;
    }
    if (p.isSet(version)) {
        *message = QCoreApplication::applicationName() + QLatin1Char(' ')
                   + QCoreApplication::applicationVersion() + QLatin1Char('\n');
        *exitCode = Exit::Ok;
        return ParseStatus::ExitNow;
    }
    if (p.isSet(shot) || p.isSet(shotState))
        return fail(QStringLiteral("--screenshot and --screenshot-state are not available yet"));
    if (!p.positionalArguments().isEmpty())
        return fail(QStringLiteral("unexpected argument '%1'").arg(p.positionalArguments().first()));

    CliOptions o;
    o.listPorts = p.isSet(listPorts);
    o.fakeMidi = p.isSet(fake);
    o.port = p.value(port);
    o.inPort = p.value(inPort);
    o.configDir = p.value(configDir);
    if (p.isSet(channel)) {
        bool ok = false;
        const int c = p.value(channel).toInt(&ok);
        if (!ok || c < 1 || c > 16)
            return fail(QStringLiteral("--channel must be 1 to 16"));
        o.channel = c;
    }
    if (p.isSet(send)) {
        o.hasSend = true;
        o.sendAddress = p.value(send);
    }
    *opts = o;
    return ParseStatus::Run;
}

int runListPorts(MidiBackend &backend, QTextStream &out, QTextStream &err)
{
    QString e;
    const QStringList outs = backend.outputPortNames(&e);
    if (!e.isEmpty()) {
        err << "MIDI error: " << e << Qt::endl;
        return Exit::MidiError;
    }
    const QStringList ins = backend.inputPortNames(&e);
    if (!e.isEmpty()) {
        err << "MIDI error: " << e << Qt::endl;
        return Exit::MidiError;
    }
    out << "MIDI outputs: " << outs.size() << '\n';
    for (int i = 0; i < outs.size(); ++i)
        out << "  [" << i << "] " << outs.at(i) << '\n';
    out << "MIDI inputs: " << ins.size() << '\n';
    for (int i = 0; i < ins.size(); ++i)
        out << "  [" << i << "] " << ins.at(i) << '\n';
    out.flush();
    return Exit::Ok;
}

int runSend(const CliOptions &opts, MidiBackend &backend, QTextStream &out, QTextStream &err)
{
    const auto address = PatchAddress::parse(opts.sendAddress);
    if (!address) {
        err << "Bad patch address '" << opts.sendAddress << "' (expected e.g. P3-45 or U1-11; digits 1-8)"
            << Qt::endl;
        return Exit::BadAddress;
    }

    QString e;
    const QStringList names = backend.outputPortNames(&e);
    if (!e.isEmpty()) {
        err << "MIDI error: " << e << Qt::endl;
        return Exit::MidiError;
    }
    QString wanted = opts.port;
    if (wanted.isEmpty() && opts.fakeMidi && !names.isEmpty())
        wanted = names.first();
    if (wanted.isEmpty()) {
        err << "No output port given; use --port NAME (see --list-ports)" << Qt::endl;
        return Exit::PortNotFound;
    }
    const QString match = MidiLink::matchPort(names, wanted);
    if (match.isEmpty()) {
        err << "Output port not found: " << wanted << Qt::endl;
        return Exit::PortNotFound;
    }

    MidiLink link(&backend);
    link.setChannel(opts.channel > 0 ? opts.channel : 1);
    link.setOutputPort(match);
    if (!link.isConnected()) {
        err << "MIDI error: " << link.status() << Qt::endl;
        return Exit::MidiError;
    }
    if (!link.sendPatch(*address)) {
        err << "MIDI error: " << link.status() << Qt::endl;
        return Exit::MidiError;
    }
    for (const QByteArray &m : MidiLink::patchMessages(*address, link.channel()))
        out << hex(m) << '\n';
    out.flush();
    return Exit::Ok;
}

}  // namespace d05::Cli
