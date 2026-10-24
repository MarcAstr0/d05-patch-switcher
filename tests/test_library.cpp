#include "app/Paths.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>
#include <climits>

using namespace d05;

namespace {
PatchAddress A(const char *s) { return *PatchAddress::parse(QString::fromLatin1(s)); }
}

class TestLibrary : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void unreadableFileIsLeftAlone();
    void sidelineFailureKeepsFile();
    void saveFailureReportedOnce();
    void countSaturates();
    void recentsSortedAndCutOnLoad();
    void timestampWithoutZoneIsUtc();
    void debounceCoalesces();
    void roundTrip();
    void recentsOrderAndCounts();
    void recentsCap();
    void clearRecents();
    void userNamesSanitised();
    void brokenFileSidelined_data();
    void brokenFileSidelined();
    void invalidEntriesSkipped();
    void debounce();
    void paths();
};

void TestLibrary::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("marcastr0"));
    QCoreApplication::setApplicationName(QStringLiteral("d05-patch-switcher"));
    QStandardPaths::setTestModeEnabled(true);
}

static void writeText(const QString &path, const QByteArray &content)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

void TestLibrary::unreadableFileIsLeftAlone()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    writeText(path, "{\"version\":1,\"favorites\":[\"P1-11\"]}");
    QFile::setPermissions(path, QFileDevice::Permissions());
    if (QFile(path).open(QIODevice::ReadOnly))
        QSKIP("file permissions are not enforced (running as root?)");
    PatchCatalog cat;
    {
        Library lib(cat, path);
        QCOMPARE(lib.load(), Library::LoadResult::Unreadable);
        QVERIFY(lib.writesBlocked());
        QVERIFY(lib.brokenFilePath().isEmpty());
        QVERIFY(lib.favorites().isEmpty());
        QSignalSpy spy(&lib, &Library::saveFailed);
        lib.setFavorite(A("P2-22"), true);
        QVERIFY(!lib.flush());
        QCOMPARE(spy.count(), 1);
        QVERIFY(lib.isFavorite(A("P2-22")));  // still in memory
    }
    QVERIFY(QFile::exists(path));
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 1);  // no sideline, no temp
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.readAll().contains("P1-11"));  // untouched
    f.close();
    PatchCatalog cat2;
    Library lib2(cat2, path);
    QCOMPARE(lib2.load(), Library::LoadResult::Loaded);  // a later load recovers
    QVERIFY(!lib2.writesBlocked());
    QVERIFY(lib2.isFavorite(A("P1-11")));
}

void TestLibrary::sidelineFailureKeepsFile()
{
    QTemporaryDir dir;
    QDir(dir.path()).mkdir(QStringLiteral("d"));
    const QString sub = dir.filePath(QStringLiteral("d"));
    const QString path = sub + QStringLiteral("/library.json");
    writeText(path, "{broken");
    QFile::setPermissions(sub, QFileDevice::ReadOwner | QFileDevice::ExeOwner);  // no write: rename fails
    if (QFileInfo(sub).isWritable()) {
        QFile::setPermissions(sub, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        QSKIP("directory permissions are not enforced (running as root?)");
    }
    PatchCatalog cat;
    {
        Library lib(cat, path);
        QCOMPARE(lib.load(), Library::LoadResult::SidelineFailed);
        QVERIFY(lib.writesBlocked());
        lib.setFavorite(A("P1-11"), true);
        QVERIFY(!lib.flush());
    }
    QFile::setPermissions(sub, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("{broken"));
}

void TestLibrary::saveFailureReportedOnce()
{
    QTemporaryDir dir;
    const QString blocker = dir.filePath(QStringLiteral("file"));
    writeText(blocker, "x");
    PatchCatalog cat;
    int failures = 0;
    {
        Library lib(cat, blocker + QStringLiteral("/library.json"));
        lib.load();
        QObject::connect(&lib, &Library::saveFailed, &lib, [&failures] { ++failures; });
        lib.setFavorite(A("P1-11"), true);
        QVERIFY(!lib.flush());
        lib.setFavorite(A("P1-12"), true);
        QVERIFY(!lib.flush());
        lib.recordSend(A("P1-13"));
        QVERIFY(!lib.flush());
        QCOMPARE(failures, 1);
        QVERIFY(lib.hasPendingChanges());
        QCOMPARE(lib.favorites().size(), 2);
        QCOMPARE(lib.recents().size(), 1);
        QVERIFY(Library::RetryMs >= 2000);
    }
    QCOMPARE(failures, 1);  // destructor flush did not emit
}

void TestLibrary::countSaturates()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    writeText(path, R"({"version":1,"recents":[{"address":"P1-11","count":2147483647,"last":"2026-01-01T00:00:00Z"},
        {"address":"P1-12","count":99999999999,"last":"2025-01-01T00:00:00Z"},
        {"address":"P1-13","count":-5,"last":"2024-01-01T00:00:00Z"}]})");
    PatchCatalog cat;
    Library lib(cat, path);
    QCOMPARE(lib.load(), Library::LoadResult::Loaded);
    QCOMPARE(lib.recents().at(0).count, INT_MAX);
    QCOMPARE(lib.recents().at(1).count, INT_MAX);
    QCOMPARE(lib.recents().at(2).count, 1);
    lib.recordSend(A("P1-11"));
    QCOMPARE(lib.recents().at(0).count, INT_MAX);
}

