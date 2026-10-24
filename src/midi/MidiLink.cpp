#include "midi/MidiLink.h"

#include "midi/MidiBackend.h"

#include <QElapsedTimer>

#include <memory>

namespace d05 {

namespace {

// Case-insensitive; exact match wins over substring. Returns "" when none.
QString matchPort(const QStringList &ports, const QString &wanted)
{
    if (wanted.isEmpty())
        return {};
    for (const QString &p : ports) {
        if (p == wanted)
            return p;
    }
    for (const QString &p : ports) {
        if (p.compare(wanted, Qt::CaseInsensitive) == 0)
            return p;
    }
    for (const QString &p : ports) {
        if (p.contains(wanted, Qt::CaseInsensitive))
            return p;
    }
    return {};
}

}  // namespace

MidiLink::MidiLink(MidiBackend *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_timer(this)
    , m_status(QStringLiteral("Port not found"))
{
    auto clock = std::make_shared<QElapsedTimer>();
    clock->start();
    m_clock = [clock] { return clock->elapsed(); };

    m_timer.setInterval(DefaultPollMs);
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &MidiLink::pollNow);
    connect(m_backend, &MidiBackend::received, this, &MidiLink::onReceived, Qt::QueuedConnection);
    connect(m_backend, &MidiBackend::outputLost, this, &MidiLink::onOutputLost);
}

void MidiLink::setStatus(const QString &s)
{
    if (s == m_status)
        return;
    m_status = s;
    emit statusChanged(m_status);
}

void MidiLink::setPollIntervalMs(int ms)
{
    m_timer.setInterval(ms);  // restarts a running timer
}

void MidiLink::setOutputPort(const QString &name)
{
    m_outName = name;
    connectOutput();
    updatePolling();
}

void MidiLink::setInputPort(const QString &name)
{
    m_inName = name;
    applyInput();
    updatePolling();
}

void MidiLink::setFollowEnabled(bool on)
{
    m_follow = on;
    applyInput();
    updatePolling();
}

void MidiLink::setChannel(int channel)
{
    m_channel = qBound(1, channel, 16);
}

void MidiLink::setCurrent(const PatchAddress &a)
{
    m_current = a;
}

bool MidiLink::connectOutput()
{
    const bool was = m_connected;
    m_backend->closeOutput();
    m_connected = false;
    m_connectedName.clear();

    bool ok = false;
    if (!outputWanted()) {
        setStatus(QStringLiteral("Port not found"));
    } else {
        const QString port = matchPort(m_backend->outputPortNames(), m_outName);
        if (port.isEmpty()) {
            setStatus(QStringLiteral("Port not found"));
        } else {
            QString err;
            if (m_backend->openOutput(port, &err)) {
                m_connected = true;
                m_connectedName = port;
                ok = true;
                setStatus(QStringLiteral("Connected to ") + port);
            } else {
                setStatus(QStringLiteral("Error: ") + err);
            }
        }
    }
    if (was != m_connected)
        emit connectedChanged(m_connected);
    return ok;
}

bool MidiLink::applyInput()
{
    m_backend->closeInput();
    m_rxMsb = m_rxLsb = -1;
    if (!inputWanted()) {
        m_inputStatus.clear();
        return false;
    }
    const QString port = matchPort(m_backend->inputPortNames(), m_inName);
    if (port.isEmpty()) {
        m_inputStatus = QStringLiteral("Port not found");
        return false;
    }
    QString err;
    if (!m_backend->openInput(port, &err)) {
        m_inputStatus = QStringLiteral("Error: ") + err;
        return false;
    }
    m_inputStatus = QStringLiteral("Connected to ") + port;
    return true;
}

void MidiLink::updatePolling()
{
    const bool need = (outputWanted() && !m_connected) || (inputWanted() && !m_backend->isInputOpen());
    if (need && !m_timer.isActive())
        m_timer.start();
    else if (!need && m_timer.isActive())
        m_timer.stop();
}

void MidiLink::pollNow()
{
    if (outputWanted() && !m_connected)
        connectOutput();
    if (inputWanted() && !m_backend->isInputOpen())
        applyInput();
    updatePolling();
}

void MidiLink::dropOutput(const QString &status)
{
    m_backend->closeOutput();
    const bool was = m_connected;
    m_connected = false;
    m_connectedName.clear();
    setStatus(status);
    if (was)
        emit connectedChanged(false);
    updatePolling();
}

void MidiLink::onOutputLost()
{
    if (m_connected)
        dropOutput(QStringLiteral("Port not found"));
}

QList<QByteArray> MidiLink::patchMessages(const PatchAddress &a, int channel)
{
    const int ch = qBound(1, channel, 16) - 1;
    const auto t = a.midiTriplet();
    auto msg = [](int status, int d1, int d2 = -1) {
        QByteArray b;
        b.append(char(status));
        b.append(char(d1));
        if (d2 >= 0)
            b.append(char(d2));
        return b;
    };
    return {msg(0xB0 | ch, 0x00, t.msb), msg(0xB0 | ch, 0x20, t.lsb), msg(0xC0 | ch, t.program)};
}

bool MidiLink::sendPatch(const PatchAddress &a)
{
    if (!m_connected)
        return false;
    for (const QByteArray &m : patchMessages(a, m_channel)) {
        QString err;
        if (!m_backend->send(m, &err)) {
            dropOutput(QStringLiteral("Error: ") + err);
            return false;
        }
    }
    m_lastSent = a.midiTriplet();
    m_lastSentAt = m_clock();
    m_current = a;
    emit sent(a);
    return true;
}

void MidiLink::onReceived(const QByteArray &message)
{
    if (!m_follow || message.size() < 2)
        return;
    const int status = uchar(message[0]);
    const int type = status & 0xF0;
    if ((status & 0x0F) != m_channel - 1)
        return;
    const int d1 = uchar(message[1]);

    if (type == 0xB0 && message.size() >= 3) {
        const int v = uchar(message[2]);
        if (d1 == 0)
            m_rxMsb = v;
        else if (d1 == 32)
            m_rxLsb = v;
        return;
    }
    if (type != 0xC0)
        return;

    // Program Change completes the triplet. Without a prior Bank Select the
    // D-05 stays in its current group, so fall back to our current group.
    const int msb = m_rxMsb < 0 ? PatchAddress::BankSelectMsb : m_rxMsb;
    int lsb = m_rxLsb;
    if (lsb < 0 && m_current)
        lsb = m_current->group();
    if (msb != PatchAddress::BankSelectMsb || lsb < 0 || d1 > 63)
        return;
    const auto addr = PatchAddress::fromParts(lsb, d1 / 8 + 1, d1 % 8 + 1);
    if (!addr)
        return;
    if (m_lastSent && *m_lastSent == addr->midiTriplet() && m_clock() - m_lastSentAt < m_echoMs)
        return;  // our own send looped back
    if (m_current && *m_current == *addr)
        return;
    m_current = addr;
    emit currentChanged(*addr);
}

}  // namespace d05
