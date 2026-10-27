#include "app/AppSettings.h"

#include "app/Paths.h"
#include "midi/MidiLink.h"

#include <QDir>
#include <QFileInfo>

namespace d05 {

namespace {
// Version 1 keys (kept readable, never deleted).
const QString kOutput = QStringLiteral("midi/outputPort");
const QString kInput = QStringLiteral("midi/inputPort");
const QString kChannel = QStringLiteral("midi/channel");
const QString kAutoSend = QStringLiteral("behaviour/autoSend");
const QString kFollow = QStringLiteral("behaviour/follow");
const QString kResend = QStringLiteral("behaviour/resendOnConnect");
const QString kLastPatch = QStringLiteral("session/lastPatch");
const QString kGeometry = QStringLiteral("window/geometry");

// Version 2 leaf names under devices/<model id>/ and the global keys.
const QString kEnabled = QStringLiteral("enabled");
const QString kOutputLeaf = QStringLiteral("outputPort");
const QString kInputLeaf = QStringLiteral("inputPort");
const QString kChannelLeaf = QStringLiteral("channel");
const QString kFollowLeaf = QStringLiteral("follow");
const QString kResendLeaf = QStringLiteral("resendOnConnect");
const QString kLastPatchLeaf = QStringLiteral("lastPatch");
const QString kEngineLeaf = QStringLiteral("engine");
const QString kGlobalAutoSend = QStringLiteral("autoSend");
const QString kCurrentDevice = QStringLiteral("currentDevice");
const QString kFollowSwitches = QStringLiteral("followSwitchesDevice");

QString devKey(Model m, const QString &leaf)
{
    return QStringLiteral("devices/") + modelInfo(m).id + QLatin1Char('/') + leaf;
}

// The version 1 key a D-05 leaf migrates from; empty for leaves version 1 never had.
QString v1KeyFor(const QString &leaf)
{
    if (leaf == kOutputLeaf) return kOutput;
    if (leaf == kInputLeaf) return kInput;
    if (leaf == kChannelLeaf) return kChannel;
    if (leaf == kFollowLeaf) return kFollow;
    if (leaf == kResendLeaf) return kResend;
    if (leaf == kLastPatchLeaf) return kLastPatch;
    return QString();
}

// Strict: only "true"/"1" and "false"/"0" count; anything else is the default (false).
bool flagText(const QString &text)
{
    const QString t = text.trimmed().toLower();
    return t == QLatin1String("true") || t == QLatin1String("1");
}

bool flag(const QSettings &s, const QString &k)
{
    return flagText(s.value(k).toString());
}

QString key(const QString &name)
{
    return MidiLink::portKey(name.trimmed());
}
}  // namespace

AppSettings::AppSettings(const QString &path)
    : m_settings(path.isEmpty() ? Paths::settingsPath() : path, QSettings::IniFormat)
{
}

void AppSettings::flush()
{
    // QSettings does not create the directory; a missing config dir must not lose the write.
    QDir().mkpath(QFileInfo(m_settings.fileName()).absolutePath());
    m_settings.sync();
    m_writeFailed = m_settings.status() != QSettings::NoError;
}

void AppSettings::store(const QString &k, const QVariant &value)
{
    migrateV1();  // the first write of any kind carries the version 1 values over
    m_settings.setValue(k, value);
}

// A file that has any version-1-only key came from the D-05-only app: its device is in use. Keys
// version 2 also writes under the same name (window/geometry) are not evidence.
bool AppSettings::isV1Install() const
{
    for (const QString &k : {kOutput, kInput, kChannel, kAutoSend, kFollow, kResend, kLastPatch})
        if (m_settings.contains(k))
            return true;
    return false;
}

// SPEC-2 3.4: copies each version 1 value that has no version 2 key into `devices/d05/` and sets
// `enabled = true`. Runs at most once per object and only when a write is about to happen; the old
// keys stay.
void AppSettings::migrateV1()
{
    if (m_migrationChecked)
        return;
    m_migrationChecked = true;
    if (m_settings.contains(devKey(Model::D05, kEnabled)) || !isV1Install())
        return;
    for (const QString &leaf : {kOutputLeaf, kInputLeaf, kChannelLeaf, kFollowLeaf, kResendLeaf, kLastPatchLeaf}) {
        const QString old = v1KeyFor(leaf);
        if (m_settings.contains(old) && !m_settings.contains(devKey(Model::D05, leaf)))
            m_settings.setValue(devKey(Model::D05, leaf), m_settings.value(old));
    }
    if (m_settings.contains(kAutoSend) && !m_settings.contains(kGlobalAutoSend))
        m_settings.setValue(kGlobalAutoSend, m_settings.value(kAutoSend));
    m_settings.setValue(devKey(Model::D05, kEnabled), true);
}

QVariant AppSettings::read(Model m, const QString &leaf, const QVariant &fallback) const
{
    const QString k = devKey(m, leaf);
    if (m_settings.contains(k))
        return m_settings.value(k);
    if (m == Model::D05) {
        const QString old = v1KeyFor(leaf);
        if (!old.isEmpty() && m_settings.contains(old))
            return m_settings.value(old);
    }
    return fallback;
}

QString AppSettings::outputPort() const { return read(Model::D05, kOutputLeaf).toString(); }
void AppSettings::setOutputPort(const QString &name) { store(devKey(Model::D05, kOutputLeaf), key(name)); flush(); }
QString AppSettings::inputPort() const { return read(Model::D05, kInputLeaf).toString(); }
void AppSettings::setInputPort(const QString &name) { store(devKey(Model::D05, kInputLeaf), key(name)); flush(); }

int AppSettings::channel() const { return device(Model::D05).channel; }
void AppSettings::setChannel(int channel)
{
    store(devKey(Model::D05, kChannelLeaf), qBound(1, channel, 16));
    flush();
}

bool AppSettings::autoSend() const
{
    return m_settings.contains(kGlobalAutoSend) ? flag(m_settings, kGlobalAutoSend) : flag(m_settings, kAutoSend);
}
void AppSettings::setAutoSend(bool on) { store(kGlobalAutoSend, on); flush(); }
bool AppSettings::follow() const { return flagText(read(Model::D05, kFollowLeaf).toString()); }
void AppSettings::setFollow(bool on) { store(devKey(Model::D05, kFollowLeaf), on); flush(); }
bool AppSettings::resendOnConnect() const { return flagText(read(Model::D05, kResendLeaf).toString()); }
void AppSettings::setResendOnConnect(bool on) { store(devKey(Model::D05, kResendLeaf), on); flush(); }

AppSettings::Values AppSettings::values() const
{
    const DeviceValues d = device(Model::D05);
    Values v;
    v.outputPort = d.outputPort;
    v.inputPort = d.inputPort;
    v.channel = d.channel;
    v.autoSend = autoSend();
    v.follow = d.follow;
    v.resendOnConnect = d.resendOnConnect;
    return v;
}

void AppSettings::setValues(const Values &v)
{
    // Values hold keys already (the dialog strips OS names). Stripping again is not safe where a
    // key can end in digits (WinMM), so they are stored as given.
    store(devKey(Model::D05, kOutputLeaf), v.outputPort.trimmed());
    store(devKey(Model::D05, kInputLeaf), v.inputPort.trimmed());
    store(devKey(Model::D05, kChannelLeaf), qBound(1, v.channel, 16));
    store(kGlobalAutoSend, v.autoSend);
    store(devKey(Model::D05, kFollowLeaf), v.follow);
    store(devKey(Model::D05, kResendLeaf), v.resendOnConnect);
    flush();
}

AppSettings::DeviceValues AppSettings::device(Model m) const
{
    DeviceValues d;
    if (m_settings.contains(devKey(m, kEnabled)))
        d.enabled = flag(m_settings, devKey(m, kEnabled));
    else
        d.enabled = m == Model::D05 && isV1Install();
    d.outputPort = read(m, kOutputLeaf).toString();
    d.inputPort = read(m, kInputLeaf).toString();
    bool ok = false;
    const int c = read(m, kChannelLeaf, 1).toInt(&ok);
    d.channel = ok ? qBound(1, c, 16) : 1;
    d.follow = flagText(read(m, kFollowLeaf).toString());
    d.resendOnConnect = flagText(read(m, kResendLeaf).toString());
    return d;
}

void AppSettings::setDevice(Model m, const DeviceValues &v)
{
    store(devKey(m, kEnabled), v.enabled);
    store(devKey(m, kOutputLeaf), v.outputPort.trimmed());
    store(devKey(m, kInputLeaf), v.inputPort.trimmed());
    store(devKey(m, kChannelLeaf), qBound(1, v.channel, 16));
    store(devKey(m, kFollowLeaf), v.follow);
    store(devKey(m, kResendLeaf), v.resendOnConnect);
    flush();
}

bool AppSettings::hasChannel(Model m) const
{
    return m_settings.contains(devKey(m, kChannelLeaf)) || (m == Model::D05 && m_settings.contains(kChannel));
}

std::optional<PatchAddress> AppSettings::lastPatch() const { return lastPatch(Model::D05); }

std::optional<PatchAddress> AppSettings::lastPatch(Model m) const
{
    const QString text = read(m, kLastPatchLeaf).toString();
    if (m == Model::D05)
        return PatchAddress::parse(text);
    // A saved bare form could not name the engine: only the canonical text of the model counts.
    return PatchAddress::parse(text, m, std::nullopt);
}

void AppSettings::setLastPatch(const PatchAddress &a) { setLastPatch(a.model(), a); }

void AppSettings::setLastPatch(Model m, const PatchAddress &a)
{
    const QString text = a.toString();
    if (read(m, kLastPatchLeaf).toString() == text && m_settings.contains(devKey(m, kLastPatchLeaf)))
        return;
    store(devKey(m, kLastPatchLeaf), text);
    flush();
}

int AppSettings::engine() const
{
    return m_settings.value(devKey(Model::JU06A, kEngineLeaf)).toInt() == 106 ? 106 : 60;
}

bool AppSettings::hasEngine() const
{
    return m_settings.contains(devKey(Model::JU06A, kEngineLeaf));
}

void AppSettings::setEngine(int engine)
{
    store(devKey(Model::JU06A, kEngineLeaf), engine == 106 ? 106 : 60);
    flush();
}

std::optional<int> AppSettings::remembered(int engine) const
{
    const QString k = devKey(Model::JU06A, engine == 106 ? QStringLiteral("remembered106") : QStringLiteral("remembered60"));
    if (!m_settings.contains(k))
        return std::nullopt;
    bool ok = false;
    const int p = m_settings.value(k).toInt(&ok);
    if (!ok || p < 0 || p > 63)
        return std::nullopt;
    return p;
}

void AppSettings::setRemembered(int engine, std::optional<int> program)
{
    const QString k = devKey(Model::JU06A, engine == 106 ? QStringLiteral("remembered106") : QStringLiteral("remembered60"));
    if (program && *program >= 0 && *program <= 63) {
        store(k, *program);
    } else if (m_settings.contains(k)) {
        migrateV1();
        m_settings.remove(k);
    } else {
        return;
    }
    flush();
}

std::optional<Model> AppSettings::currentDevice() const
{
    return modelFromId(m_settings.value(kCurrentDevice).toString().trimmed().toLower());
}

void AppSettings::setCurrentDevice(Model m)
{
    const QString id = modelInfo(m).id;
    if (m_settings.value(kCurrentDevice).toString() == id)
        return;
    store(kCurrentDevice, id);
    flush();
}

bool AppSettings::followSwitchesDevice() const
{
    if (!m_settings.contains(kFollowSwitches))
        return true;
    return flag(m_settings, kFollowSwitches);
}

void AppSettings::setFollowSwitchesDevice(bool on) { store(kFollowSwitches, on); flush(); }

QByteArray AppSettings::windowGeometry() const { return m_settings.value(kGeometry).toByteArray(); }
void AppSettings::setWindowGeometry(const QByteArray &geometry) { store(kGeometry, geometry); flush(); }

}  // namespace d05
