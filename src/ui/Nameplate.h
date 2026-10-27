#pragma once

#include <QPixmap>
#include <QWidget>

class QPainter;

namespace d05 {

// The nameplate slot (DESIGN.md 4.5, SPEC FR-14): the owner's logo.png when the file
// exists, otherwise the plain-text wordmark. The logo is loaded once and scaled once
// per device pixel ratio; paintEvent allocates nothing.
class Nameplate : public QWidget {
    Q_OBJECT
public:
    static constexpr int SlotHeight = 28;
    static constexpr int SlotMaxWidth = 360;

    explicit Nameplate(const QString &logoPath, QWidget *parent = nullptr);

    // Largest side accepted for logo.png; anything bigger falls back to the wordmark.
    static constexpr int MaxLogoSide = 4096;

    bool hasLogo() const { return !m_sourceSize.isEmpty(); }
    // Logical size the logo is drawn at (empty without a logo).
    QSize logoSize() const;
    // Width of the wordmark as drawn in the current skin (tests, About box): the plate, the divider and
    // the sub line, which is `sub` when given, else the skin's.
    static int wordmarkWidth(const QString &sub = QString());
    // Replaces the skin's sub line (the About box names the three models); empty = the skin's.
    void setSubText(const QString &sub);

    QSize sizeHint() const override { return {SlotMaxWidth, SlotHeight}; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;

    // The plate: the logo, or the wordmark and, when `withSub`, the divider and the skin's sub
    // line. Returns the plate's right edge (the logo's width, or the wordmark's advance).
    int paintPlate(QPainter &p, bool withSub);
    // Where the plate ends in the current skin (the DeviceSwitcher's divider is 14 px after it).
    int plateRight() const;

private:
    void rescale();
    void refreshSkin();

    QString m_path;
    QSize m_sourceSize;  // from the header; empty when there is no usable logo
    QPixmap m_scaled;
    qreal m_scaledDpr = 0;
    QSize m_scaledFor;
    QFont m_wordFont;
    QFont m_subFont;
    int m_wordmarkAdvance = 0;
    QString m_subText;
    QString m_subOverride;
};

}  // namespace d05
