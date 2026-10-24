// Runs data/test-vectors.json (shared with the web implementation) against the core classes.
#include "core/Library.h"
#include "core/PatchAddress.h"
#include "core/PatchCatalog.h"
#include "core/SearchQuery.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace d05;

namespace {

QStringList toStrings(const QJsonArray &a)
{
    QStringList l;
    for (const QJsonValue &v : a)
        l << v.toString();
    return l;
}

QStringList runSearch(const PatchCatalog &cat, const QString &query)
{
    const SearchQuery q(query);
    QStringList out;
    for (int i = 0; i < PatchAddress::CatalogueSize; ++i) {
        const PatchAddress a = *PatchAddress::fromCatalogueIndex(i);
        if (q.matches(a, cat.name(a)))
            out << a.toString();
    }
    return out;
}

// Returns an empty string when fine, else a description.
QString checkSearch(const PatchCatalog &cat, const QJsonObject &c)
{
    const QString query = c.value(QStringLiteral("query")).toString();
    const QStringList r = runSearch(cat, query);
    const QString tag = QStringLiteral("query '%1': ").arg(query);
    if (c.contains(QStringLiteral("expect"))) {
        const QStringList want = toStrings(c.value(QStringLiteral("expect")).toArray());
        if (r != want)
            return tag + QStringLiteral("expected [%1] got [%2]").arg(want.join(QLatin1Char(' ')), r.join(QLatin1Char(' ')));
    }
    if (c.contains(QStringLiteral("expectCount")) && r.size() != c.value(QStringLiteral("expectCount")).toInt())
        return tag + QStringLiteral("count %1, expected %2").arg(r.size()).arg(c.value(QStringLiteral("expectCount")).toInt());
    if (c.contains(QStringLiteral("expectFirst")) && (r.isEmpty() || r.first() != c.value(QStringLiteral("expectFirst")).toString()))
        return tag + QStringLiteral("first mismatch");
    if (c.contains(QStringLiteral("expectLast")) && (r.isEmpty() || r.last() != c.value(QStringLiteral("expectLast")).toString()))
        return tag + QStringLiteral("last mismatch");
    for (const QString &s : toStrings(c.value(QStringLiteral("expectContains")).toArray()))
        if (!r.contains(s))
            return tag + QStringLiteral("missing ") + s;
    for (const QString &s : toStrings(c.value(QStringLiteral("expectNot")).toArray()))
        if (r.contains(s))
            return tag + QStringLiteral("unexpected ") + s;
    if (c.value(QStringLiteral("expectOnlyPresets")).toBool())
        for (const QString &s : r)
            if (s.startsWith(QLatin1Char('U')))
                return tag + QStringLiteral("matched unnamed user slot ") + s;
    return {};
}

}  // namespace

class TestSharedVectors : public QObject {
    Q_OBJECT
    QJsonObject m_doc;

private slots:
    void initTestCase()
    {
        QFile f(QStringLiteral(D05_VECTORS_FILE));
        QVERIFY2(f.open(QIODevice::ReadOnly), "cannot open test-vectors.json");
        QJsonParseError err;
        const auto d = QJsonDocument::fromJson(f.readAll(), &err);
        QVERIFY2(d.isObject(), qPrintable(err.errorString()));
        m_doc = d.object();
    }

