// Runs data/test-vectors.json (shared with the web implementation) against the core classes.
#include "core/EngineTracker.h"
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
    const Model model = cat.model();
    const SearchQuery q(query, model);
    const int size = model == Model::D05 ? PatchAddress::CatalogueSize : (model == Model::JP08 ? 64 : 128);
    QStringList out;
    for (int i = 0; i < size; ++i) {
        const PatchAddress a = model == Model::D05 ? *PatchAddress::fromCatalogueIndex(i)
                                                   : *PatchAddress::fromCatalogueIndex(model, i);
        const QString name = cat.name(a);
        const QString factory = cat.factoryName(a);
        if (q.matches(a, name, nullptr, factory != name ? factory : QString()))
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

    // models.<id>.search: per-model grammar (SPEC-2 FR-3). Optional "userNames" per case
    // ({canonical address: name}) is applied to a fresh catalogue through Library::setUserName.
    void modelSearch_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<int>("minCases");
        QTest::newRow("jp08") << QStringLiteral("jp08") << 20;
        QTest::newRow("ju06a") << QStringLiteral("ju06a") << 20;
    }

    void modelSearch()
    {
        QFETCH(QString, id);
        QFETCH(int, minCases);
        const Model model = id == QLatin1String("jp08") ? Model::JP08 : Model::JU06A;
        const QJsonArray arr = m_doc.value(QStringLiteral("models")).toObject()
                                   .value(id).toObject().value(QStringLiteral("search")).toArray();
        QVERIFY(arr.size() >= minCases);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const QJsonValue &v : arr) {
            const QJsonObject c = v.toObject();
            PatchCatalog cat(model);
            QVERIFY(cat.isValid());
            Library seed(cat, dir.filePath(QStringLiteral("seed.json")));
            const QJsonObject users = c.value(QStringLiteral("userNames")).toObject();
            for (auto it = users.begin(); it != users.end(); ++it) {
                const auto a = PatchAddress::parse(it.key(), model);
                QVERIFY2(a, qPrintable(it.key()));
                QVERIFY(seed.setUserName(*a, it.value().toString()));
            }
            const QString msg = checkSearch(cat, c);
            QVERIFY2(msg.isEmpty(), qPrintable(msg));
        }
    }

    // models.ju06a.engine: SPEC-2 section 6 event sequences against EngineTracker.
    void modelsEngine()
    {
        const QJsonArray seqs = m_doc.value(QStringLiteral("models")).toObject()
                                    .value(QStringLiteral("ju06a")).toObject()
                                    .value(QStringLiteral("engine")).toObject()
                                    .value(QStringLiteral("sequences")).toArray();
        QVERIFY(seqs.size() >= 12);
        for (const QJsonValue &sv : seqs) {
            const QJsonObject s = sv.toObject();
            const QString name = s.value(QStringLiteral("name")).toString();
            const int channel = s.value(QStringLiteral("channel")).toInt();
            const QJsonObject init = s.value(QStringLiteral("initial")).toObject();
            auto prog = [](const QJsonValue &v) { return v.isNull() ? EngineTracker::Unknown : v.toInt(); };

            EngineTracker t;
            QStringList sent;
            connect(&t, &EngineTracker::sendRequested, this, [&](const PatchAddress &a) {
                for (const QByteArray &m : a.midiMessages(channel, t.engine()))
                    sent << QString::fromLatin1(m.toHex(' ').toUpper());
            });
            t.setFlipDetectable(s.value(QStringLiteral("flipDetectable")).toBool());
            t.setState(init.value(QStringLiteral("engine")).toInt(),
                       prog(init.value(QStringLiteral("remembered60"))),
                       prog(init.value(QStringLiteral("remembered106"))));
            sent.clear();

            auto check = [&](const QJsonObject &e, const QString &where) -> QString {
                auto addr = [](const std::optional<PatchAddress> &a) {
                    return a ? a->toString() : QString();
                };
                auto want = [&](const char *k) { return e.value(QLatin1String(k)); };
                const QString tag = QStringLiteral("[%1 %2] ").arg(name, where);
                if (want("engine").toInt() != t.engine())
                    return tag + QStringLiteral("engine %1").arg(t.engine());
                if (prog(want("remembered60")) != t.remembered(60))
                    return tag + QStringLiteral("remembered60 %1").arg(t.remembered(60));
                if (prog(want("remembered106")) != t.remembered(106))
                    return tag + QStringLiteral("remembered106 %1").arg(t.remembered(106));
                if (want("armed").toString() != addr(t.armed()))
                    return tag + QStringLiteral("armed '%1'").arg(addr(t.armed()));
                if (want("hint").toInt() != t.hint())
                    return tag + QStringLiteral("hint %1").arg(t.hint());
                if (want("current").toString() != addr(t.current()))
                    return tag + QStringLiteral("current '%1'").arg(addr(t.current()));
                const QString p = want("prompt").toString();
                const auto pk = t.prompt();
                const QString got = pk == EngineTracker::Prompt::None ? QStringLiteral("none")
                                    : pk == EngineTracker::Prompt::FlipSwitch ? QStringLiteral("flipSwitch")
                                                                              : QStringLiteral("flipThenCtrlE");
                if (p != got)
                    return tag + QStringLiteral("prompt %1").arg(got);
                if (toStrings(want("sent").toArray()) != sent)
                    return tag + QStringLiteral("sent [%1]").arg(sent.join(QLatin1String(", ")));
                return QString();
            };

            QString msg = check(s.value(QStringLiteral("expect")).toObject(), QStringLiteral("initial"));
            QVERIFY2(msg.isEmpty(), qPrintable(msg));
            int n = 0;
            for (const QJsonValue &ev : s.value(QStringLiteral("events")).toArray()) {
                const QJsonObject e = ev.toObject();
                const QString what = e.value(QStringLiteral("do")).toString();
                ++n;
                if (what == QLatin1String("select")) {
                    const auto a = PatchAddress::parse(e.value(QStringLiteral("addr")).toString(), Model::JU06A);
                    QVERIFY2(a, qPrintable(name));
                    t.select(*a);
                } else if (what == QLatin1String("pc")) {
                    t.programChange(e.value(QStringLiteral("program")).toInt());
                } else if (what == QLatin1String("ctrlE")) {
                    t.toggleEngine();
                } else if (what == QLatin1String("esc")) {
                    t.escape();
                } else if (what == QLatin1String("deviceSwitch")) {
                    t.deviceSwitch();
                } else {
                    QFAIL(qPrintable(QStringLiteral("unknown event ") + what));
                }
                msg = check(e.value(QStringLiteral("expect")).toObject(),
                            QStringLiteral("event %1 (%2)").arg(n).arg(what));
                QVERIFY2(msg.isEmpty(), qPrintable(msg));
            }
        }
    }

    // models.library: one v1 and one v2 file (SPEC-2 3.5).
    void modelsLibrary()
    {
        const QJsonObject lib = m_doc.value(QStringLiteral("models")).toObject()
                                    .value(QStringLiteral("library")).toObject();
        QVERIFY(!lib.isEmpty());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        for (const QString &which : {QStringLiteral("v1"), QStringLiteral("v2")}) {
            const QJsonObject c = lib.value(which).toObject();
            const QJsonObject file = c.value(QStringLiteral("file")).toObject();
            const QString path = dir.filePath(which + QStringLiteral(".json"));
            {
                QFile f(path);
                QVERIFY(f.open(QIODevice::WriteOnly));
                f.write(QJsonDocument(file).toJson());
            }
            PatchCatalog d05cat, jpcat(Model::JP08), jucat(Model::JU06A);
            Library l(d05cat, path);
            l.attachCatalog(jpcat);
            l.attachCatalog(jucat);
            QCOMPARE(l.load(), Library::LoadResult::Loaded);

            const QJsonObject ex = c.value(QStringLiteral("expect")).toObject();
            for (const Model m : allModels()) {
                const QString id = modelInfo(m).id;
                if (!ex.contains(id))
                    continue;
                const QJsonObject e = ex.value(id).toObject();
                const PatchCatalog &cat = m == Model::D05 ? d05cat : (m == Model::JP08 ? jpcat : jucat);
                QStringList favs, rec;
                for (const auto &a : l.favorites(m))
                    favs << a.toString();
                for (const auto &r : l.recents(m))
                    rec << r.address.toString();
                QCOMPARE(favs, toStrings(e.value(QStringLiteral("favorites")).toArray()));
                QCOMPARE(rec, toStrings(e.value(QStringLiteral("recents")).toArray()));
                QJsonObject names;
                for (int i = 0; i < modelInfo(m).catalogueSize; ++i) {
                    const PatchAddress a = m == Model::D05 ? *PatchAddress::fromCatalogueIndex(i)
                                                           : *PatchAddress::fromCatalogueIndex(m, i);
                    if (cat.hasUserName(a))
                        names.insert(a.toString(), cat.userName(a));
                }
                QCOMPARE(names, e.value(QStringLiteral("userNames")).toObject());
            }

            // A save with no net change writes the version 2 form.
            l.setFavorite(*PatchAddress::parse(QStringLiteral("P6-88")), true);
            l.setFavorite(*PatchAddress::parse(QStringLiteral("P6-88")), false);
            QVERIFY(l.flush());
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QJsonObject written = QJsonDocument::fromJson(f.readAll()).object();
            QCOMPARE(written, which == QLatin1String("v1") ? c.value(QStringLiteral("written")).toObject() : file);
            QCOMPARE(QFile::exists(l.legacyCopyPath()), which == QLatin1String("v1"));
            if (which == QLatin1String("v1")) {
                QFile copy(l.legacyCopyPath());
                QVERIFY(copy.open(QIODevice::ReadOnly));
                QCOMPARE(QJsonDocument::fromJson(copy.readAll()).object(), file);
            }
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
