#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

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

    // Port names as the OS reports them right now (enumerates each call).
    virtual QStringList outputPortNames() = 0;
    virtual QStringList inputPortNames() = 0;

    // Open the first port whose name equals `name` exactly.
    virtual bool openOutput(const QString &name, QString *error) = 0;
    virtual void closeOutput() = 0;
    virtual bool isOutputOpen() const = 0;

    virtual bool openInput(const QString &name, QString *error) = 0;
    virtual void closeInput() = 0;
    virtual bool isInputOpen() const = 0;

    // Sends one complete MIDI message on the open output.
    virtual bool send(const QByteArray &message, QString *error) = 0;

signals:
    // One complete incoming MIDI message (channel messages only; sysex, clock
    // and active sensing are filtered by the backend).
    void received(const QByteArray &message);
    // The open output port disappeared (only backends that can tell emit this).
    void outputLost();
};

}  // namespace d05
