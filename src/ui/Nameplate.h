#pragma once

#include <QPixmap>
#include <QWidget>

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
    // Width of the wordmark as drawn (tests, About box).
    static int wordmarkWidth();

    QSize sizeHint() const override { return {SlotMaxWidth, SlotHeight}; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;

private:
    void rescale();

    QString m_path;
    QSize m_sourceSize;  // from the header; empty when there is no usable logo
    QPixmap m_scaled;
    qreal m_scaledDpr = 0;
    QSize m_scaledFor;
    QFont m_wordFont;
    QFont m_subFont;
    int m_wordmarkAdvance = 0;
};

}  // namespace d05
