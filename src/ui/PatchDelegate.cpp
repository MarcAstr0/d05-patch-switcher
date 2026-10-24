#include "ui/PatchDelegate.h"

#include "ui/PatchListModel.h"
#include "ui/Theme.h"

#include <QPainter>
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
    m_starOffPen = QPen(Theme::starOff, 1);
    m_starHoverPen = QPen(Theme::labelSecondary, 1);
    m_selection = QBrush(Theme::selection);
    m_barFocus = QBrush(Theme::focusRing);
    m_barIdle = QBrush(Theme::selectionBarIdle);
    m_hover = QBrush(Theme::rowHover);
    m_ledOn = QBrush(Theme::ledOn);
    m_ledCore = QBrush(Theme::ledCore);
    m_match = QBrush(Theme::matchBack);
    m_tag = QBrush(Theme::tagBorder);
    m_starOn = QBrush(Theme::labelPrimary);
    m_addrPrimary = QPen(Theme::labelPrimary);
    m_addrSecondary = QPen(Theme::labelSecondary);
    m_namePrimary = QPen(Theme::labelPrimary);
    m_nameDim = QPen(Theme::labelDim);
    m_meta = QPen(Theme::labelSecondary);
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

    if (!row.countText.isEmpty()) {
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

}  // namespace d05
