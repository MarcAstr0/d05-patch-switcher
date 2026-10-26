// Library file version 2 (SPEC-2 3.5) and the data-directory migration.
#include "app/Paths.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace d05;

namespace {

PatchAddress D(const char *s) { return *PatchAddress::parse(QString::fromLatin1(s)); }
PatchAddress J(const char *s) { return *PatchAddress::parse(QString::fromLatin1(s), Model::JP08); }
PatchAddress U(const char *s) { return *PatchAddress::parse(QString::fromLatin1(s), Model::JU06A); }

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeBytes(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QJsonObject readJson(const QString &path) { return QJsonDocument::fromJson(readAll(path)).object(); }

struct Three {
    PatchCatalog d05;
    PatchCatalog jp{Model::JP08};
    PatchCatalog ju{Model::JU06A};
    Library lib;
    explicit Three(const QString &path) : lib(d05, path)
    {
        lib.attachCatalog(jp);
        lib.attachCatalog(ju);
    }
};

}  // namespace

class TestLibraryV2 : public QObject {
    Q_OBJECT
private slots:
    void roundTripThreeDevices();
    void userNamesPerModel();
    void signalsCarryModel();
    void realV1FixtureMigrates();
    void legacyCopyKeptOnce();
    void v1UnknownKeysRoundTrip();
    void v2UnknownDeviceAndKeysRoundTrip();
    void modelWithoutCatalogKeepsNames();
    void brokenV2Sidelined_data();
    void brokenV2Sidelined();
    void dataDirCopiedOnceAndOldUntouched();
    void dataDirNoOldNoNew();
    void configDirBypassesMigration();
    void failedCopyLeavesNothingAndRetries();
};

void TestLibraryV2::roundTripThreeDevices()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    const QDateTime t(QDate(2026, 10, 24), QTime(18, 2, 11), Qt::UTC);
    {
        Three a(path);
        QCOMPARE(a.lib.load(), Library::LoadResult::Fresh);
        a.lib.setFavorite(D("P1-11"), true);
        a.lib.setFavorite(J("4-5"), true);
        a.lib.setFavorite(U("106:1-1"), true);
        a.lib.setFavorite(U("60:2-3"), true);
        a.lib.recordSend(D("P1-11"), t);
        a.lib.recordSend(J("1-1"), t.addSecs(60));
        a.lib.recordSend(U("106:8-8"), t.addSecs(120));
        QVERIFY(a.lib.setUserName(D("U1-11"), QStringLiteral("Warm Pad Mk2")));
        QVERIFY(a.lib.setUserName(J("1-1"), QStringLiteral("CN Intro Lead")));
        QVERIFY(a.lib.setUserName(U("106:1-2"), QStringLiteral("CN Brass")));
        QVERIFY(a.lib.flush());
    }
    const QJsonObject o = readJson(path);
    QCOMPARE(o.value(QStringLiteral("version")).toInt(), 2);
    const QJsonObject devs = o.value(QStringLiteral("devices")).toObject();
    QCOMPARE(devs.keys(), (QStringList{QStringLiteral("d05"), QStringLiteral("jp08"), QStringLiteral("ju06a")}));
    QCOMPARE(devs.value(QStringLiteral("jp08")).toObject().value(QStringLiteral("favorites")).toArray().at(0).toString(),
             QStringLiteral("4-5"));
    QCOMPARE(devs.value(QStringLiteral("ju06a")).toObject().value(QStringLiteral("favorites")),
             QJsonValue(QJsonArray{QStringLiteral("60:2-3"), QStringLiteral("106:1-1")}));

