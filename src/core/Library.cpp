#include "core/Library.h"

#include "core/PatchCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>

namespace d05 {

Library::Library(PatchCatalog &catalog, const QString &path, QObject *parent)
    : QObject(parent), m_catalog(catalog), m_path(path)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(DebounceMs);
    connect(&m_timer, &QTimer::timeout, this, [this] { flush(); });
}

Library::~Library()
{
    flush();
}

Library::LoadResult Library::load()
{
    m_favorites.clear();
    m_recents.clear();
    m_catalog.clearUserNames();
    m_brokenPath.clear();
    m_dirty = false;
    m_timer.stop();

    QFile f(m_path);
    if (!f.exists())
        return LoadResult::Fresh;

    bool ok = false;
    QVector<PatchAddress> favs;
    QVector<Recent> recents;
    QVector<QPair<PatchAddress, QString>> names;

    if (f.open(QIODevice::ReadOnly)) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        f.close();
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject o = doc.object();
            const QJsonValue ver = o.value(QStringLiteral("version"));
            const QJsonValue fv = o.value(QStringLiteral("favorites"));
            const QJsonValue rv = o.value(QStringLiteral("recents"));
            const QJsonValue nv = o.value(QStringLiteral("userNames"));
            if (ver.isDouble() && ver.toInt() == FileVersion
                && (fv.isArray() || fv.isUndefined()) && (rv.isArray() || rv.isUndefined())
                && (nv.isObject() || nv.isUndefined())) {
                ok = true;
                for (const QJsonValue &v : fv.toArray()) {
                    const auto a = PatchAddress::parse(v.toString());
                    if (a && !favs.contains(*a))
                        favs.append(*a);
                }
                for (const QJsonValue &v : rv.toArray()) {
                    const QJsonObject e = v.toObject();
                    const auto a = PatchAddress::parse(e.value(QStringLiteral("address")).toString());
                    if (!a || recents.size() >= MaxRecents)
                        continue;
                    bool dup = false;
                    for (const Recent &r : recents)
                        dup = dup || r.address == *a;
                    if (dup)
                        continue;
                    Recent r;
                    r.address = *a;
                    r.count = std::max(1, e.value(QStringLiteral("count")).toInt(1));
                    r.last = QDateTime::fromString(e.value(QStringLiteral("last")).toString(), Qt::ISODate);
                    if (!r.last.isValid())
                        r.last = QDateTime::fromSecsSinceEpoch(0, Qt::UTC);
                    recents.append(r);
                }
                const QJsonObject no = nv.toObject();
                for (auto it = no.begin(); it != no.end(); ++it) {
                    const auto a = PatchAddress::parse(it.key());
                    if (a && a->isUser() && it.value().isString())
                        names.append({*a, it.value().toString()});
                }
            }
        }
    }

    if (!ok) {
        const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmss"));
        QString target = m_path + QStringLiteral(".broken-") + stamp;
        for (int i = 2; QFile::exists(target); ++i)
            target = m_path + QStringLiteral(".broken-") + stamp + QStringLiteral("-") + QString::number(i);
        if (QFile::rename(m_path, target))
            m_brokenPath = target;
        else
            QFile::remove(m_path);  // cannot sideline; do not stay stuck on it
        return LoadResult::Recovered;
    }

    std::sort(favs.begin(), favs.end());
    m_favorites = favs;
    m_recents = recents;
    for (const auto &n : names)
        m_catalog.setUserName(n.first, n.second);
    return LoadResult::Loaded;
}

bool Library::isFavorite(const PatchAddress &a) const
{
    return std::binary_search(m_favorites.begin(), m_favorites.end(), a);
}

QVector<PatchAddress> Library::favorites() const { return m_favorites; }

void Library::setFavorite(const PatchAddress &a, bool on)
{
    auto it = std::lower_bound(m_favorites.begin(), m_favorites.end(), a);
    const bool has = it != m_favorites.end() && *it == a;
    if (on == has)
        return;
    if (on)
        m_favorites.insert(it, a);
    else
        m_favorites.erase(it);
    markDirty();
    emit favoritesChanged();
}

bool Library::toggleFavorite(const PatchAddress &a)
{
    const bool now = !isFavorite(a);
    setFavorite(a, now);
    return now;
}

void Library::recordSend(const PatchAddress &a, const QDateTime &when)
{
    int count = 1;
    for (int i = 0; i < m_recents.size(); ++i) {
        if (m_recents[i].address == a) {
            count = m_recents[i].count + 1;
            m_recents.remove(i);
            break;
        }
    }
    Recent r;
    r.address = a;
    r.count = count;
    r.last = when.toUTC();
    m_recents.prepend(r);
    while (m_recents.size() > MaxRecents)
        m_recents.removeLast();
    markDirty();
    emit recentsChanged();
}

void Library::clearRecents()
{
    if (m_recents.isEmpty())
        return;
    m_recents.clear();
    markDirty();
    emit recentsChanged();
}

bool Library::setUserName(const PatchAddress &a, const QString &name)
{
    if (!a.isUser())
        return false;
    const QString before = m_catalog.name(a);
    m_catalog.setUserName(a, name);
    if (m_catalog.name(a) != before) {
        markDirty();
        emit userNamesChanged();
    }
    return true;
}

void Library::markDirty()
{
    m_dirty = true;
    m_timer.start();  // restarts the single-shot debounce
}

bool Library::flush()
{
    m_timer.stop();
    if (!m_dirty)
        return true;
    if (writeFile()) {
        m_dirty = false;
        return true;
    }
    return false;
}

bool Library::writeFile()
{
    QJsonObject o;
    o.insert(QStringLiteral("version"), FileVersion);

    QJsonArray favs;
    for (const PatchAddress &a : m_favorites)
        favs.append(a.toString());
    o.insert(QStringLiteral("favorites"), favs);

    QJsonArray recents;
    for (const Recent &r : m_recents) {
        QJsonObject e;
        e.insert(QStringLiteral("address"), r.address.toString());
        e.insert(QStringLiteral("count"), r.count);
        e.insert(QStringLiteral("last"), r.last.toUTC().toString(Qt::ISODate));
        recents.append(e);
    }
    o.insert(QStringLiteral("recents"), recents);

    QJsonObject names;
    for (int i = 0; i < 8 * 64; ++i) {
        const PatchAddress a = *PatchAddress::fromCatalogueIndex(i);
        if (m_catalog.hasUserName(a))
            names.insert(a.toString(), m_catalog.name(a));
    }
    o.insert(QStringLiteral("userNames"), names);

    const QString dir = QFileInfo(m_path).absolutePath();
    if (!QDir().mkpath(dir)) {
        emit saveFailed(QStringLiteral("Cannot create ") + dir);
        return false;
    }
    QSaveFile out(m_path);
    if (!out.open(QIODevice::WriteOnly)) {
        emit saveFailed(out.errorString());
        return false;
    }
    out.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    if (!out.commit()) {
        emit saveFailed(out.errorString());
        return false;
    }
    return true;
}

}  // namespace d05
