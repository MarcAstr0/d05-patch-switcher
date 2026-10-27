#include "ui/PatchListModel.h"

#include "core/Library.h"
#include "core/PatchCatalog.h"

#include <QKeySequence>

namespace d05 {

PatchListModel::PatchListModel(const PatchCatalog &catalog, const Library &library, QObject *parent)
    : QAbstractListModel(parent)
    , m_catalog(&catalog)
    , m_library(library)
    , m_modelId(catalog.model())
    , m_rowOf(modelInfo(catalog.model()).catalogueSize, -1)
{
    connect(&m_library, &Library::favoritesChanged, this, &PatchListModel::onFavoritesChanged);
    connect(&m_library, &Library::recentsChanged, this, &PatchListModel::onRecentsChanged);
    connect(&m_library, &Library::userNamesChanged, this, &PatchListModel::onUserNamesChanged);
    rebuild();
}

void PatchListModel::setDevice(const PatchCatalog &catalog, const QString &queryText, View view)
{
    beginResetModel();
    m_catalog = &catalog;
    m_modelId = catalog.model();
    m_rowOf.fill(-1, modelInfo(m_modelId).catalogueSize);
    m_queryText = queryText;
    m_query = SearchQuery(queryText, m_modelId);
    m_view = view;
    m_hasCurrent = false;
    m_armed.reset();
    m_otherRemembered.reset();
    fillRows();
    endResetModel();
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
    case Qt::ToolTipRole:
        if (!r.factory.isEmpty())
            return QStringLiteral("Factory name: ") + r.factory;
        return {};
    default: return {};
    }
}

Qt::ItemFlags PatchListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (canRename(m_rows[index.row()].address))
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
    m_query = SearchQuery(text, m_modelId);
    rebuild();
}

void PatchListModel::setCurrent(const PatchAddress &address)
{
    if (address == m_current && m_hasCurrent)
        return;
    const int before = m_hasCurrent ? rowOf(m_current) : -1;
    const int after = rowOf(address);
    m_current = address;
    m_hasCurrent = true;
    for (int r : {before, after}) {
        if (r >= 0)
            emit dataChanged(index(r), index(r));
    }
}

void PatchListModel::clearCurrent()
{
    if (!m_hasCurrent)
        return;
    const int before = rowOf(m_current);
    m_hasCurrent = false;
    if (before >= 0)
        emit dataChanged(index(before), index(before));
}

void PatchListModel::emitRow(const std::optional<PatchAddress> &a)
{
    if (!a)
        return;
    const int r = rowOf(*a);
    if (r >= 0)
        emit dataChanged(index(r), index(r));
}

void PatchListModel::setBelievedEngine(int engine)
{
    if (engine == m_engine)
        return;
    m_engine = engine;
    if (m_modelId == Model::JU06A && !m_rows.isEmpty())
        emit dataChanged(index(0), index(m_rows.size() - 1));  // every engine tag changes style
}

void PatchListModel::setArmed(const std::optional<PatchAddress> &a)
{
    if (a == m_armed)
        return;
    const std::optional<PatchAddress> before = m_armed;
    m_armed = a;
    emitRow(before);
    emitRow(a);
}

void PatchListModel::setOtherRemembered(const std::optional<PatchAddress> &a)
{
    if (a == m_otherRemembered)
        return;
    const std::optional<PatchAddress> before = m_otherRemembered;
    m_otherRemembered = a;
    emitRow(before);
    emitRow(a);
}

PatchAddress PatchListModel::addressAt(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return PatchAddress();
    return m_rows[index.row()].address;
}

