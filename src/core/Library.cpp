#include "core/Library.h"

#include "core/PatchCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalBlocker>
#include <algorithm>
#include <limits>
#include <optional>

namespace d05 {

namespace {

std::optional<PatchAddress> parseFor(Model model, const QString &text)
{
    return model == Model::D05 ? PatchAddress::parse(text) : PatchAddress::parse(text, model);
}

std::optional<PatchAddress> fromIndex(Model model, int i)
{
    return model == Model::D05 ? PatchAddress::fromCatalogueIndex(i) : PatchAddress::fromCatalogueIndex(model, i);
}

// The keys of a device block that the library understands; everything else is carried through.
bool isKnownBlockKey(const QString &k)
{
    return k == QLatin1String("favorites") || k == QLatin1String("recents") || k == QLatin1String("userNames");
}

}  // namespace

Library::Library(PatchCatalog &catalog, const QString &path, QObject *parent)
    : QObject(parent), m_path(path)
{
    attachCatalog(catalog);
    m_timer.setSingleShot(true);
    m_timer.setInterval(DebounceMs);
    connect(&m_timer, &QTimer::timeout, this, [this] { flush(); });
}

Library::~Library()
{
    QSignalBlocker blocker(this);
    flush();
}

void Library::attachCatalog(PatchCatalog &catalog)
{
    const Model model = catalog.model();
    const bool wasUnattached = m_catalogs[idx(model)] == nullptr;
    m_catalogs[idx(model)] = &catalog;
    // Names loaded while this model had no catalog were carried raw: move them into the catalog,
    // or the next save (which writes the catalog's names) would drop them.
    Device &d = m_dev[idx(model)];
    if (wasUnattached && !d.rawNames.isEmpty()) {
        for (auto it = d.rawNames.begin(); it != d.rawNames.end(); ++it) {
            const auto a = parseFor(model, it.key());
            if (a && it.value().isString() && (model != Model::D05 || a->isUser()))
                catalog.setUserName(*a, it.value().toString());
        }
        d.rawNames = QJsonObject();
        emit userNamesChanged(model);
    }
}

bool Library::copyLegacyFile()
{
    const QString copy = legacyCopyPath();
    if (QFile::exists(copy))
        return true;  // kept once: never overwritten
    // Written through a temporary file and renamed, so an existing copy is always complete.
    QFile src(m_path);
    if (!src.open(QIODevice::ReadOnly))
        return false;
    const QByteArray bytes = src.readAll();
    if (src.error() != QFileDevice::NoError)
        return false;
    QSaveFile out(copy);
    if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size())
        return false;
    return out.commit();
}

