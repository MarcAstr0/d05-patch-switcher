#include "midi/DeviceRouter.h"

#include "midi/PortHub.h"

namespace d05 {

DeviceRouter::DeviceRouter(PortHub *hub, QObject *parent)
    : QObject(parent)
    , m_hub(hub)
    , m_tracker(new EngineTracker(this))
    , m_timer(this)
{
    m_timer.setInterval(MidiLink::DefaultPollMs);
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &DeviceRouter::pollNow);

    for (Model m : allModels()) {
        auto *l = new MidiLink(m_hub, m, this);
        l->setExternalPolling(true);
        m_dev[idx(m)].link = l;
        connect(l, &MidiLink::pollNeededChanged, this, [this] { updatePolling(); });
        connect(l, &MidiLink::inputConnectedChanged, this, [this] { updateFlipDetectable(); });
    }
    MidiLink *ju = link(Model::JU06A);
    connect(m_tracker, &EngineTracker::engineChanged, ju, &MidiLink::setEngine);
    // The tracker commits a send only when the link really put it on the wire.
    m_tracker->setSender([ju](const PatchAddress &a) { return ju->sendPatch(a); });
    // Slots run in connection order: the tracker classifies the program first (an armed flip
    // confirmation sends the armed patch whether or not the JU-06A is the current device), then
    // the follow-induced device switch happens. Only a user switch clears an arm (setCurrentDevice).
    connect(ju, &MidiLink::programReceived, m_tracker, &EngineTracker::programChange);
    connect(ju, &MidiLink::programReceived, this, [this](int) { onFollowEvent(Model::JU06A); });
    connect(link(Model::D05), &MidiLink::followReceived, this, [this] { onFollowEvent(Model::D05); });
    connect(link(Model::JP08), &MidiLink::followReceived, this, [this] { onFollowEvent(Model::JP08); });
}

bool DeviceRouter::samePort(const QString &a, const QString &b)
{
    if (a.isEmpty() || b.isEmpty())
        return false;
    if (a.compare(b, Qt::CaseInsensitive) == 0)
        return true;
    // Two different full OS names are two ports even when only their ids differ (two identical
    // devices, MidiLink::portSpec). Keys are compared only when a value is a saved key.
    const auto isKey = [](const QString &v) { return MidiLink::portKey(v) == v; };
    if (!isKey(a) && !isKey(b))
        return false;
    return MidiLink::portKey(a).compare(MidiLink::portKey(b), Qt::CaseInsensitive) == 0;
}

std::optional<Model> DeviceRouter::conflict(Model m, const DeviceConfig &cfg) const
{
    if (!cfg.enabled)
        return std::nullopt;
    for (Model o : allModels()) {
        if (o == m)
            continue;
        const DeviceConfig &c = m_dev[idx(o)].cfg;
        if (!c.enabled || c.channel != cfg.channel)
            continue;
        if (samePort(c.outputPort, cfg.outputPort))
            return o;
        if (c.follow && cfg.follow && samePort(c.inputPort, cfg.inputPort))
            return o;
    }
    return std::nullopt;
}

bool DeviceRouter::configure(Model m, const DeviceConfig &cfgIn, Model *conflicting)
{
    DeviceConfig cfg = cfgIn;
    cfg.channel = qBound(1, cfg.channel, 16);
    if (const auto c = conflict(m, cfg)) {
        if (conflicting)
            *conflicting = *c;
        return false;
    }
    Dev &d = m_dev[idx(m)];
    d.cfg = cfg;
    MidiLink *l = d.link;
    l->setChannel(cfg.channel);
    if (cfg.enabled) {
        l->setInputPort(cfg.inputPort);
        l->setFollowEnabled(cfg.follow);
        l->setOutputPort(cfg.outputPort);
    } else {
        l->setFollowEnabled(false);
        l->setInputPort(QString());
        l->setOutputPort(QString());
    }
    updateFlipDetectable();
    updatePolling();
    return true;
}

void DeviceRouter::setJu06aState(int engine, int remembered60, int remembered106)
{
    link(Model::JU06A)->setEngine(engine);
    m_tracker->setState(engine, remembered60, remembered106);
}

void DeviceRouter::updateFlipDetectable()
{
    // A flip can be seen only through an open input (which exists only with follow on).
    m_tracker->setFlipDetectable(link(Model::JU06A)->inputConnected());
}

void DeviceRouter::updatePolling()
{
    bool need = false;
    for (const Dev &d : m_dev)
        need = need || (d.cfg.enabled && d.link->needsPoll());
    if (need && !m_timer.isActive())
        m_timer.start();
    else if (!need && m_timer.isActive())
        m_timer.stop();
}

void DeviceRouter::pollNow()
{
    for (const Dev &d : m_dev) {
        if (d.cfg.enabled && d.link->needsPoll())
            d.link->pollNow();
    }
    updatePolling();
}

void DeviceRouter::checkConnections()
{
    for (const Dev &d : m_dev)
        if (d.cfg.enabled)
            d.link->checkConnection();
    updatePolling();
}

bool DeviceRouter::allConnected() const
{
    for (const Dev &d : m_dev)
        if (d.cfg.enabled && d.link->needsPoll())
            return false;
    return true;
}

bool DeviceRouter::allInputsOpen() const
{
    for (const Dev &d : m_dev)
        if (d.cfg.enabled && d.cfg.follow && !d.link->inputConnected())
            return false;
    return true;
}

void DeviceRouter::setCurrentDevice(Model m)
{
    if (m == m_current)
        return;
    const Model left = m_current;
    m_current = m;
    if (left == Model::JU06A || m == Model::JU06A)
        m_tracker->deviceSwitch();
    emit currentDeviceChanged(m, false);
}

void DeviceRouter::onFollowEvent(Model m)
{
    // FR-20: all enabled devices must have their input open, otherwise an event could be a
    // late echo of a device we cannot see. Off or not satisfied: the event is applied to the
    // device only.
    if (m == m_current || !m_followSwitches || !isEnabled(m) || !allInputsOpen())
        return;
    m_current = m;
    // A switch caused by a follow event does not touch the JU-06A tracker's arm.
    emit currentDeviceChanged(m, true);
}

bool DeviceRouter::select(const PatchAddress &a)
{
    MidiLink *l = link(a.model());
    if (a.model() == Model::JU06A) {
        if (a.engine() == m_tracker->engine() && !l->isConnected()) {
            // Would send, and cannot. It is still a selection: it ends any arm and hint (FR-19),
            // but the patch is not marked current. Arming the other engine needs no connection.
            m_tracker->escape();
            return false;
        }
        return m_tracker->select(a);  // sends through the sender, or arms
    }
    return l->isConnected() && l->sendPatch(a);
}

}  // namespace d05
