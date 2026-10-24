#pragma once

#include <QListView>
#include <QString>

namespace d05 {

// The list view (#patchList): row height and look come from PatchDelegate and the QSS;
// this adds the centred empty-state text (DESIGN.md 7.5) and the Enter key.
class PatchListView : public QListView {
    Q_OBJECT
public:
    explicit PatchListView(QWidget *parent = nullptr);

    // The two lines drawn when the model has no rows; both empty = nothing drawn.
    void setEmptyText(const QString &title, const QString &note);
    QString emptyTitle() const { return m_title; }
    QString emptyNote() const { return m_note; }

signals:
    // Enter or Return on the highlighted row (a click is the view's own clicked()).
    void sendRequested(const QModelIndex &index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QString m_title;
    QString m_note;
};

}  // namespace d05