    void addresses()
    {
        const QJsonArray arr = m_doc.value(QStringLiteral("addresses")).toArray();
        QVERIFY(arr.size() >= 8);
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            const QString text = o.value(QStringLiteral("text")).toString();
            const auto a = PatchAddress::parse(text);
            QVERIFY2(a, qPrintable(text));
            QCOMPARE(a->toString(), text);
            QCOMPARE(a->group(), o.value(QStringLiteral("group")).toInt());
            QCOMPARE(a->bank(), o.value(QStringLiteral("bank")).toInt());
            QCOMPARE(a->number(), o.value(QStringLiteral("number")).toInt());
            QCOMPARE(a->catalogueIndex(), o.value(QStringLiteral("index")).toInt());
            const auto t = a->midiTriplet();
            QCOMPARE(t.msb, o.value(QStringLiteral("msb")).toInt());
            QCOMPARE(t.lsb, o.value(QStringLiteral("lsb")).toInt());
            QCOMPARE(t.program, o.value(QStringLiteral("program")).toInt());
            const auto b = PatchAddress::fromCatalogueIndex(o.value(QStringLiteral("index")).toInt());
            QVERIFY(b && *b == *a);
        }
    }

    void parse()
    {
        for (const QJsonValue &v : m_doc.value(QStringLiteral("parse")).toArray()) {
            const QJsonObject o = v.toObject();
            const QString in = o.value(QStringLiteral("input")).toString();
            const auto a = PatchAddress::parse(in);
            QVERIFY2(a, qPrintable(in));
            QCOMPARE(a->toString(), o.value(QStringLiteral("canonical")).toString());
        }
    }

    void invalid()
    {
        const QJsonArray arr = m_doc.value(QStringLiteral("invalid")).toArray();
        QVERIFY(arr.size() >= 10);
        for (const QJsonValue &v : arr)
            QVERIFY2(!PatchAddress::parse(v.toString()), qPrintable(v.toString()));
    }

    void search()
    {
        PatchCatalog cat;
        QVERIFY(cat.isValid());
        const QJsonArray arr = m_doc.value(QStringLiteral("search")).toArray();
        QVERIFY(arr.size() >= 25);
        for (const QJsonValue &v : arr) {
            const QString msg = checkSearch(cat, v.toObject());
            QVERIFY2(msg.isEmpty(), qPrintable(msg));
        }
    }

    void library()
    {
        const QJsonObject lib = m_doc.value(QStringLiteral("library")).toObject();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto loadFile = [&](const QJsonObject &file, const QString &name, PatchCatalog &cat) {
            const QString path = dir.filePath(name);
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly))
                return std::unique_ptr<Library>();
            f.write(QJsonDocument(file).toJson());
            f.close();
            auto l = std::make_unique<Library>(cat, path);
            if (l->load() != Library::LoadResult::Loaded)
                return std::unique_ptr<Library>();
            return l;
        };
        auto favs = [](const Library &l) {
            QStringList s;
            for (const auto &a : l.favorites())
                s << a.toString();
            return s;
        };
        auto userNames = [](const PatchCatalog &cat) {
            QJsonObject o;
            for (int i = 0; i < 512; ++i) {
                const PatchAddress a = *PatchAddress::fromCatalogueIndex(i);
                if (!cat.name(a).isEmpty())
                    o.insert(a.toString(), cat.name(a));
            }
            return o;
        };

        {
            PatchCatalog cat;
            auto l = loadFile(lib.value(QStringLiteral("file")).toObject(), QStringLiteral("a.json"), cat);
            QVERIFY(l);
            const QJsonObject ex = lib.value(QStringLiteral("expect")).toObject();
            QCOMPARE(favs(*l), toStrings(ex.value(QStringLiteral("favorites")).toArray()));
            const QJsonArray rec = ex.value(QStringLiteral("recents")).toArray();
            QCOMPARE(l->recents().size(), int(rec.size()));
            for (int i = 0; i < rec.size(); ++i) {
                const QJsonObject r = rec.at(i).toObject();
                QCOMPARE(l->recents().at(i).address.toString(), r.value(QStringLiteral("address")).toString());
                QCOMPARE(l->recents().at(i).count, r.value(QStringLiteral("count")).toInt());
                QCOMPARE(l->recents().at(i).last.toUTC().toString(Qt::ISODate), r.value(QStringLiteral("last")).toString());
            }
            QCOMPARE(userNames(cat), ex.value(QStringLiteral("userNames")).toObject());
            const QJsonObject names = ex.value(QStringLiteral("names")).toObject();
            for (auto it = names.begin(); it != names.end(); ++it)
                QCOMPARE(cat.name(*PatchAddress::parse(it.key())), it.value().toString());
            for (const QJsonValue &v : ex.value(QStringLiteral("searches")).toArray()) {
                const QString msg = checkSearch(cat, v.toObject());
                QVERIFY2(msg.isEmpty(), qPrintable(msg));
            }
        }
        {
            PatchCatalog cat;
            const QJsonObject inv = lib.value(QStringLiteral("invalidEntries")).toObject();
            auto l = loadFile(inv.value(QStringLiteral("file")).toObject(), QStringLiteral("b.json"), cat);
            QVERIFY(l);
            const QJsonObject ex = inv.value(QStringLiteral("expect")).toObject();
            QCOMPARE(favs(*l), toStrings(ex.value(QStringLiteral("favorites")).toArray()));
            QStringList rec;
            for (const auto &r : l->recents())
                rec << r.address.toString();
            QCOMPARE(rec, toStrings(ex.value(QStringLiteral("recents")).toArray()));
            QCOMPARE(userNames(cat), ex.value(QStringLiteral("userNames")).toObject());
        }
    }
};

QTEST_GUILESS_MAIN(TestSharedVectors)
#include "test_shared_vectors.moc"
