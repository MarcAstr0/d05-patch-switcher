#include "ui/PatchListView.h"

#include "ui/Theme.h"

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyle>

namespace d05 {

PatchListView::PatchListView(QWidget *parent)
    : QListView(parent)
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
    const QFont titleFont = Theme::font(Theme::Role::EmptyTitle);
    const QFont noteFont = Theme::font(Theme::Role::Note);
    const QFontMetricsF tfm(titleFont);
    const QFontMetricsF nfm(noteFont);
    const int w = viewport()->width();
    const int top = viewport()->height() * 38 / 100;
    const int titleBase = top + qRound(tfm.ascent());
    p.setFont(titleFont);
    p.setPen(Theme::labelSecondary);
    p.drawText(QPoint(qRound((w - tfm.horizontalAdvance(m_title)) / 2), titleBase), m_title);
    const int noteTop = top + qRound(tfm.ascent() + tfm.descent()) + 8;
    p.setFont(noteFont);
    p.setPen(Theme::labelDim);
    p.drawText(QPoint(qRound((w - nfm.horizontalAdvance(m_note)) / 2), noteTop + qRound(nfm.ascent())), m_note);
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
    QListView::keyPressEvent(event);
}

}  // namespace d05
