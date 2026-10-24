#pragma once

#include "core/PatchAddress.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

namespace d05 {

class PatchCatalog;

// Per-user state (SPEC 3.3): favourites, recents, user-slot names, persisted to library.json.
//
// Ownership of user names: the PatchCatalog overlay is the single in-memory store
// (so sanitising lives in one place). Library reads names out of it to save and feeds
// it on load; edit names through Library::setUserName so the change is persisted.
class Library : public QObject {
    Q_OBJECT
public:
    static constexpr int MaxRecents = 50;
    static constexpr int FileVersion = 1;
    static constexpr int DebounceMs = 500;

    struct Recent {
        PatchAddress address;
        int count = 1;
        QDateTime last;  // UTC
    };

    enum class LoadResult { Loaded, Fresh, Recovered };

    Library(PatchCatalog &catalog, const QString &path, QObject *parent = nullptr);
    ~Library() override;  // flushes pending changes

    // Reads the file. Missing file: Fresh. Malformed (bad JSON, wrong shape, unknown
    // version): renamed to library.json.broken-<yyyyMMddTHHmmss>, state reset, Recovered.
    // Invalid individual entries inside a valid file are skipped.
    LoadResult load();
    QString brokenFilePath() const { return m_brokenPath; }
    const QString &path() const { return m_path; }

    // Favourites (set; returned in catalogue order).
    bool isFavorite(const PatchAddress &a) const;
    QVector<PatchAddress> favorites() const;
    void setFavorite(const PatchAddress &a, bool on);
    // Returns the new state.
    bool toggleFavorite(const PatchAddress &a);

    // Recents, most recent first, at most 50.
    const QVector<Recent> &recents() const { return m_recents; }
    void recordSend(const PatchAddress &a, const QDateTime &when = QDateTime::currentDateTimeUtc());
    void clearRecents();

    // Sanitises through PatchCatalog::setUserName. False for preset addresses.
    bool setUserName(const PatchAddress &a, const QString &name);

    // Writes now (QSaveFile) if there are unsaved changes; stops the debounce.
    // Returns false on failure (also emitted as saveFailed).
    bool flush();
    bool hasPendingChanges() const { return m_dirty; }

signals:
    void favoritesChanged();
    void recentsChanged();
    void userNamesChanged();
    void saveFailed(const QString &message);

private:
    void markDirty();
    bool writeFile();

    PatchCatalog &m_catalog;
    QString m_path;
    QString m_brokenPath;
    QVector<PatchAddress> m_favorites;  // kept sorted
    QVector<Recent> m_recents;
    QTimer m_timer;
    bool m_dirty = false;
};

}  // namespace d05
