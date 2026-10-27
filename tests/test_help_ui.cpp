// T5.1: the shortcut sheet, the About box and the first-run hint, headless, on the fake
// MIDI backend. None of it may send MIDI.
#include "app/AppSettings.h"
#include "app/Cli.h"
#include "app/Paths.h"
#include "app/Screenshot.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiLink.h"
#include "ui/AboutDialog.h"
#include "ui/DisplayWidget.h"
#include "ui/MainWindow.h"
#include "ui/SettingsDialog.h"
#include "ui/Shortcuts.h"
#include "ui/ShortcutsDialog.h"
#include "ui/Theme.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QImage>
#include <QPushButton>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace d05;

namespace {

const QString kOut = QStringLiteral("Fake D-05 In");

struct Rig {
    QTemporaryDir dir;
    PatchCatalog catalog;
    std::unique_ptr<Library> library;
    FakeMidiBackend backend;
    std::unique_ptr<MidiLink> link;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<MainWindow> w;

    explicit Rig(const QString &port = QString())
    {
        Paths::setOverride(dir.path());
        library = std::make_unique<Library>(catalog, dir.filePath(QStringLiteral("library.json")));
        link = std::make_unique<MidiLink>(&backend);
        settings = std::make_unique<AppSettings>(dir.filePath(QStringLiteral("settings.ini")));
        w = std::make_unique<MainWindow>(catalog, *library, *link);
        w->setServices(settings.get());
        AppSettings::Values v;
        v.outputPort = port;
        w->applySettings(v);
    }
    ~Rig()
    {
        w.reset();
        settings.reset();
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
};

QString hint()
{
    return QStringLiteral("OPEN SETTINGS: ") + Shortcuts::settingsHintKey();
}

}  // namespace

class TestHelpUi : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        Theme::install(*qobject_cast<QApplication *>(qApp));
        QCoreApplication::setApplicationVersion(QStringLiteral("9.8.7"));
    }

    // The table is FR-13, and the window's actions are exactly the table.
    void tableMatchesRegisteredActions()
    {
        Rig r;
        QSet<QString> tabled;
        for (const Shortcuts::Binding &b : Shortcuts::bindings()) {
            tabled.insert(b.action);
            QAction *a = r.w->action(b.action);
            QVERIFY2(a, qPrintable(b.action));
            QCOMPARE(a->shortcut(), b.key);
        }
        QCOMPARE(tabled.size(), 18);  // T8.3: + next device, previous device, toggle engine
        const auto actions = r.w->findChildren<QAction *>();
        for (const QAction *a : actions)
            if (!a->shortcut().isEmpty())
                QVERIFY2(tabled.contains(a->objectName()), qPrintable(a->objectName()));
    }

    void sheetListsFr13AndNothingElse()
    {
        // FR-13, as portable text, in the order the sheet shows it.
        const char *literal[] = {"Up", "Down", "Home", "End", "Enter", "Space", "1-8", "1-8"};
        const char *keys[] = {"/", "Ctrl+F", "Esc", "Esc", "Ctrl+D", "F2", "Ctrl+1", "Ctrl+2", "Ctrl+3", "PgUp", "PgDown",
                              "Ctrl+G", "Ctrl+Shift+A", "Ctrl+,", "F1", "Ctrl+Q",
                              // T8.3, the DEVICE section (DESIGN-2.md 8.1); on macOS the device caps read Ctrl literally
                              "Ctrl+Tab", "Ctrl+Shift+Tab", "Ctrl+E"};
        Rig r;
        ShortcutsDialog d;
        QStringList expected;
        for (const char *c : literal)
            expected << QString::fromLatin1(c);
        for (const char *c : keys)
            expected << Shortcuts::capText(QKeySequence(QString::fromLatin1(c), QKeySequence::PortableText));
        QStringList caps = d.capTexts();
        std::sort(caps.begin(), caps.end());
        std::sort(expected.begin(), expected.end());
        QCOMPARE(caps, expected);
        // Every bound action's key appears as a cap, and every row is one of the table's.
        QStringList rows;
        for (const Shortcuts::Section &s : Shortcuts::sections())
            for (const Shortcuts::Row &row : s.rows)
                rows << row.text;
        QCOMPARE(d.rowTexts().size(), rows.size());
        const QStringList shown = d.rowTexts();
        QCOMPARE(QSet<QString>(shown.begin(), shown.end()), QSet<QString>(rows.begin(), rows.end()));
        for (const Shortcuts::Binding &b : Shortcuts::bindings())
            QVERIFY2(caps.contains(Shortcuts::capText(b.key)), qPrintable(b.action));
        QCOMPARE(d.windowTitle(), QStringLiteral("Keyboard Shortcuts"));
        QCOMPARE(d.size(), QSize(680, 438));  // DESIGN-2.md 8.1: 680 x 438 with the DEVICE section
    }

    void f1OpensAndEscCloses()
    {
        Rig r(kOut);
        QVERIFY(r.show());
        QVERIFY(!r.w->shortcutsDialog());
        QTest::keyClick(r.w.get(), Qt::Key_F1);
        QTRY_VERIFY(r.w->shortcutsDialog());
        QPointer<QDialog> box = r.w->shortcutsDialog();
        QVERIFY(QTest::qWaitForWindowExposed(box));
        QTest::keyClick(box, Qt::Key_Escape);
        QTRY_VERIFY(!box);
        // F1 inside the sheet closes it too.
        QVERIFY(r.show());
        QTest::keyClick(r.w.get(), Qt::Key_F1);
        QTRY_VERIFY(r.w->shortcutsDialog());
        box = r.w->shortcutsDialog();
        QVERIFY(QTest::qWaitForWindowExposed(box));
        QTest::keyClick(box, Qt::Key_F1);
        QTRY_VERIFY(!box);
        QVERIFY(r.backend.sentMessages().isEmpty());
    }

    void aboutShowsVersionAndNotice()
    {
        Rig r(kOut);
        QVERIFY(r.show());
        r.w->action(QStringLiteral("actionAbout"))->trigger();
        QDialog *box = r.w->aboutDialog();
        QVERIFY(box);
        auto *about = qobject_cast<AboutDialog *>(box);
        QVERIFY(about);
        const QStringList p = about->paragraphs();
        QCOMPARE(p.size(), 7);
        QCOMPARE(p[0], QStringLiteral("Version 9.8.7"));
        QCOMPARE(p[1], QStringLiteral("Selects patches on Roland Boutique D-05, JP-08 and JU-06A synths over MIDI."));
        QVERIFY(p[3].contains(QStringLiteral("MIT licence")));
        QCOMPARE(p[4], AboutDialog::trademarkNotice());
        QVERIFY(p[4].contains(QStringLiteral("not affiliated with or endorsed by Roland")));
        for (const char *name : {"D-05", "D-50", "JP-08", "JUPITER-8", "JU-06A", "JUNO-60", "JUNO-106"})  // SPEC-2 9
            QVERIFY2(p[4].contains(QString::fromLatin1(name)), name);
        QCOMPARE(p[5], QStringLiteral("Inter and Archivo typefaces, SIL Open Font License 1.1."));
        QCOMPARE(p[6], QStringLiteral("Uses Qt (LGPLv3) and RtMidi. See THIRD-PARTY-NOTICES."));
        QCOMPARE(box->width(), 440);
        QCOMPARE(box->windowTitle(), QStringLiteral("About"));
        QPushButton *ok = box->findChild<QPushButton *>(QStringLiteral("okButton"));
        QVERIFY(ok && ok->isDefault());
        QPointer<QDialog> guard = box;
        ok->click();
        QTRY_VERIFY(!guard);
        QVERIFY(r.backend.sentMessages().isEmpty());
    }

    void everyActionFitsItsLabel()
    {
        ShortcutsDialog d;
        const auto labels = d.findChildren<QLabel *>(QStringLiteral("rowText"));
        QCOMPARE(labels.size(), 21);
        for (const QLabel *l : labels) {
            QCOMPARE(l->width(), 154);
            QVERIFY2(!l->wordWrap(), qPrintable(l->text()));
            QVERIFY2(l->sizeHint().width() <= l->width(), qPrintable(l->text()));
        }
        // The key caps and joining words never run into the text column (x 156 in a 310 px column).
        for (const QLabel *l : d.findChildren<QLabel *>()) {
            if (l->objectName() == QLatin1String("rowText") || l->parentWidget() != &d)
                continue;
            if (l->geometry().y() < 20 + 16)
                continue;
            if (l->objectName() == QLatin1String("keycap") || l->property("role") == QLatin1String("dim")) {
                if (l->text().startsWith(QStringLiteral("Esc or")))
                    continue;
                const int col = l->x() >= 20 + 330 ? 350 : 20;
                QVERIFY2(l->geometry().right() - col < 156, qPrintable(l->text()));
            }
        }
    }

    void macCapMapping()
    {
        using P = Shortcuts::Platform;
        QCOMPARE(Shortcuts::capText(QKeySequence(Qt::Key_Escape), P::Mac), QStringLiteral("Esc"));
        QCOMPARE(Shortcuts::capText(QKeySequence(Qt::Key_PageUp), P::Mac), QStringLiteral("fn \u2191"));
        QCOMPARE(Shortcuts::capText(QKeySequence(Qt::Key_PageDown), P::Mac), QStringLiteral("fn \u2193"));
        QCOMPARE(Shortcuts::capText(QKeySequence(Qt::Key_F1), P::Mac), QStringLiteral("F1"));
        QCOMPARE(Shortcuts::capText(QKeySequence(Qt::Key_PageDown), P::Other), QStringLiteral("PgDn"));
        QCOMPARE(Shortcuts::capText(QKeySequence(Qt::Key_PageUp), P::Other), QStringLiteral("PgUp"));
        QCOMPARE(Shortcuts::capText(QKeySequence(Qt::Key_Escape), P::Other), QStringLiteral("Esc"));
    }

    void aboutLinkIsMouseOnlyAndPointsAtTheRepo()
    {
        AboutDialog d(QStringLiteral("1.0.0"));
        QLabel *link = d.linkLabel();
        QVERIFY(link);
        QCOMPARE(AboutDialog::projectUrl(), QStringLiteral("https://github.com/MarcAstr0/d05-patch-switcher"));
        QVERIFY(link->text().contains(QStringLiteral("href=\"https://github.com/MarcAstr0/d05-patch-switcher\"")));
        QVERIFY(link->text().contains(QStringLiteral(">github.com/MarcAstr0/d05-patch-switcher<")));
        QVERIFY(link->text().contains(QStringLiteral("text-decoration:none")));
        QVERIFY(link->openExternalLinks());
        QCOMPARE(link->textInteractionFlags(), Qt::TextInteractionFlags(Qt::LinksAccessibleByMouse));
        QCOMPARE(link->focusPolicy(), Qt::NoFocus);
        QCOMPARE(d.paragraphs().at(2), link->text());  // third line
    }

    void firstRunHintShowsWithNoPort()
    {
        Rig r;  // nothing configured
        QVERIFY(!r.link->isConnected());
        QVERIFY(r.w->display()->line2().startsWith(hint()));
#ifdef Q_OS_MACOS
        QVERIFY(hint().endsWith(QStringLiteral("CMD+,")));
#else
        QVERIFY(hint().endsWith(QStringLiteral("CTRL+,")));
#endif
        QVERIFY(r.backend.sentMessages().isEmpty());
    }

    void firstRunHintGoesAwayOnceAPortIsSaved()
    {
        Rig r;
        QVERIFY(r.show());
        QVERIFY(r.w->display()->line2().startsWith(hint()));
        r.w->openSettings();
        SettingsDialog *d = r.w->settingsDialog();
        QVERIFY(d);
        QVERIFY(d->outputCombo()->count() > 0);
        QTest::mouseClick(d->okButton(), Qt::LeftButton);
        QTRY_VERIFY(r.link->isConnected());
        QVERIFY(!r.w->display()->line2().contains(QStringLiteral("OPEN SETTINGS")));
        QVERIFY(!r.settings->outputPort().isEmpty());
    }

    void configuredButMissingPortKeepsNoMidiOutput()
    {
        Rig r(QStringLiteral("Hapax MIDI 1"));
        QVERIFY(!r.link->isConnected());
        QVERIFY(r.w->display()->line2().startsWith(QStringLiteral("NO MIDI OUTPUT")));
    }

    void screenshotStatesRender()
    {
        for (const char *state : {"shortcuts", "about", "firstrun"}) {
            QTemporaryDir out;
            QVERIFY(out.isValid());
            CliOptions o;
            o.screenshotFile = out.filePath(QStringLiteral("shot.png"));
            o.screenshotState = QString::fromLatin1(state);
            o.configDir = out.path();
            QCOMPARE(runScreenshot(o), 0);
            QImage img;
            QVERIFY(img.load(o.screenshotFile));
            QVERIFY(!img.isNull());
            Paths::setOverride(QString());
        }
    }
};

QTEST_MAIN(TestHelpUi)
#include "test_help_ui.moc"