Library::LoadResult Library::load()
{
    for (Device &d : m_dev)
        d = Device();
    m_extraTop = QJsonObject();
    m_extraDevices = QJsonObject();
    m_needLegacyCopy = false;
    for (PatchCatalog *c : m_catalogs)
        if (c)
            c->clearUserNames();
    m_brokenPath.clear();
    m_dirty = false;
    m_blocked = false;
    m_failing = false;
    m_timer.stop();

    QFile f(m_path);
    if (!f.exists())
        return LoadResult::Fresh;

    if (!f.open(QIODevice::ReadOnly)) {
        m_blocked = true;  // never overwrite what we could not read
        return LoadResult::Unreadable;
    }

    bool ok = false;
    bool legacy = false;
    std::array<Device, 3> dev;
    std::array<QVector<QPair<PatchAddress, QString>>, 3> names;
    QJsonObject extraTop, extraDevices;

    // One device block in the (identical) v1 shape. False when the block has the wrong shape.
    auto readBlock = [&](Model model, const QJsonObject &o) {
        const QJsonValue fv = o.value(QStringLiteral("favorites"));
        const QJsonValue rv = o.value(QStringLiteral("recents"));
        const QJsonValue nv = o.value(QStringLiteral("userNames"));
        if (!((fv.isArray() || fv.isUndefined()) && (rv.isArray() || rv.isUndefined())
              && (nv.isObject() || nv.isUndefined())))
            return false;
        Device &d = dev[idx(model)];
        QVector<PatchAddress> favs;
        for (const QJsonValue &v : fv.toArray()) {
            const auto a = parseFor(model, v.toString());
            if (a && !favs.contains(*a))
                favs.append(*a);
        }
        std::sort(favs.begin(), favs.end());
        d.favorites = favs;
        QVector<Recent> recents;
        for (const QJsonValue &v : rv.toArray()) {
            const QJsonObject e = v.toObject();
            const auto a = parseFor(model, e.value(QStringLiteral("address")).toString());
            if (!a)
                continue;
            Recent r;
            r.address = *a;
            const double c = e.value(QStringLiteral("count")).toDouble(1.0);
            r.count = c >= double(std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max()
                    : c < 1.0 ? 1 : int(c);
            r.last = QDateTime::fromString(e.value(QStringLiteral("last")).toString(), Qt::ISODate);
            if (r.last.isValid()) {
                if (r.last.timeSpec() == Qt::LocalTime)
                    r.last.setTimeSpec(Qt::UTC);  // no zone in the text: read as UTC
                r.last = r.last.toUTC();
            } else {
                r.last = QDateTime::fromSecsSinceEpoch(0, Qt::UTC);
            }
            recents.append(r);
        }
        std::stable_sort(recents.begin(), recents.end(),
                         [](const Recent &x, const Recent &y) { return x.last > y.last; });
        QVector<Recent> kept;
        for (const Recent &r : recents) {
            if (kept.size() >= MaxRecents)
                break;
            bool dup = false;
            for (const Recent &k : kept)
                dup = dup || k.address == r.address;
            if (!dup)
                kept.append(r);
        }
        d.recents = kept;
        const QJsonObject no = nv.toObject();
        if (catalogFor(model)) {
            for (auto it = no.begin(); it != no.end(); ++it) {
                const auto a = parseFor(model, it.key());
                if (a && it.value().isString() && (model != Model::D05 || a->isUser()))
                    names[idx(model)].append({*a, it.value().toString()});
            }
        } else {
            d.rawNames = no;
        }
        for (auto it = o.begin(); it != o.end(); ++it)
            if (!isKnownBlockKey(it.key()))
                d.extra.insert(it.key(), it.value());
        return true;
    };

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject o = doc.object();
        const QJsonValue ver = o.value(QStringLiteral("version"));
        if (ver.isDouble() && ver.toInt() == LegacyFileVersion) {
            // Version 1: the whole file is the D-05 block; other top-level keys are unknown extras.
            legacy = true;
            ok = readBlock(Model::D05, o);
            dev[idx(Model::D05)].extra = QJsonObject();
            for (auto it = o.begin(); it != o.end(); ++it)
                if (!isKnownBlockKey(it.key()) && it.key() != QLatin1String("version"))
                    extraTop.insert(it.key(), it.value());
        } else if (ver.isDouble() && ver.toInt() == FileVersion) {
            const QJsonValue dv = o.value(QStringLiteral("devices"));
            if (dv.isObject() || dv.isUndefined()) {
                ok = true;
                const QJsonObject devices = dv.toObject();
                for (auto it = devices.begin(); it != devices.end() && ok; ++it) {
                    const auto model = modelFromId(it.key());
                    if (!model)
                        extraDevices.insert(it.key(), it.value());
                    else if (it.value().isObject())
                        ok = readBlock(*model, it.value().toObject());
                    else
                        ok = false;
                }
                for (auto it = o.begin(); it != o.end(); ++it)
                    if (it.key() != QLatin1String("version") && it.key() != QLatin1String("devices"))
                        extraTop.insert(it.key(), it.value());
            }
        }
    }

    if (!ok) {
        const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmss"));
        QString target = m_path + QStringLiteral(".broken-") + stamp;
        for (int i = 2; QFile::exists(target); ++i)
            target = m_path + QStringLiteral(".broken-") + stamp + QStringLiteral("-") + QString::number(i);
        if (!QFile::rename(m_path, target)) {
            m_blocked = true;  // keep the file, do not overwrite it
            return LoadResult::SidelineFailed;
        }
        m_brokenPath = target;
        return LoadResult::Recovered;
    }

    m_dev = dev;
    m_extraTop = extraTop;
    m_extraDevices = extraDevices;
    for (Model m : allModels())
        if (PatchCatalog *c = catalogFor(m))
            for (const auto &n : names[idx(m)])
                c->setUserName(n.first, n.second);
    if (legacy && !copyLegacyFile())
        m_needLegacyCopy = true;  // retried before the first write, which then fails if it still cannot
    return LoadResult::Loaded;
}

bool Library::isFavorite(const PatchAddress &a) const
{
    const auto &f = m_dev[idx(a.model())].favorites;
    return std::binary_search(f.begin(), f.end(), a);
}

QVector<PatchAddress> Library::favorites(Model model) const { return m_dev[idx(model)].favorites; }

void Library::setFavorite(const PatchAddress &a, bool on)
{
    auto &favs = m_dev[idx(a.model())].favorites;
    auto it = std::lower_bound(favs.begin(), favs.end(), a);
    const bool has = it != favs.end() && *it == a;
    if (on == has)
        return;
    if (on)
        favs.insert(it, a);
    else
        favs.erase(it);
    markDirty();
    emit favoritesChanged(a.model());
}

