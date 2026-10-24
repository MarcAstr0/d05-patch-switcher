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
// it on load. Library is the only mutator (PatchCatalog's setters are private, with
// Library as a friend). The catalog must outlive the Library.
class Library : public QObject {
    Q_OBJECT
public:
    static constexpr int MaxRecents = 50;
    static constexpr int FileVersion = 1;
    static constexpr int DebounceMs = 500;
    static constexpr int RetryMs = 2000;  // back-off after a failed save

    struct Recent {
        PatchAddress address;
        int count = 1;
        QDateTime last;  // UTC
    };

    enum class LoadResult {
        Loaded,         // file read and valid
        Fresh,          // no file yet
        Recovered,      // file was read but malformed; sidelined, fresh state
        Unreadable,     // file exists but could not be opened; left untouched
        SidelineFailed  // file malformed but could not be renamed; left untouched
    };

    Library(PatchCatalog &catalog, const QString &path, QObject *parent = nullptr);
    ~Library() override;  // flushes pending changes

    // Reads the file and replaces all in-memory state; calling load() again discards
    // any pending (unflushed) edits, so flush() first if you want to keep them.
    // Missing file: Fresh. Malformed (bad JSON, wrong shape, unknown version): renamed to
    // library.json.broken-<yyyyMMddTHHmmss>, state reset, Recovered. A file that cannot
    // be opened is never sidelined or removed: Unreadable. If the rename of a malformed
    // file fails, SidelineFailed. In both of those cases the state is empty and writes are
    // blocked (flush() fails, saveFailed is emitted once) until a later load() succeeds,
    // so the existing file is never overwritten.
    // Invalid individual entries inside a valid file are skipped. Recents are sorted by
    // `last` descending (stable) and cut to 50; timestamps without a zone are read as UTC.
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
    // Returns false on failure. saveFailed is emitted only on the ok-to-failed transition;
    // while failing, the data stays dirty and a single-shot retry runs every RetryMs.
    bool flush();
    bool hasPendingChanges() const { return m_dirty; }
    bool writesBlocked() const { return m_blocked; }

signals:
    void favoritesChanged();
    void recentsChanged();
    void userNamesChanged();
    void saved();  // after each successful write
    void saveFailed(const QString &message);

private:
    void markDirty();
    bool writeFile(QString *error);
    void fail(const QString &message);

    PatchCatalog &m_catalog;
    QString m_path;
    QString m_brokenPath;
    QVector<PatchAddress> m_favorites;  // kept sorted
    QVector<Recent> m_recents;
    QTimer m_timer;
    bool m_dirty = false;
    bool m_blocked = false;
    bool m_failing = false;
};

}  // namespace d05
