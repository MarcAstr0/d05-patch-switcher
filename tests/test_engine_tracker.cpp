// EngineTracker (SPEC-2 section 6): signals and edge cases. The row-by-row vectors live in
// data/test-vectors.json (models.ju06a.engine) and run in test_shared_vectors.
#include "core/EngineTracker.h"

#include <QSignalSpy>
#include <QtTest>

using namespace d05;

class TestEngineTracker : public QObject {
    Q_OBJECT
private slots:
    void signalsOnArmAndFlip()
    {
        qRegisterMetaType<d05::PatchAddress>();
        EngineTracker t;
        t.setFlipDetectable(true);
        t.setState(60, 5, 28);
        QSignalSpy armed(&t, &EngineTracker::armedChanged);
        QSignalSpy engine(&t, &EngineTracker::engineChanged);
        QSignalSpy sends(&t, &EngineTracker::sendRequested);
        t.select(*PatchAddress::parse(QStringLiteral("106:1-1"), Model::JU06A));
        QCOMPARE(armed.count(), 1);
        QCOMPARE(t.prompt(), EngineTracker::Prompt::FlipSwitch);
        QCOMPARE(sends.count(), 0);
        t.programChange(28);
        QCOMPARE(engine.count(), 1);
        QCOMPARE(engine.at(0).at(0).toInt(), 106);
        QCOMPARE(sends.count(), 1);
        QCOMPARE(armed.count(), 2);
        QCOMPARE(t.prompt(), EngineTracker::Prompt::None);
    }

    void hintSignals()
    {
        EngineTracker t;
        t.setState(60, 5, 28);
        QSignalSpy hint(&t, &EngineTracker::hintChanged);
        t.programChange(28);
        QCOMPARE(hint.count(), 1);
        QCOMPARE(hint.at(0).at(0).toInt(), 106);
        t.toggleEngine();
        QCOMPARE(hint.count(), 2);
        QCOMPARE(hint.at(1).at(0).toInt(), 0);
    }

    void escapeReportsWhetherItConsumed()
    {
        EngineTracker t;
        t.setState(60, 5, 28);
        QVERIFY(!t.escape());
        t.select(*PatchAddress::parse(QStringLiteral("106:1-1"), Model::JU06A));
        QVERIFY(t.escape());
        QVERIFY(!t.escape());
    }

    void promptFollowsFlipDetectable()
    {
        EngineTracker t;
        t.setState(60, 5, 28);
        t.select(*PatchAddress::parse(QStringLiteral("106:1-1"), Model::JU06A));
        QCOMPARE(t.prompt(), EngineTracker::Prompt::FlipThenCtrlE);
        t.setFlipDetectable(true);
        QCOMPARE(t.prompt(), EngineTracker::Prompt::FlipSwitch);
    }

    void ignoresOtherModels()
    {
        EngineTracker t;
        t.setState(60, 5, 28);
        QSignalSpy sends(&t, &EngineTracker::sendRequested);
        t.select(*PatchAddress::parse(QStringLiteral("4-5"), Model::JP08));
        QCOMPARE(sends.count(), 0);
        QVERIFY(!t.armed());
    }

    void badStateIsCleaned()
    {
        EngineTracker t;
        t.setState(7, 99, -5);
        QCOMPARE(t.engine(), 60);
        QCOMPARE(t.remembered(60), EngineTracker::Unknown);
        QCOMPARE(t.remembered(106), EngineTracker::Unknown);
        QVERIFY(!t.current());
    }
};

QTEST_MAIN(TestEngineTracker)
#include "test_engine_tracker.moc"
