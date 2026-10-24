#include "ui/PatchListView.h"

#include "ui/PatchDelegate.h"
#include "ui/PatchListModel.h"
#include "ui/Theme.h"

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyle>

namespace d05 {

PatchListView::PatchListView(QWidget *parent)
    : QListView(parent)
    , m_titleFont(Theme::font(Theme::Role::EmptyTitle))
    , m_noteFont(Theme::font(Theme::Role::Note))
    , m_titleMetrics(m_titleFont)
    , m_noteMetrics(m_noteFont)
{
    setObjectName(QStringLiteral("patchList"));
    setUniformItemSizes(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_MacShowFocusRect, false);
    // The scroll bars were created (and their style rules cached) before the object name
    // existed, so `QListView#patchList QScrollBar` did not match them. Re-polish.
    for (QWidget *w : {static_cast<QWidget *>(this), static_cast<QWidget *>(verticalScrollBar()),
                       static_cast<QWidget *>(horizontalScrollBar())}) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}

void PatchListView::setEmptyText(const QString &title, const QString &note)
{
    if (title == m_title && note == m_note)
        return;
    m_title = title;
    m_note = note;
    viewport()->update();
}

void PatchListView::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);
    if ((m_title.isEmpty() && m_note.isEmpty()) || (model() && model()->rowCount() > 0))
        return;
    QPainter p(viewport());
    const QFontMetricsF &tfm = m_titleMetrics;
    const QFontMetricsF &nfm = m_noteMetrics;
    const int w = viewport()->width();
    const int top = viewport()->height() * 38 / 100;
    const int titleBase = top + qRound(tfm.ascent());
    p.setFont(m_titleFont);
    p.setPen(Theme::labelSecondary);
    p.drawText(QPoint(qRound((w - tfm.horizontalAdvance(m_title)) / 2), titleBase), m_title);
    const int noteTop = top + qRound(tfm.ascent() + tfm.descent()) + 8;
    p.setFont(m_noteFont);
    p.setPen(Theme::labelDim);
    p.drawText(QPoint(qRound((w - nfm.horizontalAdvance(m_note)) / 2), noteTop + qRound(nfm.ascent())), m_note);
}

void PatchListView::setModel(QAbstractItemModel *m)
{
    QListView::setModel(m);
    if (m) {
        connect(m, &QAbstractItemModel::modelReset, this, &PatchListView::prepareDelegate);
        connect(m, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &a, const QModelIndex &b) {
            auto *delegate = qobject_cast<PatchDelegate *>(itemDelegate());
            auto *list = qobject_cast<PatchListModel *>(model());
            if (!delegate || !list)
                return;
            for (int r = a.row(); r <= b.row(); ++r)
                delegate->prepareRow(*list, r, viewport()->width());
        });
        prepareDelegate();
    }
}

// Row layouts (elision, highlight offsets) are measured here, never in paint.
void PatchListView::prepareDelegate()
{
    auto *delegate = qobject_cast<PatchDelegate *>(itemDelegate());
    auto *list = qobject_cast<PatchListModel *>(model());
    if (delegate && list)
        delegate->prepare(*list, viewport()->width());
}

void PatchListView::resizeEvent(QResizeEvent *event)
{
    QListView::resizeEvent(event);
    prepareDelegate();
}

// The star's hit area: the right 30 px of a row (DESIGN 7.2: centre 16 px from the edge).
bool PatchListView::starHit(const QPoint &pos, QModelIndex *index) const
{
    const QModelIndex i = indexAt(pos);
    if (!i.isValid() || pos.x() < viewport()->width() - 30)
        return false;
    *index = i;
    return true;
}

void PatchListView::mousePressEvent(QMouseEvent *event)
{
    QModelIndex i;
    if (event->button() == Qt::LeftButton && starHit(event->position().toPoint(), &i)) {
        m_swallowRelease = true;
        emit starClicked(i);
        event->accept();
        return;
    }
    QListView::mousePressEvent(event);
}

void PatchListView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QModelIndex i;
    m_swallowRelease = true;
    if (event->button() == Qt::LeftButton && starHit(event->position().toPoint(), &i)) {
        emit starClicked(i);  // a quick second click on a star toggles again
        event->accept();
        return;
    }
    QListView::mouseDoubleClickEvent(event);
    // Double-click renames a user row (preset rows refuse: edit() checks the item flags).
    // The first click already sent; this one must not.
    i = indexAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && i.isValid() && (model()->flags(i) & Qt::ItemIsEditable))
        edit(i);
}

void PatchListView::mouseReleaseEvent(QMouseEvent *event)
{
    const bool swallow = m_swallowRelease;
    m_swallowRelease = false;
    if (swallow && event->button() == Qt::LeftButton) {
        event->accept();  // the press (star) or the double-click already did its work
        return;
    }
    QListView::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton && !swallow) {
        const QModelIndex i = indexAt(event->position().toPoint());
        if (i.isValid())
            emit rowClicked(i);
    }
}

void PatchListView::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && (event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier) {
        if (currentIndex().isValid())
            emit sendRequested(currentIndex());
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space && (event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier) {
        if (!event->isAutoRepeat() && currentIndex().isValid())
            emit favoriteToggleRequested(currentIndex());
        event->accept();
        return;
    }
    QListView::keyPressEvent(event);
}

}  // namespace d05
