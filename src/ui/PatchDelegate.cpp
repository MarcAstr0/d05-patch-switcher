#include "ui/PatchDelegate.h"

#include "ui/PatchListModel.h"
#include "ui/Theme.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QValidator>
#include <QTransform>

#include <cmath>

namespace d05 {

namespace {

int g_buildCount = 0;

constexpr int kBaseline = 19;
constexpr int kNameX = 84;
constexpr int kAddressX = 28;
constexpr int kCharCell = 9;

void frame1(QPainter *p, const QRect &r, const QBrush &c)
{
    p->fillRect(QRect(r.x(), r.y(), r.width(), 1), c);
    p->fillRect(QRect(r.x(), r.bottom(), r.width(), 1), c);
    p->fillRect(QRect(r.x(), r.y() + 1, 1, r.height() - 2), c);
    p->fillRect(QRect(r.right(), r.y() + 1, 1, r.height() - 2), c);
}

// Keeps printable ASCII, turns tabs and line breaks into spaces, cuts to 18. Returning
// Acceptable with a modified string makes QLineEdit adopt the modified text, so typing
// and pasting go through the same filter (a paste is cleaned, not refused).
class NameValidator : public QValidator {
public:
    using QValidator::QValidator;
    // QLineEdit hands the validator the whole candidate text; the last accepted text tells a
    // one-character insertion from a paste.
    State validate(QString &input, int &pos) const override
    {
        QString out;
        out.reserve(input.size());
        int newPos = 0;  // kept characters before the cursor
        int kept = 0;    // characters that survive the filter, before the cut
        for (int i = 0; i < input.size(); ++i) {
            const char16_t u = input.at(i).unicode();
            QChar ch;
            if (u == u'\t' || u == u'\r' || u == u'\n' || u == u'\v' || u == u'\f')
                ch = QLatin1Char(' ');
            else if (u >= 0x20 && u <= 0x7E)
                ch = input.at(i);
            else
                continue;
            ++kept;
            if (out.size() >= PatchDelegate::MaxNameLength)
                continue;
            out.append(ch);
            if (i < pos)
                ++newPos;
        }
        // One character typed at the limit is refused, not inserted at the cost of the last
        // one. Anything longer (a paste) is sanitised and cut.
        if (out.size() >= PatchDelegate::MaxNameLength && kept > PatchDelegate::MaxNameLength
            && kept - m_accepted.size() == 1)
            return Invalid;
        if (out != input) {
            input = out;
            pos = qMin(newPos, int(out.size()));
        }
        m_accepted = out;
        return Acceptable;
    }
private:
    mutable QString m_accepted;
};

// Esc and Enter belong to the editor while it is open (the window has an Esc shortcut
// that clears the search; without this it would steal the key and the focus loss would
// commit the edit).
class NameEditor : public QLineEdit {
public:
    using QLineEdit::QLineEdit;

protected:
    // QLineEdit ignores Return, which would then reach the list's Enter handler and
    // send the highlighted patch. The delegate's event filter has queued the commit.
    void keyPressEvent(QKeyEvent *e) override
    {
        QLineEdit::keyPressEvent(e);
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
            e->accept();
    }

    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::ShortcutOverride) {
            const int k = static_cast<QKeyEvent *>(e)->key();
            if (k == Qt::Key_Escape || k == Qt::Key_Return || k == Qt::Key_Enter) {
                e->accept();
                return true;
            }
        }
        return QLineEdit::event(e);
    }
};

}  // namespace

PatchDelegate::PatchDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , m_nameFont(Theme::font(Theme::Role::ListName))
    , m_addressFont(Theme::font(Theme::Role::ListAddress))
    , m_metaFont(Theme::font(Theme::Role::ListMeta))
    , m_nameMetrics(m_nameFont)
    , m_addressMetrics(m_addressFont)
{
    ++g_buildCount;
    static const char *chars = "0123456789UP-";
    for (int i = 0; i < 13; ++i) {
        m_chars[i] = QStaticText(QString(QLatin1Char(chars[i])));
        m_chars[i].setTextFormat(Qt::PlainText);
        m_chars[i].setPerformanceHint(QStaticText::AggressiveCaching);
        m_chars[i].prepare(QTransform(), m_addressFont);
        m_charX[i] = (kCharCell - m_addressMetrics.horizontalAdvance(QLatin1Char(chars[i]))) / 2.0;
    }
    m_charY = kBaseline - m_addressMetrics.ascent();
    m_modeText[0] = QStringLiteral("W");
    m_modeText[1] = QStringLiteral("D");
    m_modeText[2] = QStringLiteral("S");

    const double pi = 3.14159265358979323846;
    for (int k = 0; k < 10; ++k) {
        const double a = (-90 + 36 * k) * pi / 180.0;
        const double r = (k % 2 == 0) ? 6.0 : 2.6;
        const QPointF pt(r * std::cos(a), r * std::sin(a));
        if (k == 0)
            m_star.moveTo(pt);
        else
            m_star.lineTo(pt);
    }
    m_star.closeSubpath();
    for (int i = 0; i <= MaxNameLength; ++i)
        m_counter[i] = QString::number(i) + QStringLiteral("/18");
    refreshColours();
    connect(Theme::notifier(), &Theme::Notifier::skinChanged, this, [this] { refreshColours(); });
}

