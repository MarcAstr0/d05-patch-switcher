#pragma once

#include "midi/MidiBackend.h"

#include <QList>
#include <QMutex>

namespace d05 {

// In-memory backend. Default ports: output "Fake D-05 In" (the synth's input),
// input "Fake D-05 Out" (the synth's output).
class FakeMidiBackend : public MidiBackend {
    Q_OBJECT
public:
    explicit FakeMidiBackend(QObject *parent = nullptr);

    QStringList outputPortNames() override;
    QStringList inputPortNames() override;
    bool openOutput(const QString &name, QString *error) override;
    void closeOutput() override;
    bool isOutputOpen() const override { return m_outOpen; }
    bool openInput(const QString &name, QString *error) override;
    void closeInput() override;
    bool isInputOpen() const override { return m_inOpen; }
    bool send(const QByteArray &message, QString *error) override;

    // Test controls.
    void setOutputPorts(const QStringList &names);  // closes the open output if it vanished
    void setInputPorts(const QStringList &names);   // closes the open input if it vanished
    void setFailOpen(bool fail) { m_failOpen = fail; }
    const QList<QByteArray> &sentMessages() const { return m_sent; }
    void clearSent() { m_sent.clear(); }
    QString openOutputName() const { return m_outName; }
    // Print every sent message as hex on stderr (--fake-midi).
    void setPrintSent(bool on) { m_printSent = on; }
    // Callable from any thread: emits received().
    void injectReceived(const QByteArray &message);

private:
    QStringList m_outputs;
    QStringList m_inputs;
    QList<QByteArray> m_sent;
    QString m_outName;
    QString m_inName;
    bool m_outOpen = false;
    bool m_inOpen = false;
    bool m_failOpen = false;
    bool m_printSent = false;
};

}  // namespace d05
