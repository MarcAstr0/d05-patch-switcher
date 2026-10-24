#include "midi/FakeMidiBackend.h"

#include <cstdio>

namespace d05 {

FakeMidiBackend::FakeMidiBackend(QObject *parent)
    : MidiBackend(parent)
    , m_outputs{QStringLiteral("Fake D-05 In")}
    , m_inputs{QStringLiteral("Fake D-05 Out")}
{
}

QStringList FakeMidiBackend::outputPortNames() { return m_outputs; }
QStringList FakeMidiBackend::inputPortNames() { return m_inputs; }

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
    m_sent.append(message);
    if (m_printSent)
        std::fprintf(stderr, "MIDI out: %s\n", message.toHex(' ').constData());
    return true;
}

void FakeMidiBackend::setOutputPorts(const QStringList &names)
{
    m_outputs = names;
    if (m_outOpen && !m_outputs.contains(m_outName)) {
        closeOutput();
        emit outputLost();
    }
}

void FakeMidiBackend::setInputPorts(const QStringList &names)
{
    m_inputs = names;
    if (m_inOpen && !m_inputs.contains(m_inName))
        closeInput();
}

void FakeMidiBackend::injectReceived(const QByteArray &message)
{
    emit received(message);
}

}  // namespace d05
