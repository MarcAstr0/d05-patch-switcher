// DisplayWidget, glyph table, Theme and --screenshot, all under QT_QPA_PLATFORM=offscreen.
#include "app/Cli.h"
#include "app/Paths.h"
#include "app/Screenshot.h"
#include "core/PatchCatalog.h"
#include "core/SearchQuery.h"
#include "ui/DisplayWidget.h"
#include "ui/GlyphTable.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

using namespace d05;

namespace {

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

PatchAddress addr(const char *t)
{
    return *PatchAddress::parse(QString::fromLatin1(t));
}

QString pad(const QString &left, const QString &right = QString())
{
    return left.leftJustified(DisplayWidget::Columns - right.size(), QLatin1Char(' ')) + right;
}

}  // namespace

class TestDisplay : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Theme::install(*qobject_cast<QApplication *>(qApp)); }

    void glyphsBitForBit()
    {
        const auto row = [](char c) { return kGlyphs[int(c) - 0x20]; };
        const quint8 A[7] = {0x0E, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11};
        const quint8 zero[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
        const quint8 tilde[7] = {0x00, 0x00, 0x08, 0x15, 0x02, 0x00, 0x00};
        const quint8 g[7] = {0x00, 0x0F, 0x11, 0x11, 0x0F, 0x01, 0x0E};
        const quint8 colon[7] = {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
        for (int i = 0; i < 7; ++i) {
            QCOMPARE(row('A')[i], A[i]);
            QCOMPARE(row('0')[i], zero[i]);
            QCOMPARE(row('~')[i], tilde[i]);
            QCOMPARE(row('g')[i], g[i]);
            QCOMPARE(row(':')[i], colon[i]);
            QCOMPARE(row(' ')[i], quint8(0));
        }
    }

    void glyphsMatchDesignDocument()
    {
        QFile f(QStringLiteral(D05_SOURCE_DIR "/docs/design/DESIGN.md"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(f.readAll());
        const QString appendix = text.mid(text.indexOf(QLatin1String("## Appendix A")));
        const QRegularExpression re(
            QStringLiteral("^([0-9A-F]{2})  ((?:[0-9A-F]{2} ){6}[0-9A-F]{2})  "),
            QRegularExpression::MultilineOption);
        int n = 0;
        auto it = re.globalMatch(appendix);
        while (it.hasNext()) {
            const auto m = it.next();
            const int code = m.captured(1).toInt(nullptr, 16);
            const QStringList bytes = m.captured(2).split(QLatin1Char(' '));
            QCOMPARE(bytes.size(), 7);
            for (int r = 0; r < 7; ++r)
                QCOMPARE(int(kGlyphs[code - 0x20][r]), bytes[r].toInt(nullptr, 16));
            ++n;
        }
        QCOMPARE(n, kGlyphCount);
    }

    void themeTokensAndFonts()
    {
        QCOMPARE(Theme::lcdOn.name().toUpper(), QStringLiteral("#98CF1C"));
        QCOMPARE(Theme::panel.name().toUpper(), QStringLiteral("#202326"));
        QCOMPARE(qApp->palette().color(QPalette::Window), Theme::panel);
        QVERIFY(!qApp->styleSheet().isEmpty());
        QCOMPARE(qApp->font().pixelSize(), 13);
        QCOMPARE(Theme::font(Theme::Role::Wordmark).pixelSize(), 15);
        QCOMPARE(Theme::font(Theme::Role::Wordmark).letterSpacing(), 3.0);
        const QFont medium = Theme::font(Theme::Role::PanelLabel);
        if (Theme::mediumByWeight()) {
            QCOMPARE(medium.family(), QStringLiteral("Inter"));
            QCOMPARE(QFontInfo(medium).weight(), 500);
        } else {
            QCOMPARE(medium.family(), QStringLiteral("Inter Medium"));
            QCOMPARE(medium.weight(), QFont::Normal);
        }
        QCOMPARE(medium.pixelSize(), 10);
    }

    void geometryByPitch()
    {
        DisplayWidget d;
        const qreal dpr = d.devicePixelRatioF();
        if (dpr != 1.0)
            QSKIP("pixel sizes asserted at 1x");
        d.setPitch(3);
        QCOMPARE(d.size(), QSize(767, 79));
        d.setPitch(4);
        QCOMPARE(d.size(), QSize(1010, 98));
        d.setPitch(5);
        QCOMPARE(d.size(), QSize(1253, 117));
        QCOMPARE(DisplayWidget::pitchForWidth(800), 3);
        QCOMPARE(DisplayWidget::pitchForWidth(1042), 4);
        QCOMPARE(DisplayWidget::pitchForWidth(1285), 5);
    }

    void line1Layout()
    {
        DisplayWidget d;
        d.setPatch(addr("P1-11"), QStringLiteral("Fantasia"), QLatin1Char('D'));
        QCOMPARE(d.line1(), pad(QStringLiteral("P1-11:Fantasia"), QStringLiteral("DUAL")));
        QCOMPARE(d.line1().size(), 40);
        d.setPatch(addr("P1-18"), QStringLiteral("Alpha Omega Ensemble"), QLatin1Char('W'));
        QCOMPARE(d.line1(), pad(QStringLiteral("P1-18:Alpha Omega Ensemble"), QStringLiteral("WHOLE")));
        d.setPatch(addr("P2-11"), QStringLiteral("Some Name"), QLatin1Char('S'));
        QVERIFY(d.line1().endsWith(QStringLiteral("SPLIT")));
        d.setPatch(addr("U3-45"), QString(), QChar());
        QCOMPARE(d.line1(), pad(QStringLiteral("U3-45:")));
        d.setPatch(addr("U1-11"), QStringLiteral("Warm Pad Mk2"), QChar());
        QCOMPARE(d.line1(), pad(QStringLiteral("U1-11:Warm Pad Mk2")));
    }

    void line2Priority()
    {
        DisplayWidget d;
        d.setPatch(addr("P1-11"), QStringLiteral("Fantasia"), QLatin1Char('D'));
        QVERIFY(d.line2().startsWith(QStringLiteral("NO MIDI OUTPUT")));
        d.setConnected(true);
        QCOMPARE(d.line2(), pad(QStringLiteral("PRESET 1: ORIGINAL D-50")));
        d.setSearch(true, 23);
        QCOMPARE(d.line2(), pad(QStringLiteral("23 MATCHES")));
        d.setSearch(true, 1);
        QCOMPARE(d.line2(), pad(QStringLiteral("1 MATCH")));
        d.setSearch(true, 0);
        QCOMPARE(d.line2(), pad(QStringLiteral("NO MATCHES")));
        d.flashSent();
        QCOMPARE(d.line2(), pad(QStringLiteral("SENT")));
        d.setConnected(false);
        QCOMPARE(d.line2(), pad(QStringLiteral("NO MIDI OUTPUT")));
        d.setConnected(true);
        d.setSearch(false, 0);
        for (int g = 0; g < PatchAddress::GroupCount; ++g)
            QVERIFY(!DisplayWidget::groupLongName(g).isEmpty());
        QCOMPARE(DisplayWidget::groupLongName(0), QStringLiteral("USER 1"));
        QCOMPARE(DisplayWidget::groupLongName(13), QStringLiteral("PRESET 6: PN-D50-04"));
    }

    void disconnectClearsSentFlash()
    {
        DisplayWidget d;
        d.setPatch(addr("P1-11"), QStringLiteral("Fantasia"), QLatin1Char('D'));
        d.flashSent();  // disconnected: nothing happens
        QVERIFY(!d.sentActive());
        d.setConnected(true);
        QSignalSpy spy(&d, &DisplayWidget::sentChanged);
        d.flashSent();
        QVERIFY(d.sentActive());
        d.setConnected(false);
        QVERIFY(!d.sentActive());
        QCOMPARE(spy.count(), 2);
        QVERIFY(!spy.at(1).at(0).toBool());
        QVERIFY(d.line2().startsWith(QStringLiteral("NO MIDI OUTPUT")));
        d.setConnected(true);
        QTest::qWait(500);  // the stopped timer must not fire later
        QCOMPARE(spy.count(), 2);
        QCOMPARE(d.line2(), pad(QStringLiteral("PRESET 1: ORIGINAL D-50")));
    }

    void entryEchoAndKeyModeSwitch()
    {
        DisplayWidget d;
        d.setConnected(true);
        d.setPatch(addr("P3-45"), QStringLiteral("Whatever"), QLatin1Char('D'));
        d.setEntry(QStringLiteral("P3-4_"));
        QCOMPARE(d.line1(), pad(QStringLiteral("P3-4_")));
        d.setDisplayEntryEcho(false);
        QVERIFY(d.line1().startsWith(QStringLiteral("P3-45:Whatever")));
        d.setDisplayEntryEcho(true);
        d.setEntry(QString());
        d.setKeyModeOnLine2(true);
        QVERIFY(!d.line1().contains(QStringLiteral("DUAL")));
        QCOMPARE(d.line2(), pad(QStringLiteral("PRESET 3: PN-D50-01"), QStringLiteral("DUAL")));
    }

    void outOfRangeCharactersRenderAsQuestionMark()
    {
        DisplayWidget a, b;
        a.setLines(QStringLiteral("?"), QString());
        b.setLines(QString(QChar(0x00E9)), QString());
        QCOMPARE(a.fieldImage(), b.fieldImage());
    }

    void dotsAreDrawn()
    {
        DisplayWidget d;
        d.setLines(QStringLiteral("A"), QString());
        const QImage img = d.fieldImage();
        const int P = d.deviceDot();
        // 'A' row 0 is .###. : column 1 lit, column 0 unlit (ghost).
        QCOMPARE(QColor(img.pixel(3 * P, 2 * P)), Theme::lcdOn);
        QCOMPARE(QColor(img.pixel(2 * P, 2 * P)), Theme::lcdGhost);
        // The gap between dots stays lcdBack.
        QCOMPARE(QColor(img.pixel(3 * P + P - 1, 2 * P)), Theme::lcdBack);
        d.setLcdShowGhost(false);
        const QImage img2 = d.fieldImage();
        QCOMPARE(QColor(img2.pixel(3 * P, 2 * P)), Theme::lcdOn);
        QCOMPARE(QColor(img2.pixel(2 * P, 2 * P)), Theme::lcdBack);
    }

    void repaintsOnlyOnTextChange()
    {
        DisplayWidget d;
        PaintCounter counter;
        d.installEventFilter(&counter);
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        QTest::qWait(50);
        QVERIFY(counter.paints >= 1);

        d.setPatch(addr("P1-11"), QStringLiteral("Fantasia"), QLatin1Char('D'));
        d.setConnected(true);
        QTest::qWait(50);
        const int base = counter.paints;

        // Unchanged inputs: no repaint.
        d.setPatch(addr("P1-11"), QStringLiteral("Fantasia"), QLatin1Char('D'));
        d.setConnected(true);
        d.setSearch(false, 0);
        d.setEntry(QString());
        d.setLines(d.line1(), d.line2());
        d.setLines(d.line1().trimmed(), d.line2().trimmed());  // same text once padded
        d.setKeyModeOnLine2(false);
        QTest::qWait(50);
        QCOMPARE(counter.paints, base);

        // A real change repaints.
        int last = counter.paints;
        d.setSearch(true, 5);
        QTest::qWait(50);
        QVERIFY(counter.paints > last);

        // So do flash start, flash expiry, pitch change and ghost toggle.
        last = counter.paints;
        d.flashSent();
        QTest::qWait(50);
        QVERIFY(counter.paints > last);
        last = counter.paints;
        QTest::qWait(500);  // expiry
        QVERIFY(counter.paints > last);
        last = counter.paints;
        d.setPitch(4);
        QTest::qWait(50);
        QVERIFY(counter.paints > last);
        last = counter.paints;
        d.setLcdShowGhost(false);
        QTest::qWait(50);
        QVERIFY(counter.paints > last);
    }

    void atlasBuiltOncePerPitchAndDpr()
    {
        const int before = DisplayWidget::atlasBuildCount();
        DisplayWidget d;
        QCOMPARE(DisplayWidget::atlasBuildCount(), before + 1);
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        d.setPatch(addr("P1-11"), QStringLiteral("Fantasia"), QLatin1Char('D'));
        d.setSearch(true, 3);
        d.flashSent();
        QTest::qWait(450);
        d.setPitch(3);  // unchanged
        d.update();
        QTest::qWait(50);
        QCOMPARE(DisplayWidget::atlasBuildCount(), before + 1);
        QCOMPARE(d.atlas().width(), kGlyphCount * 5 * d.deviceDot());
        QCOMPARE(d.atlas().height(), 7 * d.deviceDot());
        d.setPitch(4);
        QCOMPARE(DisplayWidget::atlasBuildCount(), before + 2);
        d.setPitch(4);
        QCOMPARE(DisplayWidget::atlasBuildCount(), before + 2);
    }

    void dprChangeRebuildsOutsidePaint()
    {
        DisplayWidget d;
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        QTest::qWait(30);
        const int builds = DisplayWidget::atlasBuildCount();
        const QSize size = d.size();
        d.overrideDprForTest(2.0);  // the widget now disagrees with its cached resources
        d.grab();                    // runs paintEvent synchronously
        QCOMPARE(DisplayWidget::atlasBuildCount(), builds);  // paint rebuilt nothing
        QCOMPARE(d.size(), size);                            // and resized nothing
        QTRY_COMPARE(DisplayWidget::atlasBuildCount(), builds + 1);  // queued rebuild
        QTest::qWait(50);
        QCOMPARE(DisplayWidget::atlasBuildCount(), builds + 1);      // exactly once
        QCOMPARE(d.deviceDot(), DisplayWidget::devicePitch(3, 2.0, 960));
        QCOMPARE(d.atlas().height(), 7 * DisplayWidget::devicePitch(3, 2.0, 960));
    }

    void fractionalDprGeometry()
    {
        // DESIGN 5.1 table: (dpr, step p, window width W) -> device pitch P, gap g,
        // logical bezel size.
        struct Row { qreal dpr; int p; int W; int P; int g; int bw; int bh; };
        const Row table[] = {
            {1.0, 3, 800, 3, 1, 767, 79},   {1.0, 4, 1042, 4, 1, 1010, 98},  {1.0, 5, 1285, 5, 1, 1253, 117},
            {1.25, 3, 800, 3, 1, 622, 68},  {1.25, 3, 847, 3, 1, 622, 68},   {1.25, 3, 848, 4, 1, 816, 83},
            {1.25, 3, 1041, 4, 1, 816, 83}, {1.25, 4, 1042, 5, 1, 1010, 98}, {1.25, 5, 1285, 6, 1, 1205, 114},
            {1.5, 3, 800, 4, 1, 686, 73},   {1.5, 3, 1041, 4, 1, 686, 73},   {1.5, 4, 1042, 6, 2, 1010, 98},
            {1.5, 5, 1285, 7, 2, 1172, 111}, {1.5, 5, 2000, 7, 2, 1172, 111},
            {1.75, 3, 800, 5, 1, 733, 77},  {1.75, 4, 1042, 7, 2, 1010, 98},
            {1.75, 5, 1285, 8, 2, 1149, 109}, {1.75, 5, 1319, 8, 2, 1149, 109}, {1.75, 5, 1320, 9, 2, 1288, 120},
            {2.0, 3, 800, 6, 2, 767, 79},   {2.0, 4, 1042, 8, 2, 1010, 98},  {2.0, 5, 1285, 10, 2, 1253, 117},
        };
        for (const Row &r : table) {
            const QByteArray tag = QStringLiteral("dpr %1 p %2 W %3").arg(r.dpr).arg(r.p).arg(r.W).toLatin1();
            QVERIFY2(DisplayWidget::devicePitch(r.p, r.dpr, r.W) == r.P, tag.constData());
            QVERIFY2(DisplayWidget::deviceGap(r.P, r.dpr) == r.g, tag.constData());
            DisplayWidget d;
            d.setStep(r.p, r.W);
            d.rebuildForDprForTest(r.dpr);
            QVERIFY2(d.deviceDot() == r.P, tag.constData());
            QVERIFY2(d.deviceGap() == r.g, tag.constData());
            QCOMPARE(d.atlas().width(), kGlyphCount * 5 * r.P);
            QCOMPARE(d.atlas().height(), 7 * r.P);
            QVERIFY2(d.sizeHint() == QSize(r.bw, r.bh), tag.constData());
            QVERIFY2(d.size() == QSize(r.bw, r.bh), tag.constData());
        }
    }

    void atlasKeptWhenOnlyDprChangesAndPGAreEqual()
    {
        DisplayWidget d;
        d.setStep(4, 1100);
        d.rebuildForDprForTest(1.0);
        const int builds = DisplayWidget::atlasBuildCount();
        d.rebuildForDprForTest(1.0);
        QCOMPARE(DisplayWidget::atlasBuildCount(), builds);
        d.rebuildForDprForTest(1.25);  // P 4 -> 5
        QCOMPARE(DisplayWidget::atlasBuildCount(), builds + 1);
    }

    void sentFlash()
    {
        DisplayWidget d;
        d.setConnected(true);
        d.setPatch(addr("P1-11"), QStringLiteral("Fantasia"), QLatin1Char('D'));
        QSignalSpy spy(&d, &DisplayWidget::sentChanged);
        QElapsedTimer t;
        t.start();
        d.flashSent();
        QVERIFY(d.sentActive());
        QCOMPARE(d.line2(), pad(QStringLiteral("SENT")));
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.wait(1500));
        QCOMPARE(spy.count(), 2);
        QVERIFY(spy.at(1).at(0).toBool() == false);
        QVERIFY2(t.elapsed() >= 380, qPrintable(QString::number(t.elapsed())));  // 15.6 ms Windows clock tick
        QVERIFY(!d.sentActive());
        QCOMPARE(d.line2(), pad(QStringLiteral("PRESET 1: ORIGINAL D-50")));
    }

    void screenshotsOfThreeStates()
    {
        QTemporaryDir out;
        QVERIFY(out.isValid());
        PatchCatalog catalog;
        const PatchAddress p11 = addr("P1-11");
        int matches = 0;
        const SearchQuery brass(QStringLiteral("brass"));
        for (int i = 0; i < PatchAddress::CatalogueSize; ++i) {
            const PatchAddress a = *PatchAddress::fromCatalogueIndex(i);
            if (brass.matches(a, catalog.name(a)))
                ++matches;
        }
        QVERIFY(matches > 1);

        const QStringList states = {QStringLiteral("default"), QStringLiteral("disconnected"), QStringLiteral("search")};
        QImage shots[3];
        for (int i = 0; i < 3; ++i) {
            CliOptions o;
            o.screenshotFile = out.filePath(states[i] + QStringLiteral(".png"));
            o.screenshotState = states[i];
            QCOMPARE(runScreenshot(o), 0);
            QVERIFY(shots[i].load(o.screenshotFile));
            const qreal dpr = qApp->devicePixelRatio();
            QCOMPARE(shots[i].width(), int(960 * dpr));
            QCOMPARE(shots[i].height(), int(560 * dpr));

            // The display as rendered by a reference widget in the expected state.
            DisplayWidget ref;
            ref.setPatch(p11, catalog.name(p11), catalog.mode(p11));
            ref.setConnected(states[i] != QLatin1String("disconnected"));
            if (states[i] == QLatin1String("search"))
                ref.setSearch(true, matches);
            if (states[i] == QLatin1String("default"))
                QCOMPARE(ref.line2(), pad(QStringLiteral("PRESET 1: ORIGINAL D-50")));
            if (states[i] == QLatin1String("disconnected"))
                QVERIFY(ref.line2().startsWith(QStringLiteral("NO MIDI OUTPUT")));
            if (states[i] == QLatin1String("search"))
                QVERIFY(ref.line2().startsWith(QString::number(matches) + QStringLiteral(" MATCHES")));
            const QImage expected = ref.grab().toImage().convertToFormat(QImage::Format_RGB32);
            const QRect r(int(16 * dpr), int(46 * dpr), expected.width(), expected.height());
            QCOMPARE(shots[i].copy(r).convertToFormat(QImage::Format_RGB32), expected);
        }
        Paths::setOverride(QString());
        QVERIFY(shots[0] != shots[1]);
        QVERIFY(shots[0] != shots[2]);
        QVERIFY(shots[1] != shots[2]);
    }

    void displayLinesForNoMatchAndEntry()
    {
        DisplayWidget d;
        d.setPatch(addr("P1-11"), QStringLiteral("x"), QChar());
        d.setConnected(true);
        d.setSearch(true, 0);
        QVERIFY(d.line2().startsWith(QStringLiteral("NO MATCHES")));
        d.setSearch(false, 0);
        d.setDisplayEntryEcho(true);
        d.setEntry(QStringLiteral("P3-4_"));
        QVERIFY(d.line1().startsWith(QStringLiteral("P3-4_")));
    }

    void newScreenshotStatesRender()
    {
        QTemporaryDir out;
        QVERIFY(out.isValid());
        for (const char *st : {"nomatch", "entry", "rename", "clearrecents", "settings-error"}) {
            CliOptions o;
            o.screenshotFile = out.filePath(QString::fromLatin1(st) + QStringLiteral(".png"));
            o.screenshotState = QString::fromLatin1(st);
            o.screenshotSize = QSize(800, 480);
            QCOMPARE(runScreenshot(o), 0);
            QImage img;
            QVERIFY(img.load(o.screenshotFile));
            QVERIFY(img.width() > 0);
            if (o.screenshotState != QLatin1String("settings-error")) {
                QCOMPARE(img.width(), int(800 * qApp->devicePixelRatio()));
                QCOMPARE(img.height(), int(480 * qApp->devicePixelRatio()));
            }
        }
        Paths::setOverride(QString());
    }

    // Dragging the window edge changes the width every pixel; the frame is rebuilt only
    // when the width changes the pitch the rule yields (DESIGN 5.1).
    void resizeRebuildsOnlyWhenThePitchChanges()
    {
        DisplayWidget d;
        d.setStep(3, 800);
        const int base = d.frameBuildCount();
        for (int w = 801; w <= 1500; ++w)  // 1x: P = 3 up to 1041, then 4, then 5: only p changes it
            d.setStep(DisplayWidget::pitchForWidth(w), w);
        QCOMPARE(d.frameBuildCount() - base, 2);  // 3 -> 4 at 1042 and 4 -> 5 at 1285, nothing else
        for (int w = 1500; w >= 800; --w)
            d.setStep(DisplayWidget::pitchForWidth(w), w);
        QCOMPARE(d.frameBuildCount() - base, 4);

        // At 1.25 the device pitch also depends on the width inside one step.
        DisplayWidget f;
        f.rebuildForDprForTest(1.25);
        f.setStep(3, 800);
        const int start = f.frameBuildCount();
        int changes = 0;
        int last = DisplayWidget::devicePitch(3, 1.25, 800);
        for (int w = 801; w <= 1041; ++w) {
            f.setStep(3, w);
            const int p = DisplayWidget::devicePitch(3, 1.25, w);
            if (p != last)
                ++changes;
            last = p;
        }
        QVERIFY(changes >= 1);
        QCOMPARE(f.frameBuildCount() - start, changes);
        QCOMPARE(f.deviceDot(), last);
    }

    // A followed patch is not a send: it ends a SENT flash at once, the LED goes on for its
    // own 400 ms, and line 2 never says SENT for it.
    void followMoveEndsASentFlash()
    {
        DisplayWidget d;
        d.setConnected(true);
        d.setPatch(addr("P1-11"), QStringLiteral("Fantasia"), QLatin1Char('D'));
        QSignalSpy led(&d, &DisplayWidget::sentChanged);
        d.flashSent();
        QVERIFY(d.line2().startsWith(QStringLiteral("SENT")));
        QCOMPARE(led.count(), 1);
        QTest::qWait(200);
        d.setPatch(addr("P3-45"), QStringLiteral("Some Patch"), QLatin1Char('W'));
        d.flashActivity();  // the follow move, +200 ms into the SENT flash
        QVERIFY(!d.line2().startsWith(QStringLiteral("SENT")));
        QVERIFY(d.line2().startsWith(QStringLiteral("PRESET 3")));
        QVERIFY(!d.sentActive());
        QVERIFY(d.activityActive());  // the LED stays lit
        QCOMPARE(led.count(), 1);     // without a blink in between
        QTest::qWait(250);            // past the original SENT expiry: still lit, restarted by the follow
        QVERIFY(d.activityActive());
        QTRY_VERIFY_WITH_TIMEOUT(!d.activityActive(), 1000);
        QCOMPARE(led.count(), 2);
        QVERIFY(!d.line2().startsWith(QStringLiteral("SENT")));
    }

    void screenshotProcessExitCodes()
    {
        QTemporaryDir out;
        QVERIFY(out.isValid());
        const auto run = [&](const QStringList &args) {
            QProcess p;
            p.setProcessChannelMode(QProcess::MergedChannels);  // so a failing child shows its output
            p.start(QStringLiteral(D05_BINARY), args);
            if (!p.waitForFinished(30000))
                return -999;
            const QByteArray output = p.readAll();
            if (!output.isEmpty())
                qInfo().noquote() << "child output:" << QString::fromLocal8Bit(output);
            return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -998;
        };
        const QString png = out.filePath(QStringLiteral("a.png"));
        QCOMPARE(run({QStringLiteral("--screenshot"), png, QStringLiteral("--screenshot-state"),
                      QStringLiteral("recents")}),
                 0);
        QVERIFY(QFile::exists(png));
        QCOMPARE(run({QStringLiteral("--screenshot"), png, QStringLiteral("--screenshot-state"),
                      QStringLiteral("nope")}),
                 64);
        QCOMPARE(run({QStringLiteral("--screenshot"), png, QStringLiteral("--screenshot-size"),
                      QStringLiteral("799x480")}),
                 64);
        QCOMPARE(run({QStringLiteral("--screenshot"), png, QStringLiteral("--screenshot-size"),
                      QStringLiteral("800x480")}),
                 0);
    }

    void screenshotAtScaleFactor2()
    {
        QTemporaryDir out;
        QVERIFY(out.isValid());
        QProcess p;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_SCALE_FACTOR"), QStringLiteral("2"));
        p.setProcessEnvironment(env);
        p.setProcessChannelMode(QProcess::MergedChannels);
        const QString png = out.filePath(QStringLiteral("x2.png"));
        p.start(QStringLiteral(D05_BINARY), {QStringLiteral("--screenshot"), png});
        QVERIFY(p.waitForFinished(30000));
        const QByteArray output = p.readAll();
        if (p.exitCode() != 0 || p.exitStatus() != QProcess::NormalExit)
            qWarning().noquote() << "child output:" << QString::fromLocal8Bit(output);
        QCOMPARE(p.exitStatus(), QProcess::NormalExit);
        QCOMPARE(p.exitCode(), 0);
        QImage img;
        QVERIFY(img.load(png));
        QCOMPARE(img.size(), QSize(1920, 1120));
        // p = 3 at 2x: P = 6. Lit field at logical (16 + 19, 46 + 11); dot area 2P inside.
        // "P1-11": 'P' row 0 is #### . : column 0 lit, column 4 unlit (ghost).
        const int x0 = (16 + 19) * 2 + 12;
        const int y0 = (46 + 11) * 2 + 12;
        QCOMPARE(QColor(img.pixel(x0, y0)), Theme::lcdOn);
        QCOMPARE(QColor(img.pixel(x0 + 4 * 6, y0)), Theme::lcdGhost);
        // Cell 1 starts 6P = 36 px on; '1' row 0 (0x04) lights column 2 only.
        QCOMPARE(QColor(img.pixel(x0 + 36 + 2 * 6, y0)), Theme::lcdOn);
        QCOMPARE(QColor(img.pixel(x0 + 36, y0)), Theme::lcdGhost);
    }
};

QTEST_MAIN(TestDisplay)
#include "test_display.moc"
