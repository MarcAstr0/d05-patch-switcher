#pragma once

#include "midi/MidiBackend.h"

#include <QList>
#include <QMutex>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace d05 {

// In-memory backend. Default ports: output "Fake D-05 In" (the synth's input),
// input "Fake D-05 Out" (the synth's output).
class FakeMidiBackend : public MidiBackend {
    Q_OBJECT
public:
    explicit FakeMidiBackend(QObject *parent = nullptr);
    ~FakeMidiBackend() override;

    QStringList outputPortNames(QString *err) override;
    QStringList inputPortNames(QString *err) override;
    bool openOutput(const QString &name, QString *error) override;
    void closeOutput() override;
    bool isOutputOpen() const override { return m_outOpen; }
    bool openInput(const QString &name, QString *error) override;
    void closeInput() override;
    bool isInputOpen() const override { return m_inOpen; }
    bool send(const QByteArray &message, QString *error) override;

    // Test controls.
    // Close the open port if it vanished; emit outputLost / inputLost when `notify`.
    void setOutputPorts(const QStringList &names, bool notify = true);
    void setInputPorts(const QStringList &names, bool notify = true);
    // Make enumeration fail with this text (empty = work again).
    void setListError(const QString &text) { m_listError = text; }
    void setFailOpen(bool fail) { m_failOpen = fail; }
    // Sends fail ("send failed") while the port itself stays open.
    void setFailSend(bool fail) { m_failSend = fail; }
    const QList<QByteArray> &sentMessages() const { return m_sent; }
    void clearSent() { m_sent.clear(); }
    QString openOutputName() const { return m_outName; }
    // Print every sent message as hex on stderr (--fake-midi).
    void setPrintSent(bool on) { m_printSent = on; }
    // Callable from any thread: stamps the input generation, then emits received() through the
    // event queue, as the real backend does.
    void injectReceived(const QByteArray &message);

    // --fake-clock: the backend lists three ports each way (D-05, JP-08, JU-06A: "Fake <X> In" /
    // "Fake <X> Out"), and while an input is open a non-GUI thread feeds it MIDI clock (0xF8) at
    // `bpm` (24 pulses per quarter note), through the same path as RtMidi's callback: the filter
    // drops the byte before anything is allocated. Call before opening ports.
    void setFakeClock(bool on, double bpm = 120.0);
    // Test counters, readable from any thread: clock bytes the thread produced, and events that
    // actually went onto the main thread's queue (clock never adds to this).
    quint64 clockTicks() const { return m_clockTicks.load(std::memory_order_relaxed); }
    quint64 queuedEvents() const { return m_queued.load(std::memory_order_relaxed); }

private:
    // The "input callback": runs on the input thread. Filter first, allocate only if wanted.
    void inputBytes(const unsigned char *data, size_t size);
    void startClock();
    void stopClock();

    QStringList m_outputs;
    QStringList m_inputs;
    QList<QByteArray> m_sent;
    QString m_outName;
    QString m_inName;
    QString m_listError;
    bool m_outOpen = false;
    bool m_inOpen = false;
    bool m_failOpen = false;
    bool m_failSend = false;
    bool m_printSent = false;
    bool m_fakeClock = false;
    double m_bpm = 120.0;
    std::atomic<quint64> m_clockTicks{0};
    std::atomic<quint64> m_queued{0};
    std::thread m_clockThread;
    std::mutex m_clockMutex;
    std::condition_variable m_clockCv;
    bool m_clockStop = false;
};

}  // namespace d05