    Three b(path);
    QCOMPARE(b.lib.load(), Library::LoadResult::Loaded);
    QCOMPARE(b.lib.favorites(Model::D05), (QVector<PatchAddress>{D("P1-11")}));
    QCOMPARE(b.lib.favorites(Model::JP08), (QVector<PatchAddress>{J("4-5")}));
    QCOMPARE(b.lib.favorites(Model::JU06A), (QVector<PatchAddress>{U("60:2-3"), U("106:1-1")}));
    QVERIFY(b.lib.isFavorite(U("106:1-1")));
    QVERIFY(!b.lib.isFavorite(J("1-1")));
    QCOMPARE(b.lib.recents(Model::D05).size(), 1);
    QCOMPARE(b.lib.recents(Model::JP08).at(0).address, J("1-1"));
    QCOMPARE(b.lib.recents(Model::JU06A).at(0).address, U("106:8-8"));
    QCOMPARE(b.lib.recents(Model::JU06A).at(0).last, t.addSecs(120));
    QCOMPARE(b.d05.name(D("U1-11")), QStringLiteral("Warm Pad Mk2"));
    QCOMPARE(b.jp.userName(J("1-1")), QStringLiteral("CN Intro Lead"));
    QCOMPARE(b.ju.userName(U("106:1-2")), QStringLiteral("CN Brass"));
    QVERIFY(!b.lib.hasPendingChanges());
}

void TestLibraryV2::userNamesPerModel()
{
    QTemporaryDir dir;
    Three a(dir.filePath(QStringLiteral("library.json")));
    a.lib.load();
    QVERIFY(!a.lib.setUserName(D("P1-11"), QStringLiteral("x")));  // D-05 presets stay read-only
    QVERIFY(a.lib.setUserName(D("U8-88"), QStringLiteral("x")));
    QVERIFY(a.lib.setUserName(J("8-8"), QStringLiteral("  Tab\there  ")));  // any JP-08 slot
    QCOMPARE(a.jp.userName(J("8-8")), QStringLiteral("Tab here"));
    QVERIFY(a.lib.setUserName(U("60:1-1"), QStringLiteral("Mine")));
    QCOMPARE(a.ju.name(U("60:1-1")), QStringLiteral("Mine"));
    QVERIFY(!a.ju.tooltipName(U("60:1-1")).isEmpty());  // the factory name stays reachable
    // Clearing falls back to the factory name.
    QVERIFY(a.lib.setUserName(U("60:1-1"), QString()));
    QVERIFY(!a.ju.hasUserName(U("60:1-1")));
    // No catalog attached for a model: refused.
    PatchCatalog only;
    Library solo(only, dir.filePath(QStringLiteral("solo.json")));
    QVERIFY(!solo.setUserName(J("1-1"), QStringLiteral("x")));
}

void TestLibraryV2::signalsCarryModel()
{
    QTemporaryDir dir;
    Three a(dir.filePath(QStringLiteral("library.json")));
    a.lib.load();
    QSignalSpy fav(&a.lib, &Library::favoritesChanged);
    QSignalSpy rec(&a.lib, &Library::recentsChanged);
    QSignalSpy names(&a.lib, &Library::userNamesChanged);
    a.lib.setFavorite(J("1-1"), true);
    a.lib.recordSend(U("60:1-1"));
    a.lib.setUserName(D("U1-11"), QStringLiteral("n"));
    QCOMPARE(fav.count(), 1);
    QCOMPARE(fav.at(0).at(0).value<Model>(), Model::JP08);
    QCOMPARE(rec.at(0).at(0).value<Model>(), Model::JU06A);
    QCOMPARE(names.at(0).at(0).value<Model>(), Model::D05);
    a.lib.clearRecents(Model::JP08);  // nothing there: no signal
    QCOMPARE(rec.count(), 1);
    a.lib.clearRecents(Model::JU06A);
    QCOMPARE(rec.count(), 2);
}

