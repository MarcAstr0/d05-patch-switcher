#pragma once

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

// The one object the UI talks to for MIDI. Does not own the backend.
class MidiLink : public QObject {
    Q_OBJECT
public:
    static constexpr int DefaultPollMs = 2000;
    static constexpr int DefaultEchoMs = 300;

    explicit MidiLink(MidiBackend *backend, QObject *parent = nullptr);

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

    bool isConnected() const { return m_connected; }
    // Full name as the OS lists it right now.
    QString connectedPortName() const { return m_connectedName; }
    // Name without the trailing ALSA "client:port" numbers; persist this.
    QString connectedPortKey() const { return portKey(m_connectedName); }
    static QString portKey(const QString &name);
    // How `wanted` is matched against a port list: exact full name, then equal
    // keys, then case-insensitive substring of the key. "" when none.
    static QString matchPort(const QStringList &ports, const QString &wanted);
    QString status() const { return m_status; }
    QString inputStatus() const { return m_inputStatus; }
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
    int pollIntervalMs() const { return m_timer.interval(); }

    // Test hooks. Defaults: 2000 ms, 300 ms, monotonic clock.
    void setPollIntervalMs(int ms);
    void setEchoWindowMs(int ms) { m_echoMs = ms; }
    void setClock(std::function<qint64()> msClock) { m_clock = std::move(msClock); }

    // The three messages for `a` on `channel` (1..16), in send order.
    static QList<QByteArray> patchMessages(const PatchAddress &a, int channel);

signals:
    void statusChanged(const QString &status);
    void connectedChanged(bool connected);
    void sent(const d05::PatchAddress &address);
    void currentChanged(const d05::PatchAddress &address);

private slots:
    void onReceived(const QByteArray &message);
    void onOutputLost();
    void onInputLost();

private:
    bool connectOutput();
    bool applyInput();
    void updatePolling();
    void setStatus(const QString &s);
    void dropInput(const QString &status);
    void pruneSent(qint64 now);
    void dropOutput(const QString &status);
    bool outputWanted() const { return !m_outName.isEmpty(); }
    bool inputWanted() const { return m_follow && !m_inName.isEmpty(); }

    MidiBackend *m_backend;
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
