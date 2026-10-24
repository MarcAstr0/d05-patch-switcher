// T3.3: favourites, recents, user-slot rename and Clear Recents through the main window
// on the fake MIDI backend, headless. None of these actions may send MIDI.
#include "app/Paths.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiLink.h"
#include "ui/DisplayWidget.h"
#include "ui/MainWindow.h"
#include "ui/PanelButton.h"
#include "ui/PatchDelegate.h"
#include "ui/PatchListModel.h"
#include "ui/PatchListView.h"
#include "ui/Theme.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QImage>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace d05;

namespace {

PatchAddress addr(const char *t)
{
    return *PatchAddress::parse(QString::fromLatin1(t));
}

struct Rig {
    QTemporaryDir dir;
    PatchCatalog catalog;
    std::unique_ptr<Library> library;
    FakeMidiBackend backend;
    std::unique_ptr<MidiLink> link;
    std::unique_ptr<MainWindow> w;

    Rig()
    {
        Paths::setOverride(dir.path());
        library = std::make_unique<Library>(catalog, path());
        link = std::make_unique<MidiLink>(&backend);
        link->setChannel(1);
        link->setOutputPort(QStringLiteral("Fake D-05 In"));
        w = std::make_unique<MainWindow>(catalog, *library, *link);
    }
    ~Rig()
    {
        w.reset();
        link.reset();
        library.reset();
        Paths::setOverride(QString());
    }
    QString path() const { return dir.filePath(QStringLiteral("library.json")); }
    bool show()
    {
        w->show();
        w->activateWindow();
        return QTest::qWaitForWindowActive(w.get());
    }
    PatchListView *list() const { return w->list(); }
    PatchListModel *model() const { return w->model(); }
    int sentCount() const { return backend.sentMessages().size(); }

    // Highlights without sending and brings the row into view; returns its index.
    QModelIndex highlight(const char *a)
    {
        const QModelIndex i = model()->indexOf(addr(a));
        list()->setCurrentIndex(i);
        list()->scrollTo(i);
        return i;
    }
    QPoint starPos(const QModelIndex &i) const
    {
        const QRect r = list()->visualRect(i);
        return QPoint(r.right() - 16, r.center().y());
    }
    // The open editor; a closed one is hidden and only deleted later.
    QLineEdit *editor() const
    {
        for (QLineEdit *e : list()->viewport()->findChildren<QLineEdit *>(QStringLiteral("nameEditor"))) {
            if (!e->isHidden())
                return e;
        }
        return nullptr;
    }
    // A second library on the same file, as after a restart.
    QString reloadedName(const char *a)
    {
        library->flush();
        PatchCatalog c2;
        Library l2(c2, path());
        l2.load();
        return c2.name(addr(a));
    }
};

}  // namespace

class TestLibraryViews : public QObject {
    Q_OBJECT
private slots:
    // Return and Escape in the editor: the delegate commits and closes through a queued call.
    static void finish(QWidget *editor, Qt::Key key)
    {
        QTest::keyClick(editor, key);
        for (int i = 0; i < 3; ++i)
            QCoreApplication::processEvents();  // commit, close, then the library write
    }

    void initTestCase() { Theme::install(*qobject_cast<QApplication *>(qApp)); }

    // ------------------------------------------------------------------ favourites

    void starClickTogglesAndPersistsWithoutSending()
    {
        Rig r;
        QVERIFY(r.show());
        const QModelIndex i = r.highlight("P2-33");
        QSignalSpy reset(r.model(), &QAbstractItemModel::modelReset);
        QSignalSpy changed(r.model(), &QAbstractItemModel::dataChanged);
        QTest::mouseClick(r.list()->viewport(), Qt::LeftButton, Qt::NoModifier, r.starPos(i));
        QVERIFY(r.library->isFavorite(addr("P2-33")));
        QVERIFY(r.model()->row(i.row()).favorite);
        QCOMPARE(reset.count(), 0);  // only the row repaints
        QVERIFY(changed.count() >= 1);
        QCOMPARE(changed.last().at(0).toModelIndex(), i);
        QCOMPARE(r.sentCount(), 0);
        // The star did not move the highlight of another row or send; a second click clears it.
        QTest::mouseClick(r.list()->viewport(), Qt::LeftButton, Qt::NoModifier, r.starPos(i));
        QVERIFY(!r.library->isFavorite(addr("P2-33")));
        QTest::mouseClick(r.list()->viewport(), Qt::LeftButton, Qt::NoModifier, r.starPos(i));
        QVERIFY(r.library->favorites().contains(addr("P2-33")));
        QCOMPARE(r.sentCount(), 0);
        r.library->flush();
        PatchCatalog c2;
        Library l2(c2, r.path());
        l2.load();
        QVERIFY(l2.isFavorite(addr("P2-33")));
    }