// The pens and brushes are built once per skin (DESIGN-2.md 2.6), never while painting.
void PatchDelegate::refreshColours()
{
    m_metaRed = QPen(Theme::colors().labelRed);
    m_starOffPen = QPen(Theme::colors().starOff, 1);
    m_starHoverPen = QPen(Theme::colors().labelSecondary, 1);
    m_selection = QBrush(Theme::colors().selection);
    m_barFocus = QBrush(Theme::colors().focusRing);
    m_barIdle = QBrush(Theme::colors().selectionBarIdle);
    m_hover = QBrush(Theme::colors().rowHover);
    m_ledOn = QBrush(Theme::colors().ledOn);
    m_ledCore = QBrush(Theme::colors().ledCore);
    m_match = QBrush(Theme::colors().matchBack);
    m_tag = QBrush(Theme::colors().tagBorder);
    m_starOn = QBrush(Theme::colors().labelPrimary);
    m_addrPrimary = QPen(Theme::colors().labelPrimary);
    m_addrSecondary = QPen(Theme::colors().labelSecondary);
    m_namePrimary = QPen(Theme::colors().labelPrimary);
    m_nameDim = QPen(Theme::colors().labelDim);
    m_meta = QPen(Theme::colors().labelSecondary);
}

void PatchDelegate::layoutRow(const PatchListModel::Row &row, int width, RowLayout *out) const
{
    const int nameW = width - 100 - kNameX;
    out->elided.clear();
    out->highlights.clear();
    out->visibleW = nameW;
    if (m_nameMetrics.horizontalAdvance(row.shown) > nameW) {
        out->elided = m_nameMetrics.elidedText(row.shown, Qt::ElideRight, nameW);
        // The ellipsis is the last character; highlights stop where the real text does.
        out->visibleW = m_nameMetrics.horizontalAdvance(row.shown, qMax(0, out->elided.size() - 1));
    }
    for (const SearchQuery::Range &rg : row.ranges) {
        const int x0 = m_nameMetrics.horizontalAdvance(row.name, rg.start);
        const int x1 = qMin(m_nameMetrics.horizontalAdvance(row.name, rg.start + rg.length), out->visibleW);
        if (x1 > x0)
            out->highlights.append({x0, x1 - x0});
    }
}

void PatchDelegate::prepare(const PatchListModel &model, int width)
{
    m_layoutWidth = width;
    m_layouts.resize(model.rowCount());
    for (int i = 0; i < m_layouts.size(); ++i)
        layoutRow(model.row(i), width, &m_layouts[i]);
}

void PatchDelegate::prepareRow(const PatchListModel &model, int row, int width)
{
    if (width != m_layoutWidth || row < 0 || row >= m_layouts.size() || m_layouts.size() != model.rowCount())
        return;  // the cache is not current; a full prepare() is due or paint computes inline
    layoutRow(model.row(row), width, &m_layouts[row]);
}

int PatchDelegate::resourceBuildCount()
{
    return g_buildCount;
}

QSize PatchDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    return QSize(option.rect.width(), RowHeight);
}

void PatchDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const auto *model = qobject_cast<const PatchListModel *>(index.model());
    if (!model) {
        QStyledItemDelegate::paint(p, option, index);
        return;
    }
    const PatchListModel::Row &row = model->row(index.row());
    const QRect r = option.rect;
    const int Wl = r.width();
    const bool selected = option.state & QStyle::State_Selected;
    const bool hover = option.state & QStyle::State_MouseOver;
    const bool listFocused = option.widget && option.widget->hasFocus();
    const bool isCurrent = row.address == model->current();

    p->save();
    p->translate(r.topLeft());

    if (selected) {
        p->fillRect(QRect(0, 0, Wl, RowHeight), m_selection);
        p->fillRect(QRect(0, 0, 3, RowHeight), listFocused ? m_barFocus : m_barIdle);
    } else if (hover) {
        p->fillRect(QRect(0, 0, Wl, RowHeight), m_hover);
    }

    if (isCurrent) {
        p->fillRect(QRect(10, 12, 10, 4), m_ledOn);
        p->fillRect(QRect(12, 13, 6, 2), m_ledCore);
    }

    // Address: five characters, each centred in a 9 px cell.
    const PatchAddress &a = row.address;
    const int cells[5] = {a.isUser() ? 10 : 11, a.groupNumber(), 12, a.bank(), a.number()};
    p->setFont(m_addressFont);
    p->setPen(isCurrent ? m_addrPrimary : m_addrSecondary);
    for (int i = 0; i < 5; ++i)
        p->drawStaticText(QPointF(kAddressX + kCharCell * i + m_charX[cells[i]], m_charY), m_chars[cells[i]]);

    // Name, with the search ranges drawn first (all measured in prepare()).
    RowLayout local;
    const RowLayout *rl;
    if (m_layoutWidth == Wl && index.row() < m_layouts.size()) {
        rl = &m_layouts[index.row()];
    } else {
        layoutRow(row, Wl, &local);  // stale cache: same values, computed here
        rl = &local;
    }
    for (const QPair<int, int> &h : rl->highlights)
        p->fillRect(QRect(kNameX + h.first, 4, h.second, 20), m_match);
    p->setFont(m_nameFont);
    p->setPen(row.name.isEmpty() ? m_nameDim : m_namePrimary);
    p->drawText(QPoint(kNameX, kBaseline), rl->elided.isEmpty() ? row.shown : rl->elided);

    if (m_editing && *m_editing == row.address) {
        // The editor is open on this row: n/18 in place of the count.
        p->setFont(m_metaFont);
        p->setPen(m_editLength >= MaxNameLength ? m_metaRed : m_meta);
        p->drawText(QRect(Wl - 100, 0, 40, RowHeight), Qt::AlignRight | Qt::AlignVCenter, m_counter[m_editLength]);
    } else if (!row.countText.isEmpty()) {
        p->setFont(m_metaFont);
        p->setPen(m_meta);
        p->drawText(QRect(Wl - 100, 0, 40, RowHeight), Qt::AlignRight | Qt::AlignVCenter, row.countText);
    }

    if (!row.mode.isNull()) {
        const QRect box(Wl - 52, 6, 16, 16);
        frame1(p, box, m_tag);
        const int mi = row.mode == QLatin1Char('W') ? 0 : row.mode == QLatin1Char('D') ? 1 : 2;
        p->setFont(m_metaFont);
        p->setPen(m_meta);
        p->drawText(box, Qt::AlignCenter, m_modeText[mi]);
    }

    // Star (the only antialiased element of a row).
    p->setRenderHint(QPainter::Antialiasing, true);
    p->translate(Wl - 16, 14);
    if (row.favorite) {
        p->setPen(Qt::NoPen);
        p->setBrush(m_starOn);
    } else {
        p->setPen(hover ? m_starHoverPen : m_starOffPen);
        p->setBrush(Qt::NoBrush);
    }
    p->drawPath(m_star);
    p->restore();
}

// ---------------------------------------------------------------------------- editor

QWidget *PatchDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    const auto *model = qobject_cast<const PatchListModel *>(index.model());
    if (!model || !model->addressAt(index).isUser())
        return nullptr;
    auto *e = new NameEditor(parent);
    e->setObjectName(QStringLiteral("nameEditor"));
    e->setFont(m_nameFont);
    // The 18 limit lives in the validator, not setMaxLength: it must drop characters that
    // are not printable ASCII before counting, so a paste fills the 18 with usable text.
    e->setValidator(new NameValidator(e));
    e->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_editing = model->addressAt(index);
    m_editLength = 0;
    QWidget *viewport = parent;
    connect(e, &QLineEdit::textChanged, this, [this, e, viewport](const QString &text) {
        const int n = qMin(int(text.size()), int(MaxNameLength));
        if (n == m_editLength)
            return;
        m_editLength = n;
        viewport->update(QRect(0, e->y() - 2, viewport->width(), RowHeight));
    });
    m_editor = e;
    // Only the current editor's destruction ends the editing state: a closed editor is
    // deleted later, possibly after the next one opened.
    connect(e, &QObject::destroyed, this, [this](QObject *gone) {
        if (gone == m_editor) {
            m_editing.reset();
            m_editor = nullptr;
        }
    });
    return e;
}

void PatchDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    const auto *model = qobject_cast<const PatchListModel *>(index.model());
    auto *e = qobject_cast<QLineEdit *>(editor);
    if (!model || !e)
        return;
    // The view calls this again whenever the model reports a change (a favourite, the
    // current LED); that must not overwrite what the user has typed.
    if (e->property("d05Initialised").toBool())
        return;
    e->setProperty("d05Initialised", true);
    e->setText(model->row(index.row()).name);
    e->selectAll();
    m_editLength = qMin(int(e->text().size()), int(MaxNameLength));
}

void PatchDelegate::setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex &index) const
{
    const auto *model = qobject_cast<const PatchListModel *>(index.model());
    auto *e = qobject_cast<QLineEdit *>(editor);
    if (model && e)
        emit nameCommitted(model->addressAt(index), e->text());
}

void PatchDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    // The width comes from the viewport the row paint uses, so the editor ends at Wl - 100,
    // 20 px before the counter column (DESIGN 7.4).
    const QRect r = option.rect;
    const int Wl = editor->parentWidget() ? editor->parentWidget()->width() : r.width();
    editor->setGeometry(r.x() + 80, r.y() + 2, Wl - 180, 24);
}

}  // namespace d05
