#include "ui/DeviceSwitcher.h"

#include "ui/Theme.h"

#include <QEvent>
#include <QFontMetricsF>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

namespace d05 {

DeviceSwitcher::DeviceSwitcher(const QString &logoPath, QWidget *parent)
    : Nameplate(logoPath, parent)
{
    setMouseTracking(true);
    m_tabFont = Theme::font(Theme::Role::SwitcherTab);
    connect(Theme::notifier(), &Theme::Notifier::skinChanged, this, [this] {
        m_tabFont = Theme::font(Theme::Role::SwitcherTab);
        rebuild();  // the plate's width follows the skin's wordmark
        update();
    });
}

void DeviceSwitcher::setState(const std::array<DeviceState, 3> &devices, Model current)
{
    if (devices == m_state && current == m_current)
        return;
    m_state = devices;
    m_current = current;
    m_count = 0;
    for (const DeviceState &d : m_state)
        m_count += d.enabled ? 1 : 0;
    if (m_hover && !m_state[int(m_hoverModel)].enabled)
        m_hover = false;
    rebuild();
    update();
}

// The tab rects: from the plate, a divider 14 px later, the first tab 15 px after that, then
// `16 + name advance + 18` per tab (DESIGN-2.md 4.4). Built here, never while painting.
void DeviceSwitcher::rebuild()
{
    // The tabs need the mouse; the lone nameplate lets everything through to the slot below.
    setAttribute(Qt::WA_TransparentForMouseEvents, !hasTabs());
    m_tabCount = 0;
    if (!hasTabs())
        return;
    const QFontMetricsF fm(m_tabFont);
    int tx = plateRight() + 14 + 15;
    for (Model m : allModels()) {
        if (!m_state[int(m)].enabled)
            continue;
        Tab &t = m_tabs[m_tabCount++];
        t.model = m;
        t.x = tx;
        t.advance = qRound(fm.horizontalAdvance(modelInfo(m).name));
        t.hit = QRect(tx - 6, 0, 16 + t.advance + 12, SlotHeight);
        tx += 16 + t.advance + 18;
    }
}

QRect DeviceSwitcher::tabRect(Model m) const
{
    for (int i = 0; i < m_tabCount; ++i)
        if (m_tabs[i].model == m)
            return m_tabs[i].hit;
    return {};
}

bool DeviceSwitcher::tabAt(const QPoint &pos, Model *m) const
{
    for (int i = 0; i < m_tabCount; ++i) {
        if (m_tabs[i].hit.contains(pos)) {
            if (m)
                *m = m_tabs[i].model;
            return true;
        }
    }
    return false;
}

DeviceSwitcher::NameTone DeviceSwitcher::nameTone(Model m) const
{
    if (m_state[int(m)].red)
        return NameTone::Red;
    if (m == m_current || (m_hover && m_hoverModel == m))
        return NameTone::Primary;
    return NameTone::Secondary;
}

void DeviceSwitcher::paintEvent(QPaintEvent *event)
{
    if (m_count == 1) {  // exactly the nameplate of version 1
        Nameplate::paintEvent(event);
        return;
    }
    QPainter p(this);
    const int right = paintPlate(p, false);  // the wordmark alone, or the logo
    if (!hasTabs())
        return;
    const Theme::Palette &c = Theme::colors();
    p.fillRect(QRect(right + 14, 9, 1, 12), c.ruleSecondary);
    p.setFont(m_tabFont);
    for (int i = 0; i < m_tabCount; ++i) {
        const Tab &t = m_tabs[i];
        const bool lit = t.model == m_current;
        p.fillRect(QRect(t.x, 14, 10, 4), lit ? c.ledOn : c.ledOff);
        if (lit)
            p.fillRect(QRect(t.x + 2, 15, 6, 2), c.ledCore);
        const NameTone tone = nameTone(t.model);
        p.setPen(tone == NameTone::Red ? c.labelRed : tone == NameTone::Primary ? c.labelPrimary : c.labelSecondary);
        p.drawText(QPoint(t.x + 16, 20), modelInfo(t.model).name);
    }
}

void DeviceSwitcher::setHover(bool on, Model m)
{
    if (on == m_hover && (!on || m == m_hoverModel))
        return;
    m_hover = on;
    m_hoverModel = m;
    update();
}

void DeviceSwitcher::mouseMoveEvent(QMouseEvent *event)
{
    Model m = Model::D05;
    const bool over = hasTabs() && tabAt(event->position().toPoint(), &m) && m != m_current;
    setHover(over, m);
    QWidget::mouseMoveEvent(event);
}

void DeviceSwitcher::leaveEvent(QEvent *event)
{
    setHover(false, m_hoverModel);
    QWidget::leaveEvent(event);
}

void DeviceSwitcher::mousePressEvent(QMouseEvent *event)
{
    Model m = Model::D05;
    m_pressed = event->button() == Qt::LeftButton && hasTabs() && tabAt(event->position().toPoint(), &m);
    m_pressModel = m;
    event->accept();
}

// A click switches on release, and only when it ends on the tab it began on; the current tab
// ignores clicks.
void DeviceSwitcher::mouseReleaseEvent(QMouseEvent *event)
{
    Model m = Model::D05;
    const bool was = m_pressed;
    m_pressed = false;
    if (was && event->button() == Qt::LeftButton && tabAt(event->position().toPoint(), &m) && m == m_pressModel
        && m != m_current)
        emit switchRequested(m);
    event->accept();
}

bool DeviceSwitcher::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip && hasTabs()) {
        const auto *he = static_cast<QHelpEvent *>(e);
        Model m = Model::D05;
        if (tabAt(he->pos(), &m) && !m_state[int(m)].tooltip.isEmpty())
            QToolTip::showText(he->globalPos(), m_state[int(m)].tooltip, this);
        else
            QToolTip::hideText();
        return true;
    }
    return Nameplate::event(e);
}

}  // namespace d05
