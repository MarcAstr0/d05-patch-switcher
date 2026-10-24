#pragma once

#include "ui/PanelButtonField.h"

namespace d05 {

// The 14 group buttons U1..U8, P1..P6 under the PATCH GROUP rule (DESIGN.md 6.3).
// Local coordinates: the widget starts 4 px left of the column and at the rule, so the
// faces are at x = 4 + 50 col, y = 16 (row U) and 44 (row P).
class GroupBar : public PanelButtonField {
    Q_OBJECT
public:
    static constexpr int WidgetWidth = 402;
    static constexpr int WidgetHeight = 72;

    explicit GroupBar(QWidget *parent = nullptr);

    // The lit group (0..13), or -1 for none. The cursor follows unless `moveCursor` is false.
    void setLitGroup(int group, bool moveCursor = true);
    int litGroup() const { return m_lit; }
    QSize sizeHint() const override { return {WidgetWidth, WidgetHeight}; }

signals:
    // A group button was pressed (click, Enter or Space).
    void groupPressed(int group);

protected:
    PanelButton::Look lookFor(int slot) const override;
    QString tipFor(int slot) const override;
    void activate(int slot) override;
    void paintBackdrop(QPainter *p) override;

private:
    int m_lit = -1;
};

}  // namespace d05
