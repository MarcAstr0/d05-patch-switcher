#include "ui/GroupBar.h"

#include "core/PatchAddress.h"
#include "ui/SectionRule.h"

#include <QPainter>

namespace d05 {

namespace {
QString legendOf(int group)
{
    return PatchAddress::fromParts(group, 1, 1)->groupLabel();
}
}  // namespace

GroupBar::GroupBar(QWidget *parent)
    : PanelButtonField(parent)
{
    setObjectName(QStringLiteral("groupBar"));
    setFixedSize(sizeHint());
    for (int g = 0; g < PatchAddress::GroupCount; ++g) {
        Slot s;
        s.row = g < 8 ? 0 : 1;
        s.col = g < 8 ? g : g - 8;
        s.face = QPoint(4 + 50 * s.col, s.row == 0 ? 16 : 44);
        m_slots.append(s);
    }
    setCursor(8);  // P1, until the window says otherwise
}

void GroupBar::setLitGroup(int group, bool moveCursor)
{
    if (group < -1 || group >= PatchAddress::GroupCount)
        group = -1;
    if (m_lit != group) {
        m_lit = group;
        update();
    }
    if (moveCursor && group >= 0)
        setCursor(group);
}

PanelButton::Look GroupBar::lookFor(int slot) const
{
    PanelButton::Look look;
    look.kind = PanelButton::Kind::LegendLed;
    look.legend = legendOf(slot);
    look.lit = (slot == m_lit);
    return look;
}

QString GroupBar::tipFor(int slot) const
{
    return QStringLiteral("Group %1: choose a bank and number next").arg(legendOf(slot));
}

void GroupBar::activate(int slot)
{
    emit groupPressed(slot);
}

void GroupBar::paintBackdrop(QPainter *p)
{
    SectionRule::paint(p, 4, 0, 394, QStringLiteral("PATCH GROUP"), true);
}

}  // namespace d05
