#include "midi/RtMidiBackend.h"

#include <RtMidi.h>

#include <QMetaObject>

#include <vector>

namespace d05 {

namespace {

QString errText(const RtMidiError &e)
{
    return QString::fromStdString(e.getMessage());
}

template <class Rt>
int indexOfPort(Rt &rt, const QString &name)
{
    const unsigned n = rt.getPortCount();
    for (unsigned i = 0; i < n; ++i) {
        if (QString::fromStdString(rt.getPortName(i)) == name)
            return int(i);
    }
    return -1;
}

template <class Rt>
QStringList listPorts(Rt *rt)
{
    QStringList names;
    if (!rt)
        return names;
    try {
        const unsigned n = rt->getPortCount();
        for (unsigned i = 0; i < n; ++i)
            names << QString::fromStdString(rt->getPortName(i));
    } catch (const RtMidiError &) {
    }
    return names;
}

// Runs on RtMidi's thread. Only touches the backend through a queued call.
void inputCallback(double, std::vector<unsigned char> *message, void *userData)
{
    if (!message || message->empty())
        return;
    auto *self = static_cast<RtMidiBackend *>(userData);
    const QByteArray bytes(reinterpret_cast<const char *>(message->data()), qsizetype(message->size()));
    QMetaObject::invokeMethod(self, [self, bytes] { emit self->received(bytes); }, Qt::QueuedConnection);
}

}  // namespace

RtMidiBackend::RtMidiBackend(QObject *parent)
    : MidiBackend(parent)
{
    try {
        m_out = std::make_unique<RtMidiOut>();
        m_in = std::make_unique<RtMidiIn>();
        m_in->ignoreTypes(true, true, true);  // sysex, timing, active sensing
    } catch (const RtMidiError &e) {
        m_initError = errText(e);
        m_out.reset();
        m_in.reset();
    }
}

RtMidiBackend::~RtMidiBackend()
{
    closeInput();  // cancels the callback before the object goes away
    closeOutput();
}

QStringList RtMidiBackend::outputPortNames() { return listPorts(m_out.get()); }
QStringList RtMidiBackend::inputPortNames() { return listPorts(m_in.get()); }

bool RtMidiBackend::openOutput(const QString &name, QString *error)
{
    closeOutput();
    if (!m_out) {
        if (error)
            *error = m_initError;
        return false;
    }
    try {
        const int idx = indexOfPort(*m_out, name);
        if (idx < 0) {
            if (error)
                *error = QStringLiteral("port not found");
            return false;
        }
        m_out->openPort(unsigned(idx), "d05-patch-switcher out");
        return true;
    } catch (const RtMidiError &e) {
        if (error)
            *error = errText(e);
        return false;
    }
}

void RtMidiBackend::closeOutput()
{
    if (!m_out)
        return;
    try {
        if (m_out->isPortOpen())
            m_out->closePort();
    } catch (const RtMidiError &) {
    }
}

bool RtMidiBackend::isOutputOpen() const { return m_out && m_out->isPortOpen(); }

bool RtMidiBackend::openInput(const QString &name, QString *error)
{
    closeInput();
    if (!m_in) {
        if (error)
            *error = m_initError;
        return false;
    }
    try {
        const int idx = indexOfPort(*m_in, name);
        if (idx < 0) {
            if (error)
                *error = QStringLiteral("port not found");
            return false;
        }
        m_in->setCallback(&inputCallback, this);
        m_in->openPort(unsigned(idx), "d05-patch-switcher in");
        return true;
    } catch (const RtMidiError &e) {
        try {
            m_in->cancelCallback();
        } catch (const RtMidiError &) {
        }
        if (error)
            *error = errText(e);
        return false;
    }
}

void RtMidiBackend::closeInput()
{
    if (!m_in)
        return;
    try {
        if (m_in->isPortOpen())
            m_in->closePort();
        m_in->cancelCallback();
    } catch (const RtMidiError &) {
    }
}

bool RtMidiBackend::isInputOpen() const { return m_in && m_in->isPortOpen(); }

bool RtMidiBackend::send(const QByteArray &message, QString *error)
{
    if (!isOutputOpen()) {
        if (error)
            *error = QStringLiteral("output port is not open");
        return false;
    }
    try {
        m_out->sendMessage(reinterpret_cast<const unsigned char *>(message.constData()),
                           size_t(message.size()));
        return true;
    } catch (const RtMidiError &e) {
        if (error)
            *error = errText(e);
        return false;
    }
}

}  // namespace d05
