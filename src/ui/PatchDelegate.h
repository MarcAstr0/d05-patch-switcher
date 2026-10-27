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
#include <optional>

#include "core/PatchAddress.h"

namespace d05 {

// Paints one patch row (DESIGN.md 7.2, 7.3). Everything that could be built ahead of
// time is built in the constructor: the three fonts and their metrics, a QStaticText
// for each address character (0 to 9, U, P, -), the key-mode letters, the star path.
// paint() then only fills rects and draws prebuilt text; the one string it may create
// is the elided name of a row too narrow for it.
//
// Per model (DESIGN-2.md 6): the D-05 row is version 1's (five address cells, key-mode tag); the
// JP-08 and JU-06A rows have three address cells and no key-mode tag, the JU-06A row an engine tag
// (framed for the believed engine, dim for the other), the LED column of the armed selection and of
// the other engine's remembered patch, and a renamed slot shows its user name and, dim after it,
// its factory name, with the search highlights under whichever name matched.
class PatchDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    static constexpr int RowHeight = 28;

    explicit PatchDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // The name editor (DESIGN 7.4, DESIGN-2.md 6.5) on a D-05 user slot or any JP-08 / JU-06A slot:
    // a QLineEdit #nameEditor over the name column taking up to 18 printable ASCII characters
    // (filtered as typed, sanitised when pasted). Enter commits through nameCommitted (the model
    // itself is never written), Esc cancels. While it is open the row's count column shows n/18, and
    // on the 64-patch models its placeholder is the factory name and the factory name is not drawn.
    static constexpr int MaxNameLength = 18;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // Number of times the prebuilt resources were created in this process: one per
    // delegate, never per paint (tests).
    static int resourceBuildCount();

    // Computes, outside any paint, each row's elided name and the x offsets and widths of
    // its search highlights for a row `width` px wide (DESIGN 7.3). PatchListView calls it
    // when the model resets and when the view is resized. A paint finding the cache stale
    // (wrong row count or width) falls back to computing the same values inline.
    void prepare(const PatchListModel &model, int width);
    // The same for one row (a dataChanged: rename, favourite).
    void prepareRow(const PatchListModel &model, int row, int width);

signals:
    // Enter in the editor: the text as typed (the Library sanitises it again).
    void nameCommitted(const PatchAddress &address, const QString &text) const;

private:
    struct RowLayout {
        QString elided;     // empty when the name fits
        QStaticText nameStatic;     // what is drawn for the name (prepared here, never while painting)
        QStaticText factoryStatic;  // what is drawn for the factory name
        int visibleW = 0;   // px of name text actually visible (without the ellipsis)
        QVector<QPair<int, int>> highlights;  // x relative to the name start, width; clipped
        // The factory name beside a user name (JP-08 / JU-06A), after a 12 px gap.
        QString factoryText;     // what is drawn (elided when it does not fit); empty = not drawn
        int factoryX = 0;        // relative to the name start
        QVector<QPair<int, int>> factoryHighlights;  // x relative to the factory start, width; clipped
    };
    void layoutRow(const PatchListModel::Row &, int width, RowLayout *out) const;
    void refreshColours();

    QVector<RowLayout> m_layouts;
    int m_layoutWidth = -1;
    QFont m_nameFont;
    QFont m_addressFont;
    QFont m_metaFont;
    QFontMetrics m_nameMetrics;
    QFontMetrics m_addressMetrics;
    QStaticText m_chars[13];  // 0-9, U, P, -
    qreal m_charX[13] = {};   // x offset that centres each character in its 9 px cell
    qreal m_nameY = 0;        // top of the name static texts for a baseline at y 19
    qreal m_charY = 0;        // top of the static text for a baseline at y 19
    QString m_modeText[3];    // W, D, S
    QPainterPath m_star;
    QPen m_starOffPen;
    QPen m_starHoverPen;
    // Prebuilt brushes and pens: handing QPainter a QColor makes it allocate a QBrush
    // or QPen each time.
    QBrush m_selection, m_barFocus, m_barIdle, m_hover, m_ledOn, m_ledOff, m_ledCore, m_match, m_tag, m_starOn;
    QString m_engineText[2];  // 60, 106
    QString m_counter[MaxNameLength + 1];  // "0/18" to "18/18", built once
    QPen m_metaRed;
    mutable std::optional<PatchAddress> m_editing;
    mutable int m_editLength = 0;
    mutable QObject *m_editor = nullptr;  // identity only, never dereferenced
    QPen m_addrPrimary, m_addrSecondary, m_namePrimary, m_nameDim, m_meta;
};

}  // namespace d05