    void starOnAnotherRowDoesNotMoveTheHighlight()
    {
        Rig r;
        QVERIFY(r.show());
        r.highlight("P1-11");
        const QModelIndex other = r.model()->indexOf(addr("P1-13"));
        r.list()->scrollTo(other);
        QTest::mouseClick(r.list()->viewport(), Qt::LeftButton, Qt::NoModifier, r.starPos(other));
        QVERIFY(r.library->isFavorite(addr("P1-13")));
        QCOMPARE(r.w->highlighted().value(), addr("P1-11"));
        QCOMPARE(r.sentCount(), 0);
    }

    void doubleClickOnAStarTogglesTwiceAndSendsNothing()
    {
        Rig r;
        QVERIFY(r.show());
        const QModelIndex i = r.highlight("P2-33");
        QWidget *vp = r.list()->viewport();
        const QPoint pos = r.starPos(i);
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QMouseEvent dbl(QEvent::MouseButtonDblClick, pos, vp->mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton,
                        Qt::NoModifier);
        QApplication::sendEvent(vp, &dbl);
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QVERIFY(!r.library->isFavorite(addr("P2-33")));  // on, then off
        QCOMPARE(r.sentCount(), 0);
        QVERIFY(!r.editor());
    }

    void spaceAndCtrlDToggle()
    {
        Rig r;
        QVERIFY(r.show());
        r.highlight("P4-21");
        r.list()->setFocus();
        QTest::keyClick(r.list(), Qt::Key_Space);
        QVERIFY(r.library->isFavorite(addr("P4-21")));
        QTest::keyClick(r.list(), Qt::Key_Space);
        QVERIFY(!r.library->isFavorite(addr("P4-21")));
        QTest::keyClick(r.w.get(), Qt::Key_D, Qt::ControlModifier);
        QVERIFY(r.library->isFavorite(addr("P4-21")));
        // Space in the search field is a space, not a toggle.
        r.w->searchField()->setFocus();
        QTest::keyClick(r.w->searchField(), Qt::Key_Space);
        QCOMPARE(r.w->searchField()->text(), QStringLiteral(" "));
        QVERIFY(r.library->isFavorite(addr("P4-21")));
        QCOMPARE(r.sentCount(), 0);
        QCOMPARE(r.library->favorites().size(), 1);
        r.library->flush();
        PatchCatalog c2;
        Library l2(c2, r.path());
        l2.load();
        QVERIFY(l2.isFavorite(addr("P4-21")));
    }

    void favouritesViewAddsAndDropsRowsLive()
    {
        Rig r;
        QVERIFY(r.show());
        r.library->setFavorite(addr("P1-12"), true);
        r.library->setFavorite(addr("P3-45"), true);
        QTest::keyClick(r.w.get(), Qt::Key_2, Qt::ControlModifier);
        QCOMPARE(r.model()->rowCount(), 2);
        r.library->setFavorite(addr("P2-11"), true);  // added from elsewhere
        QCOMPARE(r.model()->rowCount(), 3);
        QCOMPARE(r.model()->row(1).address, addr("P2-11"));
        r.highlight("P2-11");
        r.list()->setFocus();
        QTest::keyClick(r.list(), Qt::Key_Space);  // un-star the highlighted row
        QCOMPARE(r.model()->rowCount(), 2);
        QVERIFY(r.model()->rowOf(addr("P2-11")) < 0);
        QCOMPARE(r.w->highlighted().value(), addr("P3-45"));  // the row that took its place
        // The last one gone: the empty state.
        r.library->setFavorite(addr("P1-12"), false);
        r.library->setFavorite(addr("P3-45"), false);
        QCOMPARE(r.list()->emptyTitle(), QStringLiteral("NO FAVOURITES"));
        QCOMPARE(r.sentCount(), 0);
    }

