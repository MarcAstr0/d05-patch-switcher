// EngineTracker (SPEC-2 section 6): signals and edge cases. The row-by-row vectors live in
// data/test-vectors.json (models.ju06a.engine) and run in test_shared_vectors.
#include "core/EngineTracker.h"

#include <QSignalSpy>
#include <QtTest>

using namespace d05;

class TestEngineTracker : public QObject {
    Q_OBJECT
public:
    // Records every signal by name, in order; sends also record "engine() == address engine".
    struct Recorder {
        QStringList log;
        bool wrongEngine = false;
        explicit Recorder(EngineTracker &t)
        {
            QObject::connect(&t, &EngineTracker::engineChanged, &t, [this](int e) {
                log << QStringLiteral("engineChanged:%1").arg(e);
            });
            QObject::connect(&t, &EngineTracker::armedChanged, &t, [this] { log << QStringLiteral("armedChanged"); });
            QObject::connect(&t, &EngineTracker::hintChanged, &t, [this](int e) {
                log << QStringLiteral("hintChanged:%1").arg(e);
            });
            QObject::connect(&t, &EngineTracker::currentChanged, &t, [this] { log << QStringLiteral("currentChanged"); });
            QObject::connect(&t, &EngineTracker::rememberedChanged, &t, [this] { log << QStringLiteral("rememberedChanged"); });
            QObject::connect(&t, &EngineTracker::sendRequested, &t, [this, &t](const d05::PatchAddress &a) {
                if (a.engine() != t.engine())
                    wrongEngine = true;
                log << QStringLiteral("sendRequested:%1").arg(a.toString());
            });
        }
    };

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

    void signalOrderOnConfirmingPc()
    {
        EngineTracker t;
        t.setFlipDetectable(true);
        t.setState(60, 5, 28);
        Recorder r(t);
        t.select(*PatchAddress::parse(QStringLiteral("106:1-1"), Model::JU06A));
        QCOMPARE(r.log, QStringList{QStringLiteral("armedChanged")});
        r.log.clear();
        t.programChange(28);
        const QStringList want{
            QStringLiteral("armedChanged"),
            QStringLiteral("engineChanged:106"),
            QStringLiteral("rememberedChanged"),
            QStringLiteral("currentChanged"),
            QStringLiteral("sendRequested:106:1-1"),
        };
        QCOMPARE(r.log, want);
        QVERIFY(!r.wrongEngine);
    }

    void failedSendsLeaveRememberedAndCurrentAlone()
    {
        EngineTracker t;
        bool ok = false;
        int calls = 0;
        t.setSender([&](const d05::PatchAddress &) {
            ++calls;
            return ok;
        });
        const auto a60 = *PatchAddress::parse(QStringLiteral("60:2-1"), Model::JU06A);
        const auto a106 = *PatchAddress::parse(QStringLiteral("106:1-1"), Model::JU06A);
        t.setState(60, 5, 28);
        const auto cur = t.current();
        // Plain select: not sent, nothing remembered, the arm and hint are still cleared.
        QVERIFY(!t.select(a60));
        QCOMPARE(t.remembered(60), 5);
        QCOMPARE(t.current(), cur);
        // Ctrl+E: the toggle stands, the armed send did not happen, the arm stays.
        QVERIFY(t.select(a106));
        t.toggleEngine();
        QCOMPARE(t.engine(), 106);
        QVERIFY(t.armed().has_value());
        QCOMPARE(t.remembered(106), 28);
        QCOMPARE(t.current().value(), *PatchAddress::parse(QStringLiteral("106:4-5"), Model::JU06A));
        // Confirming PC with a failing send: the same.
        t.setState(60, 5, 28);
        t.setFlipDetectable(true);
        QVERIFY(t.select(a106));
        t.programChange(28);
        QCOMPARE(t.engine(), 106);
        QVERIFY(t.armed().has_value());
        QCOMPARE(t.remembered(106), 28);
        // When the send works the usual commit follows.
        ok = true;
        t.setState(60, 5, 28);
        QVERIFY(t.select(a106));
        t.programChange(28);
        QVERIFY(!t.armed().has_value());
        QCOMPARE(t.current().value(), a106);
        QCOMPARE(t.remembered(106), 0);
        QVERIFY(calls >= 4);
    }

    void signalOrderOnCtrlE()
    {
        EngineTracker t;
        t.setState(60, 5, 28);
        Recorder r(t);
        t.select(*PatchAddress::parse(QStringLiteral("106:1-1"), Model::JU06A));
        r.log.clear();
        t.toggleEngine();
        const QStringList want{
            QStringLiteral("engineChanged:106"),
            QStringLiteral("armedChanged"),
            QStringLiteral("rememberedChanged"),
            QStringLiteral("currentChanged"),
            QStringLiteral("sendRequested:106:1-1"),
        };
        QCOMPARE(r.log, want);
        QVERIFY(!r.wrongEngine);
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