void TestLibrary::recentsSortedAndCutOnLoad()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    QJsonArray arr;
    // 60 entries, file order scrambled: entry i has time i seconds past a base.
    for (int k = 0; k < 60; ++k) {
        const int i = (k * 37) % 60;
        QJsonObject e;
        e.insert(QStringLiteral("address"), PatchAddress::fromCatalogueIndex(512 + i)->toString());
        e.insert(QStringLiteral("count"), 1);
        e.insert(QStringLiteral("last"),
                 QDateTime(QDate(2026, 1, 1), QTime(0, i / 60, i % 60), Qt::UTC).toString(Qt::ISODate));
        arr.append(e);
    }
    QJsonObject o;
    o.insert(QStringLiteral("version"), 1);
    o.insert(QStringLiteral("recents"), arr);
    writeText(path, QJsonDocument(o).toJson());
    PatchCatalog cat;
    Library lib(cat, path);
    QCOMPARE(lib.load(), Library::LoadResult::Loaded);
    QCOMPARE(lib.recents().size(), 50);
    for (int n = 0; n < 50; ++n)
        QCOMPARE(lib.recents().at(n).address, *PatchAddress::fromCatalogueIndex(512 + 59 - n));
}

void TestLibrary::timestampWithoutZoneIsUtc()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    writeText(path, R"({"version":1,"recents":[
        {"address":"P1-11","last":"2026-03-05T10:00:00"},
        {"address":"P1-12","last":"2026-03-05T10:00:00+02:00"}]})");
    PatchCatalog cat;
    Library lib(cat, path);
    lib.load();
    QCOMPARE(lib.recents().at(0).address, A("P1-11"));
    QCOMPARE(lib.recents().at(0).last, QDateTime(QDate(2026, 3, 5), QTime(10, 0), Qt::UTC));
    QCOMPARE(lib.recents().at(1).last, QDateTime(QDate(2026, 3, 5), QTime(8, 0), Qt::UTC));
    QCOMPARE(lib.recents().at(0).last.timeSpec(), Qt::UTC);
}

