#include "app/Cli.h"

#include "core/Identify.h"
#include "core/PatchAddress.h"
#include "midi/MidiBackend.h"
#include "midi/MidiLink.h"
#include "midi/PortHub.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QTextStream>

#include <memory>

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
                                                 "(0 sent, 1 bad address, 2 port not found, 3 MIDI error, 5 JU-06A engine mismatch). "
                                                 "Address forms: P3-45 (D-05), 4-5 (JP-08), 106:4-5 (JU-06A)."),
                                  QStringLiteral("addr"));
    const QCommandLineOption fake(QStringLiteral("fake-midi"),
                                  QStringLiteral("Use the fake backend (ports \"Fake D-05 In/Out\"); bytes go to stderr."));
    const QCommandLineOption fakeClock(QStringLiteral("fake-clock"),
                                       QStringLiteral("With --fake-midi: three fake devices' ports (D-05, JP-08, JU-06A), "
                                                      "each input emitting MIDI clock at 120 BPM (footprint testing)."));
    const QCommandLineOption configDir(QStringLiteral("config-dir"),
                                       QStringLiteral("Directory for library.json and settings."),
                                       QStringLiteral("dir"));
    const QCommandLineOption shot(QStringLiteral("screenshot"),
                                  QStringLiteral("Render the main window offscreen to FILE.png and exit."),
                                  QStringLiteral("file"));
    const QCommandLineOption shotState(QStringLiteral("screenshot-state"),
                                       QStringLiteral("State for --screenshot: default, search, favorites, recents, "
                                                      "disconnected, settings, nomatch, entry, rename, "
                                                      "clearrecents, settings-error, shortcuts, about, firstrun, jp08, ju06a-60, "
                                                      "ju06a-106, flip-prompt, flip-hint, devices or settings-devices."),
                                       QStringLiteral("state"));
    const QCommandLineOption shotSize(QStringLiteral("screenshot-size"),
                                      QStringLiteral("Window size for --screenshot, WxH (default 960x560, "
                                                     "minimum 800x480). Dialog states ignore it."),
                                      QStringLiteral("WxH"));
    const QCommandLineOption device(QStringLiteral("device"),
                                    QStringLiteral("Instrument for --send: d05 (default), jp08 or ju06a. "
                                                   "--port, --in-port and --channel apply to it."),
                                    QStringLiteral("model"));
    const QCommandLineOption engine(QStringLiteral("engine"),
                                    QStringLiteral("JU-06A only: the engine the unit is on, 60 or 106."),
                                    QStringLiteral("60|106"));
    const QCommandLineOption identify(QStringLiteral("identify"),
                                      QStringLiteral("Print MIDI ports with the detected Boutique model, exit 0."));
    p.addOptions({listPorts, port, inPort, channel, send, fake, fakeClock, configDir, shot, shotState, shotSize, device,
                  engine, identify});

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
    if (p.isSet(shot) && (p.isSet(listPorts) || p.isSet(send) || p.isSet(identify)))
        return fail(QStringLiteral("--screenshot cannot be combined with --list-ports, --identify or --send"));
    if (!p.positionalArguments().isEmpty())
        return fail(QStringLiteral("unexpected argument '%1'").arg(p.positionalArguments().first()));

    CliOptions o;
    o.listPorts = p.isSet(listPorts);
    o.identify = p.isSet(identify);
    if (p.isSet(device)) {
        const auto m = modelFromId(p.value(device).toLower());
        if (!m)
            return fail(QStringLiteral("--device must be d05, jp08 or ju06a"));
        o.device = *m;
        o.hasDevice = true;
    }
    if (p.isSet(engine)) {
        if (o.device != Model::JU06A)
            return fail(QStringLiteral("--engine applies only to --device ju06a"));
        const QString v = p.value(engine);
        if (v != QLatin1String("60") && v != QLatin1String("106"))
            return fail(QStringLiteral("--engine must be 60 or 106"));
        o.engine = v.toInt();
    }
    o.fakeMidi = p.isSet(fake);
    o.fakeClock = p.isSet(fakeClock);
    if (o.fakeClock && !o.fakeMidi)
        return fail(QStringLiteral("--fake-clock needs --fake-midi"));
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
        // The channel may come from the saved settings: runSend() checks it (--list-ports and --identify
        // win over --send in main.cpp and need none).
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
            QStringLiteral("clearrecents"), QStringLiteral("settings-error"), QStringLiteral("shortcuts"),
            QStringLiteral("about"),        QStringLiteral("firstrun"),
            // Version 2 (SPEC-2 FR-15, DESIGN-2.md 12).
            QStringLiteral("jp08"),         QStringLiteral("ju06a-60"),     QStringLiteral("ju06a-106"),
            QStringLiteral("flip-prompt"),  QStringLiteral("flip-hint"),    QStringLiteral("devices"),
            QStringLiteral("settings-devices")};
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

int runIdentify(MidiBackend &backend, QTextStream &out, QTextStream &err, bool systemLookup)
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
    const auto section = [&](const char *title, const QStringList &names) {
        const QList<Identify::Result> found = Identify::identifyPorts(names, systemLookup);
        out << title << names.size() << '\n';
        for (int i = 0; i < names.size(); ++i) {
            const Identify::Result &r = found.at(i);
            out << "  [" << i << "] " << names.at(i) << "  ->  ";
            if (!r.model)
                out << "no Boutique recognised";
            else if (r.firstGeneration)
                out << modelInfo(*r.model).name << " (ambiguous: first generation, JP-08 assumed)";
            else
                out << modelInfo(*r.model).name << " (" << Identify::confidenceText(r.confidence) << ')';
            out << '\n';
        }
    };
    section("MIDI outputs: ", outs);
    section("MIDI inputs: ", ins);
    out.flush();
    return Exit::Ok;
}

