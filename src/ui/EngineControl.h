#pragma once

#include "ui/PanelButtonField.h"

namespace d05 {

// The JU-06A's engine control in the group-bar slot (DESIGN-2.md 4.3, SPEC-2 FR-19): the ENGINE rule,
// two panel buttons `60` and `106` with lens LEDs and a two-line caption saying that they show the
// app's belief about the unit's switch, never the switch. The believed engine's lens is lit; the
// engine an armed selection or a flip hint is about is drawn armed (a ring around a dark centre).
// A click, Enter or Space on the unlit button asks for that engine (engineRequested); on the lit
// button it does nothing. Local coordinates are GroupBar's: the widget starts 4 px left of the column
// and at the rule, faces at x 4 and 54, y 16. One focus stop, never disabled.
class EngineControl : public PanelButtonField {
    Q_OBJECT
public:
    static constexpr int WidgetWidth = 402;
    static constexpr int WidgetHeight = 72;

    explicit EngineControl(QWidget *parent = nullptr);

    // The believed engine (60 or 106) and the engine an armed selection or hint is about (0 = none).
    void setState(int believed, int marked);
    int believed() const { return m_believed; }
    int marked() const { return m_marked; }
    // Test access: the lens of the 60 / 106 button.
    bool lit(int engine) const { return m_believed == engine; }
    bool armedLens(int engine) const { return m_marked == engine && m_believed != engine; }
    static int slotOf(int engine) { return engine == 106 ? 1 : 0; }
    // The caption's two lines (DESIGN-2.md 4.3), with the native text of the Ctrl+E key.
    static QString captionLine1();
    static QString captionLine2();

    QSize sizeHint() const override { return {WidgetWidth, WidgetHeight}; }

signals:
    // The unlit button was pressed: the user says the unit is now on `engine` (60 or 106).
    void engineRequested(int engine);

protected:
    PanelButton::Look lookFor(int slot) const override;
    QString tipFor(int slot) const override;
    void activate(int slot) override;
    void paintBackdrop(QPainter *p) override;

private:
    int m_believed = 60;
    int m_marked = 0;
    QString m_caption1, m_caption2;  // built outside paint (construction, skin change)
};

}  // namespace d05
