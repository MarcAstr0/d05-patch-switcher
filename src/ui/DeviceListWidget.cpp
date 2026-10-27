#include "ui/DeviceListWidget.h"

#include "ui/Theme.h"

#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

namespace d05 {

DeviceListWidget::DeviceListWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("deviceList"));
    setFixedSize(Width, Height);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAccessibleName(QStringLiteral("Devices"));
}

void DeviceListWidget::setRow(Model m, const Row &row)
{
    if (m_rows[int(m)] == row)
        return;
    m_rows[int(m)] = row;
    update();
}

void DeviceListWidget::setCurrent(Model m)
{
    if (m == m_current)
        return;
    m_current = m;
    update();
    emit currentChanged(m);
}

int DeviceListWidget::rowAt(const QPoint &p) const
{
    if (p.x() < 1 || p.x() >= Width - 1 || p.y() < 1 || p.y() >= 1 + 3 * RowHeight)
        return -1;
    return (p.y() - 1) / RowHeight;
}

void DeviceListWidget::setHover(int row)
{
    if (row == m_hover)
        return;
    m_hover = row;
    update();
}

void DeviceListWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const Theme::Palette &c = Theme::colors();
    // The well: fill, then the recess edges (top and left dark, bottom and right light); a focused
    // list has a 1 px focus ring instead.
    p.fillRect(rect(), c.well);
    if (hasFocus()) {
        p.setPen(c.focusRing);
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRect(0, 0, Width - 1, Height - 1));
    } else {
        p.fillRect(QRect(0, 0, Width, 1), c.wellEdgeDark);
        p.fillRect(QRect(0, 0, 1, Height), c.wellEdgeDark);
        p.fillRect(QRect(0, Height - 1, Width, 1), c.wellEdgeLight);
        p.fillRect(QRect(Width - 1, 0, 1, Height), c.wellEdgeLight);
    }
    const QFont nameFont = Theme::font(Theme::Role::Body);
    const QFont noteFont = Theme::font(Theme::Role::Note);
    for (Model m : allModels()) {
        const Row &r = m_rows[int(m)];
        const QRect rr = rowRect(m);
        const bool selected = m == m_current;
        if (selected) {
            p.fillRect(rr, c.selection);
            p.fillRect(QRect(rr.x(), rr.y(), 3, rr.height()), hasFocus() ? c.focusRing : c.selectionBarIdle);
        } else if (m_hover == int(m)) {
            p.fillRect(rr, c.rowHover);
        }
        const int x0 = rr.x();
        const int y0 = rr.y();
        // The enabled checkbox: the version 1 LED look.
        const QRect box(x0 + 10, y0 + 10, 16, 6);
        if (r.enabled) {
            p.fillRect(box, c.ledOn);
        } else {
            p.fillRect(box, c.ledOff);
            p.setPen(c.fieldBorderHover);
            p.setBrush(Qt::NoBrush);
            p.drawRect(box.adjusted(0, 0, -1, -1));
        }
        if (selected && hasFocus()) {
            p.setPen(c.focusRing);
            p.setBrush(Qt::NoBrush);
            p.drawRect(box.adjusted(-2, -2, 1, 1));
        }
        p.setFont(nameFont);
        p.setPen(r.enabled ? c.labelPrimary : c.labelSecondary);
        p.drawText(QPoint(x0 + 36, y0 + 19), modelInfo(m).name);
        int textX = x0 + 36;
        if (r.enabled) {
            p.fillRect(QRect(x0 + 36, y0 + 29, 10, 4), r.lit ? c.ledOn : c.ledOff);
            textX = x0 + 52;
        }
        p.setFont(noteFont);
        p.setPen(r.tone == Tone::Red ? c.labelRed : r.tone == Tone::Secondary ? c.labelSecondary : c.labelDim);
        p.drawText(QPoint(textX, y0 + 34), r.text);
    }
}

void DeviceListWidget::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Up:
        if (m_current != Model::D05)
            setCurrent(Model(int(m_current) - 1));
        return;
    case Qt::Key_Down:
        if (m_current != Model::JU06A)
            setCurrent(Model(int(m_current) + 1));
        return;
    case Qt::Key_Space:
        if (!e->isAutoRepeat())
            emit enabledToggled(m_current, !m_rows[int(m_current)].enabled);
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(e);
}

void DeviceListWidget::mousePressEvent(QMouseEvent *e)
{
    const int r = rowAt(e->position().toPoint());
    if (r < 0 || e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
    }
    const Model m = Model(r);
    if (toggleRect(m).contains(e->position().toPoint()))
        emit enabledToggled(m, !m_rows[r].enabled);
    else
        setCurrent(m);
}

void DeviceListWidget::mouseMoveEvent(QMouseEvent *e)
{
    setHover(rowAt(e->position().toPoint()));
    QWidget::mouseMoveEvent(e);
}

void DeviceListWidget::leaveEvent(QEvent *e)
{
    setHover(-1);
    QWidget::leaveEvent(e);
}

void DeviceListWidget::focusInEvent(QFocusEvent *e)
{
    QWidget::focusInEvent(e);
    update();
}

void DeviceListWidget::focusOutEvent(QFocusEvent *e)
{
    QWidget::focusOutEvent(e);
    update();
}

bool DeviceListWidget::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        const auto *he = static_cast<QHelpEvent *>(e);
        const int r = rowAt(he->pos());
        if (r >= 0 && !m_rows[r].tooltip.isEmpty())
            QToolTip::showText(he->globalPos(), m_rows[r].tooltip, this);
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}

}  // namespace d05
