#pragma once

#include "core/PatchAddress.h"

#include <QByteArray>
#include <QSettings>
#include <QString>

#include <optional>

namespace d05 {

// Typed access to the per-user settings (SPEC 3.3, 6): a QSettings INI file at
// Paths::settingsPath() (honours --config-dir). Every setter writes through, so the
// file is always current. Port names are stored as keys, without ALSA's trailing
// "client:port" numbers (MidiLink::portKey), which change across a replug.
//
// Nothing here is written at start-up: command-line overrides (--port, --channel, ...)
// live only in the run's effective values and are never saved unless the user confirms
// the Settings dialog.
class AppSettings {
public:
    // The connection and behaviour settings that FR-9 edits as one unit.
    struct Values {
        QString outputPort;  // port key; empty = none chosen
        QString inputPort;   // port key; empty = None
        int channel = 1;
        bool autoSend = false;
        bool follow = false;
        bool resendOnConnect = false;

        bool operator==(const Values &o) const
        {
            return outputPort == o.outputPort && inputPort == o.inputPort && channel == o.channel
                   && autoSend == o.autoSend && follow == o.follow && resendOnConnect == o.resendOnConnect;
        }
        bool operator!=(const Values &o) const { return !(*this == o); }
    };

    // `path` defaults to Paths::settingsPath().
    explicit AppSettings(const QString &path = QString());

    QString path() const { return m_settings.fileName(); }

    QString outputPort() const;
    void setOutputPort(const QString &name);  // stored as a key
    QString inputPort() const;
    void setInputPort(const QString &name);
    int channel() const;  // 1..16, default 1
    void setChannel(int channel);
    bool autoSend() const;
    void setAutoSend(bool on);
    bool follow() const;
    void setFollow(bool on);
    bool resendOnConnect() const;
    void setResendOnConnect(bool on);

    Values values() const;
    void setValues(const Values &v);  // one write

    // The patch that was current when the app last sent or followed one; restored
    // (without sending) at start so resend-on-connect resends the right patch.
    std::optional<PatchAddress> lastPatch() const;
    void setLastPatch(const PatchAddress &a);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);

private:
    void store(const QString &key, const QVariant &value);
    void flush();

    QSettings m_settings;
};

}  // namespace d05
