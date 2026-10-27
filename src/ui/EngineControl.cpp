#include "ui/EngineControl.h"

#include "ui/SectionRule.h"
#include "ui/Theme.h"

#include <QKeySequence>
#include <QPainter>

namespace d05 {

namespace {
QString engineKey()
{
    return QKeySequence(Qt::CTRL | Qt::Key_E).toString(QKeySequence::NativeText);
}
}  // namespace

QString EngineControl::captionLine1()
{
    return QStringLiteral("The app's belief about the 60/106 switch.");
}

QString EngineControl::captionLine2()
{
    return engineKey() + QStringLiteral(" changes the belief, never the unit.");
}

EngineControl::EngineControl(QWidget *parent)
    : PanelButtonField(parent)
{
    setObjectName(QStringLiteral("engineControl"));
    setFixedSize(sizeHint());
    for (int col = 0; col < 2; ++col) {
        Slot s;
        s.row = 0;
        s.col = col;
        s.face = QPoint(4 + 50 * col, 16);
        m_slots.append(s);
    }
    setCursor(0);
}

void EngineControl::setState(int believed, int marked)
{
    believed = believed == 106 ? 106 : 60;
    if (marked != 60 && marked != 106)
        marked = 0;
    if (believed == m_believed && marked == m_marked)
        return;
    m_believed = believed;
    m_marked = marked;
    update();
}

PanelButton::Look EngineControl::lookFor(int slot) const
{
    const int engine = slot == 1 ? 106 : 60;
    PanelButton::Look look;
    look.kind = PanelButton::Kind::LegendLed;
    look.legend = engine == 106 ? QStringLiteral("106") : QStringLiteral("60");
    look.lit = m_believed == engine;
    look.armed = armedLens(engine);
    look.faces = PanelButton::FaceSet::Button;
    return look;
}

QString EngineControl::tipFor(int slot) const
{
    return QStringLiteral("Engine %1: what the app believes the switch is on. %2 toggles.")
        .arg(slot == 1 ? QStringLiteral("106") : QStringLiteral("60"), engineKey());
}

void EngineControl::activate(int slot)
{
    const int engine = slot == 1 ? 106 : 60;
    if (engine != m_believed)  // the lit button does nothing
        emit engineRequested(engine);
}

void EngineControl::paintBackdrop(QPainter *p)
{
    SectionRule::paint(p, 4, 0, 394, Theme::skin().engineTitle, true, true);
    p->setFont(Theme::font(Theme::Role::Note));
    p->setPen(Theme::colors().labelDim);
    p->drawText(QPoint(4 + 112, 25), captionLine1());
    p->drawText(QPoint(4 + 112, 40), captionLine2());
}

}  // namespace d05
