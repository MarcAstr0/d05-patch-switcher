#include "ui/PanelButtonField.h"

#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

namespace d05 {

PanelButtonField::PanelButtonField(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::TabFocus);
    setMouseTracking(true);
}

int PanelButtonField::slotAt(const QPoint &pos) const
{
    for (int i = 0; i < m_slots.size(); ++i) {
        if (QRect(m_slots[i].face, QSize(PanelButton::FaceW, PanelButton::FaceH)).contains(pos))
            return i;
    }
    return -1;
}

int PanelButtonField::slotAtCell(int row, int col) const
{
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].row == row && m_slots[i].col == col)
            return i;
    }
    return -1;
}

void PanelButtonField::setCursor(int slot)
{
    if (slot < 0 || slot >= m_slots.size() || slot == m_cursor)
        return;
    m_cursor = slot;
    update();
}

void PanelButtonField::moveCursor(int dRow, int dCol)
{
    const Slot cur = m_slots[m_cursor];
    int row = cur.row + dRow;
    int col = cur.col + dCol;
    int target = slotAtCell(row, col);
    if (target < 0 && dRow != 0) {
        // Another row: the nearest existing column.
        int best = -1;
        int bestDist = 1 << 20;
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].row != row)
                continue;
            const int d = qAbs(m_slots[i].col - cur.col);
            if (d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
        target = best;
    }
    if (target >= 0)
        setCursor(target);
}

void PanelButtonField::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paintBackdrop(&p);
    for (int i = 0; i < m_slots.size(); ++i) {
        PanelButton::Look look = lookFor(i);
        look.hover = (m_hover == i);
        look.pressed = (m_pressed == i && m_hover == i);
        look.focused = hasFocus() && m_cursor == i;
        PanelButton::paintFace(&p, m_slots[i].face, look);
    }
}

void PanelButtonField::mouseMoveEvent(QMouseEvent *event)
{
    const int i = slotAt(event->position().toPoint());
    if (i != m_hover) {
        m_hover = i;
        update();
    }
}

void PanelButtonField::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_pressed = slotAt(event->position().toPoint());
    m_hover = m_pressed;
    update();
}

void PanelButtonField::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const int i = slotAt(event->position().toPoint());
    const int pressed = m_pressed;
    m_pressed = -1;
    m_hover = i;
    update();
    if (i >= 0 && i == pressed) {
        setCursor(i);
        activate(i);
    }
}

void PanelButtonField::leaveEvent(QEvent *event)
{
    if (m_hover != -1) {
        m_hover = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

void PanelButtonField::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left: moveCursor(0, -1); break;
    case Qt::Key_Right: moveCursor(0, 1); break;
    case Qt::Key_Up: moveCursor(-1, 0); break;
    case Qt::Key_Down: moveCursor(1, 0); break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        activate(m_cursor);
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

void PanelButtonField::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    update();
}

void PanelButtonField::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    update();
}

bool PanelButtonField::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const int i = slotAt(help->pos());
        if (i >= 0)
            QToolTip::showText(help->globalPos(), tipFor(i), this,
                               QRect(m_slots[i].face, QSize(PanelButton::FaceW, PanelButton::FaceH)));
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(event);
}

}  // namespace d05
