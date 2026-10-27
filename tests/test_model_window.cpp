// T8.3: the main window per model. The JP-08 and JU-06A catalogues, search, grid, display, rows and
// markers; the JU-06A engine control and its arm / prompt / hint / flip logic driven through the router
// with fake MIDI; F2 on any slot of the 64-patch models; per-device library state; the first-run hint
// with detection; Ctrl+E, Ctrl+Tab and Esc. Headless, temporary config directories.
#include "app/AppSettings.h"
#include "app/Cli.h"
#include "app/Paths.h"
#include "app/Screenshot.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "midi/DeviceRouter.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiLink.h"
#include "midi/PortHub.h"
#include "ui/BankGrid.h"
#include "ui/DeviceSwitcher.h"
#include "ui/DisplayWidget.h"
#include "ui/EngineControl.h"
#include "ui/GroupBar.h"
#include "ui/LedDisplayWidget.h"
#include "ui/MainWindow.h"
#include "ui/PanelButton.h"
#include "ui/PatchDelegate.h"
#include "ui/PatchListModel.h"
#include "ui/PatchListView.h"
#include "ui/SettingsDialog.h"
#include "ui/Shortcuts.h"
#include "ui/Theme.h"

#include <QAction>
#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStatusBar>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace d05;

namespace {

QString S(const char *t) { return QString::fromLatin1(t); }

PatchAddress jp(const char *t) { return *PatchAddress::parse(S(t), Model::JP08); }
PatchAddress ju(const char *t) { return *PatchAddress::parse(S(t), Model::JU06A); }
PatchAddress dd(const char *t) { return *PatchAddress::parse(S(t)); }

AppSettings::DeviceValues dev(int channel, const char *port = "Hapax", bool follow = false, bool enabled = true)
{
    AppSettings::DeviceValues v;
    v.enabled = enabled;
    v.outputPort = S(port);
    v.channel = channel;
    if (follow) {
        v.inputPort = S(port);
        v.follow = true;
    }
    return v;
}

class PaintCounter : public QObject {
public:
    int paints = 0;
    bool eventFilter(QObject *, QEvent *e) override
    {
        if (e->type() == QEvent::Paint)
            ++paints;
        return false;
    }
};

// A window on a router over a hub of fake backends: one forwarder port for the three devices.
// D-05 on channel 1, JP-08 on 3, JU-06A on 5.
struct Rig {
    QTemporaryDir dir;
    QString iniPath;
    PatchCatalog catalog;
    std::unique_ptr<Library> library;
    QList<FakeMidiBackend *> fakes;
    QStringList outs{S("Hapax Out")};
    QStringList ins{S("Hapax In")};
    std::unique_ptr<PortHub> hub;
    std::unique_ptr<DeviceRouter> router;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<MainWindow> w;

    explicit Rig(bool d05On = false, bool jp08On = false, bool ju06aOn = true, bool follow = true)
    {
        Paths::setOverride(dir.path());
        iniPath = dir.filePath(QStringLiteral("settings.ini"));
        library = std::make_unique<Library>(catalog, dir.filePath(QStringLiteral("library.json")));
        hub = std::make_unique<PortHub>([this]() -> MidiBackend * {
            auto *f = new FakeMidiBackend;
            f->setOutputPorts(outs, false);
            f->setInputPorts(ins, false);
            fakes << f;
            return f;
        });
        router = std::make_unique<DeviceRouter>(hub.get());
        settings = std::make_unique<AppSettings>(iniPath);
        w = std::make_unique<MainWindow>(catalog, *library, *router);
        w->setSystemLookup(false);
        w->setServices(settings.get());
        put(Model::D05, dev(1, "Hapax", follow, d05On));
        put(Model::JP08, dev(3, "Hapax", follow, jp08On));
        put(Model::JU06A, dev(5, "Hapax", follow, ju06aOn));
    }
    void put(Model m, const AppSettings::DeviceValues &v)
    {
        w->applyDevice(m, v);
        settings->setDevice(m, v);
    }
    ~Rig()
    {
        w.reset();
        settings.reset();
        router.reset();
        hub.reset();
        library.reset();
        Paths::setOverride(QString());
    }
    bool show()
    {
        w->resize(960, 560);
        w->show();
        w->activateWindow();
        return QTest::qWaitForWindowActive(w.get());
    }
    // Every message any fake backend has sent.
    QList<QByteArray> sent() const
    {
        QList<QByteArray> all;
        for (const FakeMidiBackend *f : fakes)
            all += f->sentMessages();
        return all;
    }
    void clearSent()
    {
        for (FakeMidiBackend *f : fakes)
            f->clearSent();
    }
    // A message arriving on the open input.
    void inject(std::initializer_list<int> v)
    {
        QByteArray b;
        for (int x : v)
            b.append(char(x));
        for (FakeMidiBackend *f : fakes) {
            if (f->isInputOpen()) {
                f->injectReceived(b);
                break;
            }
        }
        QCoreApplication::processEvents();
    }
    EngineTracker *tracker() const { return router->ju06aTracker(); }
    // Selects `a` in the list as the user would, and presses Enter there.
    void choose(const PatchAddress &a)
    {
        w->list()->setCurrentIndex(w->model()->indexOf(a));
        w->list()->setFocus();
        QTest::keyClick(w->list(), Qt::Key_Return);
    }
    QString context() const { return w->ledDisplay()->context(); }
};

}  // namespace

