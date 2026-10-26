#pragma once

#include <QAbstractButton>
#include <QFont>
#include <QPoint>
#include <QString>

class QPainter;

namespace d05 {

// A custom-painted panel button (DESIGN.md 6.2). The face is 44 x 24; the widget is
// 52 x 32 so the 2 px focus ring that sits 4 px outside the face has room. GroupBar and
// BankGrid paint their buttons through the same static paintFace().
class PanelButton : public QAbstractButton {
    Q_OBJECT
public:
    enum class Kind { Led, LegendLed, Plain, WedgeLeft, WedgeRight };
    // Which of the five colour sets of DESIGN-2.md 5.2 colours the face: the number row, group
    // bar and engine buttons (`button`), the bank row, ALL / FAVOURITES / RECENTS, AUTO SEND,
    // and PREV / NEXT / SYSTEM. On the D-05 all five resolve to the version 1 button colours.
    enum class FaceSet { Button, Bank, View, Commit, Transport };

    // Everything paintFace() needs about one button.
    struct Look {
        Kind kind = Kind::Led;
        QString legend;  // LegendLed only
        bool lit = false;
        bool hover = false;
        bool pressed = false;
        bool focused = false;
        bool disabled = false;
        bool armed = false;  // lens skins: ring and dark centre (DESIGN-2.md 5.2); only if not lit
        FaceSet faces = FaceSet::Button;
    };

    static constexpr int FaceW = 44;
    static constexpr int FaceH = 24;
    static constexpr int Margin = 4;  // room around the face for the ring

    explicit PanelButton(Kind kind, QWidget *parent = nullptr);

    Kind kind() const { return m_kind; }
    void setLegend(const QString &legend);
    QString legend() const { return m_legend; }
    FaceSet faceSet() const { return m_faces; }
    void setFaceSet(FaceSet faces);
    // Lit LED: set by hand, or follows the checked state of a checkable button.
    void setLit(bool lit);
    bool lit() const { return m_lit || (isCheckable() && isChecked()); }

    // Widget rect for a face whose top-left is at `face` (parent coordinates).
    static QRect widgetRectForFace(const QPoint &face)
    {
        return QRect(face.x() - Margin, face.y() - Margin, FaceW + 2 * Margin, FaceH + 2 * Margin);
    }

    // The advance of `text` in `font` at `dpr`, cached so painting measures nothing: one cache
    // per (font, device pixel ratio), for the handful of fonts a window paints with.
    static qreal textAdvance(const QString &text, const QFont &font, qreal dpr);
    // How many times a text advance was measured (tests: painting must not add to it).
    static int metricsBuildCount();

    // Draws one button with its face's top-left at `face`.
    static void paintFace(QPainter *p, const QPoint &face, const Look &look);

    QSize sizeHint() const override { return {FaceW + 2 * Margin, FaceH + 2 * Margin}; }

protected:
    void paintEvent(QPaintEvent *event) override;
    bool hitButton(const QPoint &pos) const override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    Kind m_kind;
    QString m_legend;
    bool m_lit = false;
    bool m_hover = false;
    FaceSet m_faces = FaceSet::Button;
};

}  // namespace d05
