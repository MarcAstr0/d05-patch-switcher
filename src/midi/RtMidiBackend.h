#pragma once

#include "midi/MidiBackend.h"

#include <memory>

class RtMidiIn;
class RtMidiOut;

namespace d05 {

// Real ports through RtMidi. The input callback runs on RtMidi's thread and
// forwards each message to this object's thread with a queued invocation.
class RtMidiBackend : public MidiBackend {
    Q_OBJECT
public:
    explicit RtMidiBackend(QObject *parent = nullptr);
    ~RtMidiBackend() override;

    QStringList outputPortNames() override;
    QStringList inputPortNames() override;
    bool openOutput(const QString &name, QString *error) override;
    void closeOutput() override;
    bool isOutputOpen() const override;
    bool openInput(const QString &name, QString *error) override;
    void closeInput() override;
    bool isInputOpen() const override;
    bool send(const QByteArray &message, QString *error) override;

private:
    std::unique_ptr<RtMidiOut> m_out;
    std::unique_ptr<RtMidiIn> m_in;
    QString m_initError;
};

}  // namespace d05
