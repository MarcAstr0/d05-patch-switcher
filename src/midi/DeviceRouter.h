#pragma once

#include "core/EngineTracker.h"
#include "core/Model.h"
#include "core/PatchAddress.h"
#include "midi/MidiLink.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <array>
#include <optional>

namespace d05 {

class PortHub;

// SPEC-2 sections 3.4, 4 (FR-10, FR-20) and 7: owns the three device links (one per model) and
// the JU-06A engine tracker, knows the current device, runs the one 2 s poll (only while some
// enabled device is disconnected) and dispatches follow events. The UI never calls a backend
// directly; it talks to the router and to the links it hands out.
//
// Persistence of the per-device keys is not here (AppSettings, M8); the router only keeps what it
// needs in memory.
class DeviceRouter : public QObject {
    Q_OBJECT
public:
    struct DeviceConfig {
        bool enabled = false;
        QString outputPort;  // as given to MidiLink::setOutputPort (key or full OS name); empty = none
        QString inputPort;
        int channel = 1;
        bool follow = false;
        bool resendOnConnect = false;  // kept for the UI; the router does not act on it
    };

    // `hub` must outlive the router. All three devices start disabled, D-05 current.
    explicit DeviceRouter(PortHub *hub, QObject *parent = nullptr);

    MidiLink *link(Model m) { return m_dev[idx(m)].link; }
    const MidiLink *link(Model m) const { return m_dev[idx(m)].link; }
    // The JU-06A engine tracker (the D-05 and JP-08 have none).
    EngineTracker *ju06aTracker() { return m_tracker; }

    // --- Configuration -----------------------------------------------------------------------
    // The enabled device that would collide with `cfg` for `m` (same output port and channel, or
    // same input port and channel with follow on in both), if any. Disabled configs never conflict.
    std::optional<Model> conflict(Model m, const DeviceConfig &cfg) const;
    // Applies `cfg` to device `m`: refused (false, nothing changed, *conflicting set) on a conflict.
    // A disabled device has its ports closed. Opens ports immediately; failures show in the link's
    // status and start the poll.
    bool configure(Model m, const DeviceConfig &cfg, Model *conflicting = nullptr);
    DeviceConfig config(Model m) const { return m_dev[idx(m)].cfg; }
    bool isEnabled(Model m) const { return m_dev[idx(m)].cfg.enabled; }
    // JU-06A persisted state (SPEC-2 section 6): loads the tracker and the link's engine.
    void setJu06aState(int engine, int remembered60, int remembered106);

    void setFollowSwitchesDevice(bool on) { m_followSwitches = on; }
    bool followSwitchesDevice() const { return m_followSwitches; }

    // --- Current device ----------------------------------------------------------------------
    Model currentDevice() const { return m_current; }
    // The user (or FR-20) switches the window to `m`. Sends nothing. Calls deviceSwitch() on the
    // JU-06A tracker when the JU-06A is the device left or entered.
    void setCurrentDevice(Model m);

    // --- Sending -----------------------------------------------------------------------------
    // A selection on the device the address belongs to (Enter, click, prev/next, auto-send,
    // --send). D-05 and JP-08: sends. JU-06A: goes through the engine tracker, which sends or arms.
    // False when the device is not connected (nothing is changed then) or the send failed.
    bool select(const PatchAddress &a);

    // --- Poll --------------------------------------------------------------------------------
    // Re-tries every enabled device that wants a port it does not have. The timer calls this.
    void pollNow();
    bool isPolling() const { return m_timer.isActive(); }
    int pollIntervalMs() const { return m_timer.interval(); }
    void setPollIntervalMs(int ms) { m_timer.setInterval(ms); }
    // True when every enabled device that was asked for a port has it open (FR-10).
    bool allConnected() const;
    // True when every enabled device has its input link open (the FR-20 precondition).
    bool allInputsOpen() const;
    // Window activation: drops ports that vanished on every enabled device.
    void checkConnections();

signals:
    // The current device changed; `byFollow` is true when FR-20 caused it. The UI acts in M8.
    void currentDeviceChanged(d05::Model model, bool byFollow);

private:
    struct Dev {
        MidiLink *link = nullptr;
        DeviceConfig cfg;
    };
    static constexpr int idx(Model m) { return int(m); }
    static bool samePort(const QString &a, const QString &b);
    void updatePolling();
    void updateFlipDetectable();
    void onFollowEvent(Model m);

    PortHub *m_hub;
    std::array<Dev, 3> m_dev;
    EngineTracker *m_tracker;
    QTimer m_timer;
    Model m_current = Model::D05;
    bool m_followSwitches = true;
};

}  // namespace d05