void TestLibraryV2::realV1FixtureMigrates()
{
    // The desktop-written file the web importer tests use (plan W1).
    const QByteArray original = readAll(QStringLiteral(D05_V1_FIXTURE));
    QVERIFY(!original.isEmpty());
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    writeBytes(path, original);

    Three a(path);
    QCOMPARE(a.lib.load(), Library::LoadResult::Loaded);
    QCOMPARE(a.lib.favorites(Model::D05).size(), 4);
    QVERIFY(a.lib.isFavorite(D("U3-18")));
    QCOMPARE(a.lib.recents(Model::D05).size(), 2);
    QCOMPARE(a.d05.name(D("U1-11")), QStringLiteral("Warm Pad Mk2"));
    QVERIFY(a.lib.favorites(Model::JP08).isEmpty());
    // The copy exists as soon as the file is read, byte for byte.
    QCOMPARE(a.lib.legacyCopyPath(), path + QStringLiteral(".v1"));
    QCOMPARE(readAll(a.lib.legacyCopyPath()), original);
    // Nothing is written until something changes.
    QCOMPARE(readAll(path), original);

    a.lib.setFavorite(J("1-1"), true);
    QVERIFY(a.lib.flush());
    const QJsonObject o = readJson(path);
    QCOMPARE(o.value(QStringLiteral("version")).toInt(), 2);
    QVERIFY(!o.contains(QStringLiteral("favorites")));
    const QJsonObject d = o.value(QStringLiteral("devices")).toObject();
    QCOMPARE(d.value(QStringLiteral("d05")).toObject().value(QStringLiteral("favorites")).toArray().size(), 4);
    QCOMPARE(d.value(QStringLiteral("d05")).toObject().value(QStringLiteral("userNames")).toObject().size(), 3);
    QCOMPARE(d.value(QStringLiteral("jp08")).toObject().value(QStringLiteral("favorites")).toArray().at(0).toString(),
             QStringLiteral("1-1"));
    QCOMPARE(readAll(a.lib.legacyCopyPath()), original);  // still the v1 bytes
}

void TestLibraryV2::legacyCopyKeptOnce()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    writeBytes(path, "{\"version\":1,\"favorites\":[\"P1-11\"]}");
    writeBytes(path + QStringLiteral(".v1"), "older copy");
    Three a(path);
    QCOMPARE(a.lib.load(), Library::LoadResult::Loaded);
    QCOMPARE(readAll(path + QStringLiteral(".v1")), QByteArray("older copy"));  // never overwritten
    // A version 2 file makes no copy.
    QVERIFY(a.lib.toggleFavorite(D("P1-12")));
    QVERIFY(a.lib.flush());
    QFile::remove(path + QStringLiteral(".v1"));
    Three b(path);
    QCOMPARE(b.lib.load(), Library::LoadResult::Loaded);
    QVERIFY(!QFile::exists(path + QStringLiteral(".v1")));
}

void TestLibraryV2::v1UnknownKeysRoundTrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    writeBytes(path, R"({"version":1,"favorites":["P1-11"],"future":{"a":[1,2]},"flag":true})");
    Three a(path);
    QCOMPARE(a.lib.load(), Library::LoadResult::Loaded);
    a.lib.setFavorite(D("P1-12"), true);
    QVERIFY(a.lib.flush());
    const QJsonObject o = readJson(path);
    QCOMPARE(o.value(QStringLiteral("future")).toObject().value(QStringLiteral("a")).toArray().size(), 2);
    QCOMPARE(o.value(QStringLiteral("flag")).toBool(), true);
    QCOMPARE(o.value(QStringLiteral("version")).toInt(), 2);
}

