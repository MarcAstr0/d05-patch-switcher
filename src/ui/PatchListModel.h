#pragma once

#include "core/PatchAddress.h"
#include "core/SearchQuery.h"

#include <QAbstractListModel>
#include <QString>
#include <QVector>
#include <optional>

namespace d05 {

class Library;
class PatchCatalog;

// The patch list: the catalogue (or favourites, or recents) of one device, filtered by a SearchQuery
// (SPEC FR-3, FR-4, SPEC-2 FR-3). Rows carry everything the delegate paints so that painting looks
// nothing up and builds no strings. setDevice() points the model at another device's catalogue in
// one reset; the library's per-device favourites and recents follow.
class PatchListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum class View { All, Favourites, Recents };

    struct Row {
        PatchAddress address;
        QString name;       // PatchCatalog::name: the text the Primary search ranges index into
        QString userName;   // the slot's user name, empty when none
        QString factory;    // the factory name a user name hides (JP-08 / JU-06A), the text the
                            // Alternate ranges index into; empty otherwise
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

    // Switches to another device's catalogue with its own query and view: one reset, nothing is
    // rebuilt twice. The current patch, the armed selection and the remembered marker are cleared.
    void setDevice(const PatchCatalog &catalog, const QString &queryText, View view);
    Model deviceModel() const { return m_modelId; }
    // True when the slot can carry a user name (every JP-08 / JU-06A slot, the D-05's U slots).
    static bool canRename(const PatchAddress &a) { return a.model() != Model::D05 || a.isUser(); }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    // User-slot rows are editable (the inline name editor, DESIGN 7.4); preset rows are not.
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    View view() const { return m_view; }
    void setView(View view);
    const QString &queryText() const { return m_queryText; }
    void setQuery(const QString &text);
    bool searchActive() const { return !m_query.isEmpty(); }

    // The current patch (lit LED in its row); none for a JU-06A with no patch on its engine.
    PatchAddress current() const { return m_current; }
    bool hasCurrent() const { return m_hasCurrent; }
    void setCurrent(const PatchAddress &address);
    void clearCurrent();

    // JU-06A row markers (DESIGN-2.md 6.2, 6.3): the engine the app believes (the tag of its rows is
    // framed, the other engine's is dim), the armed selection and the other engine's remembered
    // patch. Rows repaint only when one of them changes.
    int believedEngine() const { return m_engine; }
    void setBelievedEngine(int engine);
    std::optional<PatchAddress> armed() const { return m_armed; }
    void setArmed(const std::optional<PatchAddress> &a);
    std::optional<PatchAddress> otherRemembered() const { return m_otherRemembered; }
    void setOtherRemembered(const std::optional<PatchAddress> &a);

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
    void fillRows();  // the rows of the current device, view and query; no reset signals
    // Fills `out` for an address; false when the search hides it.
    bool buildRow(const PatchAddress &a, int count, Row *out) const;
    void onFavoritesChanged(Model model);
    void onRecentsChanged(Model model);
    void onUserNamesChanged(Model model);
    void emitRow(const std::optional<PatchAddress> &a);

    const PatchCatalog *m_catalog;
    const Library &m_library;
    Model m_modelId = Model::D05;
    View m_view = View::All;
    QString m_queryText;
    SearchQuery m_query;
    PatchAddress m_current;
    bool m_hasCurrent = true;
    int m_engine = 60;
    std::optional<PatchAddress> m_armed;
    std::optional<PatchAddress> m_otherRemembered;
    QVector<Row> m_rows;
    QVector<int> m_rowOf;  // catalogue index of the device -> row, -1 when hidden
};

}  // namespace d05
