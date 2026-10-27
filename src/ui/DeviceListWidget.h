#pragma once

#include "core/Model.h"

#include <QString>
#include <QWidget>

#include <array>

namespace d05 {

// The device list of the Settings dialog (DESIGN-2.md 7.2): a well of three rows of 44 px, always
// D-05, JP-08, JU-06A, each with an enabled checkbox (the version 1 LED look), the model name and,
// when enabled, a connection LED with a short text. Painted here; the QSS does not style it.
// It only reports what the user did (a row selected, a checkbox toggled): the dialog owns the data.
//
// Keyboard: Up / Down select a row, Space toggles enabled. Mouse: a click on x 0 to 32 toggles
// enabled, elsewhere it selects.
class DeviceListWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr int Width = 140;
    static constexpr int RowHeight = 44;
    static constexpr int Height = 2 + 3 * RowHeight;

    // How a row's connection text is coloured (DESIGN-2.md 7.2).
    enum class Tone { Secondary, Red, Dim };
    struct Row {
        bool enabled = false;
        bool lit = false;       // the connection LED
        QString text;           // "Connected", "Port not found", "Error", "No output port"; "Off" when disabled
        Tone tone = Tone::Dim;
        QString tooltip;
        bool operator==(const Row &o) const
        {
            return enabled == o.enabled && lit == o.lit && text == o.text && tone == o.tone && tooltip == o.tooltip;
        }
    };

    explicit DeviceListWidget(QWidget *parent = nullptr);

    void setRow(Model m, const Row &row);  // repaints only when the row changed
    Row row(Model m) const { return m_rows[int(m)]; }
    Model current() const { return m_current; }
    void setCurrent(Model m);

    // Geometry for tests: a row's rect and its checkbox hit area, widget coordinates.
    static QRect rowRect(Model m) { return QRect(1, 1 + int(m) * RowHeight, Width - 2, RowHeight); }
    static QRect toggleRect(Model m) { return QRect(0, 1 + int(m) * RowHeight, 32, RowHeight); }

    QSize sizeHint() const override { return {Width, Height}; }

signals:
    void currentChanged(d05::Model model);
    void enabledToggled(d05::Model model, bool enabled);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool event(QEvent *event) override;

private:
    int rowAt(const QPoint &p) const;
    void setHover(int row);

    std::array<Row, 3> m_rows;
    Model m_current = Model::D05;
    int m_hover = -1;
};

}  // namespace d05