void TestLibrary::debounceCoalesces()
{
    QTemporaryDir dir;
    PatchCatalog cat;
    Library lib(cat, dir.filePath(QStringLiteral("library.json")));
    lib.load();
    QSignalSpy spy(&lib, &Library::saved);
    lib.setFavorite(A("P1-11"), true);
    lib.recordSend(A("P1-12"));
    lib.setUserName(A("U1-11"), QStringLiteral("x"));
    lib.setFavorite(A("P1-13"), true);
    QCOMPARE(spy.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
    QTest::qWait(800);
    QCOMPARE(spy.count(), 1);
}

void TestLibrary::roundTrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("sub/library.json"));
    {
        PatchCatalog cat;
        Library lib(cat, path);
        QCOMPARE(lib.load(), Library::LoadResult::Fresh);
        lib.setFavorite(A("P2-57"), true);
        lib.setFavorite(A("P1-11"), true);
        lib.setFavorite(A("P1-11"), true);
        lib.recordSend(A("P1-11"), QDateTime(QDate(2026, 10, 24), QTime(18, 2, 11), Qt::UTC));
        QVERIFY(lib.setUserName(A("U1-11"), QStringLiteral("Warm Pad Mk2")));
        QVERIFY(!lib.setUserName(A("P1-11"), QStringLiteral("x")));
        QVERIFY(lib.flush());
        QVERIFY(!lib.hasPendingChanges());
    }
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    QCOMPARE(o.value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(o.value(QStringLiteral("favorites")).toArray().at(0).toString(), QStringLiteral("P1-11"));
    QCOMPARE(o.value(QStringLiteral("recents")).toArray().at(0).toObject().value(QStringLiteral("last")).toString(),
             QStringLiteral("2026-10-24T18:02:11Z"));
    QCOMPARE(o.value(QStringLiteral("userNames")).toObject().value(QStringLiteral("U1-11")).toString(),
             QStringLiteral("Warm Pad Mk2"));

    PatchCatalog cat2;
    Library lib2(cat2, path);
    QCOMPARE(lib2.load(), Library::LoadResult::Loaded);
    QCOMPARE(lib2.favorites(), (QVector<PatchAddress>{A("P1-11"), A("P2-57")}));
    QVERIFY(lib2.isFavorite(A("P2-57")));
    QCOMPARE(lib2.recents().size(), 1);
    QCOMPARE(lib2.recents().at(0).count, 1);
    QCOMPARE(lib2.recents().at(0).last.toUTC(), QDateTime(QDate(2026, 10, 24), QTime(18, 2, 11), Qt::UTC));
    QCOMPARE(cat2.name(A("U1-11")), QStringLiteral("Warm Pad Mk2"));
    QVERIFY(!lib2.hasPendingChanges());
}

void TestLibrary::recentsOrderAndCounts()
{
    QTemporaryDir dir;
    PatchCatalog cat;
    Library lib(cat, dir.filePath(QStringLiteral("library.json")));
    lib.load();
    lib.recordSend(A("P1-11"));
    lib.recordSend(A("P1-12"));
    lib.recordSend(A("P1-13"));
    lib.recordSend(A("P1-11"));
    lib.recordSend(A("P1-11"));
    QCOMPARE(lib.recents().size(), 3);
    QCOMPARE(lib.recents().at(0).address, A("P1-11"));
    QCOMPARE(lib.recents().at(0).count, 3);
    QCOMPARE(lib.recents().at(1).address, A("P1-13"));
    QCOMPARE(lib.recents().at(2).address, A("P1-12"));
    QCOMPARE(lib.recents().at(2).count, 1);
}

void TestLibrary::recentsCap()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    PatchCatalog cat;
    Library lib(cat, path);
    lib.load();
    for (int i = 0; i < 60; ++i)
        lib.recordSend(*PatchAddress::fromCatalogueIndex(512 + i));
    QCOMPARE(lib.recents().size(), 50);
    QCOMPARE(lib.recents().first().address, *PatchAddress::fromCatalogueIndex(571));
    QCOMPARE(lib.recents().last().address, *PatchAddress::fromCatalogueIndex(522));
    lib.flush();
    PatchCatalog cat2;
    Library lib2(cat2, path);
    lib2.load();
    QCOMPARE(lib2.recents().size(), 50);
    QCOMPARE(lib2.recents().first().address, *PatchAddress::fromCatalogueIndex(571));
}

void TestLibrary::clearRecents()
{
    QTemporaryDir dir;
    PatchCatalog cat;
    Library lib(cat, dir.filePath(QStringLiteral("library.json")));
    lib.load();
    lib.recordSend(A("P1-11"));
    QSignalSpy spy(&lib, &Library::recentsChanged);
    lib.clearRecents();
    QCOMPARE(spy.count(), 1);
    QVERIFY(lib.recents().isEmpty());
}

