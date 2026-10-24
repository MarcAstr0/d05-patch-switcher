#include "app/AppSettings.h"

#include "app/Paths.h"
#include "midi/MidiLink.h"

#include <QDir>
#include <QFileInfo>

namespace d05 {

namespace {
const QString kOutput = QStringLiteral("midi/outputPort");
const QString kInput = QStringLiteral("midi/inputPort");
const QString kChannel = QStringLiteral("midi/channel");
const QString kAutoSend = QStringLiteral("behaviour/autoSend");
const QString kFollow = QStringLiteral("behaviour/follow");
const QString kResend = QStringLiteral("behaviour/resendOnConnect");
const QString kLastPatch = QStringLiteral("session/lastPatch");
const QString kGeometry = QStringLiteral("window/geometry");

// Strict: only "true"/"1" and "false"/"0" count; anything else is the default (false).
bool flag(const QSettings &s, const QString &k)
{
    const QString t = s.value(k).toString().trimmed().toLower();
    return t == QLatin1String("true") || t == QLatin1String("1");
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
}

void AppSettings::store(const QString &k, const QVariant &value)
{
    m_settings.setValue(k, value);
}

QString AppSettings::outputPort() const { return m_settings.value(kOutput).toString(); }
void AppSettings::setOutputPort(const QString &name) { store(kOutput, key(name)); flush(); }
QString AppSettings::inputPort() const { return m_settings.value(kInput).toString(); }
void AppSettings::setInputPort(const QString &name) { store(kInput, key(name)); flush(); }

int AppSettings::channel() const
{
    bool ok = false;
    const int c = m_settings.value(kChannel, 1).toInt(&ok);
    return ok ? qBound(1, c, 16) : 1;
}
void AppSettings::setChannel(int channel) { store(kChannel, qBound(1, channel, 16)); flush(); }

bool AppSettings::autoSend() const { return flag(m_settings, kAutoSend); }
void AppSettings::setAutoSend(bool on) { store(kAutoSend, on); flush(); }
bool AppSettings::follow() const { return flag(m_settings, kFollow); }
void AppSettings::setFollow(bool on) { store(kFollow, on); flush(); }
bool AppSettings::resendOnConnect() const { return flag(m_settings, kResend); }
void AppSettings::setResendOnConnect(bool on) { store(kResend, on); flush(); }

AppSettings::Values AppSettings::values() const
{
    Values v;
    v.outputPort = outputPort();
    v.inputPort = inputPort();
    v.channel = channel();
    v.autoSend = autoSend();
    v.follow = follow();
    v.resendOnConnect = resendOnConnect();
    return v;
}

void AppSettings::setValues(const Values &v)
{
    store(kOutput, key(v.outputPort));
    store(kInput, key(v.inputPort));
    store(kChannel, qBound(1, v.channel, 16));
    store(kAutoSend, v.autoSend);
    store(kFollow, v.follow);
    store(kResend, v.resendOnConnect);
    flush();
}

std::optional<PatchAddress> AppSettings::lastPatch() const
{
    return PatchAddress::parse(m_settings.value(kLastPatch).toString());
}

void AppSettings::setLastPatch(const PatchAddress &a)
{
    const QString text = a.toString();
    if (m_settings.value(kLastPatch).toString() == text)
        return;
    store(kLastPatch, text);
    flush();
}

QByteArray AppSettings::windowGeometry() const { return m_settings.value(kGeometry).toByteArray(); }
void AppSettings::setWindowGeometry(const QByteArray &geometry) { store(kGeometry, geometry); flush(); }

}  // namespace d05