    // ------------------------------------------------------------------ views and recents

    void viewsKeepTheHighlightWhenThePatchIsThere()
    {
        Rig r;
        QVERIFY(r.show());
        r.library->setFavorite(addr("P1-12"), true);
        r.library->setFavorite(addr("P3-45"), true);
        r.highlight("P1-12");  // not the current patch (P1-11)
        QTest::keyClick(r.w.get(), Qt::Key_2, Qt::ControlModifier);
        QCOMPARE(r.w->highlighted().value(), addr("P1-12"));
        r.highlight("P3-45");
        QTest::keyClick(r.w.get(), Qt::Key_1, Qt::ControlModifier);
        QCOMPARE(r.w->highlighted().value(), addr("P3-45"));
        QCOMPARE(r.sentCount(), 0);
    }

    void recentsOrderAndCountsAfterSends()
    {
        Rig r;
        QVERIFY(r.show());
        for (const char *t : {"P2-33", "P1-12", "P2-33", "P2-33", "P3-45"}) {
            r.highlight(t);
            r.list()->setFocus();
            QTest::keyClick(r.list(), Qt::Key_Return);
        }
        QCOMPARE(r.sentCount(), 15);
        QTest::keyClick(r.w.get(), Qt::Key_3, Qt::ControlModifier);
        QCOMPARE(r.model()->rowCount(), 3);
        QCOMPARE(r.model()->row(0).address, addr("P3-45"));
        QCOMPARE(r.model()->row(1).address, addr("P2-33"));
        QCOMPARE(r.model()->row(2).address, addr("P1-12"));
        QCOMPARE(r.model()->row(0).countText, QStringLiteral("×1"));
        QCOMPARE(r.model()->row(1).countText, QStringLiteral("×3"));
        QCOMPARE(r.model()->row(2).countText, QStringLiteral("×1"));
        // Counts show in the Recents view only.
        QTest::keyClick(r.w.get(), Qt::Key_1, Qt::ControlModifier);
        QVERIFY(r.model()->row(0).countText.isEmpty());
        // Sending in the Recents view updates the count in place: no reorder under the cursor.
        QTest::keyClick(r.w.get(), Qt::Key_3, Qt::ControlModifier);
        r.highlight("P1-12");
        r.list()->setFocus();
        QTest::keyClick(r.list(), Qt::Key_Return);
        QCOMPARE(r.model()->row(0).address, addr("P3-45"));
        QCOMPARE(r.model()->row(2).address, addr("P1-12"));
        QCOMPARE(r.model()->row(2).countText, QStringLiteral("×2"));
        QCOMPARE(r.w->highlighted().value(), addr("P1-12"));
        // Entering the view again shows the new order.
        QTest::keyClick(r.w.get(), Qt::Key_1, Qt::ControlModifier);
        QTest::keyClick(r.w.get(), Qt::Key_3, Qt::ControlModifier);
        QCOMPARE(r.model()->row(0).address, addr("P1-12"));
        QCOMPARE(r.model()->row(0).countText, QStringLiteral("×2"));
        // Persisted.
        r.library->flush();
        PatchCatalog c2;
        Library l2(c2, r.path());
        l2.load();
        QCOMPARE(l2.recents().size(), 3);
        QCOMPARE(l2.recents().at(0).address, addr("P1-12"));
        QCOMPARE(l2.recents().at(0).count, 2);
        QCOMPARE(l2.recents().at(1).address, addr("P3-45"));
        QCOMPARE(l2.recents().at(2).address, addr("P2-33"));
        QCOMPARE(l2.recents().at(2).count, 3);
    }

