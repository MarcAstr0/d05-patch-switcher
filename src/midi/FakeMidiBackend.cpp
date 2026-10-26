#include "midi/FakeMidiBackend.h"

#include <QMetaObject>

#include <cstdio>

namespace d05 {

FakeMidiBackend::FakeMidiBackend(QObject *parent)
    : MidiBackend(parent)
    , m_outputs{QStringLiteral("Fake D-05 In")}
    , m_inputs{QStringLiteral("Fake D-05 Out")}
{
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
    return true;
}

void FakeMidiBackend::closeInput()
{
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
    // The same filter the real backend applies on RtMidi's thread.
    if (!accepts(acceptMask(), reinterpret_cast<const unsigned char *>(message.constData()),
                 size_t(message.size())))
        return;
    // Like the real backend: stamp now (the "input thread"), deliver through the event queue.
    const quint64 gen = inputGeneration();
    QMetaObject::invokeMethod(this, [this, message, gen] { emit received(message, gen); },
                              Qt::QueuedConnection);
}

}  // namespace d05
