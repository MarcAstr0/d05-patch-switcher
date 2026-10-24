#pragma once

#include "ui/PatchListModel.h"

#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QPainterPath>
#include <QPen>
#include <QStaticText>
#include <QStyledItemDelegate>
#include <QVector>

namespace d05 {

// Paints one patch row (DESIGN.md 7.2, 7.3). Everything that could be built ahead of
// time is built in the constructor: the three fonts and their metrics, a QStaticText
// for each address character (0 to 9, U, P, -), the key-mode letters, the star path.
// paint() then only fills rects and draws prebuilt text; the one string it may create
// is the elided name of a row too narrow for it.
class PatchDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    static constexpr int RowHeight = 28;

    explicit PatchDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // Number of times the prebuilt resources were created in this process: one per
    // delegate, never per paint (tests).
    static int resourceBuildCount();

    // Computes, outside any paint, each row's elided name and the x offsets and widths of
    // its search highlights for a row `width` px wide (DESIGN 7.3). PatchListView calls it
    // when the model resets and when the view is resized. A paint finding the cache stale
    // (wrong row count or width) falls back to computing the same values inline.
    void prepare(const PatchListModel &model, int width);

private:
    struct RowLayout {
        QString elided;     // empty when the name fits
        int visibleW = 0;   // px of name text actually visible (without the ellipsis)
        QVector<QPair<int, int>> highlights;  // x relative to the name start, width; clipped
    };
    void layoutRow(const PatchListModel::Row &, int width, RowLayout *out) const;
    QVector<RowLayout> m_layouts;
    int m_layoutWidth = -1;
    QFont m_nameFont;
    QFont m_addressFont;
    QFont m_metaFont;
    QFontMetrics m_nameMetrics;
    QFontMetrics m_addressMetrics;
    QStaticText m_chars[13];  // 0-9, U, P, -
    qreal m_charX[13] = {};   // x offset that centres each character in its 9 px cell
    qreal m_charY = 0;        // top of the static text for a baseline at y 19
    QString m_modeText[3];    // W, D, S
    QPainterPath m_star;
    QPen m_starOffPen;
    QPen m_starHoverPen;
    // Prebuilt brushes and pens: handing QPainter a QColor makes it allocate a QBrush
    // or QPen each time.
    QBrush m_selection, m_barFocus, m_barIdle, m_hover, m_ledOn, m_ledCore, m_match, m_tag, m_starOn;
    QPen m_addrPrimary, m_addrSecondary, m_namePrimary, m_nameDim, m_meta;
};

}  // namespace d05