void TestLibraryV2::v2UnknownDeviceAndKeysRoundTrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    const QJsonObject input = QJsonDocument::fromJson(R"({
      "version": 2, "top": {"x": 1},
      "devices": {
        "d05": {"favorites": ["P1-11"], "recents": [], "userNames": {}, "extraKey": [1, 2, 3]},
        "jp08": {"favorites": [], "recents": [], "userNames": {}},
        "ju06a": {"favorites": [], "recents": [], "userNames": {}},
        "tx81z": {"favorites": ["whatever"], "weird": null}
      }})").object();
    writeBytes(path, QJsonDocument(input).toJson());
    Three a(path);
    QCOMPARE(a.lib.load(), Library::LoadResult::Loaded);
    a.lib.setFavorite(D("P1-12"), true);
    a.lib.setFavorite(D("P1-12"), false);
    QVERIFY(a.lib.flush());
    QCOMPARE(readJson(path), input);

    // And through a change to another device.
    a.lib.setFavorite(J("2-2"), true);
    QVERIFY(a.lib.flush());
    const QJsonObject o = readJson(path);
    QCOMPARE(o.value(QStringLiteral("top")), input.value(QStringLiteral("top")));
    const QJsonObject d = o.value(QStringLiteral("devices")).toObject();
    QCOMPARE(d.value(QStringLiteral("tx81z")), input.value(QStringLiteral("devices")).toObject().value(QStringLiteral("tx81z")));
    QCOMPARE(d.value(QStringLiteral("d05")).toObject().value(QStringLiteral("extraKey")).toArray().size(), 3);
    QCOMPARE(d.value(QStringLiteral("jp08")).toObject().value(QStringLiteral("favorites")).toArray().at(0).toString(),
             QStringLiteral("2-2"));
}

void TestLibraryV2::modelWithoutCatalogKeepsNames()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    const QJsonObject input = QJsonDocument::fromJson(R"({
      "version": 2,
      "devices": {
        "d05": {"favorites": [], "recents": [], "userNames": {}},
        "jp08": {"favorites": ["4-5"], "recents": [], "userNames": {"1-1": "CN Intro Lead"}},
        "ju06a": {"favorites": [], "recents": [], "userNames": {"106:1-2": "CN Brass"}}
      }})").object();
    writeBytes(path, QJsonDocument(input).toJson());
    PatchCatalog only;  // an older build with the D-05 only
    Library lib(only, path);
    QCOMPARE(lib.load(), Library::LoadResult::Loaded);
    lib.setFavorite(D("P1-11"), true);
    QVERIFY(lib.flush());
    const QJsonObject d = readJson(path).value(QStringLiteral("devices")).toObject();
    QCOMPARE(d.value(QStringLiteral("jp08")), input.value(QStringLiteral("devices")).toObject().value(QStringLiteral("jp08")));
    QCOMPARE(d.value(QStringLiteral("ju06a")), input.value(QStringLiteral("devices")).toObject().value(QStringLiteral("ju06a")));
}

void TestLibraryV2::brokenV2Sidelined_data()
{
    QTest::addColumn<QByteArray>("content");
    QTest::newRow("devices not an object") << QByteArray(R"({"version":2,"devices":[]})");
    QTest::newRow("block not an object") << QByteArray(R"({"version":2,"devices":{"jp08":5}})");
    QTest::newRow("block wrong types") << QByteArray(R"({"version":2,"devices":{"d05":{"favorites":5}}})");
    QTest::newRow("version 3") << QByteArray(R"({"version":3,"devices":{}})");
    QTest::newRow("truncated") << QByteArray(R"({"version":2,"devices":{"d05":)");
}

void TestLibraryV2::brokenV2Sidelined()
{
    QFETCH(QByteArray, content);
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    writeBytes(path, content);
    Three a(path);
    QCOMPARE(a.lib.load(), Library::LoadResult::Recovered);
    QVERIFY(!QFile::exists(path));
    QCOMPARE(readAll(a.lib.brokenFilePath()), content);
    QVERIFY(!QFile::exists(a.lib.legacyCopyPath()));
    a.lib.setFavorite(J("1-1"), true);
    QVERIFY(a.lib.flush());
    Three b(path);
    QCOMPARE(b.lib.load(), Library::LoadResult::Loaded);
    QVERIFY(b.lib.isFavorite(J("1-1")));
}

