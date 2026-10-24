#pragma once

#include "midi/MidiBackend.h"

#include <QMutex>

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

    QStringList outputPortNames(QString *err) override;
    QStringList inputPortNames(QString *err) override;
    bool openOutput(const QString &name, QString *error) override;
    void closeOutput() override;
    bool isOutputOpen() const override;
    bool openInput(const QString &name, QString *error) override;
    void closeInput() override;
    bool isInputOpen() const override;
    bool send(const QByteArray &message, QString *error) override;

    // Error sink fed by RtMidi's error callback (may run on RtMidi's thread).
    struct ErrorSink {
        QMutex mutex;
        QString text;
        void record(const QString &t);
        QString take();
    };

private:
    ErrorSink m_outErrors;
    ErrorSink m_inErrors;
    std::unique_ptr<RtMidiOut> m_out;
    std::unique_ptr<RtMidiIn> m_in;
    QString m_initError;
};

}  // namespace d05
