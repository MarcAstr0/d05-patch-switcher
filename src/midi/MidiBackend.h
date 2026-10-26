#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <cstddef>

namespace d05 {

// Abstract MIDI transport. Implementations: RtMidiBackend (real ports) and
// FakeMidiBackend (tests, --fake-midi). No method throws: failures are reported
// through the return value and the optional error string.
//
// received() may be emitted from any thread (RtMidi's input thread, a test
// thread); MidiLink connects to it with Qt::QueuedConnection.
class MidiBackend : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~MidiBackend() override = default;

    // Port names as the OS reports them right now (enumerates each call). If
    // enumeration fails the list is empty and *err (if given) holds the text;
    // *err is left empty on success.
    // The app's own ports (this process and any other instance of the app) are never listed:
    // choosing one as the input would feed the app's sends back into follow mode.
    virtual QStringList outputPortNames(QString *err) = 0;
    virtual QStringList inputPortNames(QString *err) = 0;

    // The ALSA client name the real backend gives its RtMidi clients.
    static QString ownClientName();
    // True for a port name that belongs to the app: its client (ALSA "client:port id:n") is
    // ours, or its port part is one of the port names the app opens. Other platforms do not
    // list a process's own ports this way, so the test simply never matches there.
    static bool isOwnPort(const QString &name);
    static QStringList withoutOwnPorts(const QStringList &names);

    // Open the first port whose name equals `name` exactly.
    virtual bool openOutput(const QString &name, QString *error) = 0;
    virtual void closeOutput() = 0;
    virtual bool isOutputOpen() const = 0;

    virtual bool openInput(const QString &name, QString *error) = 0;
    virtual void closeInput() = 0;
    virtual bool isInputOpen() const = 0;

    // Sends one complete MIDI message on the open output.
    virtual bool send(const QByteArray &message, QString *error) = 0;

    // Input filter, applied on the input thread before anything is allocated. Bits 0..15: a
    // Program Change on channel n+1 is wanted; bits 16..31: Bank Select (CC 0 / CC 32) on channel
    // n+1 is wanted. Everything else (realtime, notes, other CCs, sysex) is dropped. The default
    // accepts all of the above; PortHub narrows it to what the open links need.
    static constexpr quint32 AcceptAll = 0xFFFFFFFFu;
    void setAcceptMask(quint32 mask) { m_acceptMask.store(mask, std::memory_order_relaxed); }
    quint32 acceptMask() const { return m_acceptMask.load(std::memory_order_relaxed); }
    // Pure, allocation-free, safe on any thread.
    static bool accepts(quint32 mask, const unsigned char *data, std::size_t size);

signals:
    // One complete incoming MIDI message (channel messages only; sysex, clock
    // and active sensing are filtered by the backend).
    void received(const QByteArray &message);
    // The open output port disappeared (only backends that can tell emit this).
    void outputLost();
    // Same for the open input port.
    void inputLost();

private:
    std::atomic<quint32> m_acceptMask{AcceptAll};
};

}  // namespace d05