void TestLibraryV2::dataDirCopiedOnceAndOldUntouched()
{
    QTemporaryDir root;
    const QString oldDir = root.filePath(QStringLiteral("marcastr0/d05-patch-switcher"));
    const QString newDir = root.filePath(QStringLiteral("marcastr0/boutique-patch-switcher"));
    writeBytes(oldDir + QStringLiteral("/library.json"), "{\"version\":1,\"favorites\":[\"P1-11\"]}");
    writeBytes(oldDir + QStringLiteral("/settings.ini"), "[midi]\nchannel=3\n");
    writeBytes(oldDir + QStringLiteral("/logo.png"), "png");

    struct Snap { QByteArray data; QDateTime mtime; };
    auto snap = [&] {
        QMap<QString, Snap> m;
        const QFileInfoList l = QDir(oldDir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : l)
            m.insert(fi.fileName(), {readAll(fi.absoluteFilePath()), fi.lastModified()});
        return m;
    };
    const auto before = snap();
    QCOMPARE(before.size(), 3);
    QTest::qWait(30);

    QVERIFY(Paths::migrateDataDir(oldDir, newDir));
    QCOMPARE(readAll(newDir + QStringLiteral("/library.json")), before.value(QStringLiteral("library.json")).data);
    QCOMPARE(readAll(newDir + QStringLiteral("/settings.ini")), before.value(QStringLiteral("settings.ini")).data);
    QCOMPARE(readAll(newDir + QStringLiteral("/logo.png")), QByteArray("png"));
    // The settings file is usable as it is.
    QSettings s(newDir + QStringLiteral("/settings.ini"), QSettings::IniFormat);
    QCOMPARE(s.value(QStringLiteral("midi/channel")).toInt(), 3);

    // Copied once: a later start, with the new directory in use, copies nothing.
    writeBytes(newDir + QStringLiteral("/library.json"), "{\"version\":2}");
    QVERIFY(!Paths::migrateDataDir(oldDir, newDir));
    QCOMPARE(readAll(newDir + QStringLiteral("/library.json")), QByteArray("{\"version\":2}"));

    // The old directory is exactly as it was: same files, bytes and modification times.
    const auto after = snap();
    QCOMPARE(after.keys(), before.keys());
    for (auto it = before.begin(); it != before.end(); ++it) {
        QCOMPARE(after.value(it.key()).data, it.value().data);
        QCOMPARE(after.value(it.key()).mtime, it.value().mtime);
    }
}

void TestLibraryV2::dataDirNoOldNoNew()
{
    QTemporaryDir root;
    const QString oldDir = root.filePath(QStringLiteral("old"));
    const QString newDir = root.filePath(QStringLiteral("new"));
    QVERIFY(!Paths::migrateDataDir(oldDir, newDir));  // fresh install: nothing to copy
    QVERIFY(!QFileInfo::exists(newDir));
    QDir().mkpath(oldDir);  // an empty old directory still counts as there
    QVERIFY(Paths::migrateDataDir(oldDir, newDir));
    QVERIFY(QFileInfo(newDir).isDir());
}

void TestLibraryV2::configDirBypassesMigration()
{
    QTemporaryDir cfg;
    Paths::setOverride(cfg.path());
    QVERIFY(!Paths::migrateLegacyDataDir());
    QCOMPARE(QDir(cfg.path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 0);
    Paths::setOverride(QString());
}

void TestLibraryV2::failedCopyLeavesNothingAndRetries()
{
    QTemporaryDir root;
    const QString oldDir = root.filePath(QStringLiteral("old"));
    const QString newDir = root.filePath(QStringLiteral("new"));
    writeBytes(oldDir + QStringLiteral("/a.json"), "a");
    writeBytes(oldDir + QStringLiteral("/b.json"), "b");
    // newDir cannot be created under a regular file.
    writeBytes(root.filePath(QStringLiteral("blocker")), "x");
    QVERIFY(!Paths::migrateDataDir(oldDir, root.filePath(QStringLiteral("blocker/new"))));
    QCOMPARE(readAll(oldDir + QStringLiteral("/a.json")), QByteArray("a"));
    QVERIFY(Paths::migrateDataDir(oldDir, newDir));  // a later start succeeds
    QCOMPARE(readAll(newDir + QStringLiteral("/b.json")), QByteArray("b"));
}

QTEST_GUILESS_MAIN(TestLibraryV2)
#include "test_library_v2.moc"
