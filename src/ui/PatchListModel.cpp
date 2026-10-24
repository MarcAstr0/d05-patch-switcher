#include "ui/PatchListModel.h"

#include "core/Library.h"
#include "core/PatchCatalog.h"

#include <QKeySequence>

namespace d05 {

PatchListModel::PatchListModel(const PatchCatalog &catalog, const Library &library, QObject *parent)
    : QAbstractListModel(parent)
    , m_catalog(catalog)
    , m_library(library)
    , m_rowOf(PatchAddress::CatalogueSize, -1)
{
    connect(&m_library, &Library::favoritesChanged, this, &PatchListModel::onFavoritesChanged);
    connect(&m_library, &Library::recentsChanged, this, [this] {
        if (m_view == View::Recents)
            rebuild();
    });
    connect(&m_library, &Library::userNamesChanged, this, &PatchListModel::onUserNamesChanged);
    rebuild();
}

int PatchListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant PatchListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows[index.row()];
    switch (role) {
    case Qt::DisplayRole: return r.shown;
    case AddressRole: return r.address.toString();
    default: return {};
    }
}

Qt::ItemFlags PatchListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (m_rows[index.row()].address.isUser())
        f |= Qt::ItemIsEditable;
    return f;
}

void PatchListModel::setView(View view)
{
    if (view == m_view)
        return;
    m_view = view;
    rebuild();
}

void PatchListModel::setQuery(const QString &text)
{
    if (text == m_queryText)
        return;
    m_queryText = text;
    m_query = SearchQuery(text);
    rebuild();
}

void PatchListModel::setCurrent(const PatchAddress &address)
{
    if (address == m_current)
        return;
    const int before = rowOf(m_current);
    const int after = rowOf(address);
    m_current = address;
    for (int r : {before, after}) {
        if (r >= 0)
            emit dataChanged(index(r), index(r));
    }
}

PatchAddress PatchListModel::addressAt(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return PatchAddress();
    return m_rows[index.row()].address;
}

int PatchListModel::rowOf(const PatchAddress &address) const
{
    return m_rowOf[address.catalogueIndex()];
}

QModelIndex PatchListModel::indexOf(const PatchAddress &address) const
{
    const int r = rowOf(address);
    return r < 0 ? QModelIndex() : index(r);
}

QString PatchListModel::emptyTitle() const
{
    if (!m_rows.isEmpty())
        return QString();
    if (searchActive())
        return QStringLiteral("NO MATCHES");
    switch (m_view) {
    case View::Favourites: return QStringLiteral("NO FAVOURITES");
    case View::Recents: return QStringLiteral("NOTHING SENT YET");
    case View::All: break;
    }
    return QString();
}

QString PatchListModel::emptyNote() const
{
    if (!m_rows.isEmpty())
        return QString();
    if (searchActive())
        return QStringLiteral("Esc clears the search");
    switch (m_view) {
    case View::Favourites:
        return QStringLiteral("Space or %1 adds the highlighted patch")
            .arg(QKeySequence(Qt::CTRL | Qt::Key_D).toString(QKeySequence::NativeText));
    case View::Recents: return QStringLiteral("Patches you send appear here");
    case View::All: break;
    }
    return QString();
}

void PatchListModel::onFavoritesChanged()
{
    if (m_view == View::Favourites) {
        rebuild();
        return;
    }
    for (int r = 0; r < m_rows.size(); ++r) {
        const bool fav = m_library.isFavorite(m_rows[r].address);
        if (fav != m_rows[r].favorite) {
            m_rows[r].favorite = fav;
            emit dataChanged(index(r), index(r));
        }
    }
}

// A rename. With no search the visible set cannot change, so only the renamed rows
// repaint; with a search the name may start or stop matching, so rebuild.
void PatchListModel::onUserNamesChanged()
{
    if (searchActive()) {
        rebuild();
        return;
    }
    for (int r = 0; r < m_rows.size(); ++r) {
        Row &row = m_rows[r];
        if (!row.address.isUser())
            continue;
        const QString name = m_catalog.name(row.address);
        if (name == row.name)
            continue;
        row.name = name;
        row.shown = name.isEmpty() ? row.address.toString() : name;
        emit dataChanged(index(r), index(r));
    }
}

void PatchListModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    m_rowOf.fill(-1);

    const auto add = [this](const PatchAddress &a, int count) {
        Row row;
        row.address = a;
        row.name = m_catalog.name(a);
        if (!m_query.matches(a, row.name, &row.ranges))
            return;
        row.shown = row.name.isEmpty() ? a.toString() : row.name;
        row.mode = m_catalog.mode(a);
        row.favorite = m_library.isFavorite(a);
        if (count > 0)
            row.countText = QStringLiteral("×") + QString::number(count);
        m_rowOf[a.catalogueIndex()] = m_rows.size();
        m_rows.append(std::move(row));
    };

    switch (m_view) {
    case View::All:
        for (int i = 0; i < PatchAddress::CatalogueSize; ++i)
            add(*PatchAddress::fromCatalogueIndex(i), 0);
        break;
    case View::Favourites:
        for (const PatchAddress &a : m_library.favorites())
            add(a, 0);
        break;
    case View::Recents:
        for (const Library::Recent &r : m_library.recents())
            add(r.address, r.count);
        break;
    }
    endResetModel();
}

}  // namespace d05
