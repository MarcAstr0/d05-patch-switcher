#pragma once

#include "core/Model.h"
#include "core/PatchAddress.h"

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>
#include <optional>

namespace d05 {

class MidiBackend;
class PortHub;

// One device's MIDI connection (SPEC-2 3.4). Does not own the backend. Built on a PortHub, which
// owns the ports; the D-05 link behaves exactly as the single-backend link of version 1, the
// JP-08 and JU-06A links send and follow a lone Program Change (FR-11).
class MidiLink : public QObject {
    Q_OBJECT
public:
    static constexpr int DefaultPollMs = 2000;
    static constexpr int DefaultEchoMs = 300;

    // Version 1 form: a D-05 link on its own private hub around `backend`.
    explicit MidiLink(MidiBackend *backend, QObject *parent = nullptr);
    // A link for `model` on a shared hub (DeviceRouter). The hub must outlive the link.
    MidiLink(PortHub *hub, Model model, QObject *parent = nullptr);
    ~MidiLink() override;

    Model model() const { return m_model; }
    // JU-06A only: the engine the unit is believed to be on. A send for an address of the other
    // engine fails and puts nothing on the wire (PatchAddress::midiMessages).
    void setEngine(int engine) { m_engine = engine == 106 ? 106 : 60; }
    int engine() const { return m_engine; }
    // The router runs the one 2 s poll: with this on the link starts no timer of its own and
    // announces through pollNeededChanged() / needsPoll() when it wants pollNow().
    void setExternalPolling(bool on);
    bool needsPoll() const { return m_needPoll; }

    // Output. `name` is matched against the port list: exact match first, then
    // case-insensitive substring, first hit. Opens immediately; if the port is
    // missing the 2 s poll starts and keeps trying.
    void setOutputPort(const QString &name);
    QString outputPortName() const { return m_outName; }
    // Input, used only while follow is on.
    void setInputPort(const QString &name);
    void setFollowEnabled(bool on);
    bool followEnabled() const { return m_follow; }
    void setChannel(int channel);  // clamped to 1..16
    int channel() const { return m_channel; }

    // The OS's port lists right now (the Settings dialog's source; it never touches the
    // backend). On failure the list is empty and *err holds the text; *err is cleared on success.
    QStringList outputPortNames(QString *err) const;
    QStringList inputPortNames(QString *err) const;

    bool isConnected() const { return m_connected; }
    // Full name as the OS lists it right now.
    QString connectedPortName() const { return m_connectedName; }
    // Name without the trailing ALSA "client:port" numbers; persist this.
    QString connectedPortKey() const { return portKey(m_connectedName); }
    // On Windows RtMidi (WinMM) appends " <port index>" to every name (RtMidi.cpp 6.0.0,
    // MidiInWinMM / MidiOutWinMM::getPortName, "ensure unique portnames"), and the index
    // changes as devices come and go; the key drops it there. CoreMIDI adds nothing.
    static QString portKey(const QString &name);
    // What to save or match for the port `name` among `ports`: its key when no other port has
    // the same key, else the full OS name (two identical devices differ only in their ids).
    static QString portSpec(const QString &name, const QStringList &ports);
    // The WinMM rule on its own (always compiled, so tests can exercise it on any platform):
    // removes exactly one trailing " <digits>".
    static QString stripWinMMIndex(const QString &name);
    // How `wanted` is matched against a port list: exact full name, then equal
    // keys, then case-insensitive substring of the key. "" when none.
    static QString matchPort(const QStringList &ports, const QString &wanted);
    QString status() const { return m_status; }
    QString inputStatus() const { return m_inputStatus; }
    // The input port that is wanted (as given), and whether it is open now (never while follow is off).
    QString inputPortName() const { return m_inName; }
    bool inputConnected() const { return !m_connectedInName.isEmpty(); }
    std::optional<PatchAddress> current() const { return m_current; }
    // Sets the current patch without sending anything (e.g. restored from settings).
    void setCurrent(const PatchAddress &a);

    // Sends Bank Select MSB, LSB, Program Change on the channel. False when not
    // connected or a send failed (status says why).
    bool sendPatch(const PatchAddress &a);

    // Re-enumerates and tries to (re)connect whatever is wanted but not open.
    // The poll timer calls this; tests call it directly.
    void pollNow();
    // Re-enumerates and drops any open port that is no longer listed (status
    // updated, polling started). sendPatch calls it first; the UI calls it on
    // window activation. There is deliberately no timer while connected.
public slots:
    void checkConnection();

public:
    bool isPolling() const { return m_timer.isActive(); }
    bool isOutputWanted() const { return outputWanted(); }
    bool isInputWanted() const { return inputWanted(); }
    int pollIntervalMs() const { return m_timer.interval(); }

    // Test hooks. Defaults: 2000 ms, 300 ms, monotonic clock.
    void setPollIntervalMs(int ms);
    void setEchoWindowMs(int ms) { m_echoMs = ms; }
    void setClock(std::function<qint64()> msClock) { m_clock = std::move(msClock); }

    // The three messages for `a` on `channel` (1..16), in send order.
    // D-05 form; see PatchAddress::midiMessages for the other models.
    static QList<QByteArray> patchMessages(const PatchAddress &a, int channel);

signals:
    void statusChanged(const QString &status);
    // The input side's status text changed ("Connected to ...", "Port not found", "Error: ...", or empty).
    void inputStatusChanged(const QString &status);
    void connectedChanged(bool connected);
    void sent(const d05::PatchAddress &address);
    void currentChanged(const d05::PatchAddress &address);
    // D-05 and JP-08: every follow event that is not an echo, after current() was updated (also
    // when it equals the current patch). The device router uses it for FR-20.
    void followReceived(const d05::PatchAddress &address);
    // JU-06A: a non-echo Program Change 0..63 on the channel (the tracker decides what it means).
    void programReceived(int program);
    void inputConnectedChanged(bool connected);
    void pollNeededChanged(bool needed);

private slots:
    void onReceived(const QByteArray &message);
    void onOutputLost();
    void onInputLost();

private:
    bool connectOutput();
    bool applyInput();
    void updatePolling();
    void setStatus(const QString &s);
    void setInputStatus(const QString &s);
    void dropInput(const QString &status);
    void pruneSent(qint64 now);
    void dropOutput(const QString &status);
    bool outputWanted() const { return !m_outName.isEmpty(); }
    bool inputWanted() const { return m_follow && !m_inName.isEmpty(); }

    void setConnectedIn(const QString &name);
    bool inputOpenNow() const;
    PatchAddress::MidiTriplet echoKey(const PatchAddress &a) const;

    PortHub *m_hub;
    Model m_model = Model::D05;
    int m_engine = 60;
    bool m_externalPoll = false;
    bool m_needPoll = false;
    QTimer m_timer;
    std::function<qint64()> m_clock;
    QString m_outName;
    QString m_inName;
    QString m_connectedName;
    QString m_connectedInName;
    QString m_status;
    QString m_inputStatus;
    int m_channel = 1;
    int m_echoMs = DefaultEchoMs;
    bool m_follow = false;
    bool m_connected = false;
    std::optional<PatchAddress> m_current;
    struct SentEntry {
        PatchAddress::MidiTriplet triplet;
        qint64 at;
    };
    QList<SentEntry> m_recentSent;  // sends within the echo window
    int m_rxMsb = -1;
    int m_rxLsb = -1;
};

}  // namespace d05