class TestModelWindow : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QApplication::setCursorFlashTime(0);  // as main() does: the caret timer is not a paint
        Theme::install(*qobject_cast<QApplication *>(qApp));
    }
    void init() { Theme::setSkin(QStringLiteral("d05")); }
    void cleanup() { Theme::setSkin(QStringLiteral("d05")); }

    // ------------------------------------------------------------------ SearchQuery ranges per name

    void rangesSayWhichNameTheyIndex()
    {
        using R = SearchQuery::Range;
        QCOMPARE(R{}.which, SearchQuery::NameIndex::Primary);
        const SearchQuery q(S("zebra takeoff"), Model::JU06A);
        QVector<R> r;
        QVERIFY(q.matches(ju("106:8-8"), S("Zebra Lead"), &r, S("Takeoff Pad")));
        QCOMPARE(r.size(), 2);
        QCOMPARE(r[0], (R{0, 5, SearchQuery::NameIndex::Primary}));    // zebra in the user name
        QCOMPARE(r[1], (R{0, 7, SearchQuery::NameIndex::Alternate}));  // takeoff in the factory name
        QVERIFY(!(r[0] == R{0, 5, SearchQuery::NameIndex::Alternate}));
        // A token found in both names is shown in the one the row leads with.
        QVERIFY(SearchQuery(S("pad"), Model::JP08).matches(jp("1-1"), S("Pad One"), &r, S("Warm Pad")));
        QCOMPARE(r, (QVector<R>{{0, 3, SearchQuery::NameIndex::Primary}}));
        // The D-05 query only ever yields Primary ranges (no alternate name is passed).
        QVERIFY(SearchQuery(S("brass")).matches(dd("P1-35"), S("Slap Brass"), &r));
        QCOMPARE(r, (QVector<R>{{5, 5}}));
    }

    // ------------------------------------------------------------------ the JP-08 window

    void jp08ShowsItsCatalogueSearchAndGrid()
    {
        Rig r(false, true, false);
        QVERIFY(r.show());
        QCOMPARE(r.w->currentDevice(), Model::JP08);
        QCOMPARE(Theme::skin().id, QStringLiteral("jp08"));
        QCOMPARE(r.w->model()->rowCount(), 64);
        QCOMPARE(r.w->model()->deviceModel(), Model::JP08);
        QCOMPARE(r.w->model()->row(0).address, jp("1-1"));
        QCOMPARE(r.w->model()->row(63).address, jp("8-8"));
        QCOMPARE(r.w->model()->row(0).name, S("NEG Sync"));
        // Group bar and engine control are not part of this skin: the slot is empty.
        QVERIFY(!r.w->groupBar()->isVisible());
        QVERIFY(!r.w->engineControl()->isVisible());
        QVERIFY(r.w->list()->isVisible() && r.w->bankGrid()->isVisible() && r.w->searchField()->isVisible());
        QCOMPARE(r.w->bankGrid()->deviceModel(), Model::JP08);
        // Search: a bank, an exact address, text.
        r.w->searchField()->setText(S("4"));
        QCOMPARE(r.w->model()->rowCount(), 8);
        r.w->searchField()->setText(S("45"));
        QCOMPARE(r.w->model()->rowCount(), 1);
        QCOMPARE(r.w->model()->row(0).address, jp("4-5"));
        QCOMPARE(r.w->ledDisplay()->context(), S("1 MATCH"));
        r.w->searchField()->setText(S("brass"));
        QVERIFY(r.w->model()->rowCount() > 1);
        QCOMPARE(r.w->ledDisplay()->context(), S("%1 MATCHES").arg(r.w->model()->rowCount()));
        r.w->searchField()->setText(S("zzzz"));
        QCOMPARE(r.w->ledDisplay()->context(), S("NO MATCHES"));
        r.w->searchField()->clear();
        QCOMPARE(r.w->ledDisplay()->context(), Theme::skin().idleContext);
        // The D-05's address forms are text here.
        r.w->searchField()->setText(S("P3-45"));
        QCOMPARE(r.w->model()->rowCount(), 0);
    }

    void jp08SelectionSendsOneProgramChangeAndFollowsTheDisplay()
    {
        Rig r(false, true, false);
        QVERIFY(r.show());
        r.w->setCurrent(jp("3-5"));
        QCOMPARE(r.w->ledDisplay()->readout(), S("35"));
        QCOMPARE(r.w->ledDisplay()->name(), S("Lo Brass"));
        QVERIFY(r.w->bankGrid()->bankLit(3) && r.w->bankGrid()->numberLit(5));
        QVERIFY(r.sent().isEmpty());  // restoring sends nothing
        // Highlight alone sends nothing (auto-send is off); Enter sends one Program Change.
        r.w->list()->setFocus();
        r.w->list()->setCurrentIndex(r.w->model()->indexOf(jp("4-1")));
        QVERIFY(r.sent().isEmpty());
        QTest::keyClick(r.w->list(), Qt::Key_Return);
        QCOMPARE(r.sent(), (QList<QByteArray>{QByteArray::fromHex("c218")}));  // channel 3, program 24
        QCOMPARE(r.w->currentPatch().value(), jp("4-1"));
        QCOMPARE(r.w->ledDisplay()->readout(), S("41"));
        QCOMPARE(r.w->ledDisplay()->context(), S("SENT"));
        QTRY_COMPARE(r.w->ledDisplay()->context(), Theme::skin().idleContext);  // 400 ms later
        QCOMPARE(r.library->recents(Model::JP08).size(), 1);
    }

    void jp08GridEntryAndPrevNext()
    {
        Rig r(false, true, false);
        QVERIFY(r.show());
        r.w->setCurrent(jp("8-8"));
        r.clearSent();
        QTest::keyClick(r.w.get(), Qt::Key_PageDown);  // clamped at the end: the same patch, sent again
        QCOMPARE(r.sent().size(), 1);
        QCOMPARE(r.w->currentPatch().value(), jp("8-8"));
        r.clearSent();
        QTest::keyClick(r.w.get(), Qt::Key_PageUp);
        QCOMPARE(r.w->currentPatch().value(), jp("8-7"));
        // Panel-style entry: bank, then number; the LED readout echoes it.
        r.clearSent();
        r.w->beginGridEntry();
        QCOMPARE(r.w->ledDisplay()->readout(), S("__"));
        QCOMPARE(r.w->ledDisplay()->name(), QString());
        r.w->bankGrid()->pressBank(2);
        QCOMPARE(r.w->ledDisplay()->readout(), S("2_"));
        r.w->bankGrid()->pressNumber(6);
        QCOMPARE(r.w->currentPatch().value(), jp("2-6"));
        QCOMPARE(r.sent(), (QList<QByteArray>{QByteArray::fromHex("c20d")}));  // program 13
        QCOMPARE(r.w->ledDisplay()->readout(), S("26"));
        // A number click while idle sends in the current bank.
        r.clearSent();
        r.w->bankGrid()->pressNumber(1);
        QCOMPARE(r.w->currentPatch().value(), jp("2-1"));
    }

    void renamingAnyJp08SlotShowsBothNames()
    {
        Rig r(false, true, false);
        QVERIFY(r.show());
        r.w->list()->setFocus();
        r.w->list()->setCurrentIndex(r.w->model()->indexOf(jp("3-4")));
        QTest::keyClick(r.w.get(), Qt::Key_F2);  // a factory slot is renameable here
        QVERIFY(r.w->list()->indexWidget(r.w->model()->indexOf(jp("3-4"))) != nullptr);
        auto *editor = r.w->list()->findChild<QLineEdit *>(S("nameEditor"));
        QVERIFY(editor);
        QCOMPARE(editor->placeholderText(), S("Mellow Strings"));  // the factory name
        QVERIFY(editor->text().isEmpty());
        QTest::keyClicks(editor, S("CN Strings Lift"));
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_COMPARE(r.library->catalog(Model::JP08).userName(jp("3-4")), S("CN Strings Lift"));
        const PatchListModel::Row &row = r.w->model()->row(r.w->model()->rowOf(jp("3-4")));
        QCOMPARE(row.name, S("CN Strings Lift"));
        QCOMPARE(row.userName, S("CN Strings Lift"));
        QCOMPARE(row.factory, S("Mellow Strings"));
        QCOMPARE(r.w->model()->data(r.w->model()->indexOf(jp("3-4")), Qt::ToolTipRole).toString(),
                 S("Factory name: Mellow Strings"));
        // A slot that was not renamed has no tooltip, and the display shows the user name only.
        QVERIFY(r.w->model()->data(r.w->model()->indexOf(jp("3-5")), Qt::ToolTipRole).toString().isEmpty());
        r.w->setCurrent(jp("3-4"));
        QCOMPARE(r.w->ledDisplay()->name(), S("CN Strings Lift"));
        // Both names are searched; the highlight lands under the one that matched.
        r.w->searchField()->setText(S("mellow"));
        int found = r.w->model()->rowOf(jp("3-4"));
        QVERIFY(found >= 0);
        QCOMPARE(r.w->model()->row(found).ranges.size(), 1);
        QCOMPARE(r.w->model()->row(found).ranges[0].which, SearchQuery::NameIndex::Alternate);
        r.w->searchField()->setText(S("lift"));
        found = r.w->model()->rowOf(jp("3-4"));
        QCOMPARE(r.w->model()->rowCount(), 1);
        QCOMPARE(r.w->model()->row(found).ranges[0].which, SearchQuery::NameIndex::Primary);
        // Committing an empty text removes the user name; the factory name returns.
        r.w->searchField()->clear();
        QVERIFY(r.library->setUserName(jp("3-4"), QString()));
        QCOMPARE(r.w->model()->row(r.w->model()->rowOf(jp("3-4"))).name, S("Mellow Strings"));
        QVERIFY(r.w->model()->row(r.w->model()->rowOf(jp("3-4"))).factory.isEmpty());
    }

    void d05StillRefusesPresetRenames()
    {
        Rig r(true, false, false);
        QVERIFY(r.show());
        r.w->list()->setFocus();
        r.w->list()->setCurrentIndex(r.w->model()->indexOf(dd("P1-11")));
        QTest::keyClick(r.w.get(), Qt::Key_F2);
        QCOMPARE(r.w->message(), S("Only user slots can be renamed"));
        r.w->list()->setCurrentIndex(r.w->model()->indexOf(dd("U1-11")));
        QTest::keyClick(r.w.get(), Qt::Key_F2);
        auto *editor = r.w->list()->findChild<QLineEdit *>(S("nameEditor"));
        QVERIFY(editor);
        QVERIFY(editor->placeholderText().isEmpty());  // no factory name on a D-05 user slot
    }

    // ------------------------------------------------------------------ the JU-06A window

    void ju06aShowsBothEnginesWithTagsAndTheEngineControl()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        QCOMPARE(r.w->currentDevice(), Model::JU06A);
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-60"));
        QCOMPARE(r.w->model()->rowCount(), 128);
        QCOMPARE(r.w->model()->row(0).address, ju("60:1-1"));
        QCOMPARE(r.w->model()->row(64).address, ju("106:1-1"));
        QVERIFY(r.w->engineControl()->isVisible());
        QVERIFY(!r.w->groupBar()->isVisible());
        QVERIFY(r.w->engineControl()->lit(60) && !r.w->engineControl()->lit(106));
        QCOMPARE(r.w->engineControl()->geometry().topLeft(), r.w->groupBar()->geometry().topLeft());
        QCOMPARE(r.w->model()->believedEngine(), 60);
        // The believed patch: 60:4-5, the other engine's remembered 106:7-1 is marked.
        QCOMPARE(r.w->currentPatch().value(), ju("60:4-5"));
        QCOMPARE(r.w->ledDisplay()->readout(), S("4-5"));
        QCOMPARE(r.w->ledDisplay()->name(), S("LD Silo Lead"));
        QCOMPARE(r.w->model()->otherRemembered().value(), ju("106:7-1"));
        QVERIFY(r.w->model()->hasCurrent());
        QCOMPARE(r.w->model()->current(), ju("60:4-5"));
        QCOMPARE(r.w->ledDisplay()->context(), S("JU-06A  JUNO-60"));
        // A bare 45 matches both engines; 106 filters.
        r.w->searchField()->setText(S("45"));
        QCOMPARE(r.w->model()->rowCount(), 2);
        r.w->searchField()->setText(S("106:45"));
        QCOMPARE(r.w->model()->rowCount(), 1);
        r.w->searchField()->clear();
        // The grid addresses the believed engine: a number click sends there.
        r.clearSent();
        r.w->bankGrid()->pressNumber(6);
        QCOMPARE(r.w->currentPatch().value(), ju("60:4-6"));
        QCOMPARE(r.sent(), (QList<QByteArray>{QByteArray::fromHex("c41d")}));  // channel 5, program 29
    }

    void aSelectionOfTheOtherEngineArmsAndSendsNothing()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        r.clearSent();
        r.choose(ju("106:1-1"));
        QVERIFY(r.sent().isEmpty());
        QCOMPARE(r.tracker()->armed().value(), ju("106:1-1"));
        QCOMPARE(r.w->model()->armed().value(), ju("106:1-1"));
        // Input open, follow on and 106's remembered program known: the unit can confirm.
        QCOMPARE(r.context(), S("FLIP SWITCH TO 106"));
        QCOMPARE(r.w->ledDisplay()->tone(), LedDisplayWidget::Tone::Lit);
        QVERIFY(r.w->engineControl()->armedLens(106));
        QVERIFY(r.w->engineControl()->lit(60));
        QCOMPARE(r.w->currentPatch().value(), ju("60:4-5"));  // the unit has not changed
        QCOMPARE(r.w->ledDisplay()->readout(), S("4-5"));
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-60"));
        QCOMPARE(r.w->highlighted().value(), ju("106:1-1"));  // the highlight stays on the armed row
        // A click on the row does the same as Enter, and Recents is untouched (nothing was sent).
        QVERIFY(r.library->recents(Model::JU06A).isEmpty());
        // Esc ends the arm and the prompt and goes no further (the search keeps its text).
        r.w->searchField()->setText(S("brass"));
        r.w->searchField()->setFocus();
        r.choose(ju("106:1-1"));
        QVERIFY(r.tracker()->armed());
        QTest::keyClick(r.w.get(), Qt::Key_Escape);
        QVERIFY(!r.tracker()->armed());
        QCOMPARE(r.w->searchField()->text(), S("brass"));
        QVERIFY(!r.w->model()->armed());
        QVERIFY(!r.w->engineControl()->armedLens(106));
        QVERIFY(r.context().contains(S("MATCH")));
        QTest::keyClick(r.w.get(), Qt::Key_Escape);  // now as before: clears the search
        QVERIFY(r.w->searchField()->text().isEmpty());
        QVERIFY(r.sent().isEmpty());
    }

    void thePromptSaysCtrlEWhenTheUnitCannotConfirm()
    {
        Rig r(false, false, true, false);  // no input: follow off
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        r.choose(ju("106:1-1"));
#ifdef Q_OS_MACOS
        QCOMPARE(r.context(), S("FLIP TO 106, THEN CMD+E"));
#else
        QCOMPARE(r.context(), S("FLIP TO 106, THEN CTRL+E"));
#endif
        // The other engine's program unknown: the same text even with an input path.
        Rig r2;
        r2.router->setJu06aState(60, 28, -1);
        QVERIFY(r2.show());
        r2.choose(ju("106:1-1"));
        QVERIFY(r2.context().startsWith(S("FLIP TO 106, THEN")));
    }

    void ctrlEFlipsSendsTheArmedPatchAndSwitchesTheSubSkin()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        r.choose(ju("106:1-1"));
        r.clearSent();
        QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
        QCOMPARE(r.tracker()->engine(), 106);
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-106"));
        QVERIFY(!r.tracker()->armed());
        QCOMPARE(r.sent(), (QList<QByteArray>{QByteArray::fromHex("c400")}));  // channel 5, program 0
        QCOMPARE(r.w->currentPatch().value(), ju("106:1-1"));
        QCOMPARE(r.w->ledDisplay()->readout(), S("1-1"));
        QCOMPARE(r.w->ledDisplay()->name(), S("Brass"));
        QVERIFY(r.w->engineControl()->lit(106) && !r.w->engineControl()->lit(60));
        QCOMPARE(r.w->model()->believedEngine(), 106);
        QCOMPARE(r.w->model()->otherRemembered().value(), ju("60:4-5"));
        QCOMPARE(r.w->bankGrid()->deviceEngine(), 106);
        QCOMPARE(r.context(), S("SENT"));
        // Persisted at quit: the believed engine and both remembered programs.
        r.w->close();
        const AppSettings saved(r.iniPath);
        QCOMPARE(saved.engine(), 106);
        QCOMPARE(saved.remembered(106).value(), 0);
        QCOMPARE(saved.remembered(60).value(), 28);
    }

    void ctrlEWithNothingArmedShowsTheRememberedPatchOrNone()
    {
        Rig r;
        r.router->setJu06aState(60, 28, -1);  // 106 shows --- until something is sent there
        QVERIFY(r.show());
        r.clearSent();
        QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
        QCOMPARE(r.tracker()->engine(), 106);
        QVERIFY(r.sent().isEmpty());  // a flip is a statement about the unit, never a send
        QVERIFY(!r.w->currentPatch());
        QCOMPARE(r.w->ledDisplay()->readout(), S("---"));
        QCOMPARE(r.w->ledDisplay()->name(), QString());
        QVERIFY(!r.w->model()->hasCurrent());
        QVERIFY(!r.w->bankGrid()->bankLit(4) && !r.w->bankGrid()->numberLit(5));
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-106"));
        // PREV / NEXT with no patch select 1-1 of the believed engine.
        QTest::keyClick(r.w.get(), Qt::Key_PageDown);
        QCOMPARE(r.w->currentPatch().value(), ju("106:1-1"));
        // Back to 60: its remembered patch returns.
        QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
        QCOMPARE(r.w->currentPatch().value(), ju("60:4-5"));
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-60"));
    }

    void theEngineButtonsToggleAndTheLitOneDoesNothing()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        EngineControl *c = r.w->engineControl();
        QSignalSpy spy(c, &EngineControl::engineRequested);
        const auto press = [&](int engine) {
            const QPoint p = c->faceOf(EngineControl::slotOf(engine)) + QPoint(20, 10);
            QTest::mouseClick(c, Qt::LeftButton, Qt::NoModifier, p);
        };
        press(60);  // the lit one
        QCOMPARE(spy.count(), 0);
        QCOMPARE(r.tracker()->engine(), 60);
        press(106);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(r.tracker()->engine(), 106);
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-106"));
        QVERIFY(c->lit(106));
        // Keyboard: Right moves the cursor, Space presses.
        c->setFocus();
        QTest::keyClick(c, Qt::Key_Left);
        QTest::keyClick(c, Qt::Key_Space);
        QCOMPARE(r.tracker()->engine(), 60);
        // Tooltips name the key.
        QVERIFY(EngineControl::captionLine2().contains(S("changes the belief, never the unit.")));
        QCOMPARE(EngineControl::captionLine1(), S("The app's belief about the 60/106 switch."));
        QVERIFY(c->isEnabled());
    }

    void anUnpromptedProgramChangeHintsAndCtrlEUndoesIt()
    {
        Rig r;
        r.router->setJu06aState(60, 0, 28);  // 60 shows 1-1, 106 shows 4-5
        QVERIFY(r.show());
        // The unit's own flip re-sends 106's 4-5 while the app believes 60: followed as a panel press
        // on 60, with a hint that the switch may have moved.
        r.inject({0xC4, 28});
        QCOMPARE(r.tracker()->hint(), 106);
        QCOMPARE(r.w->currentPatch().value(), ju("60:4-5"));
#ifdef Q_OS_MACOS
        QCOMPARE(r.context(), S("FLIPPED TO 106? CMD+E"));
#else
        QCOMPARE(r.context(), S("FLIPPED TO 106? CTRL+E"));
#endif
        QCOMPARE(r.w->ledDisplay()->tone(), LedDisplayWidget::Tone::Primary);
        QVERIFY(r.w->engineControl()->armedLens(106));
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-60"));  // the app never changes engine by itself
        QVERIFY(r.sent().isEmpty());
        // Ctrl+E answers it: the follow is undone, the engine is 106 with 4-5.
        QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
        QCOMPARE(r.tracker()->engine(), 106);
        QCOMPARE(r.tracker()->remembered(60), 0);
        QCOMPARE(r.w->currentPatch().value(), ju("106:4-5"));
        QCOMPARE(r.tracker()->hint(), 0);
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-106"));
        // Esc also ends a hint.
        r.router->setJu06aState(60, 0, 28);
        r.inject({0xC4, 28});
        QCOMPARE(r.tracker()->hint(), 106);
        QTest::keyClick(r.w.get(), Qt::Key_Escape);
        QCOMPARE(r.tracker()->hint(), 0);
        QVERIFY(!r.context().startsWith(S("FLIPPED")));
    }

    void aFlipConfirmedByTheUnitFulfilsTheArm()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        r.choose(ju("106:1-1"));
        r.clearSent();
        r.inject({0xC4, 48});  // the unit re-sends 106's remembered 7-1 after the flip
        QCOMPARE(r.tracker()->engine(), 106);
        QVERIFY(!r.tracker()->armed());
        QCOMPARE(r.sent(), (QList<QByteArray>{QByteArray::fromHex("c400")}));
        QCOMPARE(r.w->currentPatch().value(), ju("106:1-1"));
        QCOMPARE(Theme::skin().id, QStringLiteral("ju06a-106"));
    }

    void aFailedSendIsNeverShownAsSent()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        for (FakeMidiBackend *f : r.fakes)
            f->setFailSend(true);
        r.choose(ju("60:5-1"));
        QCOMPARE(r.w->currentPatch().value(), ju("60:4-5"));  // unchanged
        QVERIFY(r.w->message().startsWith(S("Not sent")));
        QVERIFY(r.context() != S("SENT"));
        QVERIFY(!r.w->display()->sentActive());
        QVERIFY(r.library->recents(Model::JU06A).isEmpty());
        // The armed confirmation with a failing wire keeps the arm.
        r.choose(ju("106:1-1"));
        QVERIFY(r.tracker()->armed());
        QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
        QVERIFY(r.tracker()->armed());  // not sent: still armed
        QCOMPARE(r.library->recents(Model::JU06A).size(), 0);
        QVERIFY(r.context() != S("SENT"));
    }

    void ju06aNextAndPrevStayWithinTheBelievedEngine()
    {
        Rig r;
        r.router->setJu06aState(106, 63, 63);
        QVERIFY(r.show());
        QCOMPARE(r.w->currentPatch().value(), ju("106:8-8"));
        r.clearSent();
        QTest::keyClick(r.w.get(), Qt::Key_PageDown);  // clamped: 106:8-8 again
        QCOMPARE(r.w->currentPatch().value(), ju("106:8-8"));
        QCOMPARE(r.sent().size(), 1);
        r.router->setJu06aState(106, 63, 0);
        QTest::keyClick(r.w.get(), Qt::Key_PageUp);  // 106:1-1 does not step back into the 60 engine
        QCOMPARE(r.w->currentPatch().value(), ju("106:1-1"));
        QTest::keyClick(r.w.get(), Qt::Key_PageUp);
        QCOMPARE(r.w->currentPatch().value(), ju("106:1-1"));
    }

    void rowsCarryTheEngineTagAndTheMarkers()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        QTest::qWait(30);
        const QImage plain = r.w->list()->viewport()->grab().toImage();
        // The remembered marker is a dark slot in the LED column of 106:7-1; arming 106:1-1 draws the outline.
        r.w->list()->scrollTo(r.w->model()->indexOf(ju("106:7-1")), QAbstractItemView::PositionAtTop);
        QTest::qWait(30);
        const QRect row = r.w->list()->visualRect(r.w->model()->indexOf(ju("106:7-1")));
        QVERIFY(!row.isEmpty());
        const QImage marked = r.w->list()->viewport()->grab().toImage();
        const QColor off = Theme::colors().ledOff;
        QCOMPARE(QColor(marked.pixelColor(row.x() + 14, row.y() + 14)), off);
        // The tag: framed for the believed engine, a dim number for the other (no frame).
        const QColor border = Theme::colors().tagBorder;
        r.w->list()->scrollTo(r.w->model()->indexOf(ju("60:1-1")), QAbstractItemView::PositionAtTop);
        QTest::qWait(30);
        const QRect r60 = r.w->list()->visualRect(r.w->model()->indexOf(ju("60:1-1")));
        const QImage top = r.w->list()->viewport()->grab().toImage();
        const int w = r.w->list()->viewport()->width();
        QCOMPARE(QColor(top.pixelColor(w - 58, r60.y() + 6)), border);
        r.w->list()->scrollTo(r.w->model()->indexOf(ju("106:1-1")), QAbstractItemView::PositionAtTop);
        QTest::qWait(30);
        const QRect r106 = r.w->list()->visualRect(r.w->model()->indexOf(ju("106:1-1")));
        const QImage other = r.w->list()->viewport()->grab().toImage();
        QVERIFY(QColor(other.pixelColor(w - 58, r106.y() + 6)) != border);
        Q_UNUSED(plain);
        // Arm 106:1-1: the lit outline appears in its LED column (1 px frame at x 9, y 11).
        r.choose(ju("106:1-1"));
        QTest::qWait(30);
        const QImage armed = r.w->list()->viewport()->grab().toImage();
        QCOMPARE(QColor(armed.pixelColor(9, r106.y() + 11)), Theme::colors().ledOn);
        QCOMPARE(QColor(armed.pixelColor(14, r106.y() + 13)), Theme::colors().ledOff);
    }

    void anEngineToggleRepaintsTheWindowOnce()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        QTest::qWait(80);
        const auto toggleTo = [&](const char *skin) {
            PaintCounter win, list, button, plate, leds, status;
            r.w->installEventFilter(&win);
            r.w->list()->viewport()->installEventFilter(&list);
            r.w->prevButton()->installEventFilter(&button);
            r.w->nameplate()->installEventFilter(&plate);
            r.w->ledDisplay()->installEventFilter(&leds);
            r.w->statusBar()->installEventFilter(&status);
            QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
            QTest::qWait(100);
            r.w->removeEventFilter(&win);
            r.w->list()->viewport()->removeEventFilter(&list);
            r.w->prevButton()->removeEventFilter(&button);
            r.w->nameplate()->removeEventFilter(&plate);
            r.w->ledDisplay()->removeEventFilter(&leds);
            r.w->statusBar()->removeEventFilter(&status);
            QCOMPARE(Theme::skin().id, QString::fromLatin1(skin));
            QCOMPARE(win.paints, 1);
            QCOMPARE(list.paints, 1);
            QCOMPARE(button.paints, 1);
            QCOMPARE(plate.paints, 1);
            QCOMPARE(leds.paints, 1);
        };
        toggleTo("ju06a-106");
        toggleTo("ju06a-60");
    }

    // ------------------------------------------------------------------ switching with content

    void aDeviceSwitchSwapsTheCatalogueGridAndGroupBarTogether()
    {
        Rig r(true, true, true);
        QVERIFY(r.show());
        QCOMPARE(r.w->model()->rowCount(), 896);
        QVERIFY(r.w->groupBar()->isVisible());
        QVERIFY(r.w->list()->isVisible() && r.w->bankGrid()->isVisible());
        r.w->switchDevice(Model::JP08);
        QCOMPARE(r.w->model()->rowCount(), 64);
        QVERIFY(!r.w->groupBar()->isVisible());
        QVERIFY(!r.w->engineControl()->isVisible());
        QVERIFY(r.w->list()->isVisible() && r.w->bankGrid()->isVisible() && r.w->prevButton()->isVisible());
        QCOMPARE(r.w->searchField()->placeholderText(), Theme::skin().searchPlaceholder);
        r.w->switchDevice(Model::JU06A);
        QCOMPARE(r.w->model()->rowCount(), 128);
        QVERIFY(r.w->engineControl()->isVisible());
        r.w->switchDevice(Model::D05);
        QCOMPARE(r.w->model()->rowCount(), 896);
        QVERIFY(r.w->groupBar()->isVisible());
        QVERIFY(!r.w->engineControl()->isVisible());
        QVERIFY(r.sent().isEmpty());  // a switch sends nothing
    }

    void searchAndViewAreKeptPerDevice()
    {
        Rig r(true, true, false);
        QVERIFY(r.show());
        r.w->searchField()->setText(S("brass"));
        r.w->viewButton(PatchListModel::View::Favourites)->setChecked(true);
        r.w->switchDevice(Model::JP08);
        QVERIFY(r.w->searchField()->text().isEmpty());
        QCOMPARE(r.w->model()->view(), PatchListModel::View::All);
        QCOMPARE(r.w->model()->rowCount(), 64);
        QVERIFY(r.w->viewButton(PatchListModel::View::All)->isChecked());
        r.w->searchField()->setText(S("lead"));
        r.w->switchDevice(Model::D05);
        QCOMPARE(r.w->searchField()->text(), S("brass"));
        QCOMPARE(r.w->model()->view(), PatchListModel::View::Favourites);
        QVERIFY(r.w->viewButton(PatchListModel::View::Favourites)->isChecked());
        r.w->switchDevice(Model::JP08);
        QCOMPARE(r.w->searchField()->text(), S("lead"));
    }

    void favouritesRecentsAndNamesArePerDevice()
    {
        Rig r(true, true, true);
        QVERIFY(r.show());
        r.w->switchDevice(Model::JP08);
        r.w->toggleFavorite(jp("2-2"));
        r.choose(jp("2-3"));  // a send: a Recents entry for the JP-08 only
        QVERIFY(r.library->isFavorite(jp("2-2")));
        QCOMPARE(r.library->recents(Model::JP08).size(), 1);
        QCOMPARE(r.library->recents(Model::D05).size(), 0);
        r.w->viewButton(PatchListModel::View::Favourites)->setChecked(true);
        QCOMPARE(r.w->model()->rowCount(), 1);
        QCOMPARE(r.w->model()->row(0).address, jp("2-2"));
        r.w->viewButton(PatchListModel::View::Recents)->setChecked(true);
        QCOMPARE(r.w->model()->rowCount(), 1);
        QCOMPARE(r.w->model()->row(0).address, jp("2-3"));
        // Clear Recents clears the shown device's list only.
        r.library->recordSend(dd("P1-11"));
        r.w->confirmClearRecents();
        QVERIFY(r.w->clearRecentsBox());
        r.w->clearRecentsBox()->findChild<QPushButton *>(S("clearButton"))->click();
        QTRY_VERIFY(r.library->recents(Model::JP08).isEmpty());
        QCOMPARE(r.library->recents(Model::D05).size(), 1);
        r.w->switchDevice(Model::D05);  // its own (empty) lists, view All
        QCOMPARE(r.w->model()->view(), PatchListModel::View::All);
        r.w->viewButton(PatchListModel::View::Favourites)->setChecked(true);
        QCOMPARE(r.w->model()->rowCount(), 0);
        // Everything is on disk in version 2 form after a flush.
        QVERIFY(r.library->flush());
        Library again(r.catalog, r.dir.filePath(QStringLiteral("library.json")));
        again.catalog(Model::JP08);
        again.catalog(Model::JU06A);
        again.load();
        QVERIFY(again.isFavorite(jp("2-2")));
        QCOMPARE(again.recents(Model::JP08).size(), 0);  // cleared above
        QCOMPARE(again.recents(Model::D05).size(), 1);
    }

    void aJu06aSlotCanBeRenamedAndKeepsItsFactoryName()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        r.w->list()->setFocus();
        r.w->list()->setCurrentIndex(r.w->model()->indexOf(ju("106:5-2")));
        QTest::keyClick(r.w.get(), Qt::Key_F2);
        auto *editor = r.w->list()->findChild<QLineEdit *>(S("nameEditor"));
        QVERIFY(editor);
        QCOMPARE(editor->placeholderText(), S("Junoa Bass"));
        QTest::keyClicks(editor, S("CN Sub Drone"));
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_COMPARE(r.library->catalog(Model::JU06A).userName(ju("106:5-2")), S("CN Sub Drone"));
        const PatchListModel::Row &row = r.w->model()->row(r.w->model()->rowOf(ju("106:5-2")));
        QCOMPARE(row.factory, S("Junoa Bass"));
        // 18 printable ASCII characters, as everywhere.
        QVERIFY(r.library->setUserName(ju("60:1-1"), S("0123456789012345678901234")));
        QCOMPARE(r.library->catalog(Model::JU06A).userName(ju("60:1-1")).size(), 18);
        // The D-05's catalogue is untouched.
        QVERIFY(r.catalog.userName(dd("U1-11")).isEmpty());
    }

    void thePatchOfEachDeviceIsSavedAndRestoredWithoutSending()
    {
        Rig r(false, true, false);
        QVERIFY(r.show());
        r.choose(jp("6-4"));
        r.w->close();
        QCOMPARE(AppSettings(r.iniPath).lastPatch(Model::JP08).value(), jp("6-4"));
        QCOMPARE(AppSettings(r.iniPath).currentDevice().value_or(Model::D05), Model::JP08);
    }


    // ------------------------------------------------------------------ review fixes (T8.3)

    // One paint per widget per device switch, over a long varied sequence; hidden widgets paint 0.
    void aDeviceSwitchPaintsEachWidgetOnceInAnyOrder()
    {
        Rig r(true, true, true);
        QVERIFY(r.show());
        QTest::qWait(80);
        const Model seq[] = {Model::JP08, Model::JU06A, Model::D05,   Model::JU06A, Model::JP08, Model::D05,
                             Model::JP08, Model::JU06A, Model::JP08,  Model::D05,   Model::JU06A, Model::D05};
        for (const Model to : seq) {
            PaintCounter win, list, prev, plate, leds, lcd, grid;
            QWidget *w = r.w.get();
            w->installEventFilter(&win);
            r.w->list()->viewport()->installEventFilter(&list);
            r.w->prevButton()->installEventFilter(&prev);
            r.w->nameplate()->installEventFilter(&plate);
            r.w->ledDisplay()->installEventFilter(&leds);
            r.w->display()->installEventFilter(&lcd);
            r.w->bankGrid()->installEventFilter(&grid);
            r.w->switchDevice(to);
            QTest::qWait(60);
            const bool led = to != Model::D05;
            const QString what = QString::number(int(to));
            QVERIFY2(win.paints == 1, qPrintable(S("window ") + what + S(": ") + QString::number(win.paints)));
            QVERIFY2(list.paints == 1, qPrintable(S("list ") + what + S(": ") + QString::number(list.paints)));
            QVERIFY2(prev.paints == 1, qPrintable(S("prev ") + what + S(": ") + QString::number(prev.paints)));
            QVERIFY2(plate.paints == 1, qPrintable(S("plate ") + what + S(": ") + QString::number(plate.paints)));
            QVERIFY2(grid.paints == 1, qPrintable(S("grid ") + what + S(": ") + QString::number(grid.paints)));
            QVERIFY2((led ? leds.paints : lcd.paints) == 1, qPrintable(S("display ") + what));
            QVERIFY2((led ? lcd.paints : leds.paints) == 0, qPrintable(S("hidden display ") + what));
            w->removeEventFilter(&win);
            r.w->list()->viewport()->removeEventFilter(&list);
            r.w->prevButton()->removeEventFilter(&prev);
            r.w->nameplate()->removeEventFilter(&plate);
            r.w->ledDisplay()->removeEventFilter(&leds);
            r.w->display()->removeEventFilter(&lcd);
            r.w->bankGrid()->removeEventFilter(&grid);
        }
    }

    void autoSendOnSendsNothingOnADeviceSwitch()
    {
        Rig r(true, true, true);
        QVERIFY(r.show());
        r.w->setAutoSend(true);
        r.clearSent();
        r.w->switchDevice(Model::JP08);
        r.w->switchDevice(Model::JU06A);
        r.w->switchDevice(Model::D05);
        QVERIFY(r.sent().isEmpty());
    }

    void renamingAJp08SlotLeavesTheSameIndexOfTheOtherModelsAlone()
    {
        Rig r(true, true, true);
        QVERIFY(r.show());
        r.w->switchDevice(Model::JP08);
        r.w->list()->setFocus();
        r.w->list()->setCurrentIndex(r.w->model()->indexOf(jp("3-4")));  // catalogue index 20
        QTest::keyClick(r.w.get(), Qt::Key_F2);
        auto *editor = r.w->list()->findChild<QLineEdit *>(S("nameEditor"));
        QVERIFY(editor);
        QTest::keyClicks(editor, S("CN Only Jp"));
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_COMPARE(r.library->catalog(Model::JP08).userName(jp("3-4")), S("CN Only Jp"));
        const int idx = jp("3-4").catalogueIndex();
        const auto untouched = [idx](Library &lib, const PatchCatalog &d05) {
            QVERIFY(d05.userName(*PatchAddress::fromCatalogueIndex(idx)).isEmpty());
            const PatchCatalog &ju = lib.catalog(Model::JU06A);
            QVERIFY(ju.userName(*PatchAddress::fromCatalogueIndex(Model::JU06A, idx)).isEmpty());
            QVERIFY(ju.userName(*PatchAddress::fromCatalogueIndex(Model::JU06A, idx % 64)).isEmpty());
            QVERIFY(ju.userName(*PatchAddress::fromCatalogueIndex(Model::JU06A, 64 + idx % 64)).isEmpty());
        };
        untouched(*r.library, r.catalog);
        // Saved and reloaded, through the Library.
        QVERIFY(r.library->flush());
        PatchCatalog cat2;
        Library lib2(cat2, r.dir.filePath(QStringLiteral("library.json")));
        lib2.load();
        QCOMPARE(lib2.catalog(Model::JP08).userName(jp("3-4")), S("CN Only Jp"));
        untouched(lib2, cat2);
    }

    void ctrlEDuringAGridEntryEndsTheEntryProperly()
    {
        Rig r;
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        r.w->bankGrid()->pressBank(2);
        QVERIFY(r.w->bankGrid()->stage() != BankGrid::Stage::Idle);
        QSignalSpy spy(r.w->bankGrid(), &BankGrid::entryChanged);
        QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
        QCOMPARE(r.tracker()->engine(), 106);
        QVERIFY(r.w->bankGrid()->stage() == BankGrid::Stage::Idle);
        QVERIFY(spy.count() >= 1);
        QVERIFY(r.w->bankGrid()->entryText().isEmpty());
    }

    // The JU-06A's link keeps only what was sent; the tracker also follows the panel. A resend
    // (reconnect) and TEST use the tracker's patch.
    void ju06aResendOnConnectUsesTheFollowedPatch()
    {
        Rig r(false, false, false, true);
        AppSettings::DeviceValues v = dev(5, "Hapax", true);
        v.resendOnConnect = true;
        r.router->setJu06aState(60, 0, -1);
        r.put(Model::JU06A, v);
        QVERIFY(r.show());
        r.clearSent();
        r.inject({0xC4, 28});  // the panel selects 4-5; the app followed
        QCOMPARE(r.tracker()->current().value(), ju("60:4-5"));
        r.clearSent();
        for (FakeMidiBackend *f : r.fakes)
            f->setOutputPorts(QStringList{}, true);  // unplugged
        QCoreApplication::processEvents();
        QVERIFY(!r.router->link(Model::JU06A)->isConnected());
        r.router->pollNow();
        r.outs = QStringList{S("Hapax Out")};
        for (FakeMidiBackend *f : r.fakes)
            f->setOutputPorts(r.outs, false);
        r.router->pollNow();
        QCOMPARE(r.sent(), (QList<QByteArray>{QByteArray::fromHex("c41c")}));
    }

    void settingsTestOnTheJu06aUsesTheTrackersPatch()
    {
        const auto press = [](Rig &r) {
            r.w->openSettings();
            SettingsDialog *d = r.w->settingsDialog();
            d->setShown(Model::JU06A);
            QVERIFY(d->testButton()->isEnabled());
            r.clearSent();
            QTest::mouseClick(d->testButton(), Qt::LeftButton);
            d->reject();
            QTRY_VERIFY(!r.w->settingsDialog());
        };
        // A followed panel press.
        {
            Rig r;
            r.router->setJu06aState(60, 0, 48);
            QVERIFY(r.show());
            r.inject({0xC4, 28});
            QCOMPARE(r.tracker()->current().value(), ju("60:4-5"));
            press(r);
            QVERIFY(r.sent().contains(QByteArray::fromHex("c41c")));
            QVERIFY(!r.sent().contains(QByteArray::fromHex("c400")));
        }
        // Ctrl+E into a remembered patch of the other engine: that patch is now the believed engine's.
        {
            Rig r;
            r.router->setJu06aState(60, 28, 48);
            QVERIFY(r.show());
            QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
            QCOMPARE(r.tracker()->current().value(), ju("106:7-1"));  // 48 = 6*8+0
            press(r);
            QCOMPARE(r.sent(), (QList<QByteArray>{QByteArray::fromHex("c430")}));
        }
        // No patch on the believed engine: TEST sends 1-1 of it (the TEST rule), and a resend nothing.
        {
            Rig r;
            r.router->setJu06aState(60, 28, -1);
            QVERIFY(r.show());
            QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
            QVERIFY(!r.tracker()->current());
            press(r);
            QCOMPARE(r.sent(), (QList<QByteArray>{QByteArray::fromHex("c400")}));
        }
    }

    // ------------------------------------------------------------------ first run: no ports named

    void aDisabledDeviceHasNoPortsInTheStatusBarAndNoOpenInput()
    {
        Rig r(false, false, false);  // all three have ports saved and follow on, none is enabled
        QVERIFY(r.show());
        for (const FakeMidiBackend *f : r.fakes) {
            QVERIFY(!f->isInputOpen());
            QVERIFY(!f->isOutputOpen());
        }
        QString all;
        for (const QLabel *l : r.w->statusBar()->findChildren<QLabel *>())
            if (l->isVisible())
                all += l->text() + QLatin1Char('|');
        QVERIFY2(!all.contains(S("Hapax")), qPrintable(all));
        QVERIFY2(all.contains(S("None")), qPrintable(all));
        // Turning the current device off again leaves the same.
        r.put(Model::D05, dev(1, "Hapax Out", true, true));
        r.put(Model::D05, dev(1, "Hapax Out", true, false));
        all.clear();
        for (const QLabel *l : r.w->statusBar()->findChildren<QLabel *>())
            if (l->isVisible())
                all += l->text() + QLatin1Char('|');
        QVERIFY2(!all.contains(S("Hapax")), qPrintable(all));
        for (const FakeMidiBackend *f : r.fakes)
            QVERIFY(!f->isInputOpen());
    }

    // ------------------------------------------------------------------ keys

    void ctrlETogglesOnlyOnTheJu06a()
    {
        Rig r(true, false, true);
        QVERIFY(r.show());
        QCOMPARE(r.w->currentDevice(), Model::D05);
        QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
        QCOMPARE(r.tracker()->engine(), 60);  // nothing happened
        QVERIFY(r.sent().isEmpty());
        r.w->switchDevice(Model::JU06A);
        QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
        QCOMPARE(r.tracker()->engine(), 106);
    }

    void deviceAndEngineKeysAreInTheTable()
    {
        Rig r(true, true, true);
        for (const char *name : {"actionNextDevice", "actionPreviousDevice", "actionToggleEngine"}) {
            QAction *a = r.w->action(S(name));
            QVERIFY2(a, name);
            QVERIFY2(!a->shortcut().isEmpty(), name);
        }
        QCOMPARE(r.w->action(S("actionToggleEngine"))->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_E));
        QVERIFY(r.show());
        QTest::keyClick(r.w.get(), Qt::Key_Tab, Qt::ControlModifier);
        QCOMPARE(r.w->currentDevice(), Model::JP08);
        QTest::keyClick(r.w.get(), Qt::Key_Tab, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(r.w->currentDevice(), Model::D05);
    }

    void escWhileArmedOnlyActsOnTheJu06a()
    {
        Rig r(true, false, true);
        r.router->setJu06aState(60, 28, 48);
        QVERIFY(r.show());
        r.w->switchDevice(Model::JU06A);
        r.choose(ju("106:1-1"));
        QVERIFY(r.tracker()->armed());
        r.w->switchDevice(Model::D05);  // a device switch clears the arm
        QVERIFY(!r.tracker()->armed());
    }

    // ------------------------------------------------------------------ first-run hint with detection

    void firstRunHintNamesTheUnitFoundOnUsb()
    {
        Rig r(false, false, false);
        QVERIFY(r.w->display()->line2().startsWith(S("OPEN SETTINGS")));
        QVERIFY(r.w->detectedModels().isEmpty());
        // A JU-06A shows up on the computer's USB; the port list is read again when the window is
        // activated (never on a timer), and the hint says so.
        r.outs = QStringList{S("JU-06A:JU-06A MIDI 1 28:0")};
        r.ins = r.outs;
        for (FakeMidiBackend *f : r.fakes) {
            f->setOutputPorts(r.outs, false);
            f->setInputPorts(r.ins, false);
        }
        QVERIFY(r.show());
        QCOMPARE(r.w->detectedModels(), (QList<Model>{Model::JU06A}));
        QCOMPARE(r.w->firstRunHint(), S("FOUND JU-06A ON USB: OPEN SETTINGS"));
        QCOMPARE(r.w->display()->line2().trimmed(), S("FOUND JU-06A ON USB: OPEN SETTINGS"));
        QVERIFY(r.w->display()->isVisible());  // the D-05 skin and its LCD
        // Two units.
        r.outs = QStringList{S("JU-06A:JU-06A MIDI 1 28:0"), S("Boutique:Boutique MIDI 1 24:0")};
        r.ins = r.outs;
        for (FakeMidiBackend *f : r.fakes) {
            f->setOutputPorts(r.outs, false);
            f->setInputPorts(r.ins, false);
        }
        QCoreApplication::sendEvent(r.w.get(), new QEvent(QEvent::WindowActivate));
        QCOMPARE(r.w->detectedModels().size(), 2);
        QCOMPARE(r.w->firstRunHint(), S("FOUND 2 UNITS ON USB: OPEN SETTINGS"));
        QVERIFY(r.w->display()->line2().startsWith(S("FOUND 2 UNITS ON USB: OPEN SETTINGS")));
        // Nothing found: version 1's hint.
        r.outs = QStringList{S("Hapax MIDI 1")};
        r.ins = r.outs;
        for (FakeMidiBackend *f : r.fakes) {
            f->setOutputPorts(r.outs, false);
            f->setInputPorts(r.ins, false);
        }
        QCoreApplication::sendEvent(r.w.get(), new QEvent(QEvent::WindowActivate));
        QCOMPARE(r.w->firstRunHint(), S("OPEN SETTINGS: ") + Shortcuts::settingsHintKey());
        QVERIFY(r.sent().isEmpty());
    }

    void thePollNeverReadsDetection()
    {
        Rig r(false, false, false);
        QVERIFY(!r.router->isPolling());  // no device enabled: no poll, and so no detection on a timer
    }

    void withADeviceEnabledTheHintIsVersion1s()
    {
        Rig r(true, false, false);  // D-05 enabled, no port saved
        r.put(Model::D05, dev(1, ""));
        QVERIFY(r.w->display()->line2().startsWith(S("OPEN SETTINGS")));
        QVERIFY(r.w->detectedModels().isEmpty());
    }

    // ------------------------------------------------------------------ the states the designer reviews

    // Every state of the LED display and the first-run line that no --screenshot-state names, rendered from
    // real window state. With D05_T83_SHOT_DIR set the grabs are written there (at the process's scale
    // factor, QT_SCALE_FACTOR=2 for 2x); without it they are only taken, which still exercises every path.
    void everyDisplayStateRenders()
    {
        const QString dir = qEnvironmentVariable("D05_T83_SHOT_DIR");
        const QString suffix = qEnvironmentVariable("D05_T83_SHOT_SUFFIX", QStringLiteral("1x"));
        const auto shot = [&](Rig &r, const char *name) {
            QTest::qWait(30);
            const QPixmap p = r.w->grab();
            QVERIFY(!p.isNull());
            if (!dir.isEmpty())
                QVERIFY(p.save(dir + QLatin1Char('/') + S(name) + QLatin1Char('@') + suffix + QStringLiteral(".png")));
        };
        {
            Rig r;  // JU-06A on 106 with nothing remembered on it: --- and a blank name
            r.router->setJu06aState(60, 28, -1);
            QVERIFY(r.show());
            QTest::keyClick(r.w.get(), Qt::Key_E, Qt::ControlModifier);
            QCOMPARE(r.w->ledDisplay()->readout(), S("---"));
            QTest::qWait(450);
            shot(r, "ju06a-no-patch");
            // Entry echo on three cells: awaiting the bank, then the number.
            r.w->beginGridEntry();
            QCOMPARE(r.w->ledDisplay()->readout(), S("_-_"));
            shot(r, "ju06a-entry-bank");
            r.w->bankGrid()->pressBank(4);
            QCOMPARE(r.w->ledDisplay()->readout(), S("4-_"));
            shot(r, "ju06a-entry-number");
            r.w->bankGrid()->cancelEntry();
            // The SENT flash, with the status LED lit.
            r.clearSent();
            r.w->bankGrid()->pressNumber(5);
            QCOMPARE(r.context(), S("SENT"));
            shot(r, "ju06a-sent");
        }
        {
            Rig r(false, false, true, false);  // no input: the prompt says to press Ctrl+E
            r.router->setJu06aState(60, 28, 48);
            QVERIFY(r.show());
            r.choose(ju("106:1-1"));
            QVERIFY(r.context().startsWith(S("FLIP TO 106, THEN")));
            shot(r, "ju06a-prompt-ctrle");
        }
        {
            Rig r(false, false, true);
            r.put(Model::JU06A, dev(5, "Nowhere", true));
            QVERIFY(r.show());
            QCOMPARE(r.context(), S("NO MIDI OUTPUT"));
            shot(r, "ju06a-no-output");
            r.put(Model::JU06A, dev(5, "", false));
            QVERIFY(r.context().startsWith(S("OPEN SETTINGS")));
            shot(r, "ju06a-open-settings");
        }
        {
            Rig r(false, true, false);
            QVERIFY(r.show());
            r.w->beginGridEntry();
            QCOMPARE(r.w->ledDisplay()->readout(), S("__"));
            shot(r, "jp08-entry-bank");
            r.w->bankGrid()->pressBank(7);
            QCOMPARE(r.w->ledDisplay()->readout(), S("7_"));
            shot(r, "jp08-entry-number");
            r.w->bankGrid()->cancelEntry();
            r.w->searchField()->setText(S("zzzz"));
            shot(r, "jp08-no-matches");
        }
        {
            Theme::setSkin(QStringLiteral("d05"));  // as the application starts
            Rig r(false, false, false);  // first run, nothing enabled: the LCD and the FOUND line
            r.outs = QStringList{S("JU-06A:JU-06A MIDI 1 28:0")};
            r.ins = r.outs;
            for (FakeMidiBackend *f : r.fakes) {
                f->setOutputPorts(r.outs, false);
                f->setInputPorts(r.ins, false);
            }
            QVERIFY(r.show());
            shot(r, "firstrun-found-one");
            r.outs = QStringList{S("JU-06A:JU-06A MIDI 1 28:0"), S("Boutique:Boutique MIDI 1 24:0")};
            r.ins = r.outs;
            for (FakeMidiBackend *f : r.fakes) {
                f->setOutputPorts(r.outs, false);
                f->setInputPorts(r.ins, false);
            }
            QCoreApplication::sendEvent(r.w.get(), new QEvent(QEvent::WindowActivate));
            shot(r, "firstrun-found-two");
        }
    }

    // ------------------------------------------------------------------ screenshots

    void sceneStatesComeFromRealWindowState()
    {
        QTemporaryDir out;
        QVERIFY(out.isValid());
        for (const char *state : {"jp08", "ju06a-60", "ju06a-106", "flip-prompt", "flip-hint"}) {
            CliOptions o;
            o.screenshotFile = out.filePath(S(state) + S(".png"));
            o.screenshotState = S(state);
            QCOMPARE(runScreenshot(o), 0);
            QImage img;
            QVERIFY(img.load(o.screenshotFile));
            Paths::setOverride(QString());
        }
    }
};

QTEST_MAIN(TestModelWindow)
#include "test_model_window.moc"
