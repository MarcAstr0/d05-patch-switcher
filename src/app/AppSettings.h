#pragma once

#include "core/Model.h"
#include "core/PatchAddress.h"

#include <QByteArray>
#include <QSettings>
#include <QString>

#include <optional>

namespace d05 {

// Typed access to the per-user settings (SPEC 3.3, 6; SPEC-2 3.4): a QSettings INI file at
// Paths::settingsPath() (honours --config-dir). Every setter writes through, so the
// file is always current. Port names are stored as keys, without ALSA's trailing
// "client:port" numbers (MidiLink::portKey), which change across a replug.
//
// Version 2 keys: `devices/<model id>/{enabled,outputPort,inputPort,channel,follow,resendOnConnect,
// lastPatch}` (JU-06A also `engine`, `remembered60`, `remembered106`) and the global keys
// `currentDevice`, `autoSend`, `followSwitchesDevice`, `window/geometry`. The version 1 keys
// (`midi/*`, `behaviour/*`, `session/lastPatch`) are never deleted. They stay readable: a value
// that has no version 2 key yet is read from its version 1 key, and a file that has version 1
// keys reads as D-05 enabled. The first write of any kind copies the version 1 values once into
// `devices/d05/` with `enabled = true`, so reading never writes (the file is not created by a
// read) and the old keys survive for a downgrade.
//
// Nothing here is written at start-up: command-line overrides (--port, --channel, ...)
// live only in the run's effective values and are never saved unless the user confirms
// the Settings dialog.
class AppSettings {
public:
    // The connection and behaviour settings that FR-9 edits as one unit.
    struct Values {
        QString outputPort;  // port key (MidiLink::portKey of an OS name); empty = none chosen
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

    // One device's connection settings (SPEC-2 3.4).
    struct DeviceValues {
        bool enabled = false;
        QString outputPort;  // port key; empty = none chosen
        QString inputPort;   // port key; empty = None
        int channel = 1;
        bool follow = false;
        bool resendOnConnect = false;

        bool operator==(const DeviceValues &o) const
        {
            return enabled == o.enabled && outputPort == o.outputPort && inputPort == o.inputPort
                   && channel == o.channel && follow == o.follow && resendOnConnect == o.resendOnConnect;
        }
        bool operator!=(const DeviceValues &o) const { return !(*this == o); }
    };

    // `path` defaults to Paths::settingsPath().
    explicit AppSettings(const QString &path = QString());

    QString path() const { return m_settings.fileName(); }
    // True when the last write to the file failed (read-only disk, bad path).
    bool writeFailed() const { return m_writeFailed; }

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

    // The D-05's connection settings plus the global auto-send, as version 1 had them. `values()`
    // reads the same keys as device(Model::D05) (it ignores `enabled`); `setValues()` writes them
    // and leaves `enabled` alone.
    Values values() const;
    void setValues(const Values &v);  // one write

    // --- Per device (SPEC-2 3.4) ---------------------------------------------------------------
    DeviceValues device(Model m) const;
    void setDevice(Model m, const DeviceValues &v);  // one write
    // True when a channel was ever saved for `m` (the CLI's --send needs it for a device whose
    // default would otherwise be a guess).
    bool hasChannel(Model m) const;
    // The canonical address text of the patch to restore for `m`, empty when none.
    std::optional<PatchAddress> lastPatch(Model m) const;
    void setLastPatch(Model m, const PatchAddress &a);
    // JU-06A: the app's last known engine (60 or 106, default 60) and the remembered program of
    // each engine (0..63) or none.
    int engine() const;
    bool hasEngine() const;  // an engine was ever saved
    void setEngine(int engine);
    std::optional<int> remembered(int engine) const;
    void setRemembered(int engine, std::optional<int> program);

    // --- Global ----------------------------------------------------------------------------------
    // The device the window showed last; empty when never saved.
    std::optional<Model> currentDevice() const;
    void setCurrentDevice(Model m);
    bool followSwitchesDevice() const;  // default true
    void setFollowSwitchesDevice(bool on);

    // The patch that was current when the app was last closed; restored (without sending)
    // at start so resend-on-connect resends the right patch.
    std::optional<PatchAddress> lastPatch() const;
    void setLastPatch(const PatchAddress &a);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);

private:
    void store(const QString &key, const QVariant &value);
    void flush();
    // Reads `key` of device `m`, falling back to the version 1 key of the D-05.
    QVariant read(Model m, const QString &key, const QVariant &fallback = QVariant()) const;
    bool isV1Install() const;
    void migrateV1();

    bool m_migrationChecked = false;

    QSettings m_settings;
    bool m_writeFailed = false;
};

}  // namespace d05
