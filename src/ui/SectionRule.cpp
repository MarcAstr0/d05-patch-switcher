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

void SectionRule::paint(QPainter *p, int x, int y, int width, const QString &title, bool accent, bool engine)
{
    const Theme::Palette &col = Theme::colors();
    const qreal dpr = p->device()->devicePixelRatioF();
    // Secondary rules (dialogs) are the version 1 hairline on every skin (DESIGN-2.md 5.1).
    const Theme::RuleStyle style = accent ? Theme::skin().sectionRule : Theme::RuleStyle::Hairline;
    if (style == Theme::RuleStyle::Hairline) {
        const QColor &rule = accent ? col.ruleAccent : col.ruleSecondary;
        const QColor &text = accent ? col.labelAccent : col.labelSecondary;
        const QFont font = Theme::font(Theme::Role::PanelLabel);
        const int adv = qRound(PanelButton::textAdvance(title, font, dpr));
        const int left = x + qRound((width - qreal(adv)) / 2);
        const int right = left + adv;
        p->fillRect(QRect(x, y + 8, qMax(0, left - 6 - x), 1), rule);
        p->fillRect(QRect(right + 6, y + 8, qMax(0, x + width - (right + 6)), 1), rule);
        p->setFont(font);
        p->setPen(text);
        p->drawText(QPoint(left, y + 10), title);
        return;
    }
    const QFont font = Theme::font(Theme::Role::RuleTitle);
    const int adv = qRound(PanelButton::textAdvance(title, font, dpr));
    const int left = x + qRound((width - qreal(adv)) / 2);
    const int right = left + adv;
    p->setFont(font);
    if (style == Theme::RuleStyle::DoubleBar) {
        // Two bars across the full width, a rounded plate behind the title drawn over them.
        p->fillRect(QRect(x, y + 2, width, 3), col.ruleAccent);
        p->fillRect(QRect(x, y + 7, width, 3), col.ruleAccent);
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(Qt::NoPen);
        p->setBrush(col.ruleTitleBack);
        p->drawRoundedRect(QRect(left - 8, y, right - left + 16, Height), 2, 2);
        p->restore();
        p->setPen(col.ruleTitleText);
    } else {
        p->fillRect(QRect(x, y, width, Height), engine ? col.ruleBand2 : col.ruleBand);
        p->setPen(engine ? col.ruleBand2Text : col.ruleBandText);
    }
    p->drawText(QPoint(left, y + 10), title);
}

void SectionRule::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paint(&p, 0, 0, width(), m_title, m_accent);
}

}  // namespace d05
