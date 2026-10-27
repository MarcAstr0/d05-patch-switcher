#include "midi/MidiLink.h"

#include "midi/MidiBackend.h"
#include "midi/PortHub.h"

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

QString MidiLink::portSpec(const QString &name, const QStringList &ports)
{
    const QString key = portKey(name);
    int same = 0;
    for (const QString &p : ports)
        if (portKey(p) == key)
            ++same;
    return same > 1 ? name : key;
}

MidiLink::MidiLink(MidiBackend *backend, QObject *parent)
    : MidiLink(new PortHub(backend), Model::D05, parent)
{
    m_hub->setParent(this);  // the private hub lives and dies with the link
}

MidiLink::MidiLink(PortHub *hub, Model model, QObject *parent)
    : QObject(parent)
    , m_hub(hub)
    , m_model(model)
    , m_timer(this)
    , m_status(QStringLiteral("No port selected"))
{
    auto clock = std::make_shared<QElapsedTimer>();
    clock->start();
    m_clock = [clock] { return clock->elapsed(); };

    m_timer.setInterval(DefaultPollMs);
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &MidiLink::pollNow);

    PortHub::Client c;
    c.received = [this](const QByteArray &m) { onReceived(m); };
    c.outputLost = [this] { onOutputLost(); };
    c.inputLost = [this] { onInputLost(); };
    m_hub->attach(this, std::move(c));
}

MidiLink::~MidiLink()
{
    if (m_hub)
        m_hub->detach(this);
}

void MidiLink::setExternalPolling(bool on)
{
    m_externalPoll = on;
    updatePolling();
}

QStringList MidiLink::outputPortNames(QString *err) const
{
    QString e;
    const QStringList list = m_hub->outputPortNames(&e);
    if (err)
        *err = e;
    return list;
}

QStringList MidiLink::inputPortNames(QString *err) const
{
    QString e;
    const QStringList list = m_hub->inputPortNames(&e);
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
    m_hub->setInputChannel(this, m_channel);
}

void MidiLink::setCurrent(const PatchAddress &a)
{
    m_current = a;
}