bool Library::toggleFavorite(const PatchAddress &a)
{
    const bool now = !isFavorite(a);
    setFavorite(a, now);
    return now;
}

void Library::recordSend(const PatchAddress &a, const QDateTime &when)
{
    auto &recents = m_dev[idx(a.model())].recents;
    int count = 1;
    for (int i = 0; i < recents.size(); ++i) {
        if (recents[i].address == a) {
            const int old = recents[i].count;
            count = old < std::numeric_limits<int>::max() ? old + 1 : old;  // saturate
            recents.remove(i);
            break;
        }
    }
    Recent r;
    r.address = a;
    r.count = count;
    r.last = when.toUTC();
    recents.prepend(r);
    while (recents.size() > MaxRecents)
        recents.removeLast();
    markDirty();
    emit recentsChanged(a.model());
}

void Library::clearRecents(Model model)
{
    auto &recents = m_dev[idx(model)].recents;
    if (recents.isEmpty())
        return;
    recents.clear();
    markDirty();
    emit recentsChanged(model);
}

bool Library::setUserName(const PatchAddress &a, const QString &name)
{
    PatchCatalog *cat = catalogFor(a.model());
    if (!cat || (a.model() == Model::D05 && !a.isUser()))
        return false;
    const bool hadUser = cat->hasUserName(a);
    const QString before = cat->userName(a);
    if (!cat->setUserName(a, name))
        return false;
    if (cat->hasUserName(a) != hadUser || cat->userName(a) != before) {
        markDirty();
        emit userNamesChanged(a.model());
    }
    return true;
}

void Library::markDirty()
{
    m_dirty = true;
    if (m_blocked)
        return;  // nothing will be written until a later load() succeeds
    m_timer.setInterval(m_failing ? RetryMs : DebounceMs);
    m_timer.start();  // restarts the single-shot debounce
}

void Library::fail(const QString &message)
{
    const bool first = !m_failing;
    m_failing = true;
    if (!m_blocked) {
        m_timer.setInterval(RetryMs);
        m_timer.start();
    }
    if (first)
        emit saveFailed(message);
}

bool Library::flush()
{
    m_timer.stop();
    if (!m_dirty)
        return true;
    if (m_blocked) {
        fail(QStringLiteral("library.json could not be read at start; not overwriting it"));
        return false;
    }
    QString error;
    if (writeFile(&error)) {
        m_dirty = false;
        m_failing = false;
        emit saved();
        return true;
    }
    fail(error);
    return false;
}

bool Library::writeFile(QString *error)
{
    if (m_needLegacyCopy) {
        if (!copyLegacyFile()) {
            *error = QStringLiteral("Cannot keep a copy of the version 1 library as ") + legacyCopyPath();
            return false;
        }
        m_needLegacyCopy = false;
    }

    QJsonObject devices = m_extraDevices;
    for (Model model : allModels()) {
        const Device &d = m_dev[idx(model)];
        QJsonObject block = d.extra;

        QJsonArray favs;
        for (const PatchAddress &a : d.favorites)
            favs.append(a.toString());
        block.insert(QStringLiteral("favorites"), favs);

        QJsonArray recents;
        for (const Recent &r : d.recents) {
            QJsonObject e;
            e.insert(QStringLiteral("address"), r.address.toString());
            e.insert(QStringLiteral("count"), r.count);
            e.insert(QStringLiteral("last"), r.last.toUTC().toString(Qt::ISODate));
            recents.append(e);
        }
        block.insert(QStringLiteral("recents"), recents);

        QJsonObject names = d.rawNames;
        if (const PatchCatalog *cat = catalogFor(model)) {
            names = QJsonObject();
            for (int i = 0; i < modelInfo(model).catalogueSize; ++i) {
                const PatchAddress a = *fromIndex(model, i);
                if (cat->hasUserName(a))
                    names.insert(a.toString(), cat->name(a));
            }
        }
        block.insert(QStringLiteral("userNames"), names);
        devices.insert(modelInfo(model).id, block);
    }

    QJsonObject o = m_extraTop;
    o.insert(QStringLiteral("version"), FileVersion);
    o.insert(QStringLiteral("devices"), devices);

    const QString dir = QFileInfo(m_path).absolutePath();
    if (!QDir().mkpath(dir)) {
        *error = QStringLiteral("Cannot create ") + dir;
        return false;
    }
    QSaveFile out(m_path);
    if (!out.open(QIODevice::WriteOnly)) {
        *error = out.errorString();
        return false;
    }
    out.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    if (!out.commit()) {
        *error = out.errorString();
        return false;
    }
    return true;
}

}  // namespace d05
