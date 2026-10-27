#pragma once

#include "core/Model.h"
#include "ui/Nameplate.h"

#include <QFont>
#include <QRect>
#include <QString>

#include <array>

namespace d05 {

// The nameplate and device switcher (DESIGN-2.md 4.4, SPEC-2 FR-16). With no device enabled it is
// the wordmark alone, with one it is exactly the nameplate of version 1 (the skin's sub line, or
// the owner's logo alone), with two or three it is the plate, a divider and one tab per enabled
// device in the fixed order D-05, JP-08, JU-06A: a slit LED (lit for the current device), the
// model name (red when that device's output is wanted but not open) and a hit rect that switches
// to the device on release. The widget only reports clicks (switchRequested); the window decides.
//
// Repaints only when the device set, the current device, a connection state or the hover
// changes. Nothing here allocates while painting: the tab rects and fonts are built when the
// state or the skin changes.
class DeviceSwitcher : public Nameplate {
    Q_OBJECT
public:
    struct DeviceState {
        bool enabled = false;
        bool red = false;     // output configured but not open, or no output port: the "not found" red
        QString tooltip;      // "<model>: <port>, channel <n> (Ctrl+Tab)" or "<model>: no MIDI output (Ctrl+Tab)"

        bool operator==(const DeviceState &o) const
        {
            return enabled == o.enabled && red == o.red && tooltip == o.tooltip;
        }
    };

    explicit DeviceSwitcher(const QString &logoPath, QWidget *parent = nullptr);

    // Replaces the whole state. Does nothing, and does not repaint, when nothing changed.
    void setState(const std::array<DeviceState, 3> &devices, Model current);

    int enabledCount() const { return m_count; }
    Model current() const { return m_current; }
    // True when the tabs are shown (two or three enabled devices).
    bool hasTabs() const { return m_count >= 2; }
    // The hit rect of a tab in widget coordinates (empty when the device has no tab).
    QRect tabRect(Model m) const;
    // The tab under `pos` (widget coordinates), if any.
    bool tabAt(const QPoint &pos, Model *m) const;
    QString tabToolTip(Model m) const { return m_state[int(m)].tooltip; }
    // What a tab is drawn with, for tests: the name colour role and whether its LED is lit.
    enum class NameTone { Primary, Secondary, Red };
    NameTone nameTone(Model m) const;
    bool tabLit(Model m) const { return hasTabs() && m == m_current && m_state[int(m)].enabled; }
    bool hovered(Model m) const { return m_hover && m_hoverModel == m; }

signals:
    // A click on a tab of a device that is not the current one.
    void switchRequested(d05::Model model);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    struct Tab {
        Model model = Model::D05;
        int x = 0;       // tx: the LED's left edge
        int advance = 0; // the name's advance
        QRect hit;
    };
    void rebuild();
    void setHover(bool on, Model m);

    std::array<DeviceState, 3> m_state;
    Model m_current = Model::D05;
    int m_count = 0;
    std::array<Tab, 3> m_tabs;
    int m_tabCount = 0;
    QFont m_tabFont;
    bool m_hover = false;
    Model m_hoverModel = Model::D05;
    bool m_pressed = false;
    Model m_pressModel = Model::D05;
};

}  // namespace d05
