#pragma once

#include <QImage>
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

    bool hasLogo() const { return !m_logo.isNull(); }
    // Logical size the logo is drawn at (empty without a logo).
    QSize logoSize() const;
    // Width of the wordmark as drawn (tests, About box).
    static int wordmarkWidth();

    QSize sizeHint() const override { return {SlotMaxWidth, SlotHeight}; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void rescale();

    QImage m_logo;
    QPixmap m_scaled;
    qreal m_scaledDpr = 0;
    QSize m_scaledFor;
    int m_wordmarkAdvance = 0;
};

}  // namespace d05
