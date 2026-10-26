#pragma once

#include "core/PatchAddress.h"

#include <QObject>
#include <functional>
#include <optional>

namespace d05 {

// SPEC-2 section 6: the JU-06A [60/106] engine state machine, one instance per JU-06A device.
// Pure logic: no MIDI, no timers, no widgets. The caller (DeviceRouter) feeds it user events and
// incoming Program Changes that are NOT echoes of the app's own sends (echo suppression is the
// link's job), and performs the sends it asks for through sendRequested().
//
// Sends. With a sender installed (setSender, the router's way) the tracker asks it to put the
// patch on the wire and commits remembered/current only when it returns true; without one it
// assumes the send happens and emits sendRequested() (tests, tools). Signals are emitted after the
// state is consistent; for a send, engineChanged() (if any) comes first, so the sender can build
// the message with engine() == address.engine().
//
// A send that fails (sender returned false) leaves remembered programs and current() as they were
// and never marks the patch as sent:
//   - select() of the current engine: the arm and hint are cleared (it was a selection), the
//     rest is unchanged; select() returns false.
//   - toggleEngine() with a matching arm: the toggle itself is the user's statement about the
//     unit and stands (engine changes, current = the new engine's remembered program), but the
//     armed send did not happen, so the arm stays armed with its prompt.
//   - programChange() confirming an arm: the PC is a fact about the unit, so the engine follows
//     and current = (engine, p), but the armed send did not happen: the arm stays armed.
//
// When the hint ends (FR-19, "stays until the next action"): hint() is non-zero from an
// unprompted PC that matches the other engine's remembered program until the next of
//   - select() of any address,
//   - toggleEngine() (this is the one action that may undo the follow, see below),
//   - any incoming PC 0..63 (it either sets a new hint or clears it),
//   - escape(), on every call whether or not an arm was cleared (Esc is an action).
// Not ending it: PC 64..127 (ignored), deviceSwitch() (a hint raised while another device was
// current can still be answered with Ctrl+E after switching). While the hint is shown,
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

    // Puts a patch on the wire for the tracker; true when it was sent. Keeps the tracker free of
    // MIDI types. Replaces the sendRequested() signal while set.
    using Sender = std::function<bool(const PatchAddress &)>;
    void setSender(Sender s) { m_sender = std::move(s); }

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
    // The user selected a JU-06A address (Enter, click, prev/next, auto-send, --send). False only
    // when a send was attempted and the sender reported failure (arming returns true).
    bool select(const d05::PatchAddress &a);
    // Ctrl+E or engine control click.
    void toggleEngine();
    // Esc. Clears the arm and the hint. Returns true only when it cleared an armed selection
    // (so the caller knows Esc was consumed).
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
    bool doSend(const PatchAddress &a);  // sends A; commits remembered/current only if it was sent
    std::optional<PatchAddress> at(int engine, int program) const;

    int m_engine = 60;
    int m_rem60 = Unknown;
    int m_rem106 = Unknown;
    std::optional<PatchAddress> m_armed;
    std::optional<PatchAddress> m_current;
    int m_hint = 0;
    int m_undoPrevious = Unknown;  // remembered[engine at hint time] before the follow
    bool m_flipDetectable = false;
    Sender m_sender;
};

}  // namespace d05
