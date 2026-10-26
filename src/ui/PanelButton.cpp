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
    p->fillRect(r, lit ? Theme::colors().ledOn : Theme::colors().ledOff);
    if (lit)
        p->fillRect(QRect(r.x() + 2, r.y() + 1, r.width() - 4, qMax(1, r.height() - 2)), Theme::colors().ledCore);
}

// The window paints with a handful of fonts (panel label, rule title, tab): one cache per
// (font, device pixel ratio), so alternating between them measures nothing again.
struct AdvanceCache {
    struct Entry {
        QFont font;
        qreal dpr = 0;
        QHash<QString, qreal> advances;
    };
    static constexpr int Capacity = 6;
    Entry entries[Capacity];
    int used = 0;
    int next = 0;
    int builds = 0;
};

AdvanceCache &advanceCache()
{
    static AdvanceCache c;
    return c;
}

// The five face sets of DESIGN-2.md 5.2 as pointers into the palette.
struct FaceColours {
    QColor Theme::Palette::*face;
    QColor Theme::Palette::*hover;
    QColor Theme::Palette::*pressed;
    QColor Theme::Palette::*highlight;
    QColor Theme::Palette::*lit;
};

const FaceColours kFaceSets[5] = {
    {&Theme::Palette::buttonFace, &Theme::Palette::buttonFaceHover, &Theme::Palette::buttonPressed,
     &Theme::Palette::buttonHighlight, &Theme::Palette::buttonFaceLit},
    {&Theme::Palette::bankFace, &Theme::Palette::bankFaceHover, &Theme::Palette::bankPressed,
     &Theme::Palette::bankHighlight, &Theme::Palette::bankFaceLit},
    {&Theme::Palette::viewFace, &Theme::Palette::viewFaceHover, &Theme::Palette::viewPressed,
     &Theme::Palette::viewHighlight, &Theme::Palette::viewFaceLit},
    {&Theme::Palette::commitFace, &Theme::Palette::commitFaceHover, &Theme::Palette::commitPressed,
     &Theme::Palette::commitHighlight, &Theme::Palette::commitFaceLit},
    {&Theme::Palette::transportFace, &Theme::Palette::transportFaceHover, &Theme::Palette::transportPressed,
     &Theme::Palette::transportHighlight, &Theme::Palette::transportFaceLit},
};

// DESIGN-2.md 5.2 `lens`: a ring of lensRim (radius 5) around the lens (radius 4); armed is a 2 px
// ledOn ring (radius 5) around a dark centre (radius 3). Antialiased, like the face corners.
void paintLens(QPainter *p, const QPointF &c, bool lit, bool armed)
{
    const Theme::Palette &col = Theme::colors();
    p->setRenderHint(QPainter::Antialiasing, true);
    if (armed) {
        p->setBrush(col.ledOn);
        p->drawEllipse(c, 5.0, 5.0);
        p->setBrush(col.ledOff);
        p->drawEllipse(c, 3.0, 3.0);
    } else {
        p->setBrush(col.lensRim);
        p->drawEllipse(c, 5.0, 5.0);
        p->setBrush(lit ? col.ledOn : col.ledOff);
        p->drawEllipse(c, 4.0, 4.0);
    }
    p->setRenderHint(QPainter::Antialiasing, false);
}

}  // namespace

qreal PanelButton::textAdvance(const QString &text, const QFont &font, qreal dpr)
{
    AdvanceCache &c = advanceCache();
    AdvanceCache::Entry *e = nullptr;
    for (int i = 0; i < c.used; ++i) {
        if (c.entries[i].dpr == dpr && c.entries[i].font == font) {
            e = &c.entries[i];
            break;
        }
    }
    if (!e) {
        e = c.used < AdvanceCache::Capacity ? &c.entries[c.used++]
                                            : &c.entries[c.next++ % AdvanceCache::Capacity];
        e->advances.clear();
        e->font = font;
        e->dpr = dpr;
    }
    const auto it = e->advances.constFind(text);
    if (it != e->advances.constEnd())
        return *it;
    ++c.builds;
    const qreal adv = QFontMetricsF(font).horizontalAdvance(text);
    e->advances.insert(text, adv);
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
    connect(Theme::notifier(), &Theme::Notifier::skinChanged, this, [this] { update(); });
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

void PanelButton::setFaceSet(FaceSet faces)
{
    if (m_faces == faces)
        return;
    m_faces = faces;
    update();
}

void PanelButton::paintFace(QPainter *p, const QPoint &face, const Look &look)
{
    const Theme::Palette &col = Theme::colors();
    const FaceColours &set = kFaceSets[static_cast<int>(look.faces)];
    const bool lens = Theme::skin().buttonLed == Theme::LedStyle::Lens;
    const bool pressed = look.pressed && !look.disabled;
    p->save();
    p->setPen(Qt::NoPen);
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setBrush(col.buttonSlot);
    p->drawRoundedRect(QRect(face.x() - 1, face.y() - 1, FaceW + 2, FaceH + 2), 3, 3);
    const QColor *faceColour = &(col.*set.face);
    if (look.disabled)
        faceColour = &col.buttonDisabled;
    else if (pressed)
        faceColour = &(col.*set.pressed);
    else if (look.hover)
        faceColour = &(col.*set.hover);
    else if (look.lit)
        faceColour = &(col.*set.lit);
    p->setBrush(*faceColour);
    p->drawRoundedRect(QRect(face.x(), face.y(), FaceW, FaceH), 2, 2);
    p->setRenderHint(QPainter::Antialiasing, false);

    if (!pressed && !look.disabled)
        p->fillRect(QRect(face.x() + 2, face.y() + 1, FaceW - 4, 1), col.*set.highlight);

    const int dy = pressed ? 1 : 0;
    const bool on = look.lit && !look.disabled;
    const bool armed = look.armed && !on && !look.disabled;
    switch (look.kind) {
    case Kind::Led:
        if (lens)
            paintLens(p, QPointF(face.x() + 22, face.y() + 12 + dy), on, armed);
        else
            paintLed(p, QRect(face.x() + 15, face.y() + 10 + dy, 14, 4), on);
        break;
    case Kind::LegendLed: {
        const QFont font = Theme::font(Theme::Role::PanelLabel);
        const int adv = qRound(textAdvance(look.legend, font, p->device()->devicePixelRatioF()));
        p->setFont(font);
        p->setPen(look.disabled ? col.labelDisabled : col.buttonLegend);
        if (lens) {
            // Ring on the left, the legend centred on x 28 (DESIGN-2.md 5.2).
            p->drawText(QPoint(face.x() + qRound(28 - qreal(adv) / 2), face.y() + 16 + dy), look.legend);
            p->setPen(Qt::NoPen);
            paintLens(p, QPointF(face.x() + 10, face.y() + 12 + dy), on, armed);
        } else {
            p->drawText(QPoint(face.x() + qRound((FaceW - qreal(adv)) / 2), face.y() + 11 + dy), look.legend);
            p->setPen(Qt::NoPen);
            paintLed(p, QRect(face.x() + 15, face.y() + 16 + dy, 14, 3), on);
        }
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
        p->setBrush(look.disabled ? col.labelDisabled : col.wedge);
        p->drawPolygon(tri, 3);
        break;
    }
    }

    if (look.focused && !look.disabled) {
        const QRect ring(face.x() - 4, face.y() - 4, 52, 32);
        const QColor &c = col.focusRing;
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
    look.faces = m_faces;
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
