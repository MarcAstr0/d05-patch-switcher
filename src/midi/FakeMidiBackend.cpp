#include "midi/FakeMidiBackend.h"

#include <QMetaObject>

#include <chrono>
#include <cstdio>

namespace d05 {

FakeMidiBackend::FakeMidiBackend(QObject *parent)
    : MidiBackend(parent)
    , m_outputs{QStringLiteral("Fake D-05 In")}
    , m_inputs{QStringLiteral("Fake D-05 Out")}
{
}

FakeMidiBackend::~FakeMidiBackend()
{
    stopClock();
}

void FakeMidiBackend::setFakeClock(bool on, double bpm)
{
    m_fakeClock = on;
    m_bpm = bpm > 0 ? bpm : 120.0;
    if (on) {
        m_outputs = {QStringLiteral("Fake D-05 In"), QStringLiteral("Fake JP-08 In"),
                     QStringLiteral("Fake JU-06A In")};
        m_inputs = {QStringLiteral("Fake D-05 Out"), QStringLiteral("Fake JP-08 Out"),
                    QStringLiteral("Fake JU-06A Out")};
    }
}

void FakeMidiBackend::startClock()
{
    stopClock();
    if (!m_fakeClock)
        return;
    {
        std::lock_guard<std::mutex> l(m_clockMutex);
        m_clockStop = false;
    }
    m_clockThread = std::thread([this] {
        using clock = std::chrono::steady_clock;
        const auto period = std::chrono::duration_cast<clock::duration>(
            std::chrono::duration<double>(60.0 / (m_bpm * 24.0)));
        auto next = clock::now() + period;
        std::unique_lock<std::mutex> l(m_clockMutex);
        while (!m_clockStop) {
            if (m_clockCv.wait_until(l, next, [this] { return m_clockStop; }))
                break;
            next += period;
            static const unsigned char tick = 0xF8;
            m_clockTicks.fetch_add(1, std::memory_order_relaxed);
            inputBytes(&tick, 1);
        }
    });
}

void FakeMidiBackend::stopClock()
{
    if (!m_clockThread.joinable())
        return;
    {
        std::lock_guard<std::mutex> l(m_clockMutex);
        m_clockStop = true;
    }
    m_clockCv.notify_all();
    m_clockThread.join();
}

QStringList FakeMidiBackend::outputPortNames(QString *err)
{
    if (!m_listError.isEmpty()) {
        if (err)
            *err = m_listError;
        return {};
    }
    return withoutOwnPorts(m_outputs);
}

QStringList FakeMidiBackend::inputPortNames(QString *err)
{
    if (!m_listError.isEmpty()) {
        if (err)
            *err = m_listError;
        return {};
    }
    return withoutOwnPorts(m_inputs);
}

bool FakeMidiBackend::openOutput(const QString &name, QString *error)
{
    closeOutput();
    if (m_failOpen || !m_outputs.contains(name)) {
        if (error)
            *error = m_failOpen ? QStringLiteral("open failed") : QStringLiteral("no such port");
        return false;
    }
    m_outName = name;
    m_outOpen = true;
    return true;
}

void FakeMidiBackend::closeOutput()
{
    m_outOpen = false;
    m_outName.clear();
}

bool FakeMidiBackend::openInput(const QString &name, QString *error)
{
    closeInput();
    if (m_failOpen || !m_inputs.contains(name)) {
        if (error)
            *error = m_failOpen ? QStringLiteral("open failed") : QStringLiteral("no such port");
        return false;
    }
    m_inName = name;
    m_inOpen = true;
    startClock();
    return true;
}

void FakeMidiBackend::closeInput()
{
    stopClock();
    m_inOpen = false;
    m_inName.clear();
}

bool FakeMidiBackend::send(const QByteArray &message, QString *error)
{
    if (!m_outOpen) {
        if (error)
            *error = QStringLiteral("output port is not open");
        return false;
    }
    if (m_failSend) {
        if (error)
            *error = QStringLiteral("send failed");
        return false;
    }
    m_sent.append(message);
    if (m_printSent)
        std::fprintf(stderr, "MIDI out: %s\n", message.toHex(' ').constData());
    return true;
}

void FakeMidiBackend::setOutputPorts(const QStringList &names, bool notify)
{
    m_outputs = names;
    if (m_outOpen && !m_outputs.contains(m_outName)) {
        closeOutput();
        if (notify)
            emit outputLost();
    }
}

void FakeMidiBackend::setInputPorts(const QStringList &names, bool notify)
{
    m_inputs = names;
    if (m_inOpen && !m_inputs.contains(m_inName)) {
        closeInput();
        if (notify)
            emit inputLost();
    }
}

void FakeMidiBackend::injectReceived(const QByteArray &message)
{
    inputBytes(reinterpret_cast<const unsigned char *>(message.constData()), size_t(message.size()));
}

void FakeMidiBackend::inputBytes(const unsigned char *data, size_t size)
{
    // The same filter the real backend applies on RtMidi's thread, before the first allocation.
    if (!accepts(acceptMask(), data, size))
        return;
    // Like the real backend: stamp now (the "input thread"), deliver through the event queue.
    const quint64 gen = inputGeneration();
    const QByteArray message(reinterpret_cast<const char *>(data), qsizetype(size));
    m_queued.fetch_add(1, std::memory_order_relaxed);
    QMetaObject::invokeMethod(this, [this, message, gen] { emit received(message, gen); },
                              Qt::QueuedConnection);
}

}  // namespace d05