bool MidiLink::connectOutput()
{
    const bool was = m_connected;
    m_hub->releaseOutput(this);
    m_connected = false;
    m_connectedName.clear();

    bool ok = false;
    if (!outputWanted()) {
        setStatus(QStringLiteral("No port selected"));
    } else {
        QString listErr;
        const QStringList ports = m_hub->outputPortNames(&listErr);
        const QString port = matchPort(ports, m_outName);
        if (!listErr.isEmpty()) {
            setStatus(QStringLiteral("Error: ") + listErr);
        } else if (port.isEmpty()) {
            setStatus(QStringLiteral("Port not found"));
        } else if (const QString veto = m_openGuard ? m_openGuard(port, false) : QString(); !veto.isEmpty()) {
            setStatus(veto);
        } else {
            QString err;
            if (m_hub->acquireOutput(this, port, &err)) {
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
    m_hub->releaseInput(this);
    setConnectedIn(QString());
    m_rxMsb = m_rxLsb = -1;
    if (!inputWanted()) {
        setInputStatus(QString());
        return false;
    }
    QString listErr;
    const QStringList ports = m_hub->inputPortNames(&listErr);
    if (!listErr.isEmpty()) {
        setInputStatus(QStringLiteral("Error: ") + listErr);
        return false;
    }
    const QString port = matchPort(ports, m_inName);
    if (port.isEmpty()) {
        setInputStatus(QStringLiteral("Port not found"));
        return false;
    }
    if (const QString veto = m_openGuard ? m_openGuard(port, true) : QString(); !veto.isEmpty()) {
        setInputStatus(veto);
        return false;
    }
    QString err;
    if (!m_hub->acquireInput(this, port, m_channel, m_model == Model::D05, &err)) {
        setInputStatus(QStringLiteral("Error: ") + err);
        return false;
    }
    setConnectedIn(port);
    setInputStatus(QStringLiteral("Connected to ") + portKey(port));
    return true;
}

void MidiLink::setConnectedIn(const QString &name)
{
    const bool was = !m_connectedInName.isEmpty();
    m_connectedInName = name;
    if (was != !name.isEmpty())
        emit inputConnectedChanged(!name.isEmpty());
}

bool MidiLink::inputOpenNow() const
{
    return m_hub->isInputOpen(this);
}

void MidiLink::updatePolling()
{
    const bool need = (outputWanted() && !m_connected) || (inputWanted() && !inputOpenNow());
    if (need != m_needPoll) {
        m_needPoll = need;
        emit pollNeededChanged(need);
    }
    if (need && !m_externalPoll && !m_timer.isActive())
        m_timer.start();
    else if ((!need || m_externalPoll) && m_timer.isActive())
        m_timer.stop();
}

void MidiLink::pollNow()
{
    if (outputWanted() && !m_connected)
        connectOutput();
    if (inputWanted() && !inputOpenNow())
        applyInput();
    updatePolling();
}

void MidiLink::dropOutput(const QString &status)
{
    // The port is shared: close it for every user so none keeps a stale handle and the poll
    // reopens it for all of them.
    m_hub->invalidateOutput(this);
    m_hub->releaseOutput(this);
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
    m_hub->invalidateInput(this);
    m_hub->releaseInput(this);
    setConnectedIn(QString());
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
        const QStringList ports = m_hub->outputPortNames(&err);
        if (!err.isEmpty())
            dropOutput(QStringLiteral("Error: ") + err);
        else if (!ports.contains(m_connectedName))
            dropOutput(QStringLiteral("Port not found"));
    }
    if (!m_connectedInName.isEmpty()) {
        QString err;
        const QStringList ports = m_hub->inputPortNames(&err);
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

PatchAddress::MidiTriplet MidiLink::echoKey(const PatchAddress &a) const
{
    // The D-05 is recognised by its triplet. The 64-patch models send a lone Program Change, so
    // only the program counts (both JU-06A engines send the same bytes).
    if (m_model == Model::D05)
        return a.midiTriplet();
    return {-1, -1, a.program()};
}

bool MidiLink::sendPatch(const PatchAddress &a)
{
    if (!m_connected)
        return false;
    if (a.model() != m_model)
        return false;
    checkConnection();
    if (!m_connected)
        return false;
    const QList<QByteArray> messages = m_model == Model::D05 ? patchMessages(a, m_channel)
                                       : m_model == Model::JU06A ? a.midiMessages(m_channel, m_engine)
                                                                 : a.midiMessages(m_channel);
    if (messages.isEmpty())  // JU-06A address of the other engine: nothing may go out
        return false;
    for (const QByteArray &m : messages) {
        QString err;
        if (!m_hub->sendOn(this, m, &err)) {
            dropOutput(QStringLiteral("Error: ") + err);
            return false;
        }
    }
    const qint64 now = m_clock();
    pruneSent(now);
    m_recentSent.append({echoKey(a), now});
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

    if (m_model != Model::D05) {
        // A lone Program Change; 64..127 mean nothing on a 64-patch unit.
        if (type != 0xC0 || d1 > 63)
            return;
        pruneSent(m_clock());
        for (const SentEntry &e : std::as_const(m_recentSent)) {
            if (e.triplet.program == d1)
                return;  // our own send looped back
        }
        if (m_model == Model::JU06A) {
            emit programReceived(d1);  // the engine tracker owns what this means
            return;
        }
        const auto addr = PatchAddress::fromBankNumber(m_model, d1 / 8 + 1, d1 % 8 + 1);
        if (!addr)
            return;
        const bool changed = !m_current || *m_current != *addr;
        m_current = addr;
        if (changed)
            emit currentChanged(*addr);
        emit followReceived(*addr);
        return;
    }

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
    if (m_current && *m_current == *addr) {
        emit followReceived(*addr);
        return;
    }
    m_current = addr;
    emit currentChanged(*addr);
    emit followReceived(*addr);
}

}  // namespace d05
