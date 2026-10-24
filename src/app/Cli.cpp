#include "app/Cli.h"

#include "core/PatchAddress.h"
#include "midi/MidiBackend.h"
#include "midi/MidiLink.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QRegularExpression>
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
                                  QStringLiteral("Render the main window offscreen to FILE.png and exit."),
                                  QStringLiteral("file"));
    const QCommandLineOption shotState(QStringLiteral("screenshot-state"),
                                       QStringLiteral("State for --screenshot: default, search, favorites, recents, "
                                                      "disconnected, settings, nomatch, entry, rename, "
                                                      "clearrecents or settings-error."),
                                       QStringLiteral("state"));
    const QCommandLineOption shotSize(QStringLiteral("screenshot-size"),
                                      QStringLiteral("Window size for --screenshot, WxH (default 960x560, "
                                                     "minimum 800x480). Dialog states ignore it."),
                                      QStringLiteral("WxH"));
    p.addOptions({listPorts, port, inPort, channel, send, fake, configDir, shot, shotState, shotSize});

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
    if (p.isSet(shotState) && !p.isSet(shot))
        return fail(QStringLiteral("--screenshot-state needs --screenshot FILE.png"));
    if (p.isSet(shotSize) && !p.isSet(shot))
        return fail(QStringLiteral("--screenshot-size needs --screenshot FILE.png"));
    if (p.isSet(shot) && (p.isSet(listPorts) || p.isSet(send)))
        return fail(QStringLiteral("--screenshot cannot be combined with --list-ports or --send"));
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
    if (p.isSet(shot)) {
        o.screenshotFile = p.value(shot);
        o.screenshotState = p.isSet(shotState) ? p.value(shotState) : QStringLiteral("default");
        if (o.screenshotFile.isEmpty())
            return fail(QStringLiteral("--screenshot needs a file name"));
        if (!screenshotStates().contains(o.screenshotState))
            return fail(QStringLiteral("--screenshot-state must be one of: %1")
                            .arg(screenshotStates().join(QStringLiteral(", "))));
    }
    if (p.isSet(shotSize)) {
        static const QRegularExpression re(QStringLiteral("^(\\d{1,5})x(\\d{1,5})$"));
        const QRegularExpressionMatch m = re.match(p.value(shotSize));
        const int w = m.hasMatch() ? m.captured(1).toInt() : 0;
        const int h = m.hasMatch() ? m.captured(2).toInt() : 0;
        if (!m.hasMatch() || w < 800 || h < 480 || w > 8000 || h > 8000)
            return fail(QStringLiteral("--screenshot-size must be WxH, at least 800x480 and at most 8000x8000"));
        o.screenshotSize = QSize(w, h);
    }
    if (p.isSet(send)) {
        o.hasSend = true;
        o.sendAddress = p.value(send);
    }
    *opts = o;
    return ParseStatus::Run;
}

QStringList screenshotStates()
{
    return {QStringLiteral("default"),      QStringLiteral("search"),   QStringLiteral("favorites"),
            QStringLiteral("recents"),      QStringLiteral("disconnected"), QStringLiteral("settings"),
            QStringLiteral("nomatch"),      QStringLiteral("entry"),        QStringLiteral("rename"),
            QStringLiteral("clearrecents"), QStringLiteral("settings-error")};
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

AppSettings::Values startupValues(const AppSettings &saved, const CliOptions &opts)
{
    AppSettings::Values v = saved.values();
    if (opts.channel > 0)
        v.channel = opts.channel;
    if (!opts.port.isEmpty())
        v.outputPort = opts.port;
    else if (opts.fakeMidi)
        v.outputPort = QStringLiteral("Fake D-05 In");
    if (!opts.inPort.isEmpty())
        v.inputPort = opts.inPort;
    return v;
}

int runSend(const CliOptions &opts, MidiBackend &backend, QTextStream &out, QTextStream &err,
            const AppSettings *saved)
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
    else if (wanted.isEmpty() && !opts.fakeMidi && saved)
        wanted = saved->outputPort();
    const int channel = opts.channel > 0 ? opts.channel : (saved ? saved->channel() : 1);
    if (wanted.isEmpty()) {
        err << "No output port given; use --port NAME (see --list-ports) or choose one in Settings" << Qt::endl;
        return Exit::PortNotFound;
    }
    const QString match = MidiLink::matchPort(names, wanted);
    if (match.isEmpty()) {
        err << "Output port not found: " << wanted << Qt::endl;
        return Exit::PortNotFound;
    }

    MidiLink link(&backend);
    link.setChannel(channel);
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
