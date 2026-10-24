#include "ui/SectionRule.h"

#include "ui/PanelButton.h"
#include "ui/Theme.h"

#include <QPaintDevice>
#include <QPainter>

namespace d05 {

SectionRule::SectionRule(const QString &title, bool accent, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_accent(accent)
{
    setFixedHeight(Height);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

QSize SectionRule::sizeHint() const
{
    return {200, Height};
}

void SectionRule::paint(QPainter *p, int x, int y, int width, const QString &title, bool accent)
{
    const QColor &rule = accent ? Theme::ruleAccent : Theme::ruleSecondary;
    const QColor &text = accent ? Theme::labelAccent : Theme::labelSecondary;
    const QFont font = Theme::font(Theme::Role::PanelLabel);
    const int adv = qRound(PanelButton::textAdvance(title, font, p->device()->devicePixelRatioF()));
    const int left = x + qRound((width - qreal(adv)) / 2);
    const int right = left + adv;
    p->fillRect(QRect(x, y + 8, qMax(0, left - 6 - x), 1), rule);
    p->fillRect(QRect(right + 6, y + 8, qMax(0, x + width - (right + 6)), 1), rule);
    p->setFont(font);
    p->setPen(text);
    p->drawText(QPoint(left, y + 10), title);
}

void SectionRule::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paint(&p, 0, 0, width(), m_title, m_accent);
}

}  // namespace d05
