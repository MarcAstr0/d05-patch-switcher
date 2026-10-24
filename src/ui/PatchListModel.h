#pragma once

#include "core/PatchAddress.h"
#include "core/SearchQuery.h"

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace d05 {

class Library;
class PatchCatalog;

// The patch list: the catalogue (or favourites, or recents) filtered by a SearchQuery
// (SPEC FR-3, FR-4). Rows carry everything the delegate paints so that painting looks
// nothing up and builds no strings.
class PatchListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum class View { All, Favourites, Recents };

    struct Row {
        PatchAddress address;
        QString name;       // PatchCatalog::name: the text the search ranges index into
        QString shown;      // name, or the address for an unnamed user slot
        QString countText;  // "x12" in the Recents view, else empty
        QChar mode;         // 'W', 'D', 'S' or null
        bool favorite = false;
        QVector<SearchQuery::Range> ranges;
    };

    enum Roles {
        AddressRole = Qt::UserRole + 1,  // the canonical address text, e.g. "P3-45"
    };

    PatchListModel(const PatchCatalog &catalog, const Library &library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    // User-slot rows are editable (the inline name editor, DESIGN 7.4); preset rows are not.
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    View view() const { return m_view; }
    void setView(View view);
    const QString &queryText() const { return m_queryText; }
    void setQuery(const QString &text);
    bool searchActive() const { return !m_query.isEmpty(); }

    // The current patch (lit LED in its row).
    PatchAddress current() const { return m_current; }
    void setCurrent(const PatchAddress &address);

    const Row &row(int r) const { return m_rows[r]; }
    PatchAddress addressAt(const QModelIndex &index) const;
    // Row of an address in the visible list, or -1.
    int rowOf(const PatchAddress &address) const;
    QModelIndex indexOf(const PatchAddress &address) const;

    // The text for the empty list (DESIGN 7.5); both empty when the list has rows.
    QString emptyTitle() const;
    QString emptyNote() const;

private:
    void rebuild();
    void onFavoritesChanged();
    void onRecentsChanged();
    void onUserNamesChanged();

    const PatchCatalog &m_catalog;
    const Library &m_library;
    View m_view = View::All;
    QString m_queryText;
    SearchQuery m_query;
    PatchAddress m_current;
    QVector<Row> m_rows;
    QVector<int> m_rowOf;  // catalogue index -> row, -1 when hidden
};

}  // namespace d05