    void clearRecentsAsksFirst()
    {
        Rig r;
        QVERIFY(r.show());
        r.library->recordSend(addr("P2-33"));
        r.library->recordSend(addr("P1-12"));
        QTest::keyClick(r.w.get(), Qt::Key_3, Qt::ControlModifier);
        QCOMPARE(r.model()->rowCount(), 2);

        r.w->action(QStringLiteral("actionClearRecents"))->trigger();
        QDialog *box = r.w->clearRecentsBox();
        QVERIFY(box);
        QCOMPARE(box->findChild<QLabel *>()->text(), QStringLiteral("Clear the list of recently sent patches?"));
        QCOMPARE(box->windowTitle(), QStringLiteral("Clear Recents"));
        QVERIFY(box->isModal());
        QPushButton *clear = box->findChild<QPushButton *>(QStringLiteral("clearButton"));
        QVERIFY(clear);
        QCOMPARE(clear->text(), QStringLiteral("CLEAR"));
        QPushButton *cancel = box->findChild<QPushButton *>(QStringLiteral("cancelButton"));
        QVERIFY(cancel);
        QCOMPARE(cancel->text(), QStringLiteral("CANCEL"));
        QVERIFY(cancel->isDefault());
        QVERIFY(!clear->isDefault());
        QCOMPARE(box->focusWidget(), static_cast<QWidget *>(cancel));  // initial focus
        // DESIGN 10.5: client 360 x 104, 20 px margins, buttons 100 x 28 right aligned 8 px apart.
        QVERIFY(QTest::qWaitForWindowExposed(box));
        QCOMPARE(box->size(), QSize(360, 104));
        QCOMPARE(box->findChild<QLabel *>()->geometry().x(), 20);
        QCOMPARE(clear->geometry(), QRect(360 - 20 - 100 - 8 - 100, 104 - 20 - 28, 100, 28));
        QCOMPARE(cancel->geometry(), QRect(360 - 20 - 100, 104 - 20 - 28, 100, 28));
        // Asking again while the box is open does not stack a second one.
        r.w->action(QStringLiteral("actionClearRecents"))->trigger();
        QCOMPARE(r.w->clearRecentsBox(), box);
        QCOMPARE(r.model()->rowCount(), 2);  // nothing happened yet

        cancel->click();
        QTRY_VERIFY(!r.w->clearRecentsBox());  // closing may lag (a macOS sheet; deferred delete)
        QCOMPARE(r.library->recents().size(), 2);
        QCOMPARE(r.model()->rowCount(), 2);

        r.w->action(QStringLiteral("actionClearRecents"))->trigger();
        box = r.w->clearRecentsBox();
        QVERIFY(box);
        box->findChild<QPushButton *>(QStringLiteral("clearButton"))->click();
        QVERIFY(r.library->recents().isEmpty());
        QTRY_VERIFY(!r.w->clearRecentsBox());
        QCOMPARE(r.model()->rowCount(), 0);
        QCOMPARE(r.list()->emptyTitle(), QStringLiteral("NOTHING SENT YET"));
        r.library->flush();
        PatchCatalog c2;
        Library l2(c2, r.path());
        l2.load();
        QVERIFY(l2.recents().isEmpty());

        // Esc on the box is Cancel.
        r.library->recordSend(addr("P1-11"));
        r.w->action(QStringLiteral("actionClearRecents"))->trigger();
        box = r.w->clearRecentsBox();
        QVERIFY(box);
        QTest::keyClick(box, Qt::Key_Escape);
        QTRY_VERIFY(!r.w->clearRecentsBox());  // closing may lag (a macOS sheet; deferred delete)
        QCOMPARE(r.library->recents().size(), 1);
        QCOMPARE(r.sentCount(), 0);
    }

    // ------------------------------------------------------------------ rename

    void f2OpensTheEditorOnUserRowsOnly()
    {
        Rig r;
        QVERIFY(r.show());
        r.highlight("P1-12");
        QTest::keyClick(r.w.get(), Qt::Key_F2);
        QVERIFY(!r.editor());
        QVERIFY(r.w->message().contains(QStringLiteral("user")));
        r.highlight("U2-34");
        QTest::keyClick(r.w.get(), Qt::Key_F2);
        QLineEdit *e = r.editor();
        QVERIFY(e);
        QVERIFY(e->hasFocus());
        QVERIFY(e->text().isEmpty());
        QVERIFY(e->validator());
        QCOMPARE(r.sentCount(), 0);
        // Editor geometry: over (80, 2, Wl - 180, 24) of the row, Wl being the viewport width
        // the row paint uses, so the right edge is at Wl - 100 (DESIGN 7.4).
        const QRect row = r.list()->visualRect(r.model()->indexOf(addr("U2-34")));
        const int Wl = r.list()->viewport()->width();
        QCOMPARE(e->geometry(), QRect(row.x() + 80, row.y() + 2, Wl - 180, 24));
        QCOMPARE(e->geometry().x() + e->geometry().width(), Wl - 100);
        QCOMPARE(e->objectName(), QStringLiteral("nameEditor"));
    }

