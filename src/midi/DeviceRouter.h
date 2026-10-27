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

        bool operator==(const DeviceConfig &o) const
        {
            return enabled == o.enabled && outputPort == o.outputPort && inputPort == o.inputPort
                   && channel == o.channel && follow == o.follow && resendOnConnect == o.resendOnConnect;
        }
        bool operator!=(const DeviceConfig &o) const { return !(*this == o); }
    };
    using Configs = std::array<DeviceConfig, 3>;  // in Model order: D-05, JP-08, JU-06A

    // `hub` must outlive the router. All three devices start disabled, D-05 current. `d05Link`
    // (not owned, must outlive the router) is the version 1 form of the D-05 link on its own
    // private hub; the router then uses it instead of making one on `hub`, treats the D-05 as
    // enabled with the link's current port, channel and follow settings, and leaves the polling of
    // that link to the link itself (its own 2 s timer). For code that already holds such a link
    // (the tests of the single-device window).
    explicit DeviceRouter(PortHub *hub, QObject *parent = nullptr, MidiLink *d05Link = nullptr);

    MidiLink *link(Model m) { return m_dev[idx(m)].link; }
    const MidiLink *link(Model m) const { return m_dev[idx(m)].link; }
    // The JU-06A engine tracker (the D-05 and JP-08 have none).
    EngineTracker *ju06aTracker() { return m_tracker; }

    // --- Configuration -----------------------------------------------------------------------
    // The enabled device that would collide with `cfg` for `m` (same output port and channel, or
    // same input port and channel with follow on in both), if any. Disabled configs never conflict.
    std::optional<Model> conflict(Model m, const DeviceConfig &cfg) const;
    // The same rule over a whole set of pending configurations (the Settings dialog): what device
    // `m` collides with, and whether it is the output or the input that collides. With
    // `earlierOnly` only devices before `m` in Model order count, so that of two devices exactly
    // one (the later one) reports the conflict.
    struct Conflict {
        Model other = Model::D05;
        bool input = false;  // false: same output port and channel; true: same input port and channel
    };
    static std::optional<Conflict> findConflict(const Configs &cfgs, Model m, bool earlierOnly = false);
    // Applies `cfg` to device `m`: refused (false, nothing changed, *conflicting set) on a conflict.
    // A disabled device has its ports closed. Opens ports immediately; failures show in the link's
    // status and start the poll.
    bool configure(Model m, const DeviceConfig &cfg, Model *conflicting = nullptr);
    // Applies a whole set at once: refused (false, nothing changed, *a and *b the colliding pair)
    // when the set has a conflict, otherwise every device whose configuration changed is applied.
    // Unlike configure() one by one, a swap of channels between two devices is not refused for the
    // moment in between.
    bool configureAll(const Configs &cfgs, Model *a = nullptr, Model *b = nullptr);
    DeviceConfig config(Model m) const { return m_dev[idx(m)].cfg; }
    bool isEnabled(Model m) const { return m_dev[idx(m)].cfg.enabled; }
    // JU-06A persisted state (SPEC-2 section 6): loads the tracker and the link's engine.
    void setJu06aState(int engine, int remembered60, int remembered106);

    void setFollowSwitchesDevice(bool on) { m_followSwitches = on; }
    bool followSwitchesDevice() const { return m_followSwitches; }

    // --- Current device ----------------------------------------------------------------------
    Model currentDevice() const { return m_current; }
    // The user (or FR-20) switches the window to `m`. Sends nothing. Always calls deviceSwitch() on
    // the JU-06A tracker (ends a pending arm, keeps a hint), whichever devices are involved.
    void setCurrentDevice(Model m);

    // --- Sending -----------------------------------------------------------------------------
    // A selection on the device the address belongs to (Enter, click, prev/next, auto-send,
    // --send). D-05 and JP-08: sends. JU-06A: goes through the engine tracker, which sends or arms.
    // False when the device is not connected or the send failed; the JU-06A tracker's engine,
    // remembered programs and current patch are then as they were (a disconnected JU-06A
    // selection still ends an arm and a hint). See EngineTracker for the failed-send rules.
    bool select(const PatchAddress &a);
    // A send that is not a selection (resend on connect, Settings TEST): the same wire message, but a
    // JU-06A arm or flip hint is left as it is, and nothing is sent for the other engine.
    bool resend(const PatchAddress &a);

    // --- Poll --------------------------------------------------------------------------------
    // Re-tries every enabled device that wants a port it does not have. The timer calls this.
    void pollNow();
    bool isPolling() const { return m_timer.isActive(); }
    int pollIntervalMs() const { return m_timer.interval(); }
    void setPollIntervalMs(int ms) { m_timer.setInterval(ms); }
    // True when every enabled device that was asked for a port has it open (FR-10).
    bool allConnected() const;
    // True when every enabled device with follow on has its input link open (the FR-20
    // precondition). A device with follow off has no input by design and does not count.
    bool allInputsOpen() const;
    // Window activation: drops ports that vanished on every enabled device.
    void checkConnections();

signals:
    // A JU-06A Program Change was fully handled: the tracker classified it and, for FR-20, the
    // window device was switched. `sentPatch` is true when the tracker put a patch on the wire for
    // it (an armed flip confirmation). The window uses it to end a SENT flash for an ordinary panel
    // change but not for a confirmation.
    void ju06aProgramHandled(bool sentPatch);
    // The current device changed; `byFollow` is true when FR-20 caused it. The UI acts in M8.
    void currentDeviceChanged(d05::Model model, bool byFollow);

private:
    struct Dev {
        MidiLink *link = nullptr;
        DeviceConfig cfg;
        bool selfPolled = false;  // an adopted link: it polls itself, the router's timer skips it
    };
    static constexpr int idx(Model m) { return int(m); }
    static bool samePort(const QString &a, const QString &b);
    void applyConfig(Model m, const DeviceConfig &cfg);
    static bool sameOutput(const MidiLink *l, const QString &port);
    void updatePolling();
    void updateFlipDetectable();
    void onFollowEvent(Model m);
    QString openVeto(Model m, const QString &port, bool input) const;

    PortHub *m_hub;
    std::array<Dev, 3> m_dev;
    EngineTracker *m_tracker;
    QTimer m_timer;
    int m_ju06aSends = 0;       // JU-06A patches the tracker put on the wire
    int m_sendsAtEvent = 0;
    std::array<bool, 3> m_pending{};  // devices of a configureAll() batch not applied yet
    Model m_current = Model::D05;
    bool m_followSwitches = true;
};

}  // namespace d05
