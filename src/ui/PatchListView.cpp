#include "ui/PatchListView.h"

#include "ui/PatchDelegate.h"
#include "ui/PatchListModel.h"
#include "ui/Theme.h"

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QLineEdit>
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
    connect(Theme::notifier(), &Theme::Notifier::skinChanged, this, [this] { viewport()->update(); });
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
    p.setPen(Theme::colors().labelSecondary);
    p.drawText(QPoint(qRound((w - tfm.horizontalAdvance(m_title)) / 2), titleBase), m_title);
    const int noteTop = top + qRound(tfm.ascent() + tfm.descent()) + 8;
    p.setFont(m_noteFont);
    p.setPen(Theme::colors().labelDim);
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
    if (event->button() == Qt::LeftButton) {
        const QModelIndex under = indexAt(event->position().toPoint());
        if (under.isValid() && qobject_cast<PatchListModel *>(model()))
            m_pressAddress = static_cast<PatchListModel *>(model())->addressAt(under);
        else
            m_pressAddress.reset();
    }
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
    auto *list = qobject_cast<PatchListModel *>(model());
    if (event->button() != Qt::LeftButton || !list) {
        QListView::mouseDoubleClickEvent(event);
        return;
    }
    if (starHit(event->position().toPoint(), &i)) {
        // A second click on the same star toggles again. If the first click removed the
        // row (Favourites view) another row has slid under the cursor: leave that one alone.
        if (m_pressAddress && list->addressAt(i) == *m_pressAddress)
            emit starClicked(i);
        event->accept();
        return;
    }
    // The base class turns a double-click on a row other than the pressed one (the Recents
    // reorder) into a press of its own, which would overwrite m_pressAddress.
    const std::optional<PatchAddress> pressed = m_pressAddress;
    QListView::mouseDoubleClickEvent(event);
    // Double-click renames a user row (preset rows refuse: edit() checks the item flags).
    // The first click already sent, and in the Recents view that moved the row: edit the
    // address that was pressed, wherever it is now, never whatever is under the cursor.
    if (pressed && pressed->isUser()) {
        const QModelIndex target = list->indexOf(*pressed);
        if (target.isValid() && (list->flags(target) & Qt::ItemIsEditable)) {
            setCurrentIndex(target);
            edit(target);
        }
    }
}

bool PatchListView::edit(const QModelIndex &index, EditTrigger trigger, QEvent *event)
{
    const bool opened = QListView::edit(index, trigger, event);
    if (opened)
        m_editIndex = index;
    return opened;
}

std::optional<QPair<PatchAddress, QString>> PatchListView::takeEditor()
{
    auto *list = qobject_cast<PatchListModel *>(model());
    if (!list || !m_editIndex.isValid())
        return std::nullopt;
    auto *editor = qobject_cast<QLineEdit *>(indexWidget(m_editIndex));
    if (!editor || editor->isHidden())
        return std::nullopt;
    const QPair<PatchAddress, QString> held(list->addressAt(m_editIndex), editor->text());
    closeEditor(editor, QAbstractItemDelegate::NoHint);
    m_editIndex = QModelIndex();
    return held;
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
    if (!event->isAutoRepeat())
        m_repeatKey = 0;  // a fresh press (this key or any other) ends a stale hold
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && (event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier) {
        if (currentIndex().isValid() && !event->isAutoRepeat())  // a held Enter sends once
            emit sendRequested(currentIndex());
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space && (event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier) {
        // An invalid index (empty list) still asks: the window toggles the current patch.
        if (!event->isAutoRepeat())
            emit favoriteToggleRequested(currentIndex());
        event->accept();
        return;
    }
    const QModelIndex before = currentIndex();
    QListView::keyPressEvent(event);
    const QModelIndex after = currentIndex();
    if (after.isValid() && after != before) {
        if (event->isAutoRepeat())
            m_repeatKey = event->key();
        else
            emit highlightMovedByKey(after);
    }
}

void PatchListView::keyReleaseEvent(QKeyEvent *event)
{
    QListView::keyReleaseEvent(event);
    if (!event->isAutoRepeat() && m_repeatKey != 0 && event->key() == m_repeatKey) {
        m_repeatKey = 0;
        if (currentIndex().isValid())
            emit highlightMovedByKey(currentIndex());
    }
}

void PatchListView::focusOutEvent(QFocusEvent *event)
{
    m_repeatKey = 0;  // the release will go elsewhere
    QListView::focusOutEvent(event);
}

}  // namespace d05
