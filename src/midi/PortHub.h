#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace d05 {

class MidiBackend;

// SPEC-2 section 7: opens each OS port once for input and once for output, reference-counted, and
// demultiplexes incoming messages to the links that registered for the port and channel.
//
// A MidiBackend holds one input and one output. In fixed mode (the legacy single-link setup) the
// hub wraps the one backend it is given and can serve one input port and one output port. In
// factory mode it asks the factory for another backend whenever a different port is needed (at
// most one per port per direction, so three links need at most three input threads) and owns them.
//
// Threads: everything here runs on the main thread. The backends emit received() from RtMidi's
// thread; the hub's queued connection moves it here, and the backend has already dropped what no
// registered link wants (setAcceptMask), before allocating.
class PortHub : public QObject {
    Q_OBJECT
public:
    using BackendFactory = std::function<MidiBackend *()>;

    // What a link gets back. All callbacks run on the main thread.
    struct Client {
        std::function<void(const QByteArray &)> received;  // a message that passed the filter
        std::function<void()> outputLost;                  // the output port vanished (already released)
        std::function<void()> inputLost;                   // same for the input
    };

    // Fixed mode: `backend` is not owned and serves one input and one output.
    explicit PortHub(MidiBackend *backend, QObject *parent = nullptr);
    // Factory mode: backends made by `factory` are owned by the hub (a null result is an error).
    explicit PortHub(BackendFactory factory, QObject *parent = nullptr);
    ~PortHub() override;

    // Registration of a link (any QObject identity). Detach releases both ports. It is safe from
    // inside a client callback (even the owner's own): the registration is only marked dead then
    // and swept when the outermost dispatch returns.
    void attach(const QObject *owner, Client client);
    void detach(const QObject *owner);

    // The OS's lists right now (any backend can answer). See MidiBackend::outputPortNames.
    QStringList outputPortNames(QString *err);
    QStringList inputPortNames(QString *err);

    // Opens (or joins) the output / input port `osName` (a name as listed). The owner holds at
    // most one of each; acquiring again releases the previous one first. On failure *error says why.
    bool acquireOutput(const QObject *owner, const QString &osName, QString *error);
    void releaseOutput(const QObject *owner);
    // The owner found its output port unusable (a send failed, the port vanished from the list).
    // The shared port is closed and released for every user; the other users are told through
    // outputLost (the owner is not: it is the caller and does its own bookkeeping). No-op when the
    // owner holds no output.
    void invalidateOutput(const QObject *owner);
    // `channel` is 1..16, `wantBankSelect` also lets CC 0 / CC 32 through (the D-05 needs them).
    bool acquireInput(const QObject *owner, const QString &osName, int channel, bool wantBankSelect,
                      QString *error);
    void releaseInput(const QObject *owner);
    // Same for the input side (inputLost for the other users).
    void invalidateInput(const QObject *owner);
    void setInputChannel(const QObject *owner, int channel);

    bool sendOn(const QObject *owner, const QByteArray &message, QString *error);
    bool isOutputOpen(const QObject *owner) const;
    bool isInputOpen(const QObject *owner) const;

    // Open OS ports right now (tests, diagnostics).
    int openInputCount() const;
    int openOutputCount() const;
    int backendCount() const { return int(m_backends.size()); }

private:
    struct Backend {
        MidiBackend *b = nullptr;
        bool owned = false;
    };
    struct Port {
        QString name;
        MidiBackend *b = nullptr;
        QList<const QObject *> users;
        quint64 gen = 0;  // inputs only
    };
    struct Reg {
        const QObject *owner = nullptr;
        Client client;
        Port *out = nullptr;
        Port *in = nullptr;
        int channel = 1;
        bool bankSelect = false;
        bool dead = false;  // detached during a dispatch; swept afterwards
    };
    struct DispatchGuard;
    void endInput(Port *p);  // forgets the generation, closes the backend side, drops the port

    Reg *reg(const QObject *owner);
    const Reg *reg(const QObject *owner) const;
    MidiBackend *lister();
    MidiBackend *backendFor(bool input);  // an idle backend, a new one, or nullptr
    void adopt(MidiBackend *b, bool owned);
    void refreshMask(const Port *in);
    void onReceived(MidiBackend *b, quint64 gen, const QByteArray &message);
    void onOutputLost(MidiBackend *b);
    void onInputLost(MidiBackend *b);

    BackendFactory m_factory;
    QList<Backend> m_backends;
    // Stable addresses: Regs and Ports are referenced by pointer.
    std::vector<std::unique_ptr<Reg>> m_regs;
    std::vector<std::unique_ptr<Port>> m_outs;
    std::vector<std::unique_ptr<Port>> m_ins;
    int m_dispatching = 0;
    quint64 m_nextGen = 0;
};

}  // namespace d05
