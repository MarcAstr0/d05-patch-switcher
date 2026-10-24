#include "midi/MidiLink.h"

#include "midi/MidiBackend.h"

#include <QElapsedTimer>
#include <QRegularExpression>

#include <memory>

namespace d05 {

// Exact full-name match first, then equal keys (ALSA ids stripped), then a
// case-insensitive substring of the key. Returns "" when none.
QString MidiLink::matchPort(const QStringList &ports, const QString &wanted)
{
    if (wanted.isEmpty())
        return {};
    for (const QString &p : ports) {
        if (p == wanted)
            return p;
    }
    // A saved key may itself end in digits ("Launchpad 2" on Windows, where the index was already
    // stripped): try it as it is before stripping it again.
    for (const QString &p : ports) {
        if (MidiLink::portKey(p).compare(wanted, Qt::CaseInsensitive) == 0)
            return p;
    }
    const QString key = MidiLink::portKey(wanted);
    for (const QString &p : ports) {
        if (MidiLink::portKey(p).compare(key, Qt::CaseInsensitive) == 0)
            return p;
    }
    for (const QString &p : ports) {
        if (MidiLink::portKey(p).contains(key, Qt::CaseInsensitive))
            return p;
    }
    return {};
}

QString MidiLink::stripWinMMIndex(const QString &name)
{
    static const QRegularExpression suffix(QStringLiteral("\\s\\d+$"));
    QString k = name;
    k.remove(suffix);
    return k;
}

QString MidiLink::portKey(const QString &name)
{
    static const QRegularExpression suffix(QStringLiteral("\\s+\\d+:\\d+$"));
    QString k = name;
    k.remove(suffix);
#if defined(Q_OS_WIN) || defined(D05_WINMM_PORT_KEYS)
    // WinMM names carry no ALSA suffix; their " <index>" is always present.
    if (k == name)
        k = stripWinMMIndex(name);
#endif
    return k;
}

MidiLink::MidiLink(MidiBackend *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_timer(this)
    , m_status(QStringLiteral("No port selected"))
{
    auto clock = std::make_shared<QElapsedTimer>();
    clock->start();
    m_clock = [clock] { return clock->elapsed(); };

    m_timer.setInterval(DefaultPollMs);
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &MidiLink::pollNow);
    connect(m_backend, &MidiBackend::received, this, &MidiLink::onReceived, Qt::QueuedConnection);
    connect(m_backend, &MidiBackend::outputLost, this, &MidiLink::onOutputLost);
    connect(m_backend, &MidiBackend::inputLost, this, &MidiLink::onInputLost);
}

QStringList MidiLink::outputPortNames(QString *err) const
{
    QString e;
    const QStringList list = m_backend->outputPortNames(&e);
    if (err)
        *err = e;
    return list;
}

QStringList MidiLink::inputPortNames(QString *err) const
{
    QString e;
    const QStringList list = m_backend->inputPortNames(&e);
    if (err)
        *err = e;
    return list;
}

void MidiLink::setStatus(const QString &s)
{
    if (s == m_status)
        return;
    m_status = s;
    emit statusChanged(m_status);
}

void MidiLink::setInputStatus(const QString &s)
{
    if (s == m_inputStatus)
        return;
    m_inputStatus = s;
    emit inputStatusChanged(m_inputStatus);
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
    m_rxMsb = m_rxLsb = -1;
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
        setStatus(QStringLiteral("No port selected"));
    } else {
        QString listErr;
        const QStringList ports = m_backend->outputPortNames(&listErr);
        const QString port = matchPort(ports, m_outName);
        if (!listErr.isEmpty()) {
            setStatus(QStringLiteral("Error: ") + listErr);
        } else if (port.isEmpty()) {
            setStatus(QStringLiteral("Port not found"));
        } else {
            QString err;
            if (m_backend->openOutput(port, &err)) {
                m_connected = true;
                m_connectedName = port;
                ok = true;
                setStatus(QStringLiteral("Connected to ") + portKey(port));
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
    m_connectedInName.clear();
    m_rxMsb = m_rxLsb = -1;
    if (!inputWanted()) {
        setInputStatus(QString());
        return false;
    }
    QString listErr;
    const QStringList ports = m_backend->inputPortNames(&listErr);
    if (!listErr.isEmpty()) {
        setInputStatus(QStringLiteral("Error: ") + listErr);
        return false;
    }
    const QString port = matchPort(ports, m_inName);
    if (port.isEmpty()) {
        setInputStatus(QStringLiteral("Port not found"));
        return false;
    }
    QString err;
    if (!m_backend->openInput(port, &err)) {
        setInputStatus(QStringLiteral("Error: ") + err);
        return false;
    }
    m_connectedInName = port;
    setInputStatus(QStringLiteral("Connected to ") + portKey(port));
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

void MidiLink::dropInput(const QString &status)
{
    m_backend->closeInput();
    m_connectedInName.clear();
    m_rxMsb = m_rxLsb = -1;
    setInputStatus(status);
    updatePolling();
}

void MidiLink::onInputLost()
{
    if (inputWanted())
        dropInput(QStringLiteral("Port not found"));
}

void MidiLink::checkConnection()
{
    if (m_connected) {
        QString err;
        const QStringList ports = m_backend->outputPortNames(&err);
        if (!err.isEmpty())
            dropOutput(QStringLiteral("Error: ") + err);
        else if (!ports.contains(m_connectedName))
            dropOutput(QStringLiteral("Port not found"));
    }
    if (!m_connectedInName.isEmpty()) {
        QString err;
        const QStringList ports = m_backend->inputPortNames(&err);
        if (!err.isEmpty())
            dropInput(QStringLiteral("Error: ") + err);
        else if (!ports.contains(m_connectedInName))
            dropInput(QStringLiteral("Port not found"));
    }
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
    checkConnection();
    if (!m_connected)
        return false;
    for (const QByteArray &m : patchMessages(a, m_channel)) {
        QString err;
        if (!m_backend->send(m, &err)) {
            dropOutput(QStringLiteral("Error: ") + err);
            return false;
        }
    }
    const qint64 now = m_clock();
    pruneSent(now);
    m_recentSent.append({a.midiTriplet(), now});
    m_current = a;
    emit sent(a);
    return true;
}

void MidiLink::pruneSent(qint64 now)
{
    for (qsizetype i = m_recentSent.size() - 1; i >= 0; --i) {
        if (now - m_recentSent.at(i).at >= m_echoMs)
            m_recentSent.removeAt(i);
    }
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

    // Program Change completes the triplet and consumes the bank state. Without
    // a prior Bank Select the D-05 stays in its current group, so fall back to
    // our current group.
    const int msb = m_rxMsb < 0 ? PatchAddress::BankSelectMsb : m_rxMsb;
    int lsb = m_rxLsb;
    m_rxMsb = m_rxLsb = -1;
    if (lsb < 0 && m_current)
        lsb = m_current->group();
    if (msb != PatchAddress::BankSelectMsb || lsb < 0 || d1 > 63)
        return;
    const auto addr = PatchAddress::fromParts(lsb, d1 / 8 + 1, d1 % 8 + 1);
    if (!addr)
        return;
    pruneSent(m_clock());
    for (const SentEntry &e : std::as_const(m_recentSent)) {
        if (e.triplet == addr->midiTriplet())
            return;  // our own send looped back
    }
    if (m_current && *m_current == *addr)
        return;
    m_current = addr;
    emit currentChanged(*addr);
}

}  // namespace d05
