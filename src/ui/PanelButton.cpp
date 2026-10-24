#include "ui/PanelButton.h"

#include "ui/Theme.h"

#include <QEvent>
#include <QHash>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintDevice>

namespace d05 {

namespace {

void paintLed(QPainter *p, const QRect &r, bool lit)
{
    p->fillRect(r, lit ? Theme::ledOn : Theme::ledOff);
    if (lit)
        p->fillRect(QRect(r.x() + 2, r.y() + 1, r.width() - 4, qMax(1, r.height() - 2)), Theme::ledCore);
}

struct AdvanceCache {
    QFont font;
    qreal dpr = 0;
    QHash<QString, qreal> advances;
    int builds = 0;
};

AdvanceCache &advanceCache()
{
    static AdvanceCache c;
    return c;
}

}  // namespace

qreal PanelButton::textAdvance(const QString &text, const QFont &font, qreal dpr)
{
    AdvanceCache &c = advanceCache();
    if (dpr != c.dpr || font != c.font) {
        c.advances.clear();
        c.font = font;
        c.dpr = dpr;
    }
    const auto it = c.advances.constFind(text);
    if (it != c.advances.constEnd())
        return *it;
    ++c.builds;
    const qreal adv = QFontMetricsF(font).horizontalAdvance(text);
    c.advances.insert(text, adv);
    return adv;
}

int PanelButton::metricsBuildCount()
{
    return advanceCache().builds;
}

PanelButton::PanelButton(Kind kind, QWidget *parent)
    : QAbstractButton(parent)
    , m_kind(kind)
{
    // Hardware feel: the mouse leaves no ring, only Tab does (DESIGN 9).
    setFocusPolicy(Qt::TabFocus);
    setMouseTracking(true);
    setFixedSize(sizeHint());
}

void PanelButton::setLegend(const QString &legend)
{
    if (m_legend == legend)
        return;
    m_legend = legend;
    update();
}

void PanelButton::setLit(bool lit)
{
    if (m_lit == lit)
        return;
    m_lit = lit;
    update();
}

void PanelButton::paintFace(QPainter *p, const QPoint &face, const Look &look)
{
    const bool pressed = look.pressed && !look.disabled;
    p->save();
    p->setPen(Qt::NoPen);
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setBrush(Theme::buttonSlot);
    p->drawRoundedRect(QRect(face.x() - 1, face.y() - 1, FaceW + 2, FaceH + 2), 3, 3);
    const QColor *faceColour = &Theme::buttonFace;
    if (look.disabled)
        faceColour = &Theme::buttonDisabled;
    else if (pressed)
        faceColour = &Theme::buttonPressed;
    else if (look.hover)
        faceColour = &Theme::buttonFaceHover;
    else if (look.lit)
        faceColour = &Theme::buttonFaceLit;
    p->setBrush(*faceColour);
    p->drawRoundedRect(QRect(face.x(), face.y(), FaceW, FaceH), 2, 2);
    p->setRenderHint(QPainter::Antialiasing, false);

    if (!pressed && !look.disabled)
        p->fillRect(QRect(face.x() + 2, face.y() + 1, FaceW - 4, 1), Theme::buttonHighlight);

    const int dy = pressed ? 1 : 0;
    const bool on = look.lit && !look.disabled;
    switch (look.kind) {
    case Kind::Led:
        paintLed(p, QRect(face.x() + 15, face.y() + 10 + dy, 14, 4), on);
        break;
    case Kind::LegendLed: {
        const QFont font = Theme::font(Theme::Role::PanelLabel);
        const int adv = qRound(textAdvance(look.legend, font, p->device()->devicePixelRatioF()));
        p->setFont(font);
        p->setPen(look.disabled ? Theme::labelDisabled : Theme::labelPrimary);
        p->drawText(QPoint(face.x() + qRound((FaceW - qreal(adv)) / 2), face.y() + 11 + dy), look.legend);
        p->setPen(Qt::NoPen);
        paintLed(p, QRect(face.x() + 15, face.y() + 16 + dy, 14, 3), on);
        break;
    }
    case Kind::Plain:
        break;
    case Kind::WedgeLeft:
    case Kind::WedgeRight: {
        // A fixed array on the stack: nothing is allocated while painting.
        const int ox = face.x();
        const int oy = face.y() + dy;
        const QPoint tri[3] = {look.kind == Kind::WedgeLeft ? QPoint(ox + 18, oy + 12) : QPoint(ox + 26, oy + 12),
                               look.kind == Kind::WedgeLeft ? QPoint(ox + 25, oy + 8) : QPoint(ox + 19, oy + 8),
                               look.kind == Kind::WedgeLeft ? QPoint(ox + 25, oy + 16) : QPoint(ox + 19, oy + 16)};
        p->setBrush(look.disabled ? Theme::labelDisabled : Theme::arrow);
        p->drawPolygon(tri, 3);
        break;
    }
    }

    if (look.focused && !look.disabled) {
        const QRect ring(face.x() - 4, face.y() - 4, 52, 32);
        const QColor &c = Theme::focusRing;
        p->fillRect(QRect(ring.x(), ring.y(), ring.width(), 2), c);
        p->fillRect(QRect(ring.x(), ring.bottom() - 1, ring.width(), 2), c);
        p->fillRect(QRect(ring.x(), ring.y() + 2, 2, ring.height() - 4), c);
        p->fillRect(QRect(ring.right() - 1, ring.y() + 2, 2, ring.height() - 4), c);
    }
    p->restore();
}

void PanelButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    Look look;
    look.kind = m_kind;
    look.legend = m_legend;
    look.lit = lit();
    look.hover = m_hover && isEnabled();
    look.pressed = isDown();
    look.focused = hasFocus();
    look.disabled = !isEnabled();
    paintFace(&p, QPoint(Margin, Margin), look);
}

bool PanelButton::hitButton(const QPoint &pos) const
{
    return QRect(Margin, Margin, FaceW, FaceH).contains(pos);
}

void PanelButton::mouseMoveEvent(QMouseEvent *event)
{
    const bool over = hitButton(event->position().toPoint());
    if (over != m_hover) {
        m_hover = over;
        update();
    }
    QAbstractButton::mouseMoveEvent(event);
}

void PanelButton::leaveEvent(QEvent *event)
{
    if (m_hover) {
        m_hover = false;
        update();
    }
    QAbstractButton::leaveEvent(event);
}

void PanelButton::focusInEvent(QFocusEvent *event)
{
    QAbstractButton::focusInEvent(event);
    update();
}

void PanelButton::focusOutEvent(QFocusEvent *event)
{
    QAbstractButton::focusOutEvent(event);
    update();
}

void PanelButton::changeEvent(QEvent *event)
{
    QAbstractButton::changeEvent(event);
    if (event->type() == QEvent::EnabledChange)
        update();
}

}  // namespace d05