Startup startup(const AppSettings &saved, const CliOptions &opts)
{
    Startup st;
    for (Model m : allModels())
        st.devices[int(m)] = saved.device(m);
    st.autoSend = saved.autoSend();
    st.followSwitches = saved.followSwitchesDevice();
    st.engine = opts.engine > 0 ? opts.engine : saved.engine();
    st.remembered60 = saved.remembered(60).value_or(-1);
    st.remembered106 = saved.remembered(106).value_or(-1);

    const Model target = opts.hasDevice ? opts.device : saved.currentDevice().value_or(Model::D05);
    AppSettings::DeviceValues &d = st.devices[int(target)];
    bool touched = opts.hasDevice;
    if (opts.channel > 0) {
        d.channel = opts.channel;
        touched = true;
    }
    if (!opts.port.isEmpty()) {
        d.outputPort = opts.port;
        touched = true;
    } else if (opts.fakeMidi) {
        d.outputPort = QStringLiteral("Fake D-05 In");
        touched = true;
    }
    if (!opts.inPort.isEmpty()) {
        d.inputPort = opts.inPort;
        touched = true;
    }
    if (touched)
        d.enabled = true;

    if (opts.hasDevice) {
        st.current = opts.device;
    } else {
        const Model want = saved.currentDevice().value_or(Model::D05);
        st.current = Model::D05;
        if (st.devices[int(want)].enabled) {
            st.current = want;
        } else {
            for (Model m : allModels()) {
                if (st.devices[int(m)].enabled) {
                    st.current = m;
                    break;
                }
            }
        }
    }
    return st;
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
    const Model model = opts.hasDevice ? opts.device : (saved ? saved->currentDevice().value_or(Model::D05) : Model::D05);
    const bool isD05 = model == Model::D05;
    const AppSettings::DeviceValues savedDevice = saved ? saved->device(model) : AppSettings::DeviceValues();
    std::optional<int> engine;
    if (opts.engine > 0)
        engine = opts.engine;
    else if (saved && model == Model::JU06A && saved->hasEngine())
        engine = saved->engine();  // the app's last known engine state

    std::optional<PatchAddress> address = isD05 ? PatchAddress::parse(opts.sendAddress)
                                                : PatchAddress::parse(opts.sendAddress, model, engine);
    if (!address) {
        if (model == Model::JU06A && !engine && PatchAddress::parse(opts.sendAddress, model, 60)) {
            err << "The JU-06A address '" << opts.sendAddress
                << "' does not say which engine; use --engine 60|106 or write 60:4-5 / 106:4-5" << Qt::endl;
        } else {
            err << "Bad patch address '" << opts.sendAddress << "' (expected "
                << (isD05 ? "e.g. P3-45 or U1-11" : model == Model::JP08 ? "e.g. 4-5" : "e.g. 4-5, 60:4-5 or 106:4-5")
                << "; digits 1-8)" << Qt::endl;
        }
        return Exit::BadAddress;
    }
    if (model == Model::JU06A) {
        if (!engine) {
            // No saved engine state either, and the unit cannot be asked.
            err << "The JU-06A engine is unknown; give --engine 60|106" << Qt::endl;
            return Exit::BadAddress;
        }
        if (address->engine() != *engine) {
            err << "Engine mismatch: the address is for the " << address->engine() << " engine, the unit is on "
                << *engine << "; nothing sent" << Qt::endl;
            return Exit::EngineMismatch;
        }
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
        wanted = savedDevice.outputPort;
    int channel = opts.channel;
    if (channel <= 0) {
        if (saved && saved->hasChannel(model))
            channel = savedDevice.channel;
        else if (isD05 || opts.fakeMidi)
            channel = 1;
        else {
            err << "The " << modelInfo(model).name << " has no saved channel; give --channel 1-16" << Qt::endl;
            return Exit::Usage;
        }
    }
    if (wanted.isEmpty()) {
        err << "No output port given; use --port NAME (see --list-ports) or choose one in Settings" << Qt::endl;
        return Exit::PortNotFound;
    }
    const QString match = MidiLink::matchPort(names, wanted);
    if (match.isEmpty()) {
        err << "Output port not found: " << wanted << Qt::endl;
        return Exit::PortNotFound;
    }

    // The D-05 keeps its original single-backend link; the others get a model-aware link on a hub.
    std::unique_ptr<PortHub> hub;
    if (!isD05)
        hub = std::make_unique<PortHub>(&backend);
    std::unique_ptr<MidiLink> link = isD05 ? std::make_unique<MidiLink>(&backend)
                                           : std::make_unique<MidiLink>(hub.get(), model);
    if (model == Model::JU06A)
        link->setEngine(*engine);
    link->setChannel(channel);
    link->setOutputPort(match);
    if (!link->isConnected()) {
        err << "MIDI error: " << link->status() << Qt::endl;
        return Exit::MidiError;
    }
    if (!link->sendPatch(*address)) {
        err << "MIDI error: " << link->status() << Qt::endl;
        return Exit::MidiError;
    }
    const QList<QByteArray> sent = isD05 ? MidiLink::patchMessages(*address, link->channel())
                                         : address->midiMessages(link->channel(), engine);
    for (const QByteArray &m : sent)
        out << hex(m) << '\n';
    out.flush();
    return Exit::Ok;
}

}  // namespace d05::Cli
