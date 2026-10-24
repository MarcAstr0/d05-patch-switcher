#pragma once

#include "ui/PanelButton.h"

#include <QPoint>
#include <QVector>
#include <QWidget>

namespace d05 {

// Base of GroupBar and BankGrid (DESIGN.md 6.3, 6.4): one focusable widget that paints a
// field of panel buttons itself and keeps a keyboard cursor. Subclasses fill m_slots
// and say how each button looks and what pressing it does.
class PanelButtonField : public QWidget {
    Q_OBJECT
public:
    explicit PanelButtonField(QWidget *parent = nullptr);

    int cursor() const { return m_cursor; }
    int slotCount() const { return m_slots.size(); }
    // Face top-left of a button in widget coordinates (tests, hit testing).
    QPoint faceOf(int slot) const { return m_slots[slot].face; }
    // Index of the button whose face contains `pos`, or -1.
    int slotAt(const QPoint &pos) const;
    // Press a button as if clicked (keyboard Enter/Space and tests).
    void pressSlot(int slot) { activate(slot); }

protected:
    struct Slot {
        QPoint face;
        int row = 0;
        int col = 0;
    };

    virtual PanelButton::Look lookFor(int slot) const = 0;
    virtual QString tipFor(int slot) const = 0;
    virtual void activate(int slot) = 0;
    // Rules and numerals, painted before the buttons.
    virtual void paintBackdrop(QPainter *p) { Q_UNUSED(p); }

    void setCursor(int slot);
    int slotAtCell(int row, int col) const;

    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool event(QEvent *event) override;

    QVector<Slot> m_slots;

private:
    void moveCursor(int dRow, int dCol);

    int m_cursor = 0;
    int m_hover = -1;
    int m_pressed = -1;
};

}  // namespace d05