int PatchListModel::rowOf(const PatchAddress &address) const
{
    if (address.model() != m_modelId)
        return -1;
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

// While the Recents view is shown a send must not reorder the rows: with auto-send the
// highlight would chase the row it just sent. Counts update in place; the order refreshes
// when the view is entered again. Rows that left the library (cleared, capped) rebuild.
void PatchListModel::onRecentsChanged(Model model)
{
    if (model != m_modelId || m_view != View::Recents)
        return;
    QVector<int> counts(m_rowOf.size(), 0);
    for (const Library::Recent &r : m_library.recents(m_modelId))
        counts[r.address.catalogueIndex()] = r.count;
    for (const Row &row : std::as_const(m_rows)) {
        if (counts[row.address.catalogueIndex()] == 0) {
            rebuild();
            return;
        }
    }
    for (int r = 0; r < m_rows.size(); ++r) {
        const int n = counts[m_rows[r].address.catalogueIndex()];
        const QString text = QStringLiteral("×") + QString::number(n);
        if (text != m_rows[r].countText) {
            m_rows[r].countText = text;
            emit dataChanged(index(r), index(r));
        }
    }
    // An address sent for the first time while the view is shown joins at the top; the
    // existing rows keep their order. Recents are newest first, so insert oldest first.
    QVector<Library::Recent> missing;
    for (const Library::Recent &r : m_library.recents(m_modelId))
        if (m_rowOf[r.address.catalogueIndex()] < 0)
            missing.append(r);
    for (int i = missing.size() - 1; i >= 0; --i) {
        Row row;
        if (!buildRow(missing[i].address, missing[i].count, &row))
            continue;  // hidden by the search
        beginInsertRows(QModelIndex(), 0, 0);
        m_rows.prepend(std::move(row));
        for (int &idx : m_rowOf)
            if (idx >= 0)
                ++idx;
        m_rowOf[m_rows[0].address.catalogueIndex()] = 0;
        endInsertRows();
    }
}

bool PatchListModel::buildRow(const PatchAddress &a, int count, Row *out) const
{
    Row row;
    row.address = a;
    row.name = m_catalog->name(a);
    row.userName = m_catalog->userName(a);
    // A renamed JP-08 / JU-06A slot shows both names (DESIGN-2.md 6.4); a D-05 user slot has no factory name.
    if (!row.userName.isEmpty())
        row.factory = m_catalog->factoryName(a);
    if (!m_query.matches(a, row.name, &row.ranges, row.factory))
        return false;
    row.shown = row.name.isEmpty() ? a.toString() : row.name;
    row.mode = m_catalog->mode(a);
    row.favorite = m_library.isFavorite(a);
    if (count > 0)
        row.countText = QStringLiteral("×") + QString::number(count);
    *out = std::move(row);
    return true;
}

void PatchListModel::onFavoritesChanged(Model model)
{
    if (model != m_modelId)
        return;
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
void PatchListModel::onUserNamesChanged(Model model)
{
    if (model != m_modelId)
        return;
    if (searchActive()) {
        rebuild();
        return;
    }
    for (int r = 0; r < m_rows.size(); ++r) {
        Row &row = m_rows[r];
        if (!canRename(row.address))
            continue;
        const QString name = m_catalog->name(row.address);
        const QString user = m_catalog->userName(row.address);
        if (name == row.name && user == row.userName)
            continue;
        row.name = name;
        row.userName = user;
        row.factory = user.isEmpty() ? QString() : m_catalog->factoryName(row.address);
        row.shown = name.isEmpty() ? row.address.toString() : name;
        emit dataChanged(index(r), index(r));
    }
}

void PatchListModel::rebuild()
{
    beginResetModel();
    fillRows();
    endResetModel();
}

void PatchListModel::fillRows()
{
    m_rows.clear();
    m_rowOf.fill(-1);

    const auto add = [this](const PatchAddress &a, int count) {
        Row row;
        if (!buildRow(a, count, &row))
            return;
        m_rowOf[a.catalogueIndex()] = m_rows.size();
        m_rows.append(std::move(row));
    };

    switch (m_view) {
    case View::All:
        for (int i = 0; i < m_rowOf.size(); ++i)
            add(m_modelId == Model::D05 ? *PatchAddress::fromCatalogueIndex(i)
                                        : *PatchAddress::fromCatalogueIndex(m_modelId, i),
                0);
        break;
    case View::Favourites:
        for (const PatchAddress &a : m_library.favorites(m_modelId))
            add(a, 0);
        break;
    case View::Recents:
        for (const Library::Recent &r : m_library.recents(m_modelId))
            add(r.address, r.count);
        break;
    }
}

}  // namespace d05
