// MainWindow and its parts (grid, group bar, model, delegate, nameplate) on the fake
// MIDI backend, headless under QT_QPA_PLATFORM=offscreen.
#include "app/Paths.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "core/SearchQuery.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiLink.h"
#include "ui/BankGrid.h"
#include "ui/DisplayWidget.h"
#include "ui/GroupBar.h"
#include "ui/MainWindow.h"
#include "ui/Nameplate.h"
#include "ui/PanelButton.h"
#include "ui/PatchDelegate.h"
#include "ui/PatchListModel.h"
#include "ui/PatchListView.h"
#include "ui/SectionRule.h"
#include "ui/Theme.h"

#include <QAction>
#include <QApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStatusBar>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>

// Counting operator new, to measure what the delegate allocates while painting.
namespace {
std::atomic<long> g_allocs{0};
std::atomic<bool> g_counting{false};
}  // namespace

void *operator new(std::size_t n)
{
    if (g_counting.load(std::memory_order_relaxed))
        g_allocs.fetch_add(1, std::memory_order_relaxed);
    if (void *p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

using namespace d05;

namespace {

PatchAddress addr(const char *t)
{
    return *PatchAddress::parse(QString::fromLatin1(t));
}

QString pad(const QString &s)
{
    return s.leftJustified(DisplayWidget::Columns, QLatin1Char(' '));
}

// A window on the fake backend, library in a temp dir, output port open (or not).
struct Rig {
    QTemporaryDir dir;
    PatchCatalog catalog;
    std::unique_ptr<Library> library;
    FakeMidiBackend backend;
    std::unique_ptr<MidiLink> link;
    std::unique_ptr<MainWindow> w;

    explicit Rig(bool connected = true, bool failOpen = false)
    {
        backend.setFailOpen(failOpen);
        Paths::setOverride(dir.path());
        library = std::make_unique<Library>(catalog, dir.filePath(QStringLiteral("library.json")));
        link = std::make_unique<MidiLink>(&backend);
        link->setChannel(1);
        link->setOutputPort(connected ? QStringLiteral("Fake D-05 In") : QStringLiteral("Not There"));
        w = std::make_unique<MainWindow>(catalog, *library, *link);
    }
    ~Rig()
    {
        w.reset();
        link.reset();
        library.reset();
        Paths::setOverride(QString());
    }

    bool show()
    {
        w->show();
        w->activateWindow();
        return QTest::qWaitForWindowActive(w.get());
    }

    const QList<QByteArray> &sent() const { return backend.sentMessages(); }
    // True when exactly one triplet for `a` on channel 1 has been sent.
    bool sentExactly(const PatchAddress &a) const
    {
        return sent() == MidiLink::patchMessages(a, 1);
    }
};

}  // namespace

class TestMainWindow : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Theme::install(*qobject_cast<QApplication *>(qApp)); }

    // ------------------------------------------------------------------ layout

    void constructsAtDesignSize()
    {
        Rig r;
        QCOMPARE(r.w->size(), QSize(960, 560));
        QCOMPARE(r.w->minimumSize(), QSize(800, 480));
        QCOMPARE(r.w->windowTitle(), QStringLiteral("D-05 Patch Switcher"));
        QVERIFY(r.show());
        // DESIGN 4.3 at 960 wide: p = 3, bezel 767 x 79 (1x), B = 139.
        const qreal dpr = r.w->devicePixelRatioF();
        if (dpr == 1.0) {
            QCOMPARE(r.w->display()->geometry(), QRect(16, 46, 767, 79));
            QCOMPARE(r.w->bandTop(), 139);
            QCOMPARE(r.w->searchField()->geometry(), QRect(434, 139 + 64, 960 - 450, 32));
            QCOMPARE(r.w->list()->geometry(), QRect(434, 139 + 104, 960 - 450, 560 - 22 - 139 - 116));
        }
        QCOMPARE(r.w->nameplate()->geometry(), QRect(16, 10, 360, 28));
        QVERIFY(r.w->searchField()->hasFocus());  // DESIGN 9: initial focus
    }

    void minimumSizeKeepsListUsable()
    {
        Rig r;
        r.w->resize(800, 480);
        QVERIFY(r.show());
        QCOMPARE(r.w->size(), QSize(800, 480));
        const QRect list = r.w->list()->geometry();
        QCOMPARE(list.x(), 434);
        QCOMPARE(list.width(), 350);
        QVERIFY(list.height() >= 203 - 2);  // DESIGN 4.3: 350 x 203 at 1x and 2x
        QVERIFY(list.bottom() < r.w->height() - 22);
        // Wide window: the display steps up (DESIGN 4.3) and the band moves down.
        r.w->resize(1100, 600);
        QCoreApplication::processEvents();
        QCOMPARE(r.w->display()->pitch(), 4);
        QCOMPARE(r.w->list()->geometry().x(), 434);
        QCOMPARE(r.w->list()->geometry().width(), 1100 - 450);
    }

    void statusBarShowsPortChannelInput()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->setInputPortLabel(QStringLiteral("Some Input"));
        QStringList texts;
        for (const QLabel *l : r.w->statusBar()->findChildren<QLabel *>())
            if (l->isVisibleTo(r.w.get()))
                texts << l->text();
        QVERIFY(texts.contains(QStringLiteral("OUT")));
        QVERIFY(texts.contains(QStringLiteral("Fake D-05 In")));
        QVERIFY(texts.contains(QStringLiteral("CH")));
        QVERIFY(texts.contains(QStringLiteral("Some Input")));
        QVERIFY(!texts.contains(QStringLiteral("NOT FOUND")));
        Rig d(false);
        QVERIFY(d.show());
        bool notFound = false;
        for (const QLabel *l : d.w->statusBar()->findChildren<QLabel *>())
            notFound = notFound || (l->text() == QStringLiteral("NOT FOUND") && l->isVisibleTo(d.w.get()));
        QVERIFY(notFound);
    }

    // ------------------------------------------------------------------ sending

    void highlightSendsNothing()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->list()->setFocus();
        for (int i = 0; i < 5; ++i)
            QTest::keyClick(r.w->list(), Qt::Key_Down);
        QTest::keyClick(r.w->list(), Qt::Key_End);
        QTest::keyClick(r.w->list(), Qt::Key_Home);
        r.w->list()->setCurrentIndex(r.w->model()->index(40));
        r.w->searchField()->setFocus();
        QTest::keyClicks(r.w->searchField(), QStringLiteral("brass"));
        QTest::keyClick(r.w->searchField(), Qt::Key_Down);
        QTest::keyClick(r.w->searchField(), Qt::Key_Down);
        QTest::keyClick(r.w->searchField(), Qt::Key_Up);
        QVERIFY(r.sent().isEmpty());
        QCOMPARE(r.w->current(), addr("P1-11"));
        QVERIFY(!r.w->display()->sentActive());
    }

    void enterOnListSendsOneTriplet()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->list()->setFocus();
        QTest::keyClick(r.w->list(), Qt::Key_Home);
        QTest::keyClick(r.w->list(), Qt::Key_Down);
        QTest::keyClick(r.w->list(), Qt::Key_Down);
        const PatchAddress target = *r.w->highlighted();
        QCOMPARE(target, addr("U1-13"));
        QVERIFY(r.sent().isEmpty());
        QTest::keyClick(r.w->list(), Qt::Key_Return);
        QVERIFY(r.sentExactly(target));
        QCOMPARE(r.sent().size(), 3);
        QCOMPARE(r.sent()[0], QByteArray::fromHex("b00057"));
        QCOMPARE(r.sent()[1], QByteArray::fromHex("b02000"));
        QCOMPARE(r.sent()[2], QByteArray::fromHex("c002"));
        QCOMPARE(r.w->current(), target);
        QVERIFY(r.w->display()->sentActive());
        QVERIFY(r.w->display()->line1().startsWith(QStringLiteral("U1-13:")));
        // Recorded in the library as a send.
        QCOMPARE(r.library->recents().first().address, target);
    }

    void listClickSendsOneTriplet()
    {
        Rig r;
        QVERIFY(r.show());
        const QModelIndex idx = r.w->model()->indexOf(addr("U2-11"));
        r.w->list()->scrollTo(idx);
        QTest::mouseClick(r.w->list()->viewport(), Qt::LeftButton, Qt::NoModifier,
                          r.w->list()->visualRect(idx).center());
        QVERIFY(r.sentExactly(addr("U2-11")));
        QCOMPARE(r.sent()[1], QByteArray::fromHex("b02001"));
    }

    void enterInSearchSendsFirstMatch()
    {
        Rig r;
        QVERIFY(r.show());
        QTest::keyClicks(r.w->searchField(), QStringLiteral("brass"));
        QVERIFY(r.sent().isEmpty());
        const PatchAddress first = r.w->model()->row(0).address;
        QCOMPARE(*r.w->highlighted(), first);
        QTest::keyClick(r.w->searchField(), Qt::Key_Return);
        QVERIFY(r.sentExactly(first));
        // Down in the field moves the highlight without sending.
        r.backend.clearSent();
        QTest::keyClick(r.w->searchField(), Qt::Key_Down);
        QCOMPARE(*r.w->highlighted(), r.w->model()->row(1).address);
        QVERIFY(r.sent().isEmpty());
    }

    void gridEntryFromKeyboardSendsOnSecondDigit()
    {
        Rig r;
        QVERIFY(r.show());
        QTest::keyClick(r.w.get(), Qt::Key_G, Qt::ControlModifier);
        QVERIFY(r.w->bankGrid()->hasFocus());
        QCOMPARE(r.w->bankGrid()->stage(), BankGrid::Stage::AwaitBank);
        QCOMPARE(r.w->display()->line1(), pad(QStringLiteral("P1-__")));
        QVERIFY(!r.w->bankGrid()->bankLit(1));  // all bank LEDs dark while awaiting
        QVERIFY(r.w->bankGrid()->bankNumeralsBright());
        QTest::keyClick(r.w->bankGrid(), Qt::Key_3);
        QCOMPARE(r.w->display()->line1(), pad(QStringLiteral("P1-3_")));
        QCOMPARE(r.w->bankGrid()->stage(), BankGrid::Stage::AwaitNumber);
        QVERIFY(r.w->bankGrid()->bankLit(3));
        QVERIFY(r.w->bankGrid()->numberNumeralsBright());
        QVERIFY(r.sent().isEmpty());
        QTest::keyClick(r.w->bankGrid(), Qt::Key_4);
        // P1-34: lsb 8, program (3-1)*8 + (4-1) = 19.
        QCOMPARE(r.sent().size(), 3);
        QCOMPARE(r.sent()[0], QByteArray::fromHex("b00057"));
        QCOMPARE(r.sent()[1], QByteArray::fromHex("b02008"));
        QCOMPARE(r.sent()[2], QByteArray::fromHex("c013"));
        QCOMPARE(r.w->current(), addr("P1-34"));
        QCOMPARE(r.w->bankGrid()->stage(), BankGrid::Stage::Idle);
        QVERIFY(r.w->bankGrid()->bankLit(3));
        QVERIFY(r.w->bankGrid()->numberLit(4));
        QVERIFY(r.w->display()->line1().startsWith(QStringLiteral("P1-34:")));
    }

    void gridEntryFromGroupClick()
    {
        Rig r;
        QVERIFY(r.show());
        GroupBar *bar = r.w->groupBar();
        const int p3 = 10;  // P3
        QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, bar->faceOf(p3) + QPoint(20, 10));
        QCOMPARE(r.w->display()->line1(), pad(QStringLiteral("P3-__")));
        QCOMPARE(bar->litGroup(), p3);
        QVERIFY(r.w->bankGrid()->hasFocus());
        QTest::keyClick(r.w->bankGrid(), Qt::Key_4);
        QTest::keyClick(r.w->bankGrid(), Qt::Key_5);
        QVERIFY(r.sentExactly(addr("P3-45")));
        QCOMPARE(r.sent()[1], QByteArray::fromHex("b0200a"));
        QCOMPARE(r.sent()[2], QByteArray::fromHex("c01c"));  // docs/MIDI.md: P3-45 is PC 28
        QCOMPARE(bar->litGroup(), p3);
    }

    void bankClickThenNumberClick()
    {
        Rig r;
        QVERIFY(r.show());
        BankGrid *g = r.w->bankGrid();
        // A bank click while idle starts at "awaiting number" in the current group.
        QTest::mouseClick(g, Qt::LeftButton, Qt::NoModifier, g->faceOf(BankGrid::bankSlot(5)) + QPoint(20, 10));
        QCOMPARE(g->stage(), BankGrid::Stage::AwaitNumber);
        QCOMPARE(r.w->display()->line1(), pad(QStringLiteral("P1-5_")));
        QVERIFY(r.sent().isEmpty());
        QTest::mouseClick(g, Qt::LeftButton, Qt::NoModifier, g->faceOf(BankGrid::numberSlot(7)) + QPoint(20, 10));
        QVERIFY(r.sentExactly(addr("P1-57")));
    }

    void numberClickWhileIdleSendsAtOnce()
    {
        Rig r;
        QVERIFY(r.show());
        BankGrid *g = r.w->bankGrid();
        QTest::mouseClick(g, Qt::LeftButton, Qt::NoModifier, g->faceOf(BankGrid::numberSlot(6)) + QPoint(20, 10));
        QVERIFY(r.sentExactly(addr("P1-16")));  // current group and bank, that number
    }

    void gridKeyboardCursorEnter()
    {
        Rig r;
        QVERIFY(r.show());
        BankGrid *g = r.w->bankGrid();
        g->setFocus();
        QCOMPARE(g->cursor(), BankGrid::bankSlot(1));  // starts on the current bank
        QTest::keyClick(g, Qt::Key_Right);
        QTest::keyClick(g, Qt::Key_Right);
        QTest::keyClick(g, Qt::Key_Return);  // bank 3 pressed
        QCOMPARE(g->stage(), BankGrid::Stage::AwaitNumber);
        QCOMPARE(g->entryBank(), 3);
        QTest::keyClick(g, Qt::Key_Down);
        QTest::keyClick(g, Qt::Key_Space);   // the number button under the cursor: number 3
        QVERIFY(r.sentExactly(addr("P1-33")));
    }

    void entryCancelledByEscAndByFocusLoss()
    {
        Rig r;
        QVERIFY(r.show());
        QTest::keyClick(r.w.get(), Qt::Key_G, Qt::ControlModifier);
        QCOMPARE(r.w->bankGrid()->stage(), BankGrid::Stage::AwaitBank);
        QTest::keyClick(r.w.get(), Qt::Key_Escape);
        QCOMPARE(r.w->bankGrid()->stage(), BankGrid::Stage::Idle);
        QVERIFY(r.w->display()->line1().startsWith(QStringLiteral("P1-11:Fantasia")));  // echo gone
        QTest::keyClick(r.w.get(), Qt::Key_G, Qt::ControlModifier);
        r.w->searchField()->setFocus();
        QCOMPARE(r.w->bankGrid()->stage(), BankGrid::Stage::Idle);
        QVERIFY(r.sent().isEmpty());
    }

    void pageDownAndUpSendOneTripletEach()
    {
        Rig r;
        QVERIFY(r.show());
        QTest::keyClick(r.w.get(), Qt::Key_PageDown);
        QVERIFY(r.sentExactly(addr("P1-12")));
        r.backend.clearSent();
        QTest::keyClick(r.w.get(), Qt::Key_PageUp);
        QVERIFY(r.sentExactly(addr("P1-11")));
        // Also with the list focused (the view must not swallow PgUp/PgDn).
        r.backend.clearSent();
        r.w->list()->setFocus();
        QTest::keyClick(r.w->list(), Qt::Key_PageDown);
        QVERIFY(r.sentExactly(addr("P1-12")));
    }

    void prevNextAcrossGroupsAndAtTheEnds()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->setCurrent(addr("P1-88"));
        QTest::keyClick(r.w.get(), Qt::Key_PageDown);
        QVERIFY(r.sentExactly(addr("P2-11")));
        QCOMPARE(r.sent()[1], QByteArray::fromHex("b02009"));
        QCOMPARE(r.sent()[2], QByteArray::fromHex("c000"));
        QCOMPARE(r.w->current(), addr("P2-11"));
        QCOMPARE(r.w->groupBar()->litGroup(), 9);

        r.backend.clearSent();
        r.w->setCurrent(addr("P6-88"));
        QTest::keyClick(r.w.get(), Qt::Key_PageDown);  // stops at the end and resends
        QVERIFY(r.sentExactly(addr("P6-88")));
        QCOMPARE(r.sent()[2], QByteArray::fromHex("c03f"));

        r.backend.clearSent();
        r.w->setCurrent(addr("P2-11"));
        QTest::keyClick(r.w.get(), Qt::Key_PageUp);
        QVERIFY(r.sentExactly(addr("P1-88")));

        r.backend.clearSent();
        r.w->setCurrent(addr("U8-88"));
        r.w->nextButton()->click();  // across the U/P boundary
        QVERIFY(r.sentExactly(addr("P1-11")));
        r.backend.clearSent();
        r.w->setCurrent(addr("U1-11"));
        r.w->prevButton()->click();
        QVERIFY(r.sentExactly(addr("U1-11")));
    }

    void disconnectedSendsNothingAndSaysSo()
    {
        Rig r(false);
        QVERIFY(r.show());
        QVERIFY(r.w->display()->line2().startsWith(QStringLiteral("NO MIDI OUTPUT")));
        QTest::keyClick(r.w.get(), Qt::Key_PageDown);
        r.w->list()->setFocus();
        QTest::keyClick(r.w->list(), Qt::Key_Return);
        QVERIFY(r.sent().isEmpty());
        QVERIFY(r.w->message().startsWith(QStringLiteral("Not sent")));
        QCOMPARE(r.w->current(), addr("P1-11"));  // the display keeps mirroring the synth
        QCOMPARE(r.w->bankGrid()->stage(), BankGrid::Stage::Idle);
    }

    void activationChecksTheConnection()
    {
        Rig r;
        QVERIFY(r.show());
        QVERIFY(r.link->isConnected());
        r.backend.setOutputPorts({}, false);  // unplugged, no notification
        QVERIFY(r.link->isConnected());
        QEvent ev(QEvent::WindowActivate);
        QApplication::sendEvent(r.w.get(), &ev);
        QVERIFY(!r.link->isConnected());
        QVERIFY(r.w->display()->line2().startsWith(QStringLiteral("NO MIDI OUTPUT")));
    }

    // ------------------------------------------------------------------ search and views

    void searchFiltersTheModelAndTheDisplayCount()
    {
        Rig r;
        QVERIFY(r.show());
        QCOMPARE(r.w->model()->rowCount(), PatchAddress::CatalogueSize);
        const SearchQuery brass(QStringLiteral("brass"));
        int expected = 0;
        for (int i = 0; i < PatchAddress::CatalogueSize; ++i) {
            const PatchAddress a = *PatchAddress::fromCatalogueIndex(i);
            if (brass.matches(a, r.catalog.name(a)))
                ++expected;
        }
        QVERIFY(expected > 1);
        QTest::keyClicks(r.w->searchField(), QStringLiteral("brass"));
        QCOMPARE(r.w->model()->rowCount(), expected);
        QVERIFY(r.w->display()->line2().startsWith(QString::number(expected) + QStringLiteral(" MATCHES")));
        QCOMPARE(*r.w->highlighted(), r.w->model()->row(0).address);
        // Ranges point at "brass" inside the name.
        const auto &row0 = r.w->model()->row(0);
        QVERIFY(!row0.ranges.isEmpty());
        QCOMPARE(row0.name.mid(row0.ranges[0].start, row0.ranges[0].length).toLower(), QStringLiteral("brass"));

        r.w->searchField()->setText(QStringLiteral("zzzqqq"));
        QCOMPARE(r.w->model()->rowCount(), 0);
        QVERIFY(r.w->display()->line2().startsWith(QStringLiteral("NO MATCHES")));
        QCOMPARE(r.w->list()->emptyTitle(), QStringLiteral("NO MATCHES"));
        QVERIFY(!r.w->highlighted());

        r.w->searchField()->setText(QStringLiteral("p3-45"));
        QCOMPARE(r.w->model()->rowCount(), 1);
        QVERIFY(r.w->display()->line2().startsWith(QStringLiteral("1 MATCH")));
        QVERIFY(!r.w->display()->line2().startsWith(QStringLiteral("1 MATCHES")));

        r.w->searchField()->clear();
        QCOMPARE(r.w->model()->rowCount(), PatchAddress::CatalogueSize);
        QVERIFY(r.w->display()->line2().startsWith(QStringLiteral("PRESET 1: ORIGINAL D-50")));
        QVERIFY(r.sent().isEmpty());
    }

    void viewsFilterAndKeepTheSearch()
    {
        Rig r;
        r.library->setFavorite(addr("P1-12"), true);
        r.library->setFavorite(addr("P3-45"), true);
        r.library->recordSend(addr("P2-33"));
        r.library->recordSend(addr("P1-12"));
        QVERIFY(r.show());
        QTest::keyClick(r.w.get(), Qt::Key_2, Qt::ControlModifier);
        QCOMPARE(r.w->model()->view(), PatchListModel::View::Favourites);
        QVERIFY(r.w->viewButton(PatchListModel::View::Favourites)->lit());
        QVERIFY(!r.w->viewButton(PatchListModel::View::All)->lit());
        QCOMPARE(r.w->model()->rowCount(), 2);
        QTest::keyClick(r.w.get(), Qt::Key_3, Qt::ControlModifier);
        QCOMPARE(r.w->model()->view(), PatchListModel::View::Recents);
        QCOMPARE(r.w->model()->rowCount(), 2);
        QCOMPARE(r.w->model()->row(0).address, addr("P1-12"));  // most recent first
        r.w->searchField()->setText(QStringLiteral("p2"));
        QCOMPARE(r.w->model()->rowCount(), 1);
        r.w->searchField()->clear();
        QTest::keyClick(r.w.get(), Qt::Key_1, Qt::ControlModifier);
        QCOMPARE(r.w->model()->view(), PatchListModel::View::All);
        QCOMPARE(r.w->model()->rowCount(), PatchAddress::CatalogueSize);
        // Empty states (DESIGN 7.5).
        Rig e;
        QVERIFY(e.show());
        e.w->viewButton(PatchListModel::View::Favourites)->click();
        QCOMPARE(e.w->list()->emptyTitle(), QStringLiteral("NO FAVOURITES"));
        e.w->viewButton(PatchListModel::View::Recents)->click();
        QCOMPARE(e.w->list()->emptyTitle(), QStringLiteral("NOTHING SENT YET"));
    }

    // ------------------------------------------------------------------ shortcuts (FR-13)

    void everyShortcutTriggersItsAction()
    {
        Rig r;
        QVERIFY(r.show());
        MainWindow &w = *r.w;
        const auto seq = [&](const char *name) { return w.action(QString::fromLatin1(name))->shortcut(); };
        QCOMPARE(seq("actionFocusSearch"), QKeySequence(Qt::Key_Slash));
        QCOMPARE(seq("actionFocusSearchCtrlF"), QKeySequence(Qt::CTRL | Qt::Key_F));
        QCOMPARE(seq("actionEscape"), QKeySequence(Qt::Key_Escape));
        QCOMPARE(seq("actionPrev"), QKeySequence(Qt::Key_PageUp));
        QCOMPARE(seq("actionNext"), QKeySequence(Qt::Key_PageDown));
        QCOMPARE(seq("actionGridEntry"), QKeySequence(Qt::CTRL | Qt::Key_G));
        QCOMPARE(seq("actionViewAll"), QKeySequence(Qt::CTRL | Qt::Key_1));
        QCOMPARE(seq("actionViewFavourites"), QKeySequence(Qt::CTRL | Qt::Key_2));
        QCOMPARE(seq("actionViewRecents"), QKeySequence(Qt::CTRL | Qt::Key_3));
        QCOMPARE(seq("actionAutoSend"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
        QCOMPARE(seq("actionSettings"), QKeySequence(Qt::CTRL | Qt::Key_Comma));
        QCOMPARE(seq("actionShortcuts"), QKeySequence(Qt::Key_F1));
        QCOMPARE(seq("actionQuit"), QKeySequence(Qt::CTRL | Qt::Key_Q));

        // "/" and Ctrl+F focus the search field from the list.
        w.list()->setFocus();
        QTest::keyClick(w.list(), Qt::Key_Slash);
        QVERIFY(w.searchField()->hasFocus());
        QVERIFY(w.searchField()->text().isEmpty());  // the slash was a shortcut, not text
        w.list()->setFocus();
        QTest::keyClick(w.list(), Qt::Key_F, Qt::ControlModifier);
        QVERIFY(w.searchField()->hasFocus());
        // Typing a slash in the field inserts it (the field takes plain keys first).
        QTest::keyClick(w.searchField(), Qt::Key_Slash);
        QCOMPARE(w.searchField()->text(), QStringLiteral("/"));
        w.searchField()->clear();

        // Esc: clears the search, then the second press returns focus to the list.
        QTest::keyClicks(w.searchField(), QStringLiteral("harp"));
        QVERIFY(w.model()->rowCount() < PatchAddress::CatalogueSize);
        QTest::keyClick(w.searchField(), Qt::Key_Escape);
        QVERIFY(w.searchField()->text().isEmpty());
        QCOMPARE(w.model()->rowCount(), PatchAddress::CatalogueSize);
        QVERIFY(w.searchField()->hasFocus());
        QTest::keyClick(w.searchField(), Qt::Key_Escape);
        QVERIFY(w.list()->hasFocus());

        // Ctrl+Shift+A, Ctrl+, and F1 are stubs until T3.4 / M5: they say so, send nothing.
        QTest::keyClick(w.list(), Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(w.message().contains(QStringLiteral("Auto-send")));
        QTest::keyClick(w.list(), Qt::Key_Comma, Qt::ControlModifier);
        QVERIFY(w.message().contains(QStringLiteral("Settings")));
        QTest::keyClick(w.list(), Qt::Key_F1);
        QVERIFY(w.message().contains(QStringLiteral("shortcut sheet")));
        QVERIFY(r.sent().isEmpty());
    }

    void systemMenuHasTheDesignItems()
    {
        Rig r;
        QStringList texts;
        for (const QAction *a : r.w->systemMenu()->actions())
            texts << (a->isSeparator() ? QStringLiteral("-") : a->text());
        QCOMPARE(texts, (QStringList{QStringLiteral("Settings..."), QStringLiteral("Keyboard Shortcuts"),
                                     QStringLiteral("Clear Recents..."), QStringLiteral("-"),
                                     QStringLiteral("About D-05 Patch Switcher"), QStringLiteral("-"),
                                     QStringLiteral("Quit")}));
    }

    // ------------------------------------------------------------------ parts

    void groupBarAndGridShowTheCurrentPatch()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->setCurrent(addr("U7-52"));
        QCOMPARE(r.w->groupBar()->litGroup(), 6);
        for (int b = 1; b <= 8; ++b) {
            QCOMPARE(r.w->bankGrid()->bankLit(b), b == 5);
            QCOMPARE(r.w->bankGrid()->numberLit(b), b == 2);
        }
        QCOMPARE(r.w->display()->line1().left(6), QStringLiteral("U7-52:"));
        // The list's current row carries the LED and follows the highlight.
        QCOMPARE(r.w->model()->current(), addr("U7-52"));
        QCOMPARE(*r.w->highlighted(), addr("U7-52"));
        QVERIFY(r.sent().isEmpty());  // restoring is not sending
    }

    void panelButtonStatesPaintDifferently()
    {
        PanelButton b(PanelButton::Kind::Led);
        b.show();
        const auto shot = [&] { return b.grab().toImage(); };
        const QImage normal = shot();
        b.setLit(true);
        QVERIFY(shot() != normal);
        b.setLit(false);
        QCOMPARE(shot(), normal);
        b.setFocus(Qt::TabFocusReason);
        b.setEnabled(false);
        QVERIFY(shot() != normal);
        QCOMPARE(b.size(), QSize(52, 32));
    }

    void sectionRuleSmoke()
    {
        SectionRule rule(QStringLiteral("PATCH BANK"), true);
        rule.resize(394, SectionRule::Height);
        const QImage img = rule.grab().toImage();
        QVERIFY(!img.isNull());
        QCOMPARE(img.height(), int(SectionRule::Height * rule.devicePixelRatioF()));
    }

    void nameplateWordmarkAndLogo()
    {
        QTemporaryDir dir;
        Nameplate none(dir.filePath(QStringLiteral("missing.png")));
        QVERIFY(!none.hasLogo());
        QVERIFY(Nameplate::wordmarkWidth() > 280 && Nameplate::wordmarkWidth() < 340);  // "about 310"
        QImage logo(720, 40, QImage::Format_ARGB32);
        logo.fill(Qt::white);
        const QString path = dir.filePath(QStringLiteral("logo.png"));
        QVERIFY(logo.save(path));
        Nameplate with(path);
        QVERIFY(with.hasLogo());
        with.resize(360, 28);
        const QSize s = with.logoSize();
        QVERIFY(s.width() <= 360 && s.height() <= 28);
        QCOMPARE(qRound(double(s.width()) / s.height()), 18);  // aspect kept
        QImage small(100, 10, QImage::Format_ARGB32);
        small.fill(Qt::white);
        const QString path2 = dir.filePath(QStringLiteral("small.png"));
        QVERIFY(small.save(path2));
        Nameplate tiny(path2);
        tiny.resize(360, 28);
        QVERIFY(tiny.logoSize().width() <= qRound(100 / tiny.devicePixelRatioF()) + 1);  // never upscaled
        QVERIFY(!with.grab().isNull());
    }

    void logoPathIsInTheDataDir()
    {
        QTemporaryDir dir;
        Paths::setOverride(dir.path());
        QCOMPARE(Paths::logoPath(), Paths::dataDir() + QStringLiteral("/logo.png"));
        Paths::setOverride(QString());
    }

    // ------------------------------------------------------------------ painting discipline

    void delegateBuildsNothingWhilePainting()
    {
        Rig r;
        QVERIFY(r.show());
        QTest::keyClicks(r.w->searchField(), QStringLiteral("brass"));
        const int built = PatchDelegate::resourceBuildCount();
        for (int i = 0; i < 20; ++i) {
            r.w->list()->viewport()->repaint();
            r.w->list()->setCurrentIndex(r.w->model()->index(i % r.w->model()->rowCount()));
        }
        r.w->grab();
        QCOMPARE(PatchDelegate::resourceBuildCount(), built);  // one set per delegate, never per paint
    }

    // Allocations while painting rows with the list's own delegate (its layouts are
    // prepared by the view, outside paint). Counted with a replaced operator new.
    long allocsPerPaints(Rig &r, int row, int n, int width = 320)
    {
        auto *delegate = qobject_cast<PatchDelegate *>(r.w->list()->itemDelegate());
        delegate->prepare(*r.w->model(), width);  // as the view does on reset and resize
        QImage img(width, PatchDelegate::RowHeight, QImage::Format_RGB32);
        QPainter painter(&img);
        QStyleOptionViewItem opt;
        opt.widget = r.w->list();
        opt.rect = QRect(0, 0, img.width(), PatchDelegate::RowHeight);
        opt.state = QStyle::State_Enabled | QStyle::State_Selected | QStyle::State_MouseOver;
        const QModelIndex idx = r.w->model()->index(row);
        delegate->paint(&painter, opt, idx);  // warm up glyph caches
        g_allocs = 0;
        g_counting = true;
        for (int i = 0; i < n; ++i)
            delegate->paint(&painter, opt, idx);
        g_counting = false;
        painter.end();
        return g_allocs.load();
    }

    void delegatePaintAllocationsDoNotGrowWithElisionOrRanges()
    {
        Rig r;
        r.w->resize(800, 480);
        QVERIFY(r.show());

        // A plain row: the baseline of what Qt itself allocates per paint.
        r.w->searchField()->setText(QStringLiteral("p1-11"));
        const long plain = allocsPerPaints(r, 0, 100);
        // The longest preset name in a 320 px row (name column 136 px): elided, one match range.
        r.w->searchField()->setText(QStringLiteral("alpha omega ensemble"));
        QVERIFY(r.w->model()->rowCount() > 0);
        const int elidedRow = 0;
        const QString full = r.w->model()->row(elidedRow).shown;
        QCOMPARE(full, QStringLiteral("Alpha Omega Ensemble"));
        QVERIFY(QFontMetrics(Theme::font(Theme::Role::ListName)).horizontalAdvance(full) > 320 - 184);
        const long elided = allocsPerPaints(r, elidedRow, 100);
        // Several match ranges at once.
        r.w->searchField()->setText(QStringLiteral("al om en"));
        QVERIFY(r.w->model()->row(0).ranges.size() >= 3);
        const long ranges = allocsPerPaints(r, 0, 100);
        // Many more paints, same per-paint cost.
        const long elided1000 = allocsPerPaints(r, 0, 1000);

        // What remains is inside Qt (found with a backtrace): QPainter::save (1),
        // QPainter::setFont resolving the font (3), drawText text engines (2 or 3).
        // None of it depends on elision or on the number of ranges.
        QVERIFY2(plain <= 100 * 8, qPrintable(QString::number(plain)));
        QVERIFY2(elided <= plain + 100, qPrintable(QStringLiteral("%1 vs %2").arg(elided).arg(plain)));
        QVERIFY2(ranges <= plain + 100, qPrintable(QStringLiteral("%1 vs %2").arg(ranges).arg(plain)));
        QVERIFY2(elided1000 <= ranges * 10 + 50, qPrintable(QStringLiteral("%1 vs %2").arg(elided1000).arg(ranges)));
    }

    void elidedRowHighlightStopsAtTheVisibleText()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->searchField()->setText(QStringLiteral("ensemble"));
        int row = -1;
        for (int i = 0; i < r.w->model()->rowCount(); ++i)
            if (r.w->model()->row(i).shown == QStringLiteral("Alpha Omega Ensemble"))
                row = i;
        QVERIFY(row >= 0);
        // A 320 px row leaves a 136 px name column: the name is elided and the match
        // ("Ensemble", from about x 85 of the name) runs into the hidden part.
        const int width = 320;
        auto *delegate = qobject_cast<PatchDelegate *>(r.w->list()->itemDelegate());
        delegate->prepare(*r.w->model(), width);
        QImage img(width, PatchDelegate::RowHeight, QImage::Format_RGB32);
        img.fill(Theme::well);
        QPainter painter(&img);
        QStyleOptionViewItem opt;
        opt.widget = r.w->list();
        opt.rect = QRect(0, 0, width, PatchDelegate::RowHeight);
        opt.state = QStyle::State_Enabled;
        delegate->paint(&painter, opt, r.w->model()->index(row));
        painter.end();
        const QFontMetrics fm(Theme::font(Theme::Role::ListName));
        const QString name = QStringLiteral("Alpha Omega Ensemble");
        const QString elided = fm.elidedText(name, Qt::ElideRight, width - 100 - 84);
        QVERIFY(elided != name);
        const int visibleRight = 84 + fm.horizontalAdvance(name, elided.size() - 1);
        int first = -1, last = -1;
        for (int x = 0; x < width; ++x) {
            if (QColor(img.pixel(x, 5)) == Theme::matchBack) {
                if (first < 0)
                    first = x;
                last = x;
            }
        }
        QVERIFY(first >= 84 + fm.horizontalAdvance(name, 12) - 1);
        QVERIFY(last < visibleRight);
        QVERIFY(last >= visibleRight - 2);  // runs right up to the end of the visible text
    }

    void oversizedLogoFallsBackToTheWordmark()
    {
        QTemporaryDir dir;
        QImage huge(4097, 8, QImage::Format_ARGB32);
        huge.fill(Qt::white);
        const QString path = dir.filePath(QStringLiteral("logo.png"));
        QVERIFY(huge.save(path));
        Nameplate n(path);
        QVERIFY(!n.hasLogo());
        QVERIFY(n.logoSize().isEmpty());
        QVERIFY(!n.grab().isNull());
        QImage ok(4096, 8, QImage::Format_ARGB32);
        ok.fill(Qt::white);
        QVERIFY(ok.save(path));
        Nameplate n2(path);
        QVERIFY(n2.hasLogo());
        QVERIFY(n2.logoSize().width() <= 360);
    }

    void clearingTheSearchRestoresTheHighlight()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->list()->setFocus();
        r.w->list()->setCurrentIndex(r.w->model()->indexOf(addr("U3-45")));
        r.w->searchField()->setFocus();
        QTest::keyClicks(r.w->searchField(), QStringLiteral("brass"));
        QCOMPARE(*r.w->highlighted(), r.w->model()->row(0).address);
        QTest::keyClick(r.w->searchField(), Qt::Key_Escape);
        QVERIFY(r.w->searchField()->text().isEmpty());
        QCOMPARE(*r.w->highlighted(), addr("U3-45"));
        QVERIFY(r.w->list()->viewport()->rect().intersects(r.w->list()->visualRect(r.w->list()->currentIndex())));
        QVERIFY(r.sent().isEmpty());
        // A search that hides the old row: clearing falls back to the current patch.
        r.w->list()->setCurrentIndex(r.w->model()->indexOf(addr("U3-45")));
        r.w->searchField()->setText(QStringLiteral("brass"));
        r.w->searchField()->setText(QString());
        QCOMPARE(*r.w->highlighted(), addr("U3-45"));
        QCOMPARE(r.w->current(), addr("P1-11"));
    }

    void doubleClickOnARowSendsOnce()
    {
        Rig r;
        QVERIFY(r.show());
        const QModelIndex idx = r.w->model()->indexOf(addr("U2-11"));
        r.w->list()->scrollTo(idx);
        QWidget *vp = r.w->list()->viewport();
        const QPoint pos = r.w->list()->visualRect(idx).center();
        // What a real double-click delivers: press, release, double-click, release.
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QMouseEvent dbl(QEvent::MouseButtonDblClick, pos, vp->mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton,
                        Qt::NoModifier);
        QApplication::sendEvent(vp, &dbl);
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QVERIFY2(r.sentExactly(addr("U2-11")), qPrintable(QString::number(r.sent().size())));
        // QTest's own mouseDClick never sends more than one triplet either.
        r.backend.clearSent();
        QTest::mouseDClick(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QVERIFY(r.sent().size() <= 3);
    }

    void prevNextDoNotAutoRepeat()
    {
        Rig r;
        QVERIFY(r.show());
        QVERIFY(!r.w->action(QStringLiteral("actionPrev"))->autoRepeat());
        QVERIFY(!r.w->action(QStringLiteral("actionNext"))->autoRepeat());
        QVERIFY(!r.w->prevButton()->autoRepeat());
        QVERIFY(!r.w->nextButton()->autoRepeat());
        // A key press delivered with the auto-repeat flag must not reach the synth.
        QKeyEvent press(QEvent::KeyPress, Qt::Key_PageDown, Qt::NoModifier, QString(), true);
        QApplication::sendEvent(r.w.get(), &press);
        QVERIFY(r.sent().isEmpty());
        QTest::keyClick(r.w.get(), Qt::Key_PageDown);
        QCOMPARE(r.sent().size(), 3);
    }

    void startupPortErrorShowsItsText()
    {
        Rig r(true, true);
        QVERIFY(r.show());
        QVERIFY2(r.w->message().startsWith(QStringLiteral("Error")), qPrintable(r.w->message()));
        QVERIFY(r.w->display()->line2().startsWith(QStringLiteral("NO MIDI OUTPUT")));
        // The list failing at startup reads the same way.
        QTemporaryDir dir;
        PatchCatalog catalog;
        Library lib(catalog, dir.filePath(QStringLiteral("library.json")));
        FakeMidiBackend backend;
        backend.setListError(QStringLiteral("no ALSA"));
        MidiLink link(&backend);
        link.setOutputPort(QStringLiteral("Fake D-05 In"));
        MainWindow w(catalog, lib, link);
        QVERIFY2(w.message().contains(QStringLiteral("no ALSA")), qPrintable(w.message()));
    }

    void clickOutsideTheGridCancelsAPendingEntry()
    {
        Rig r;
        QVERIFY(r.show());
        BankGrid *g = r.w->bankGrid();
        const int p3 = 10;
        const QPoint face(20, 10);
        const auto startEntry = [&] {
            QTest::mouseClick(r.w->groupBar(), Qt::LeftButton, Qt::NoModifier, r.w->groupBar()->faceOf(p3) + face);
            QCOMPARE(g->stage(), BankGrid::Stage::AwaitBank);
        };
        startEntry();
        QTest::mouseClick(r.w->viewButton(PatchListModel::View::Favourites), Qt::LeftButton, Qt::NoModifier,
                          QPoint(26, 16));
        QCOMPARE(g->stage(), BankGrid::Stage::Idle);
        QCOMPARE(r.w->model()->view(), PatchListModel::View::Favourites);
        QTest::keyClick(g, Qt::Key_4);  // not the stale P3 entry: a fresh one in the current group
        QCOMPARE(g->entryGroup(), 8);
        QVERIFY(r.sent().isEmpty());
        g->cancelEntry();

        startEntry();
        QTest::mouseClick(r.w->searchField(), Qt::LeftButton, Qt::NoModifier, QPoint(20, 16));
        QCOMPARE(g->stage(), BankGrid::Stage::Idle);
        startEntry();
        QTest::mouseClick(r.w->list()->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
        QCOMPARE(g->stage(), BankGrid::Stage::Idle);
        r.backend.clearSent();
        // A click inside the grid does not cancel.
        startEntry();
        QTest::mouseClick(g, Qt::LeftButton, Qt::NoModifier, g->faceOf(BankGrid::bankSlot(4)) + face);
        QCOMPARE(g->stage(), BankGrid::Stage::AwaitNumber);
    }

    void delegatePaintsMatchHighlightAndKeyModeBox()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->searchField()->setText(QStringLiteral("brass"));
        const auto &row = r.w->model()->row(0);
        PatchDelegate delegate;
        QImage img(r.w->list()->viewport()->width(), PatchDelegate::RowHeight, QImage::Format_RGB32);
        img.fill(Theme::well);
        QPainter painter(&img);
        QStyleOptionViewItem opt;
        opt.widget = r.w->list();
        opt.rect = QRect(0, 0, img.width(), PatchDelegate::RowHeight);
        opt.state = QStyle::State_Enabled;
        delegate.paint(&painter, opt, r.w->model()->index(0));
        painter.end();
        // DESIGN 7.3: matchBack at y 4..23, starting at x 84 + advance(name, start).
        QFontMetrics fm(Theme::font(Theme::Role::ListName));
        const int x = 84 + fm.horizontalAdvance(row.name, row.ranges[0].start) + 2;
        QCOMPARE(QColor(img.pixel(x, 10)), Theme::matchBack);
        QCOMPARE(QColor(img.pixel(x, 3)), Theme::well);
        QCOMPARE(QColor(img.pixel(x, 24)), Theme::well);
        // The key-mode box border (DESIGN 7.2): 1 px tagBorder at (Wl - 52, 6).
        QCOMPARE(QColor(img.pixel(img.width() - 52, 10)), Theme::tagBorder);
    }
};

QTEST_MAIN(TestMainWindow)
#include "test_main_window.moc"
