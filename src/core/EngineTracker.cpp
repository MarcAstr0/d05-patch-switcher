#include "core/EngineTracker.h"

namespace d05 {

EngineTracker::EngineTracker(QObject *parent) : QObject(parent) {}

std::optional<PatchAddress> EngineTracker::at(int engine, int program) const
{
    if (program < 0 || program > 63)
        return std::nullopt;
    return PatchAddress::fromBankNumber(Model::JU06A, program / 8 + 1, program % 8 + 1, engine);
}

EngineTracker::Prompt EngineTracker::prompt() const
{
    if (!m_armed)
        return Prompt::None;
    return (m_flipDetectable && remembered(m_armed->engine()) != Unknown) ? Prompt::FlipSwitch
                                                                          : Prompt::FlipThenCtrlE;
}

void EngineTracker::setArmed(const std::optional<PatchAddress> &a)
{
    if (m_armed == a)
        return;
    m_armed = a;
    emit armedChanged();
}

void EngineTracker::setHint(int engine)
{
    if (m_hint == engine)
        return;
    m_hint = engine;
    emit hintChanged(engine);
}

void EngineTracker::setCurrent(const std::optional<PatchAddress> &a)
{
    if (m_current == a)
        return;
    m_current = a;
    emit currentChanged();
}

void EngineTracker::setRemembered(int engine, int program)
{
    int &slot = engine == 60 ? m_rem60 : m_rem106;
    if (slot == program)
        return;
    slot = program;
    emit rememberedChanged();
}

void EngineTracker::setState(int engine, int remembered60, int remembered106)
{
    const int e = engine == 106 ? 106 : 60;
    auto clean = [](int p) { return (p >= 0 && p <= 63) ? p : Unknown; };
    const bool engineDiffers = e != m_engine;
    m_engine = e;
    setArmed(std::nullopt);
    setHint(0);
    setRemembered(60, clean(remembered60));
    setRemembered(106, clean(remembered106));
    setCurrent(at(m_engine, remembered(m_engine)));
    if (engineDiffers)
        emit engineChanged(m_engine);
}

void EngineTracker::setFlipDetectable(bool on)
{
    if (m_flipDetectable == on)
        return;
    m_flipDetectable = on;
    if (m_armed)
        emit armedChanged();  // prompt kind may have changed
}

bool EngineTracker::doSend(const PatchAddress &a)
{
    if (m_sender && !m_sender(a))
        return false;
    setRemembered(a.engine(), a.program());
    setCurrent(a);
    if (!m_sender)
        emit sendRequested(a);
    return true;
}

bool EngineTracker::select(const PatchAddress &a)
{
    if (a.model() != Model::JU06A)
        return true;
    setHint(0);
    if (a.engine() == m_engine) {
        setArmed(std::nullopt);
        return doSend(a);
    }
    setArmed(a);
    return true;
}

void EngineTracker::toggleEngine()
{
    const int newEngine = otherEngine();
    if (m_hint != 0) {
        // Undo the follow: the engine that followed gets its previous program back.
        const int p = remembered(m_engine);
        const int oldEngine = m_engine;
        setRemembered(oldEngine, m_undoPrevious);
        m_engine = newEngine;
        setHint(0);
        setCurrent(at(newEngine, p));
        emit engineChanged(m_engine);
        return;
    }
    m_engine = newEngine;
    emit engineChanged(m_engine);
    if (m_armed && m_armed->engine() == m_engine) {
        const PatchAddress a = *m_armed;
        if (m_sender) {
            if (doSend(a))
                setArmed(std::nullopt);
            else
                setCurrent(at(m_engine, remembered(m_engine)));  // arm stays, nothing was sent
        } else {
            setArmed(std::nullopt);
            doSend(a);
        }
    } else {
        setCurrent(at(m_engine, remembered(m_engine)));
    }
}

bool EngineTracker::escape()
{
    const bool hadArm = m_armed.has_value();
    setArmed(std::nullopt);
    setHint(0);
    return hadArm;
}

void EngineTracker::deviceSwitch()
{
    setArmed(std::nullopt);
}

void EngineTracker::programChange(int p)
{
    if (p < 0 || p > 63)
        return;
    // Only while the armed engine differs from the believed one: otherwise the PC is an ordinary
    // panel press (e.g. Ctrl+E already switched the engine and the armed send failed).
    if (m_armed && m_armed->engine() != m_engine && remembered(m_armed->engine()) == p) {
        // The flip re-sent the armed engine's old patch.
        const PatchAddress a = *m_armed;
        const bool changed = a.engine() != m_engine;
        m_engine = a.engine();
        if (!m_sender)
            setArmed(std::nullopt);
        setHint(0);
        if (changed)
            emit engineChanged(m_engine);
        if (m_sender) {
            if (doSend(a))
                setArmed(std::nullopt);
            else
                setCurrent(at(m_engine, p));  // arm stays, nothing was sent
        } else {
            doSend(a);
        }
        return;
    }
    // Not a flip confirmation: a panel selection, which ends any armed selection (FR-19).
    setArmed(std::nullopt);
    const int previous = remembered(m_engine);
    if (remembered(otherEngine()) == p && previous != p) {
        m_undoPrevious = previous;
        setRemembered(m_engine, p);
        setCurrent(at(m_engine, p));
        setHint(otherEngine());
        return;
    }
    setRemembered(m_engine, p);
    setCurrent(at(m_engine, p));
    setHint(0);
}

}  // namespace d05
