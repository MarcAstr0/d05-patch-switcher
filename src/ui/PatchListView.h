#pragma once

#include "core/PatchAddress.h"

#include <QFont>
#include <QFontMetricsF>
#include <QListView>
#include <QPersistentModelIndex>
#include <QPair>
#include <QString>
#include <optional>

namespace d05 {

// The list view (#patchList): row height and look come from PatchDelegate and the QSS;
// this adds the centred empty-state text (DESIGN.md 7.5) and the Enter key.
class PatchListView : public QListView {
    Q_OBJECT
public:
    explicit PatchListView(QWidget *parent = nullptr);

    void setModel(QAbstractItemModel *model) override;

    // The two lines drawn when the model has no rows; both empty = nothing drawn.
    void setEmptyText(const QString &title, const QString &note);
    QString emptyTitle() const { return m_title; }
    QString emptyNote() const { return m_note; }

    // Closes the open name editor, if any, and returns what it held (address, text) so the
    // caller can store it. Used before anything that would reset the list under the editor.
    std::optional<QPair<PatchAddress, QString>> takeEditor();
    using QListView::edit;  // keeps edit(index) visible next to the override below

signals:
    // The star of a row was pressed (the press does not select the row or send).
    void starClicked(const QModelIndex &index);
    // Space on the highlighted row.
    void favoriteToggleRequested(const QModelIndex &index);
    // Enter or Return on the highlighted row (a click is the view's own clicked()).
    void sendRequested(const QModelIndex &index);
    // A single left click released on a row. A double-click yields exactly one of these
    // (its second release is swallowed), so a double-click sends once and stays free
    // for the rename gesture on user rows.
    void rowClicked(const QModelIndex &index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) override;

private:
    void prepareDelegate();

    QFont m_titleFont;
    QFont m_noteFont;
    QFontMetricsF m_titleMetrics;
    QFontMetricsF m_noteMetrics;
    bool starHit(const QPoint &pos, QModelIndex *index) const;
    QPersistentModelIndex m_editIndex;
    std::optional<PatchAddress> m_pressAddress;  // the row of the last left press
    bool m_swallowRelease = false;  // the release after a double-click or a star press
    QString m_title;
    QString m_note;
};

}  // namespace d05