void TestLibrary::userNamesSanitised()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    PatchCatalog cat;
    Library lib(cat, path);
    lib.load();
    lib.setUserName(A("U3-45"), QStringLiteral("  A very long user patch name here "));
    lib.flush();
    PatchCatalog cat2;
    Library lib2(cat2, path);
    lib2.load();
    QCOMPARE(cat2.name(A("U3-45")), QStringLiteral("A very long user p"));
    QCOMPARE(cat2.name(A("U3-45")).size() <= 18, true);
    // clearing removes it from the file
    lib2.setUserName(A("U3-45"), QString());
    lib2.flush();
    PatchCatalog cat3;
    Library lib3(cat3, path);
    lib3.load();
    QVERIFY(!cat3.hasUserName(A("U3-45")));
}

void TestLibrary::brokenFileSidelined_data()
{
    QTest::addColumn<QByteArray>("content");
    QTest::newRow("garbage") << QByteArray("{not json");
    QTest::newRow("array") << QByteArray("[1,2]");
    QTest::newRow("no version") << QByteArray("{\"favorites\":[]}");
    QTest::newRow("future version") << QByteArray("{\"version\":2}");
    QTest::newRow("wrong types") << QByteArray("{\"version\":1,\"favorites\":5}");
    QTest::newRow("empty") << QByteArray();
}

void TestLibrary::brokenFileSidelined()
{
    QFETCH(QByteArray, content);
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
    f.close();

    PatchCatalog cat;
    Library lib(cat, path);
    QCOMPARE(lib.load(), Library::LoadResult::Recovered);
    QVERIFY(!QFile::exists(path));
    QVERIFY(lib.brokenFilePath().contains(QStringLiteral("library.json.broken-")));
    QFile b(lib.brokenFilePath());
    QVERIFY(b.open(QIODevice::ReadOnly));
    QCOMPARE(b.readAll(), content);
    // the app proceeds: fresh state, and saving works
    QVERIFY(lib.favorites().isEmpty());
    lib.setFavorite(A("P1-11"), true);
    QVERIFY(lib.flush());
    PatchCatalog cat2;
    Library lib2(cat2, path);
    QCOMPARE(lib2.load(), Library::LoadResult::Loaded);
    QVERIFY(lib2.isFavorite(A("P1-11")));
}

void TestLibrary::invalidEntriesSkipped()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(R"({"version":1,"favorites":["P1-11","P1-19","junk",7],
        "recents":[{"address":"P1-12","count":4,"last":"bad"},{"address":"zzz"}],
        "userNames":{"U1-11":"ok","P1-11":"no","U9-11":"no"}})");
    f.close();
    PatchCatalog cat;
    Library lib(cat, path);
    QCOMPARE(lib.load(), Library::LoadResult::Loaded);
    QCOMPARE(lib.favorites(), (QVector<PatchAddress>{A("P1-11")}));
    QCOMPARE(lib.recents().size(), 1);
    QCOMPARE(lib.recents().at(0).count, 4);
    QCOMPARE(cat.name(A("U1-11")), QStringLiteral("ok"));
}

void TestLibrary::debounce()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("library.json"));
    PatchCatalog cat;
    Library lib(cat, path);
    lib.load();
    lib.setFavorite(A("P1-11"), true);
    QVERIFY(!QFile::exists(path));  // not written synchronously
    QVERIFY(lib.hasPendingChanges());
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(path), 2000);
    QVERIFY(!lib.hasPendingChanges());
}

void TestLibrary::paths()
{
    QTemporaryDir dir;
    Paths::setOverride(dir.path());
    QCOMPARE(Paths::dataDir(), QDir::cleanPath(dir.path()));
    QCOMPARE(Paths::libraryPath(), QDir::cleanPath(dir.path()) + QStringLiteral("/library.json"));
    QCOMPARE(Paths::settingsPath(), QDir::cleanPath(dir.path()) + QStringLiteral("/settings.ini"));
    Paths::setOverride(QString());
    QVERIFY(Paths::overrideDir().isEmpty());
    QCOMPARE(Paths::dataDir(),
             QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
    QVERIFY(Paths::dataDir().endsWith(QStringLiteral("marcastr0/d05-patch-switcher")));
}

QTEST_MAIN(TestLibrary)
#include "test_library.moc"
