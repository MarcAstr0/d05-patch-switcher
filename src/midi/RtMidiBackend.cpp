#include "midi/RtMidiBackend.h"

#include <RtMidi.h>

#include <QMetaObject>

#include <utility>
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
QStringList listPorts(Rt *rt, RtMidiBackend::ErrorSink &sink, QString *err)
{
    QStringList names;
    if (!rt)
        return names;
    sink.take();
    try {
        const unsigned n = rt->getPortCount();
        for (unsigned i = 0; i < n; ++i)
            names << QString::fromStdString(rt->getPortName(i));
        names = MidiBackend::withoutOwnPorts(names);
    } catch (const RtMidiError &e) {
        sink.record(errText(e));
    }
    const QString e = sink.take();
    if (!e.isEmpty()) {
        if (err)
            *err = e;
        return {};
    }
    return names;
}

void errorCallback(RtMidiError::Type, const std::string &text, void *userData)
{
    static_cast<RtMidiBackend::ErrorSink *>(userData)->record(QString::fromStdString(text));
}

// Runs on RtMidi's thread. Only touches the backend through a queued call.
void inputCallback(double, std::vector<unsigned char> *message, void *userData)
{
    if (!message || message->empty())
        return;
    auto *self = static_cast<RtMidiBackend *>(userData);
    // Drop everything nobody wants (clock, notes, other channels, other CCs) before the first
    // allocation: this reads the vector RtMidi owns and one atomic, nothing else.
    if (!MidiBackend::accepts(self->acceptMask(), message->data(), message->size()))
        return;
    const quint64 gen = self->inputGeneration();  // stamped here, on RtMidi's thread
    const QByteArray bytes(reinterpret_cast<const char *>(message->data()), qsizetype(message->size()));
    QMetaObject::invokeMethod(self, [self, bytes, gen] { emit self->received(bytes, gen); }, Qt::QueuedConnection);
}

}  // namespace

void RtMidiBackend::ErrorSink::record(const QString &t)
{
    QMutexLocker l(&mutex);
    if (text.isEmpty())
        text = t;
}

QString RtMidiBackend::ErrorSink::take()
{
    QMutexLocker l(&mutex);
    return std::exchange(text, QString());
}

RtMidiBackend::RtMidiBackend(SimulateFailure simulate, QObject *parent)
    : MidiBackend(parent)
{
    const bool failOut = simulate == SimulateFailure::Output || simulate == SimulateFailure::Both;
    const bool failIn = simulate == SimulateFailure::Input || simulate == SimulateFailure::Both;
    // Output and input are set up independently: a broken input side must not
    // take a working output down with it.
    try {
        if (failOut)
            throw RtMidiError("simulated output failure", RtMidiError::DRIVER_ERROR);
        m_out = std::make_unique<RtMidiOut>(RtMidi::UNSPECIFIED, MidiBackend::ownClientName().toStdString());
        // With a callback installed RtMidi no longer throws or prints; every
        // operation below checks the sink afterwards.
        m_out->setErrorCallback(&errorCallback, &m_outErrors);
    } catch (const RtMidiError &e) {
        m_outInitError = errText(e);
        m_out.reset();
    }
    try {
        if (failIn)
            throw RtMidiError("simulated input failure", RtMidiError::DRIVER_ERROR);
        m_in = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, MidiBackend::ownClientName().toStdString());
        m_in->ignoreTypes(true, true, true);  // sysex, timing, active sensing
        m_in->setErrorCallback(&errorCallback, &m_inErrors);
    } catch (const RtMidiError &e) {
        m_inInitError = errText(e);
        m_in.reset();
    }
}

RtMidiBackend::~RtMidiBackend()
{
    closeInput();  // cancels the callback before the object goes away
    closeOutput();
}

QStringList RtMidiBackend::outputPortNames(QString *err)
{
    if (!m_out) {
        if (err)
            *err = m_outInitError;
        return {};
    }
    return listPorts(m_out.get(), m_outErrors, err);
}

QStringList RtMidiBackend::inputPortNames(QString *err)
{
    if (!m_in) {
        if (err)
            *err = m_inInitError;
        return {};
    }
    return listPorts(m_in.get(), m_inErrors, err);
}

bool RtMidiBackend::openOutput(const QString &name, QString *error)
{
    closeOutput();
    if (!m_out) {
        if (error)
            *error = m_outInitError;
        return false;
    }
    try {
        const int idx = MidiBackend::isOwnPort(name) ? -1 : indexOfPort(*m_out, name);
        if (idx < 0) {
            if (error)
                *error = QStringLiteral("port not found");
            return false;
        }
        m_outErrors.take();
        m_out->openPort(unsigned(idx), "d05-patch-switcher out");
        const QString e = m_outErrors.take();
        if (!e.isEmpty() || !m_out->isPortOpen()) {
            closeOutput();
            if (error)
                *error = e.isEmpty() ? QStringLiteral("could not open port") : e;
            return false;
        }
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
            *error = m_inInitError;
        return false;
    }
    try {
        const int idx = MidiBackend::isOwnPort(name) ? -1 : indexOfPort(*m_in, name);
        if (idx < 0) {
            if (error)
                *error = QStringLiteral("port not found");
            return false;
        }
        m_inErrors.take();
        m_in->setCallback(&inputCallback, this);
        m_in->openPort(unsigned(idx), "d05-patch-switcher in");
        const QString e = m_inErrors.take();
        if (!e.isEmpty() || !m_in->isPortOpen()) {
            closeInput();
            if (error)
                *error = e.isEmpty() ? QStringLiteral("could not open port") : e;
            return false;
        }
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
        m_outErrors.take();
        m_out->sendMessage(reinterpret_cast<const unsigned char *>(message.constData()),
                           size_t(message.size()));
        // ALSA reports a dropped event as a warning (callback), not an exception.
        const QString e = m_outErrors.take();
        if (!e.isEmpty()) {
            if (error)
                *error = e;
            return false;
        }
        return true;
    } catch (const RtMidiError &e) {
        if (error)
            *error = errText(e);
        return false;
    }
}

}  // namespace d05
