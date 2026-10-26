#pragma once

#include "core/PatchAddress.h"

#include <QObject>
#include <optional>

namespace d05 {

// SPEC-2 section 6: the JU-06A [60/106] engine state machine, one instance per JU-06A device.
// Pure logic: no MIDI, no timers, no widgets. The caller (DeviceRouter) feeds it user events and
// incoming Program Changes that are NOT echoes of the app's own sends (echo suppression is the
// link's job), and performs the sends it asks for through sendRequested().
//
// State changes are applied when the signal is emitted: the tracker assumes a requested send
// happens. Signals are emitted after the state is consistent; for a send, engineChanged() (if
// any) comes first, so a receiver can build the message with engine() == address.engine().
//
// When the hint ends (FR-19, "stays until the next action"): hint() is non-zero from an
// unprompted PC that matches the other engine's remembered program until the next of
//   - select() of any address,
//   - toggleEngine() (this is the one action that may undo the follow, see below),
//   - any incoming PC 0..63 (it either sets a new hint or clears it).
// Not ending it: PC 64..127 (ignored), escape(), deviceSwitch(). While the hint is shown,
// toggleEngine() undoes the follow: the engine that followed gets its previous remembered
// program back (possibly unknown) and current = (new engine, p). After the hint has ended
// toggleEngine() is a plain toggle and undoes nothing.
class EngineTracker : public QObject {
    Q_OBJECT
public:
    static constexpr int Unknown = -1;  // remembered program not known

    // What the flip prompt asks for while armed (display text is the UI's, DESIGN-2.md).
    enum class Prompt {
        None,
        FlipSwitch,      // "FLIP SWITCH TO 106": flip confirmation possible (input path, follow on,
                         // and the armed engine's remembered program is known)
        FlipThenCtrlE,   // "FLIP TO 106, THEN CTRL+E": no input path, or that program is unknown
    };
    Q_ENUM(Prompt)

    explicit EngineTracker(QObject *parent = nullptr);

    // Load persisted state. engine must be 60 or 106 (else 60); remembered values 0..63 or Unknown.
    // current becomes (engine, remembered[engine]) or none. Clears armed and hint. Emits changes.
    void setState(int engine, int remembered60, int remembered106);
    // True when the device has an input port and follow is on, so a flip can be seen.
    void setFlipDetectable(bool on);

    int engine() const { return m_engine; }
    int otherEngine() const { return m_engine == 60 ? 106 : 60; }
    int remembered(int engine) const { return engine == 60 ? m_rem60 : m_rem106; }
    std::optional<PatchAddress> armed() const { return m_armed; }
    int hint() const { return m_hint; }  // 0, or the engine the user may have flipped to (60/106)
    std::optional<PatchAddress> current() const { return m_current; }  // nullopt = "no patch"
    Prompt prompt() const;

public slots:
    // The user selected a JU-06A address (Enter, click, prev/next, auto-send, --send).
    void select(const d05::PatchAddress &a);
    // Ctrl+E or engine control click.
    void toggleEngine();
    // Esc. Returns true when it cleared an armed selection (so the caller does nothing else).
    bool escape();
    // The window switched to or away from this device.
    void deviceSwitch();
    // A non-echo Program Change on the device's channel.
    void programChange(int program);

signals:
    void engineChanged(int engine);
    void armedChanged();                       // armed() or prompt() changed
    void hintChanged(int engine);              // 0 = cleared
    void currentChanged();                     // current() changed
    void rememberedChanged();                  // remembered pair changed (persist it)
    void sendRequested(const d05::PatchAddress &a);

private:
    void setArmed(const std::optional<PatchAddress> &a);
    void setHint(int engine);
    void setCurrent(const std::optional<PatchAddress> &a);
    void setRemembered(int engine, int program);
    void doSend(const PatchAddress &a);  // state update for "app sends A" then sendRequested
    std::optional<PatchAddress> at(int engine, int program) const;

    int m_engine = 60;
    int m_rem60 = Unknown;
    int m_rem106 = Unknown;
    std::optional<PatchAddress> m_armed;
    std::optional<PatchAddress> m_current;
    int m_hint = 0;
    int m_undoPrevious = Unknown;  // remembered[engine at hint time] before the follow
    bool m_flipDetectable = false;
};

}  // namespace d05
Q_DECLARE_METATYPE(d05::PatchAddress)