    void doubleClickRenamesUserRowsOnly()
    {
        Rig r;
        QVERIFY(r.show());
        for (const char *t : {"P1-14", "U1-14"}) {
            const QModelIndex i = r.model()->indexOf(addr(t));
            r.list()->scrollTo(i);
            const QPoint pos = r.list()->visualRect(i).center();
            QWidget *vp = r.list()->viewport();
            QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, pos);
            QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, pos);
            QMouseEvent dbl(QEvent::MouseButtonDblClick, pos, vp->mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton,
                            Qt::NoModifier);
            QApplication::sendEvent(vp, &dbl);
            QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, pos);
            QCOMPARE(r.editor() != nullptr, t[0] == 'U');
            // One send per double-click, from the first click only.
            QCOMPARE(r.sentCount(), 3);
            QVERIFY(r.backend.sentMessages() == MidiLink::patchMessages(addr(t), 1));
            r.backend.clearSent();
            if (r.editor())
                finish(r.editor(), Qt::Key_Escape);
        }
    }

    void editorLimitsTypingAndSanitisesPaste()
    {
        Rig r;
        QVERIFY(r.show());
        r.highlight("U3-11");
        r.w->renameHighlighted();
        QLineEdit *e = r.editor();
        QVERIFY(e);
        QTest::keyClicks(e, QStringLiteral("0123456789012345678901234"));
        QCOMPARE(e->text(), QStringLiteral("012345678901234567"));  // 18
        e->clear();
        e->insert(QStringLiteral("aéb€k"));  // the platform delivers these one at a time
        QCOMPARE(e->text(), QStringLiteral("abk"));
        e->clear();
        QApplication::clipboard()->setText(QStringLiteral("Täb\there\nline ü and a very long tail of text"));
        e->paste();
        QCOMPARE(e->text(), QStringLiteral("Tb here line  and "));  // sanitised, then cut at 18
        QVERIFY(e->text().size() <= PatchDelegate::MaxNameLength);
        for (const QChar c : e->text())
            QVERIFY(c.unicode() >= 0x20 && c.unicode() <= 0x7E);
        QCOMPARE(r.sentCount(), 0);
    }

    void enterEscAndEmptySemantics()
    {
        Rig r;
        QVERIFY(r.show());
        r.highlight("U1-11");
        r.w->renameHighlighted();
        QTest::keyClicks(r.editor(), QStringLiteral("Warm Pad"));
        finish(r.editor(), Qt::Key_Return);
        QVERIFY(!r.editor());
        QCOMPARE(r.catalog.name(addr("U1-11")), QStringLiteral("Warm Pad"));
        QVERIFY(r.list()->hasFocus());
        QCOMPARE(r.sentCount(), 0);  // Enter committed; it did not send
        QCOMPARE(r.reloadedName("U1-11"), QStringLiteral("Warm Pad"));

        // Esc cancels.
        r.w->renameHighlighted();
        QCOMPARE(r.editor()->text(), QStringLiteral("Warm Pad"));
        QTest::keyClicks(r.editor(), QStringLiteral("XYZ"));
        finish(r.editor(), Qt::Key_Escape);
        QVERIFY(!r.editor());
        QCOMPARE(r.catalog.name(addr("U1-11")), QStringLiteral("Warm Pad"));

        // Esc in the editor must not clear the search or commit.
        r.w->searchField()->setText(QStringLiteral("warm"));
        r.highlight("U1-11");
        r.w->renameHighlighted();
        QTest::keyClicks(r.editor(), QStringLiteral("Q"));
        finish(r.editor(), Qt::Key_Escape);
        QVERIFY(!r.editor());
        QCOMPARE(r.w->searchField()->text(), QStringLiteral("warm"));
        QCOMPARE(r.catalog.name(addr("U1-11")), QStringLiteral("Warm Pad"));
        r.w->searchField()->clear();

        // Empty clears the name.
        r.highlight("U1-11");
        r.w->renameHighlighted();
        r.editor()->clear();
        finish(r.editor(), Qt::Key_Return);
        QCOMPARE(r.catalog.name(addr("U1-11")), QString());
        QCOMPARE(r.model()->row(r.model()->rowOf(addr("U1-11"))).shown, QStringLiteral("U1-11"));
        QCOMPARE(r.reloadedName("U1-11"), QString());
        QCOMPARE(r.sentCount(), 0);
    }

    void renamedNameShowsInListDisplayAndSearch()
    {
        Rig r;
        QVERIFY(r.show());
        r.w->setCurrent(addr("U2-22"));
        const QString blank = r.w->display()->line1();
        r.highlight("U2-22");
        QSignalSpy reset(r.model(), &QAbstractItemModel::modelReset);
        QSignalSpy changed(r.model(), &QAbstractItemModel::dataChanged);
        r.w->renameHighlighted();
        QTest::keyClicks(r.editor(), QStringLiteral("Zebra Lead"));
        finish(r.editor(), Qt::Key_Return);
        const int row = r.model()->rowOf(addr("U2-22"));
        QCOMPARE(r.model()->row(row).shown, QStringLiteral("Zebra Lead"));
        QCOMPARE(reset.count(), 0);  // only that row repainted
        QVERIFY(changed.count() >= 1);
        QVERIFY(r.w->display()->line1().contains(QStringLiteral("ZEBRA LEAD"), Qt::CaseInsensitive));
        QVERIFY(r.w->display()->line1() != blank);
        r.w->searchField()->setText(QStringLiteral("zebra"));
        QCOMPARE(r.model()->rowCount(), 1);
        QCOMPARE(r.model()->row(0).address, addr("U2-22"));
        QVERIFY(!r.model()->row(0).ranges.isEmpty());
        // Renaming while a search is active drops a row that stops matching.
        r.highlight("U2-22");
        r.w->renameHighlighted();
        r.editor()->setText(QStringLiteral("Quartz"));
        finish(r.editor(), Qt::Key_Return);
        QCOMPARE(r.model()->rowCount(), 0);
        QVERIFY(r.list()->hasFocus());
        QCOMPARE(r.sentCount(), 0);
    }


    // ------------------------------------------------------------------ fix round

    // Opens the editor on a user slot and types; returns it.
    QLineEdit *typeName(Rig &r, const char *a, const QString &text)
    {
        r.highlight(a);
        r.w->renameHighlighted();
        QLineEdit *e = r.editor();
        if (e)
            QTest::keyClicks(e, text);
        return e;
    }

    void viewSwitchCommitsAnOpenEditor()
    {
        for (const Qt::Key key : {Qt::Key_2, Qt::Key_3}) {
            Rig r;
            QVERIFY(r.show());
            r.library->setFavorite(addr("P1-12"), true);
            QVERIFY(typeName(r, "U1-11", QStringLiteral("Kept")));
            QTest::keyClick(r.w.get(), key, Qt::ControlModifier);
            QCOMPARE(r.catalog.name(addr("U1-11")), QStringLiteral("Kept"));
            QVERIFY(!r.editor());
            QCOMPARE(r.reloadedName("U1-11"), QStringLiteral("Kept"));
            QCOMPARE(r.sentCount(), 0);
        }
        Rig r;  // the view buttons, and back to All
        QVERIFY(r.show());
        r.library->recordSend(addr("U1-12"));
        QVERIFY(typeName(r, "U1-12", QStringLiteral("Btn")));
        r.w->viewButton(PatchListModel::View::Recents)->click();
        QCOMPARE(r.catalog.name(addr("U1-12")), QStringLiteral("Btn"));
        QVERIFY(typeName(r, "U1-12", QStringLiteral("2")));
        QTest::keyClick(r.w.get(), Qt::Key_1, Qt::ControlModifier);
        QCOMPARE(r.catalog.name(addr("U1-12")), QStringLiteral("2"));  // the editor opened with the name selected
    }

    void favouriteToggleCommitsAnOpenEditor()
    {
        Rig r;
        QVERIFY(r.show());
        r.library->setFavorite(addr("U1-11"), true);
        r.library->setFavorite(addr("P1-12"), true);
        QTest::keyClick(r.w.get(), Qt::Key_2, Qt::ControlModifier);
        QVERIFY(typeName(r, "U1-11", QStringLiteral("Fav")));
        QTest::keyClick(r.w.get(), Qt::Key_D, Qt::ControlModifier);  // un-stars the row being edited
        QCOMPARE(r.catalog.name(addr("U1-11")), QStringLiteral("Fav"));
        QVERIFY(!r.library->isFavorite(addr("U1-11")));
        // And a star click on another row.
        QVERIFY(typeName(r, "U1-11", QStringLiteral("X")) == nullptr);  // no longer listed
        QCOMPARE(r.sentCount(), 0);
    }

    void clearRecentsAndSendsCommitAnOpenEditor()
    {
        Rig r;
        QVERIFY(r.show());
        r.library->recordSend(addr("P1-12"));
        r.library->recordSend(addr("U1-11"));
        QTest::keyClick(r.w.get(), Qt::Key_3, Qt::ControlModifier);
        QVERIFY(typeName(r, "U1-11", QStringLiteral("Sent")));
        r.w->sendHighlighted();  // the highlighted row is the one being edited; sending reorders the list
        QCOMPARE(r.catalog.name(addr("U1-11")), QStringLiteral("Sent"));
        QVERIFY(!r.editor());

        QVERIFY(typeName(r, "U1-11", QStringLiteral("Clr")));
        r.w->action(QStringLiteral("actionClearRecents"))->trigger();
        r.w->clearRecentsBox()->findChild<QPushButton *>(QStringLiteral("clearButton"))->click();
        QVERIFY(r.library->recents().isEmpty());
        QCOMPARE(r.catalog.name(addr("U1-11")), QStringLiteral("Clr"));
        QCOMPARE(r.reloadedName("U1-11"), QStringLiteral("Clr"));
    }

    void doubleClickInRecentsEditsThePressedAddress()
    {
        Rig r;
        QVERIFY(r.show());
        r.library->recordSend(addr("U1-11"));
        r.library->recordSend(addr("P1-12"));
        r.library->recordSend(addr("U1-13"));
        QTest::keyClick(r.w.get(), Qt::Key_3, Qt::ControlModifier);
        // Rows: U1-13, P1-12, U1-11. Double-click U1-11 (last row): the first click sends it,
        // it moves to the top, and the editor must open on it, not on what slid under the cursor.
        const QModelIndex i = r.model()->indexOf(addr("U1-11"));
        QWidget *vp = r.list()->viewport();
        const QPoint pos = r.list()->visualRect(i).center();
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QCOMPARE(r.model()->row(2).address, addr("U1-11"));  // in place: it did not move
        QMouseEvent dbl(QEvent::MouseButtonDblClick, pos, vp->mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton,
                        Qt::NoModifier);
        QApplication::sendEvent(vp, &dbl);
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QLineEdit *e = r.editor();
        QVERIFY(e);
        QCOMPARE(r.list()->visualRect(r.model()->indexOf(addr("U1-11"))).y() + 2, e->y());
        QCOMPARE(r.sentCount(), 3);
        QTest::keyClicks(e, QStringLiteral("Mine"));
        finish(e, Qt::Key_Return);
        QCOMPARE(r.catalog.name(addr("U1-11")), QStringLiteral("Mine"));
        QCOMPARE(r.catalog.name(addr("U1-13")), QString());

        // Pressing a preset that then moves away, with a user row sliding under: no editor.
        r.backend.clearSent();
        const QModelIndex p = r.model()->indexOf(addr("P1-12"));
        const QPoint ppos = r.list()->visualRect(p).center();
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, ppos);
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, ppos);
        QMouseEvent dbl2(QEvent::MouseButtonDblClick, ppos, vp->mapToGlobal(ppos), Qt::LeftButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(vp, &dbl2);
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, ppos);
        QVERIFY(!r.editor());
    }

    void starDoubleClickInFavouritesDoesNotToggleTheNextRow()
    {
        Rig r;
        QVERIFY(r.show());
        r.library->setFavorite(addr("P1-12"), true);
        r.library->setFavorite(addr("P1-13"), true);
        QTest::keyClick(r.w.get(), Qt::Key_2, Qt::ControlModifier);
        const QModelIndex i = r.model()->indexOf(addr("P1-12"));
        QWidget *vp = r.list()->viewport();
        const QPoint pos = r.starPos(i);
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QVERIFY(!r.library->isFavorite(addr("P1-12")));
        QMouseEvent dbl(QEvent::MouseButtonDblClick, pos, vp->mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton,
                        Qt::NoModifier);
        QApplication::sendEvent(vp, &dbl);
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, pos);
        QVERIFY(r.library->isFavorite(addr("P1-13")));  // slid into place, untouched
        QVERIFY(!r.library->isFavorite(addr("P1-12")));
        QCOMPARE(r.sentCount(), 0);
    }

    void focusOutCommitKeepsTheFocusWhereTheUserPutIt()
    {
        Rig r;
        QVERIFY(r.show());
        QVERIFY(typeName(r, "U1-11", QStringLiteral("Away")));
        QTest::mouseClick(r.w->searchField(), Qt::LeftButton);
        for (int i = 0; i < 3; ++i)
            QCoreApplication::processEvents();
        QCOMPARE(r.catalog.name(addr("U1-11")), QStringLiteral("Away"));
        QVERIFY(r.w->searchField()->hasFocus());
        QVERIFY(!r.editor());
        QCOMPARE(r.sentCount(), 0);
    }

    void typingAtTheLimitIsRefusedNotShifted()
    {
        Rig r;
        QVERIFY(r.show());
        QLineEdit *e = typeName(r, "U1-11", QStringLiteral("012345678901234567"));
        QVERIFY(e);
        QCOMPARE(e->text().size(), 18);
        e->setCursorPosition(0);
        QTest::keyClick(e, Qt::Key_Z);
        QCOMPARE(e->text(), QStringLiteral("012345678901234567"));  // the last character survives
        e->setCursorPosition(5);
        QTest::keyClick(e, Qt::Key_Z);
        QCOMPARE(e->text(), QStringLiteral("012345678901234567"));
        QTest::keyClick(e, Qt::Key_Backspace);
        QTest::keyClick(e, Qt::Key_Z);
        QCOMPARE(e->text(), QStringLiteral("0123z5678901234567"));
        // A paste at the limit still sanitises and cuts.
        e->setCursorPosition(0);
        QApplication::clipboard()->setText(QStringLiteral("ab\tcd"));
        e->paste();
        QCOMPARE(e->text().size(), 18);
    }

    void spaceAndCtrlDInAnEmptyFavouritesViewAddThePatch()
    {
        Rig r;
        QVERIFY(r.show());
        r.highlight("P2-22");
        QTest::keyClick(r.w.get(), Qt::Key_2, Qt::ControlModifier);
        QCOMPARE(r.model()->rowCount(), 0);
        r.list()->setFocus();
        QTest::keyClick(r.list(), Qt::Key_Space);  // the last highlight, P2-22
        QVERIFY(r.library->isFavorite(addr("P2-22")));
        QCOMPARE(r.model()->rowCount(), 1);
        QTest::keyClick(r.w.get(), Qt::Key_D, Qt::ControlModifier);  // now on the row: removes it
        QCOMPARE(r.model()->rowCount(), 0);
        QTest::keyClick(r.w.get(), Qt::Key_D, Qt::ControlModifier);
        QVERIFY(r.library->isFavorite(addr("P2-22")));
        QCOMPARE(r.sentCount(), 0);
        // Never highlighted anything before: the current patch.
        Rig q;
        QVERIFY(q.show());
        q.w->setCurrent(addr("P5-55"));
        q.library->setFavorite(addr("P1-11"), true);
        q.library->setFavorite(addr("P1-11"), false);
        QTest::keyClick(q.w.get(), Qt::Key_2, Qt::ControlModifier);
        QTest::keyClick(q.w.get(), Qt::Key_D, Qt::ControlModifier);
        QCOMPARE(q.library->favorites().size(), 1);
    }

    void editorCountsAndPaints()
    {
        Rig r;
        QVERIFY(r.show());
        r.highlight("U1-11");
        r.w->renameHighlighted();
        QLineEdit *e = r.editor();
        QVERIFY(e);
        QTest::keyClicks(e, QStringLiteral("abc"));
        const QImage a = r.list()->viewport()->grab().toImage();
        QTest::keyClicks(e, QStringLiteral("defghijklmnopqrstu"));
        QCOMPARE(e->text().size(), 18);
        const QImage b = r.list()->viewport()->grab().toImage();
        QVERIFY(a != b);
        finish(e, Qt::Key_Escape);
        QCOMPARE(r.sentCount(), 0);
    }
};

QTEST_MAIN(TestLibraryViews)
#include "test_library_views.moc"
