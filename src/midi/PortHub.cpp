#include "midi/PortHub.h"

#include "midi/MidiBackend.h"

#include <algorithm>
#include <vector>

namespace {
template <class C, class P>
void eraseIf(C &c, P pred)
{
    c.erase(std::remove_if(c.begin(), c.end(), pred), c.end());
}
}  // namespace

namespace d05 {

PortHub::PortHub(MidiBackend *backend, QObject *parent) : QObject(parent)
{
    adopt(backend, false);
}

PortHub::PortHub(BackendFactory factory, QObject *parent) : QObject(parent), m_factory(std::move(factory)) {}

PortHub::~PortHub()
{
    for (const Backend &b : std::as_const(m_backends)) {
        if (!b.b)
            continue;
        disconnect(b.b, nullptr, this, nullptr);
        b.b->closeInput();
        b.b->closeOutput();
        if (b.owned)
            delete b.b;
    }
}

void PortHub::adopt(MidiBackend *b, bool owned)
{
    m_backends.append({b, owned});
    // received() is emitted on the main thread (the backend queued it) and carries the input
    // generation the backend stamped on RtMidi's thread; onReceived drops stale ones.
    connect(b, &MidiBackend::received, this,
            [this, b](const QByteArray &m, quint64 g) { onReceived(b, g, m); });
    connect(b, &MidiBackend::outputLost, this, [this, b] { onOutputLost(b); });
    connect(b, &MidiBackend::inputLost, this, [this, b] { onInputLost(b); });
    b->setAcceptMask(0);  // nothing registered yet
}

PortHub::Reg *PortHub::reg(const QObject *owner)
{
    for (auto &r : m_regs)
        if (r->owner == owner && !r->dead)
            return r.get();
    return nullptr;
}

const PortHub::Reg *PortHub::reg(const QObject *owner) const
{
    for (const auto &r : m_regs)
        if (r->owner == owner && !r->dead)
            return r.get();
    return nullptr;
}

void PortHub::attach(const QObject *owner, Client client)
{
    if (Reg *r = reg(owner)) {
        r->client = std::move(client);
        return;
    }
    auto r = std::make_unique<Reg>();
    r->owner = owner;
    r->client = std::move(client);
    m_regs.push_back(std::move(r));
}

void PortHub::detach(const QObject *owner)
{
    releaseOutput(owner);
    releaseInput(owner);
    if (m_dispatching > 0) {
        // A client callback may be running right now (possibly this very one): keep the Reg and
        // its functors alive until the dispatch is over.
        if (Reg *r = reg(owner))
            r->dead = true;
        return;
    }
    eraseIf(m_regs, [owner](const std::unique_ptr<Reg> &r) { return r->owner == owner; });
}

// Counts the callbacks being dispatched; sweeps the registrations detached meanwhile at the end.
struct PortHub::DispatchGuard {
    explicit DispatchGuard(PortHub *h) : hub(h) { ++hub->m_dispatching; }
    ~DispatchGuard()
    {
        if (--hub->m_dispatching == 0)
            eraseIf(hub->m_regs, [](const std::unique_ptr<Reg> &r) { return r->dead; });
    }
    PortHub *hub;
};

void PortHub::endInput(Port *p)
{
    p->b->setInputGeneration(0);
    p->b->setAcceptMask(0);
    p->b->closeInput();
    eraseIf(m_ins, [p](const std::unique_ptr<Port> &q) { return q.get() == p; });
}

MidiBackend *PortHub::lister()
{
    if (m_backends.isEmpty() && m_factory) {
        if (MidiBackend *b = m_factory())
            adopt(b, true);
    }
    return m_backends.isEmpty() ? nullptr : m_backends.first().b;
}

QStringList PortHub::outputPortNames(QString *err)
{
    if (err)
        err->clear();
    MidiBackend *b = lister();
    if (!b) {
        if (err)
            *err = QStringLiteral("no MIDI backend");
        return {};
    }
    return b->outputPortNames(err);
}

QStringList PortHub::inputPortNames(QString *err)
{
    if (err)
        err->clear();
    MidiBackend *b = lister();
    if (!b) {
        if (err)
            *err = QStringLiteral("no MIDI backend");
        return {};
    }
    return b->inputPortNames(err);
}

MidiBackend *PortHub::backendFor(bool input)
{
    for (const Backend &bk : std::as_const(m_backends)) {
        bool used = false;
        if (input) {
            for (const auto &p : m_ins)
                used = used || p->b == bk.b;
        } else {
            for (const auto &p : m_outs)
                used = used || p->b == bk.b;
        }
        if (!used)
            return bk.b;
    }
    if (m_factory) {
        if (MidiBackend *b = m_factory()) {
            adopt(b, true);
            return b;
        }
    }
    return nullptr;
}

bool PortHub::acquireOutput(const QObject *owner, const QString &osName, QString *error)
{
    Reg *r = reg(owner);
    if (!r) {
        if (error)
            *error = QStringLiteral("link is not attached");
        return false;
    }
    releaseOutput(owner);
    for (auto &p : m_outs) {
        if (p->name != osName)
            continue;
        // Shared: join it. If it was closed behind our back (no notification), reopen it first.
        if (p->b->isOutputOpen() || p->b->openOutput(osName, error)) {
            p->users.append(owner);
            r->out = p.get();
            return true;
        }
        return false;
    }
    MidiBackend *b = backendFor(false);
    if (!b) {
        if (error)
            *error = QStringLiteral("no free MIDI output");
        return false;
    }
    if (!b->openOutput(osName, error))
        return false;
    auto port = std::make_unique<Port>();
    port->name = osName;
    port->b = b;
    port->users.append(owner);
    r->out = port.get();
    m_outs.push_back(std::move(port));
    return true;
}

void PortHub::releaseOutput(const QObject *owner)
{
    Reg *r = reg(owner);
    if (!r || !r->out)
        return;
    Port *p = r->out;
    r->out = nullptr;
    p->users.removeAll(owner);
    if (p->users.isEmpty()) {
        p->b->closeOutput();
        eraseIf(m_outs, [p](const std::unique_ptr<Port> &q) { return q.get() == p; });
    }
}

void PortHub::invalidateOutput(const QObject *owner)
{
    Reg *r = reg(owner);
    if (!r || !r->out)
        return;
    Port *p = r->out;
    const QList<const QObject *> users = p->users;
    for (const QObject *o : users)
        if (Reg *u = reg(o))
            u->out = nullptr;
    p->b->closeOutput();
    eraseIf(m_outs, [p](const std::unique_ptr<Port> &q) { return q.get() == p; });
    DispatchGuard guard(this);
    for (const QObject *o : users) {
        const Reg *u = reg(o);
        if (o != owner && u && u->client.outputLost)
            u->client.outputLost();
    }
}

void PortHub::invalidateInput(const QObject *owner)
{
    Reg *r = reg(owner);
    if (!r || !r->in)
        return;
    Port *p = r->in;
    const QList<const QObject *> users = p->users;
    for (const QObject *o : users)
        if (Reg *u = reg(o))
            u->in = nullptr;
    endInput(p);
    DispatchGuard guard(this);
    for (const QObject *o : users) {
        const Reg *u = reg(o);
        if (o != owner && u && u->client.inputLost)
            u->client.inputLost();
    }
}

void PortHub::refreshMask(const Port *in)
{
    quint32 mask = 0;
    for (const auto &r : m_regs) {
        if (r->in != in)
            continue;
        const unsigned ch = unsigned(r->channel - 1);
        mask |= 1u << ch;
        if (r->bankSelect)
            mask |= 1u << (16 + ch);
    }
    in->b->setAcceptMask(mask);
}

bool PortHub::acquireInput(const QObject *owner, const QString &osName, int channel, bool wantBankSelect,
                           QString *error)
{
    Reg *r = reg(owner);
    if (!r) {
        if (error)
            *error = QStringLiteral("link is not attached");
        return false;
    }
    releaseInput(owner);
    r->channel = std::clamp(channel, 1, 16);
    r->bankSelect = wantBankSelect;
    for (auto &p : m_ins) {
        if (p->name != osName)
            continue;
        if (p->b->isInputOpen() || p->b->openInput(osName, error)) {
            p->users.append(owner);
            r->in = p.get();
            refreshMask(p.get());
            return true;
        }
        r->in = nullptr;
        return false;
    }
    MidiBackend *b = backendFor(true);
    if (!b) {
        if (error)
            *error = QStringLiteral("no free MIDI input");
        return false;
    }
    b->setAcceptMask(0);
    const quint64 gen = ++m_nextGen;
    b->setInputGeneration(gen);  // before the port opens, so its first message carries it
    if (!b->openInput(osName, error)) {
        b->setInputGeneration(0);
        return false;
    }
    auto port = std::make_unique<Port>();
    port->name = osName;
    port->b = b;
    port->gen = gen;
    port->users.append(owner);
    r->in = port.get();
    m_ins.push_back(std::move(port));
    refreshMask(r->in);
    return true;
}

void PortHub::releaseInput(const QObject *owner)
{
    Reg *r = reg(owner);
    if (!r || !r->in)
        return;
    Port *p = r->in;
    r->in = nullptr;
    p->users.removeAll(owner);
    if (p->users.isEmpty()) {
        endInput(p);
    } else {
        refreshMask(p);
    }
}

void PortHub::setInputChannel(const QObject *owner, int channel)
{
    Reg *r = reg(owner);
    if (!r)
        return;
    r->channel = std::clamp(channel, 1, 16);
    if (r->in)
        refreshMask(r->in);
}

bool PortHub::sendOn(const QObject *owner, const QByteArray &message, QString *error)
{
    const Reg *r = reg(owner);
    if (!r || !r->out) {
        if (error)
            *error = QStringLiteral("output port is not open");
        return false;
    }
    return r->out->b->send(message, error);
}

bool PortHub::isOutputOpen(const QObject *owner) const
{
    const Reg *r = reg(owner);
    return r && r->out && r->out->b->isOutputOpen();
}

bool PortHub::isInputOpen(const QObject *owner) const
{
    const Reg *r = reg(owner);
    return r && r->in && r->in->b->isInputOpen();
}

int PortHub::openInputCount() const
{
    int n = 0;
    for (const auto &p : m_ins)
        n += p->b->isInputOpen() ? 1 : 0;
    return n;
}

int PortHub::openOutputCount() const
{
    int n = 0;
    for (const auto &p : m_outs)
        n += p->b->isOutputOpen() ? 1 : 0;
    return n;
}

void PortHub::onReceived(MidiBackend *b, quint64 gen, const QByteArray &message)
{
    if (message.size() < 2)
        return;
    DispatchGuard guard(this);
    const int ch = (uchar(message[0]) & 0x0F) + 1;
    // A callback may release or detach a link: work from a copy of the targets.
    QList<const QObject *> targets;
    for (const auto &r : m_regs)
        if (!r->dead && r->in && r->in->b == b && r->in->gen == gen && r->channel == ch)
            targets.append(r->owner);
    for (const QObject *o : std::as_const(targets)) {
        const Reg *r = reg(o);
        if (r && r->in && r->in->b == b && r->in->gen == gen && r->client.received)
            r->client.received(message);
    }
}

void PortHub::onOutputLost(MidiBackend *b)
{
    DispatchGuard guard(this);
    QList<const QObject *> users;
    for (const auto &p : m_outs)
        if (p->b == b)
            users = p->users;
    for (const QObject *o : std::as_const(users)) {
        const Reg *r = reg(o);
        if (r && r->out && r->out->b == b && r->client.outputLost) {
            r->client.outputLost();
        }
    }
    // Links that did not release (no callback) must not keep a dead port.
    for (const QObject *o : std::as_const(users)) {
        const Reg *r = reg(o);
        if (r && r->out && r->out->b == b)
            releaseOutput(o);
    }
}

void PortHub::onInputLost(MidiBackend *b)
{
    DispatchGuard guard(this);
    QList<const QObject *> users;
    for (const auto &p : m_ins)
        if (p->b == b)
            users = p->users;
    for (const QObject *o : std::as_const(users)) {
        const Reg *r = reg(o);
        if (r && r->in && r->in->b == b && r->client.inputLost)
            r->client.inputLost();
    }
    for (const QObject *o : std::as_const(users)) {
        const Reg *r = reg(o);
        if (r && r->in && r->in->b == b)
            releaseInput(o);
    }
}

}  // namespace d05
