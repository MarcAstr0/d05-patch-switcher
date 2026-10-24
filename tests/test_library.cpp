#include "app/Paths.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace d05;

namespace {
PatchAddress A(const char *s) { return *PatchAddress::parse(QString::fromLatin1(s)); }
}

class TestLibrary : public QObject {
    Q_OBJECT
private slots:
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
    QVERIFY(Paths::dataDir().endsWith(QStringLiteral("marcastr0/d05-patch-switcher")));
}

QTEST_MAIN(TestLibrary)
#include "test_library.moc"
