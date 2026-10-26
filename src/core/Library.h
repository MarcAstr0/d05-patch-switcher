#pragma once

#include "core/PatchAddress.h"

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>
#include <array>

namespace d05 {

class PatchCatalog;

// Per-user state (SPEC 3.3, SPEC-2 3.5): per device (D-05, JP-08, JU-06A) favourites, recents
// and user-slot names, persisted to library.json as file version 2. A version 1 file is read as
// the D-05 block (a copy is kept once as library.json.v1) and written as version 2 at the next
// save. Unknown top-level keys, unknown device ids and unknown keys inside a device block are
// preserved untouched.
//
// Ownership of user names: the PatchCatalog overlay is the single in-memory store
// (so sanitising lives in one place). Library reads names out of it to save and feeds
// it on load. Library is the only mutator (PatchCatalog's setters are private, with
// Library as a friend). The catalogs must outlive the Library. The constructor takes one
// catalog (normally the D-05's); attachCatalog() adds the others, before load(). A model with
// no attached catalog keeps its favourites and recents, and its user names are carried through
// unchanged (setUserName refuses it).
class Library : public QObject {
    Q_OBJECT
public:
    static constexpr int MaxRecents = 50;
    static constexpr int FileVersion = 2;
    static constexpr int LegacyFileVersion = 1;
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

    // Registers the catalog of another model (its model() decides which). Call before load().
    void attachCatalog(PatchCatalog &catalog);

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

    // Path of the one-time copy of a version 1 file.
    QString legacyCopyPath() const { return m_path + QStringLiteral(".v1"); }

    // Favourites (set; returned in catalogue order). Per model of the address.
    bool isFavorite(const PatchAddress &a) const;
    QVector<PatchAddress> favorites(Model model = Model::D05) const;
    void setFavorite(const PatchAddress &a, bool on);
    // Returns the new state.
    bool toggleFavorite(const PatchAddress &a);

    // Recents of one model, most recent first, at most 50.
    const QVector<Recent> &recents(Model model = Model::D05) const { return m_dev[idx(model)].recents; }
    void recordSend(const PatchAddress &a, const QDateTime &when = QDateTime::currentDateTimeUtc());
    void clearRecents(Model model = Model::D05);

    // Sanitises through PatchCatalog::setUserName. False for D-05 preset addresses and for a
    // model whose catalog is not attached. Any JP-08 / JU-06A slot, and D-05 U slots, are allowed.
    bool setUserName(const PatchAddress &a, const QString &name);

    // Writes now (QSaveFile) if there are unsaved changes; stops the debounce.
    // Returns false on failure. saveFailed is emitted only on the ok-to-failed transition;
    // while failing, the data stays dirty and a single-shot retry runs every RetryMs.
    bool flush();
    bool hasPendingChanges() const { return m_dirty; }
    bool writesBlocked() const { return m_blocked; }

signals:
    void favoritesChanged(d05::Model model);
    void recentsChanged(d05::Model model);
    void userNamesChanged(d05::Model model);
    void saved();  // after each successful write
    void saveFailed(const QString &message);

private:
    struct Device {
        QVector<PatchAddress> favorites;  // kept sorted
        QVector<Recent> recents;
        QJsonObject extra;     // unknown keys of the block, written back as they were
        QJsonObject rawNames;  // userNames of a model with no attached catalog, carried through
    };
    static int idx(Model m) { return int(m); }
    PatchCatalog *catalogFor(Model m) const { return m_catalogs[idx(m)]; }
    bool copyLegacyFile();
    void markDirty();
    bool writeFile(QString *error);
    void fail(const QString &message);

    std::array<PatchCatalog *, 3> m_catalogs{};
    QString m_path;
    QString m_brokenPath;
    std::array<Device, 3> m_dev;
    QJsonObject m_extraTop;      // unknown top-level keys
    QJsonObject m_extraDevices;  // unknown device ids
    bool m_needLegacyCopy = false;
    QTimer m_timer;
    bool m_dirty = false;
    bool m_blocked = false;
    bool m_failing = false;
};

}  // namespace d05
