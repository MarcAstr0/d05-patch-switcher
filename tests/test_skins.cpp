// Theme as a runtime palette, the four skins, LedDisplayWidget, the nameplate faces and the
// version 2 --screenshot-state values (DESIGN-2.md 2 to 4), headless under QT_QPA_PLATFORM=offscreen.
#include "app/Cli.h"
#include "app/Paths.h"
#include "app/Screenshot.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "midi/FakeMidiBackend.h"
#include "midi/MidiLink.h"
#include "ui/BankGrid.h"
#include "ui/DisplayWidget.h"
#include "ui/GroupBar.h"
#include "ui/LedDisplayWidget.h"
#include "ui/MainWindow.h"
#include "ui/Nameplate.h"
#include "ui/PanelButton.h"
#include "ui/SectionRule.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QFontInfo>
#include <QImage>
#include <QLineEdit>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <atomic>
#include <cstdlib>
#include <new>

// Counting operator new, to measure what the LED display allocates while painting. Not on Windows
// (see test_main_window.cpp): the test that needs it skips there.
#ifndef Q_OS_WIN
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
#else
namespace {
std::atomic<long> g_allocs{0};
std::atomic<bool> g_counting{false};
}  // namespace
#endif

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

// The 45 version 1 colour tokens exactly as version 1's Theme.h declared them (DESIGN.md 2): the
// D-05 skin must resolve to these, byte for byte.
struct V1Colour {
    const char *name;
    QColor Theme::Palette::*member;
    quint32 rgb;
};
#define V1(n, v) {#n, &Theme::Palette::n, v}
const V1Colour kV1[] = {
    V1(panel, 0x202326), V1(band, 0x26292B), V1(bandEdge, 0x3A3E41), V1(well, 0x141617),
    V1(wellEdgeDark, 0x0B0C0D), V1(wellEdgeLight, 0x34383B), V1(fieldBack, 0x0E0F10),
    V1(fieldBorder, 0x3A3D40), V1(fieldBorderHover, 0x4C5053), V1(rowHover, 0x1B1D1F),
    V1(selection, 0x33373B), V1(selectionBarIdle, 0x6B6E70), V1(matchBack, 0x2E4366),
    V1(bezel, 0x050606), V1(bezelRim, 0x2E3134), V1(bezelRimTop, 0x45494D), V1(lcdBack, 0x071104),
    V1(lcdOn, 0x98CF1C), V1(lcdGhost, 0x0F1D09), V1(labelPrimary, 0xE2E0D4),
    V1(labelSecondary, 0xA3A293), V1(labelDim, 0x6E6D64), V1(labelDisabled, 0x5F5F58),
    V1(labelAccent, 0x86A6D4), V1(ruleAccent, 0x6E8EBA), V1(ruleSecondary, 0x8C8B7E),
    V1(labelRed, 0xE8565C), V1(arrow, 0xBDB9A0), V1(starOff, 0x5C5E5A), V1(tagBorder, 0x4A4C48),
    V1(buttonSlot, 0x121415), V1(buttonFace, 0x3F4244), V1(buttonFaceHover, 0x4A4D50),
    V1(buttonPressed, 0x2F3133), V1(buttonHighlight, 0x56595C), V1(buttonFaceLit, 0x4B3B3C),
    V1(buttonDisabled, 0x2C2E30), V1(ledOn, 0xF0383F), V1(ledCore, 0xFF9C8C), V1(ledOff, 0x2A1A1A),
    V1(focusRing, 0x9DBBE8),
};
#undef V1

// A skin's main window parts, for the window tests.
struct Rig {
    QTemporaryDir dir;
    PatchCatalog catalog;
    Library library{catalog, dir.filePath(QStringLiteral("library.json"))};
    FakeMidiBackend backend;
    MidiLink link{&backend};
    MainWindow window{catalog, library, link};
    Rig() { link.setChannel(1); }
};

QColor px(const QImage &img, int x, int y)
{
    return QColor(img.pixel(x, y));
}

}  // namespace

class TestSkins : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Theme::install(*qobject_cast<QApplication *>(qApp)); }
    void init() { Theme::setSkin(QStringLiteral("d05")); }
    void cleanup() { Theme::setSkin(QStringLiteral("d05")); }

    // ---------------------------------------------------------------- Theme as a runtime palette
    void d05PaletteIsVersion1Exactly()
    {
        QCOMPARE(Theme::skin().id, QStringLiteral("d05"));
        for (const V1Colour &c : kV1) {
            QVERIFY2(Theme::colors().*(c.member) == QColor::fromRgb(c.rgb), c.name);
        }
        QCOMPARE(int(sizeof(kV1) / sizeof(kV1[0])), 41);  // every colour of groups 2.1 to 2.4
    }

    void d05NewTokensStandInForVersion1Ones()
    {
        const Theme::Palette &c = Theme::colors();
        QCOMPARE(c.bankFace, c.buttonFace);
        QCOMPARE(c.viewFaceHover, c.buttonFaceHover);
        QCOMPARE(c.commitPressed, c.buttonPressed);
        QCOMPARE(c.transportHighlight, c.buttonHighlight);
        QCOMPARE(c.transportFaceLit, c.buttonFaceLit);
        QCOMPARE(c.controlFace, c.buttonFace);
        QCOMPARE(c.buttonLegend, c.labelPrimary);
        QCOMPARE(c.wedge, c.arrow);
        QCOMPARE(c.lensRim, c.buttonSlot);
        QCOMPARE(c.labelNumeral, c.labelAccent);
        QCOMPARE(c.labelCommit, c.labelRed);
        QCOMPARE(c.ruleTitleBack, c.panel);
        QCOMPARE(c.ruleBand, c.ruleAccent);
        QCOMPARE(c.ledBack, QColor::fromRgb(0x0A0302));
        QCOMPARE(c.ledSegOn, QColor::fromRgb(0xFF3A2A));
        QCOMPARE(c.ledSegGhost, QColor::fromRgb(0x1C0806));
    }

    void skinsDifferWhereDesign2SaysSo()
    {
        QCOMPARE(Theme::skinIds(), (QStringList{QStringLiteral("d05"), QStringLiteral("jp08"),
                                                QStringLiteral("ju06a-60"), QStringLiteral("ju06a-106")}));
        QVERIFY(Theme::setSkin(QStringLiteral("jp08")));
        QCOMPARE(Theme::colors().panel, QColor::fromRgb(0x121314));
        QCOMPARE(Theme::colors().labelAccent, QColor::fromRgb(0xF07A4A));
        QCOMPARE(Theme::colors().viewFace, QColor::fromRgb(0x2F74B5));
        QCOMPARE(Theme::colors().ledSegOn, QColor::fromRgb(0xFF6B1A));
        QCOMPARE(Theme::wordmarkColour(), QColor::fromRgb(0xF07A4A));
        QCOMPARE(Theme::skin().ledCells, 2);
        QVERIFY(Theme::skin().display == Theme::DisplayKind::Led);
        QVERIFY(Theme::skin().groupSlot == Theme::GroupSlot::Empty);
        QVERIFY(Theme::skin().sectionRule == Theme::RuleStyle::DoubleBar);
        QVERIFY(Theme::skin().buttonLed == Theme::LedStyle::Lens);
        QCOMPARE(Theme::skin().bankTitle, QStringLiteral("BANK"));
        QCOMPARE(Theme::skin().numberTitle, QStringLiteral("NUMBER"));
        QCOMPARE(Theme::skin().idleContext, QStringLiteral("JP-08"));
        QCOMPARE(Theme::skin().wordmarkSub, QStringLiteral("FOR ROLAND JP-08"));

        QVERIFY(Theme::setSkin(QStringLiteral("ju06a-60")));
        QCOMPARE(Theme::colors().panel, QColor::fromRgb(0x18191A));
        QCOMPARE(Theme::wordmarkColour(), QColor::fromRgb(0xF0EDE2));  // labelPrimary on the JUNO skins
        QCOMPARE(Theme::skin().ledCells, 3);
        QCOMPARE(Theme::skin().engine, 60);
        QVERIFY(Theme::skin().groupSlot == Theme::GroupSlot::Engine);
        QCOMPARE(Theme::skin().numberTitle, QStringLiteral("PATCH"));
        QCOMPARE(Theme::skin().engineTitle, QStringLiteral("ENGINE"));
        QCOMPARE(Theme::skin().idleContext, QStringLiteral("JU-06A  JUNO-60"));

        QVERIFY(Theme::setSkin(QStringLiteral("ju06a-106")));
        QCOMPARE(Theme::colors().panel, QColor::fromRgb(0x505155));
        QCOMPARE(Theme::colors().focusRing, QColor::fromRgb(0xC4DCFA));
        QCOMPARE(Theme::skin().engine, 106);
        QCOMPARE(Theme::skin().idleContext, QStringLiteral("JU-06A  JUNO-106"));
        QCOMPARE(Theme::wordmarkColour(), QColor::fromRgb(0xF4F3EE));

        QVERIFY(Theme::setSkin(QStringLiteral("d05")));
        QCOMPARE(Theme::colors().panel, QColor::fromRgb(0x202326));
        QCOMPARE(Theme::skin().searchPlaceholder, QStringLiteral("Search names or addresses: brass, P3-45"));
    }

    void skinIdForDevice()
    {
        QCOMPARE(Theme::skinIdFor(QStringLiteral("d05"), 0), QStringLiteral("d05"));
        QCOMPARE(Theme::skinIdFor(QStringLiteral("jp08"), 0), QStringLiteral("jp08"));
        QCOMPARE(Theme::skinIdFor(QStringLiteral("ju06a"), 60), QStringLiteral("ju06a-60"));
        QCOMPARE(Theme::skinIdFor(QStringLiteral("ju06a"), 106), QStringLiteral("ju06a-106"));
        QCOMPARE(Theme::skinIdFor(QStringLiteral("ju06a"), 0), QStringLiteral("ju06a-60"));
    }

    void setSkinEmitsOnceAndSwapsTheStyleSheet()
    {
        QSignalSpy spy(Theme::notifier(), &Theme::Notifier::skinChanged);
        QVERIFY(qApp->styleSheet().contains(QStringLiteral("Skin: d05 (D-05)")));
        QVERIFY(Theme::setSkin(QStringLiteral("jp08")));
        QCOMPARE(spy.count(), 1);
        QVERIFY(qApp->styleSheet().contains(QStringLiteral("Skin: jp08 (JP-08)")));
        QVERIFY(qApp->styleSheet().contains(QStringLiteral("#121314")));
        QCOMPARE(qApp->palette().color(QPalette::Window), QColor::fromRgb(0x121314));
        // The same skin again, and an unknown one, change nothing and emit nothing.
        QVERIFY(Theme::setSkin(QStringLiteral("jp08")));
        QVERIFY(!Theme::setSkin(QStringLiteral("nope")));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(Theme::skin().id, QStringLiteral("jp08"));
        QVERIFY(Theme::setSkin(QStringLiteral("ju06a-106")));
        QCOMPARE(spy.count(), 2);
        QVERIFY(qApp->styleSheet().contains(QStringLiteral("Skin: ju06a-106 (JU-06A, engine 106)")));
        // The skin switch adds no timer to the application (SPEC-2 5).
        QVERIFY(qApp->findChildren<QTimer *>().isEmpty());
    }

    void styleSheetsCarryNoLiteralHexOutsideTheirTokenBlock()
    {
        for (const QString &id : Theme::skinIds()) {
            QFile f(QStringLiteral(":/theme/%1.qss").arg(id));
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QString text = QString::fromUtf8(f.readAll());
            QVERIFY2(!text.contains(QStringLiteral("@{")), qPrintable(id));
            QVERIFY(text.contains(QStringLiteral("Skin: %1 (").arg(id)));
        }
    }

    // ---------------------------------------------------------------- nameplate faces (DESIGN-2.md 3.1)
    void nameplateFacesResolveAsDesigned()
    {
        // The two Archivo files report family "Archivo"; weight 600 and 700 resolve to them.
        QFont semi(QStringLiteral("Archivo"));
        semi.setPixelSize(16);
        semi.setWeight(QFont::DemiBold);
        QCOMPARE(QFontInfo(semi).family(), QStringLiteral("Archivo"));
        QCOMPARE(QFontInfo(semi).weight(), 600);
        QFont bold(QStringLiteral("Archivo"));
        bold.setPixelSize(16);
        bold.setWeight(QFont::Bold);
        QCOMPARE(QFontInfo(bold).family(), QStringLiteral("Archivo"));
        QCOMPARE(QFontInfo(bold).weight(), 700);

        // d05: Inter Medium 15, spacing 3.
        QVERIFY(Theme::wordmarkIsBaseRole());
        QCOMPARE(Theme::font(Theme::Role::Wordmark).family(), QStringLiteral("Inter"));
        QCOMPARE(Theme::font(Theme::Role::Wordmark).pixelSize(), 15);
        QCOMPARE(Theme::font(Theme::Role::Wordmark).letterSpacing(), 3.0);

        Theme::setSkin(QStringLiteral("jp08"));
        QVERIFY(!Theme::wordmarkIsBaseRole());
        const QFont jp = Theme::font(Theme::Role::Wordmark);
        QCOMPARE(QFontInfo(jp).family(), QStringLiteral("Archivo"));
        QCOMPARE(QFontInfo(jp).weight(), 600);
        QCOMPARE(jp.pixelSize(), 16);
        QCOMPARE(jp.letterSpacing(), 2.0);

        Theme::setSkin(QStringLiteral("ju06a-60"));
        const QFont ju = Theme::font(Theme::Role::Wordmark);
        QCOMPARE(QFontInfo(ju).family(), QStringLiteral("Archivo"));
        QCOMPARE(QFontInfo(ju).weight(), 700);
        QCOMPARE(ju.pixelSize(), 16);
        QCOMPARE(ju.letterSpacing(), 1.0);

        Theme::setSkin(QStringLiteral("d05"));
        QCOMPARE(Theme::font(Theme::Role::Wordmark).family(), QStringLiteral("Inter"));
    }

    void nameplateWordmarkWidthsFollowTheSkin()
    {
        // The code measures; the mockup values are 174 / 169 / 158 (DESIGN-2.md 3.1), Qt's 177 / 172 / 160.
        const auto advance = [] { return qRound(QFontMetricsF(Theme::font(Theme::Role::Wordmark))
                                                    .horizontalAdvance(QStringLiteral("PATCH SWITCHER"))); };
        const int d05 = advance();
        Theme::setSkin(QStringLiteral("jp08"));
        const int jp08 = advance();
        Theme::setSkin(QStringLiteral("ju06a-106"));
        const int ju = advance();
        QVERIFY2(qAbs(d05 - 177) <= 3, qPrintable(QString::number(d05)));
        QVERIFY2(qAbs(jp08 - 172) <= 3, qPrintable(QString::number(jp08)));
        QVERIFY2(qAbs(ju - 160) <= 3, qPrintable(QString::number(ju)));
    }

    void nameplateRebuildsOnSkinChange()
    {
        Nameplate plate{QString()};
        plate.resize(360, 28);
        plate.show();
        QVERIFY(QTest::qWaitForWindowExposed(&plate));
        const QImage d05 = plate.grab().toImage();
        Theme::setSkin(QStringLiteral("jp08"));
        QTest::qWait(30);
        const QImage jp = plate.grab().toImage();
        QVERIFY(d05 != jp);
        // The wordmark is drawn in the skin's colour: some pixel of it is the JP-08 accent, none the D-05's.
        const auto has = [](const QImage &img, const QColor &c) {
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                    if (QColor(img.pixel(x, y)) == c)
                        return true;
            return false;
        };
        QVERIFY(has(jp, QColor::fromRgb(0xF07A4A)));
        QVERIFY(!has(jp, QColor::fromRgb(0x86A6D4)));
        QVERIFY(has(d05, QColor::fromRgb(0x86A6D4)));
    }

    // ---------------------------------------------------------------- LedDisplayWidget
    void ledGeometryMatchesTheLcd()
    {
        struct Row { int p; int windowWidth; int win2; int win3; int bezelW; int bezelH; };
        const Row rows[] = {{3, 900, 103, 136, 767, 79}, {4, 1100, 130, 174, 1010, 98}, {5, 1300, 157, 212, 1253, 117}};
        for (const Row &r : rows) {
            for (const qreal dpr : {1.0, 2.0}) {
                DisplayWidget lcd;
                lcd.overrideDprForTest(dpr);
                lcd.setStep(r.p, r.windowWidth);
                lcd.rebuildForDprForTest(dpr);
                for (const char *skin : {"jp08", "ju06a-60"}) {
                    Theme::setSkin(QString::fromLatin1(skin));
                    LedDisplayWidget led;
                    led.rebuildForDprForTest(dpr);
                    led.setStep(r.p, r.windowWidth);
                    const QString what = QStringLiteral("p=%1 dpr=%2 %3").arg(r.p).arg(dpr).arg(QString::fromLatin1(skin));
                    QCOMPARE(led.size(), lcd.size());
                    QVERIFY2(led.size() == QSize(r.bezelW, r.bezelH), qPrintable(what));
                    QVERIFY2(led.windowWidth() == (led.cells() == 2 ? r.win2 : r.win3), qPrintable(what));
                    QCOMPARE(led.deviceDot(), lcd.deviceDot());
                }
                Theme::setSkin(QStringLiteral("d05"));
            }
        }
    }

    void ledCellCountAndTextPositions()
    {
        struct Row { int p; int windowWidth; int x3; int x2; int nameBase; int contextBase; };
        const Row rows[] = {{3, 900, 160, 127, 37, 62}, {4, 1100, 198, 154, 46, 79}, {5, 1300, 236, 181, 54, 96}};
        for (const Row &r : rows) {
            Theme::setSkin(QStringLiteral("ju06a-60"));
            LedDisplayWidget three;
            three.rebuildForDprForTest(1.0);
            three.setStep(r.p, r.windowWidth);
            QCOMPARE(three.cells(), 3);
            QCOMPARE(three.textLeft(), r.x3);
            QCOMPARE(three.nameBaseline(), r.nameBase);
            QCOMPARE(three.contextBaseline(), r.contextBase);
            Theme::setSkin(QStringLiteral("jp08"));
            LedDisplayWidget two;
            two.rebuildForDprForTest(1.0);
            two.setStep(r.p, r.windowWidth);
            QCOMPARE(two.cells(), 2);
            QCOMPARE(two.textLeft(), r.x2);
            QCOMPARE(two.nameBaseline(), r.nameBase);
        }
    }

    void ledDrawsSegmentsDashesAndGhosts()
    {
        Theme::setSkin(QStringLiteral("ju06a-60"));
        const Theme::Palette &c = Theme::colors();
        LedDisplayWidget led;
        led.rebuildForDprForTest(1.0);
        led.setStep(3, 900);  // P = 3
        QCOMPARE(led.deviceDot(), 3);
        led.setContent(QStringLiteral("7-1"), QString(), QString(), LedDisplayWidget::Tone::Secondary);
        const QImage img = led.frameImage();
        QCOMPARE(img.size(), QSize(767, 79));
        // The window, its frame, the field and the panel beside it.
        QCOMPARE(px(img, 0, 0), c.bezelRimTop);
        QCOMPARE(px(img, 0, 40), c.bezelRim);
        QCOMPARE(px(img, 135, 40), c.bezelRim);
        QCOMPARE(px(img, 60, 78), c.bezelRim);
        QCOMPARE(px(img, 5, 5), c.bezel);
        QCOMPARE(px(img, 11, 11), c.ledBack);
        QCOMPARE(px(img, 136, 5), c.panel);
        QCOMPARE(px(img, 700, 70), c.panel);
        // Cell k's origin is field (11, 11) + (3 + 11 k, 2) P; the middle of segment a at digit-box
        // (4 + 1.4, 1), g at (4 + 0.75, 7.5), the dot at (9.6, 14).
        const auto at = [&](int cell, double bx, double by) {
            return px(img, int(11 + (3 + 11 * cell) * 3 + bx * 3), int(11 + 2 * 3 + by * 3));
        };
        QCOMPARE(at(0, 5.4, 1), c.ledSegOn);     // '7': a
        QCOMPARE(at(0, 4.75, 7.5), c.ledSegGhost);  // '7': g unlit
        QCOMPARE(at(0, 8.05, 4.5), c.ledSegOn);  // '7': b (x 7 leans right by 0.1 * (15 - 4.5))
        QCOMPARE(at(0, 2.05, 4.5), c.ledSegGhost);  // '7': f unlit
        QCOMPARE(at(1, 4.75, 7.5), c.ledSegOn);  // '-': g only
        QCOMPARE(at(1, 5.4, 1), c.ledSegGhost);
        QCOMPARE(at(2, 8.05, 4.5), c.ledSegOn);  // '1': b, c
        QCOMPARE(at(2, 5.4, 1), c.ledSegGhost);  // '1': a unlit
        // The decimal point is never lit: ghost on every cell.
        for (int k = 0; k < 3; ++k)
            QCOMPARE(at(k, 9.6, 14), c.ledSegGhost);
    }

    void ledGhostSwitchOff()
    {
        Theme::setSkin(QStringLiteral("jp08"));
        const Theme::Palette &c = Theme::colors();
        LedDisplayWidget led;
        led.rebuildForDprForTest(1.0);
        led.setStep(3, 900);
        led.setContent(QStringLiteral("71"), QString(), QString(), LedDisplayWidget::Tone::Secondary);
        led.overrideGhostForTest(0);
        const QImage img = led.frameImage();
        const auto at = [&](int cell, double bx, double by) {
            return px(img, int(11 + (3 + 11 * cell) * 3 + bx * 3), int(11 + 2 * 3 + by * 3));
        };
        QCOMPARE(at(0, 5.4, 1), c.ledSegOn);
        QCOMPARE(at(0, 4.75, 7.5), c.ledBack);  // unlit: nothing at all
        QCOMPARE(at(1, 9.6, 14), c.ledBack);
        led.overrideGhostForTest(1);
        QCOMPARE(px(led.frameImage(), int(11 + 14 * 3 + 9.6 * 3), int(11 + 6 + 14 * 3)), c.ledSegGhost);
    }

    void ledReadoutIsPaddedCutAndBlankForUnknownCharacters()
    {
        Theme::setSkin(QStringLiteral("jp08"));
        LedDisplayWidget led;
        led.setContent(QStringLiteral("7"), QString(), QString(), LedDisplayWidget::Tone::Secondary);
        QCOMPARE(led.readout(), QStringLiteral("7 "));
        led.setContent(QStringLiteral("123"), QString(), QString(), LedDisplayWidget::Tone::Secondary);
        QCOMPARE(led.readout(), QStringLiteral("12"));
        led.setContent(QStringLiteral("x_"), QString(), QString(), LedDisplayWidget::Tone::Secondary);
        QCOMPARE(led.readout(), QStringLiteral("x_"));  // stored as given, drawn blank
        Theme::setSkin(QStringLiteral("ju06a-106"));
        led.setContent(QStringLiteral("---"), QString(), QString(), LedDisplayWidget::Tone::Secondary);
        QCOMPARE(led.cells(), 3);
        QCOMPARE(led.readout(), QStringLiteral("---"));
    }

    void ledDrawsNameAndContextInTheirTones()
    {
        Theme::setSkin(QStringLiteral("ju06a-60"));
        const Theme::Palette &c = Theme::colors();
        LedDisplayWidget led;
        led.rebuildForDprForTest(1.0);
        led.setStep(3, 900);
        const auto count = [&](const QRect &r, const QColor &colour) {
            const QImage img = led.frameImage();
            int n = 0;
            for (int y = r.top(); y <= r.bottom(); ++y)
                for (int x = r.left(); x <= r.right(); ++x)
                    n += px(img, x, y) == colour ? 1 : 0;
            return n;
        };
        const QRect nameBox(led.textLeft(), 0, led.textWidth(), 49);
        const QRect contextBox(led.textLeft(), 50, led.textWidth(), 29);
        led.setContent(QStringLiteral("4-5"), QString(), QString(), LedDisplayWidget::Tone::Secondary);
        QCOMPARE(count(nameBox, c.labelPrimary), 0);
        led.setContent(QStringLiteral("4-5"), QStringLiteral("LD Silo Lead"), QStringLiteral("FLIP SWITCH TO 60"),
                       LedDisplayWidget::Tone::Lit);
        QVERIFY(count(nameBox, c.labelPrimary) > 100);
        QVERIFY(count(contextBox, c.ledSegOn) > 30);
        QCOMPARE(count(contextBox, c.labelSecondary), 0);
        led.setContent(QStringLiteral("4-5"), QStringLiteral("LD Silo Lead"), QStringLiteral("NO MIDI OUTPUT"),
                       LedDisplayWidget::Tone::Red);
        QCOMPARE(count(contextBox, c.ledSegOn), 0);  // the strip was cleared, then redrawn in labelRed
        QVERIFY(count(contextBox, c.labelRed) > 30);
        QVERIFY(count(nameBox, c.labelPrimary) > 100);  // the name was not touched
        // A name cleared leaves the panel colour only.
        led.setContent(QStringLiteral("4-5"), QString(), QStringLiteral("NO MIDI OUTPUT"), LedDisplayWidget::Tone::Red);
        QCOMPARE(count(nameBox, c.labelPrimary), 0);
        // Text drawn stays inside the strips: the window and the digits are untouched.
        QCOMPARE(px(led.frameImage(), 5, 5), c.bezel);
        // The longest factory name and the longest context fit (DESIGN-2.md 4.2: 319 and 298 px at p = 3).
        led.setContent(QStringLiteral("4-5"), QStringLiteral("Alpha Omega Ensemble"),
                       QStringLiteral("FOUND JU-06A ON USB: OPEN SETTINGS"), LedDisplayWidget::Tone::Primary);
        const QFontMetrics nm(Theme::font(Theme::Role::DisplayName3));
        QVERIFY(nm.horizontalAdvance(QStringLiteral("Alpha Omega Ensemble")) < led.textWidth());
    }

    void ledRepaintsOnlyOnChange()
    {
        Theme::setSkin(QStringLiteral("ju06a-60"));
        LedDisplayWidget led;
        PaintCounter counter;
        led.installEventFilter(&counter);
        led.setStep(3, 900);
        led.show();
        QVERIFY(QTest::qWaitForWindowExposed(&led));
        QTest::qWait(50);
        led.setContent(QStringLiteral("4-5"), QStringLiteral("LD Silo Lead"), QStringLiteral("JU-06A  JUNO-60"),
                       LedDisplayWidget::Tone::Secondary);
        QTest::qWait(50);
        const int base = counter.paints;
        const int frames = led.frameBuildCount();
        const int atlases = LedDisplayWidget::atlasBuildCount();

        for (int i = 0; i < 5; ++i)  // unchanged: no repaint, nothing rebuilt
            led.setContent(QStringLiteral("4-5"), QStringLiteral("LD Silo Lead"), QStringLiteral("JU-06A  JUNO-60"),
                           LedDisplayWidget::Tone::Secondary);
        led.setStep(3, 900);
        led.setStep(3, 905);  // same step, same P
        QTest::qWait(50);
        QCOMPARE(counter.paints, base);
        QCOMPARE(led.frameBuildCount(), frames);

        int last = counter.paints;
        led.setContent(QStringLiteral("4-6"), QStringLiteral("LD Silo Lead"), QStringLiteral("JU-06A  JUNO-60"),
                       LedDisplayWidget::Tone::Secondary);
        QTest::qWait(50);
        QVERIFY(counter.paints > last);
        last = counter.paints;
        led.setContent(QStringLiteral("4-6"), QStringLiteral("LD Silo Lead"), QStringLiteral("SENT"),
                       LedDisplayWidget::Tone::Primary);
        QTest::qWait(50);
        QVERIFY(counter.paints > last);
        // Content changes redraw parts of the one frame: no frame or atlas is rebuilt for them.
        QCOMPARE(led.frameBuildCount(), frames);
        QCOMPARE(LedDisplayWidget::atlasBuildCount(), atlases);
    }

    void ledAtlasBuiltOncePerStepAndSkin()
    {
        Theme::setSkin(QStringLiteral("jp08"));
        const int before = LedDisplayWidget::atlasBuildCount();
        LedDisplayWidget led;
        led.rebuildForDprForTest(1.0);
        led.setStep(3, 900);
        const int afterFirst = LedDisplayWidget::atlasBuildCount();
        QVERIFY(afterFirst >= before + 1);
        QCOMPARE(led.atlas().width(), LedDisplayWidget::CharacterCount * 2 * 11 * led.deviceDot());
        QCOMPARE(led.atlas().height(), 15 * led.deviceDot());
        led.setContent(QStringLiteral("35"), QStringLiteral("Lo Brass"), QStringLiteral("JP-08"),
                       LedDisplayWidget::Tone::Secondary);
        led.setStep(3, 950);  // same P
        QCOMPARE(LedDisplayWidget::atlasBuildCount(), afterFirst);
        led.setStep(4, 1100);  // new P
        QCOMPARE(LedDisplayWidget::atlasBuildCount(), afterFirst + 1);
        led.setStep(4, 1100);
        QCOMPARE(LedDisplayWidget::atlasBuildCount(), afterFirst + 1);
        // The DPR change that leaves P alone keeps the atlas; a DPR that changes P rebuilds it.
        led.rebuildForDprForTest(2.0);
        QCOMPARE(led.deviceDot(), 8);
        QCOMPARE(LedDisplayWidget::atlasBuildCount(), afterFirst + 2);
        // A skin change rebuilds it once (the colours differ), the frame with it.
        const int frames = led.frameBuildCount();
        led.show();
        QVERIFY(QTest::qWaitForWindowExposed(&led));
        Theme::setSkin(QStringLiteral("ju06a-106"));
        QCOMPARE(LedDisplayWidget::atlasBuildCount(), afterFirst + 3);
        QCOMPARE(led.frameBuildCount(), frames + 1);
        QCOMPARE(led.cells(), 3);
        QCOMPARE(led.readout(), QStringLiteral("35 "));
        QCOMPARE(px(led.frameImage(), 0, 0), Theme::colors().bezelRimTop);
    }

    void ledSkinSwitchRepaintsOnce()
    {
        Theme::setSkin(QStringLiteral("jp08"));
        LedDisplayWidget led;
        led.setStep(3, 900);
        led.show();
        QVERIFY(QTest::qWaitForWindowExposed(&led));
        QTest::qWait(50);
        PaintCounter counter;
        led.installEventFilter(&counter);
        Theme::setSkin(QStringLiteral("ju06a-60"));
        QTest::qWait(100);
        QCOMPARE(counter.paints, 1);
    }

    void ledPaintAllocatesNothingPerPaint()
    {
#ifdef Q_OS_WIN
        QSKIP("allocation counting needs the replaced operator new, not used on Windows");
#endif
        Theme::setSkin(QStringLiteral("ju06a-60"));
        LedDisplayWidget led;
        led.setStep(3, 900);
        led.setContent(QStringLiteral("4-5"), QStringLiteral("LD Silo Lead"), QStringLiteral("JU-06A  JUNO-60"),
                       LedDisplayWidget::Tone::Secondary);
        led.show();
        QVERIFY(QTest::qWaitForWindowExposed(&led));
        const auto paintAll = [&](int n) {
            for (int i = 0; i < n; ++i) {
                QImage img(led.size(), QImage::Format_RGB32);
                QPainter p(&img);
                led.render(&p);
            }
        };
        paintAll(2);
        const int frames = led.frameBuildCount();
        const int atlases = LedDisplayWidget::atlasBuildCount();
        const int measured = PanelButton::metricsBuildCount();
        g_allocs = 0;
        g_counting = true;
        paintAll(10);
        g_counting = false;
        const long ten = g_allocs.load();
        g_allocs = 0;
        g_counting = true;
        paintAll(100);
        g_counting = false;
        const long hundred = g_allocs.load();
        QCOMPARE(led.frameBuildCount(), frames);
        QCOMPARE(LedDisplayWidget::atlasBuildCount(), atlases);
        QCOMPARE(PanelButton::metricsBuildCount(), measured);
        // What Qt itself allocates per paint (the image, the painter) does not grow with the content.
        QVERIFY2(hundred <= ten * 10 + 100, qPrintable(QStringLiteral("%1 vs %2").arg(hundred).arg(ten)));
    }

    // ---------------------------------------------------------------- the window
    void windowSwapsDisplaysAndKeepsEveryRectOnASkinSwitch()
    {
        Rig r;
        r.window.resize(960, 560);
        r.window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&r.window));
        const int bandTop = r.window.bandTop();
        const QRect grid = r.window.bankGrid()->geometry();
        const QRect prev = r.window.prevButton()->geometry();
        QVERIFY(r.window.display()->isVisible());
        QVERIFY(!r.window.ledDisplay()->isVisible());
        QVERIFY(r.window.groupBar()->isVisible());
        const QSize lcdSize = r.window.display()->size();
        const QString d05Placeholder = r.window.searchField()->placeholderText();

        for (const QString &id : {QStringLiteral("jp08"), QStringLiteral("ju06a-60"), QStringLiteral("ju06a-106")}) {
            QVERIFY(Theme::setSkin(id, &r.window));
            QTest::qWait(30);
            QVERIFY(!r.window.display()->isVisible());
            QVERIFY(r.window.ledDisplay()->isVisible());
            QCOMPARE(r.window.ledDisplay()->geometry(), QRect(QPoint(16, 46), lcdSize));
            QCOMPARE(r.window.ledDisplay()->cells(), id == QLatin1String("jp08") ? 2 : 3);
            QCOMPARE(r.window.bandTop(), bandTop);
            QCOMPARE(r.window.bankGrid()->geometry(), grid);
            QCOMPARE(r.window.prevButton()->geometry(), prev);
            QVERIFY(!r.window.groupBar()->isVisible());
            QCOMPARE(r.window.searchField()->placeholderText(), Theme::skin().searchPlaceholder);
            QVERIFY(r.window.searchField()->placeholderText() != d05Placeholder);
            QCOMPARE(r.window.ledDisplay()->context(), Theme::skin().idleContext);
        }
        QVERIFY(Theme::setSkin(QStringLiteral("d05"), &r.window));
        QTest::qWait(30);
        QVERIFY(r.window.display()->isVisible());
        QVERIFY(!r.window.ledDisplay()->isVisible());
        QVERIFY(r.window.groupBar()->isVisible());
        QCOMPARE(r.window.searchField()->placeholderText(), d05Placeholder);
        QCOMPARE(r.window.display()->size(), lcdSize);
    }

    void windowBuiltInASkinHasItFromTheStart()
    {
        Theme::setSkin(QStringLiteral("jp08"));
        Rig r;
        r.window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&r.window));
        QVERIFY(r.window.ledDisplay()->isVisible());
        QVERIFY(!r.window.display()->isVisible());
        QCOMPARE(r.window.ledDisplay()->cells(), 2);
        // The window's own pixels follow the skin: its background is the JP-08 panel.
        const QImage shot = r.window.grab().toImage();
        QCOMPARE(px(shot, 3, 3), Theme::colors().panel);
        QCOMPARE(px(shot, 3, shot.height() - 40), Theme::colors().band);
    }

    void panelPartsPaintInSkinColours()
    {
        Theme::setSkin(QStringLiteral("jp08"));
        PanelButton view(PanelButton::Kind::Led);
        view.setFaceSet(PanelButton::FaceSet::View);
        PanelButton commit(PanelButton::Kind::Led);
        commit.setFaceSet(PanelButton::FaceSet::Commit);
        PanelButton transport(PanelButton::Kind::WedgeLeft);
        transport.setFaceSet(PanelButton::FaceSet::Transport);
        const auto face = [](QWidget &w) { return px(w.grab().toImage(), PanelButton::Margin + 4, PanelButton::Margin + 4); };
        QCOMPARE(face(view), Theme::colors().viewFace);
        QCOMPARE(face(commit), Theme::colors().commitFace);
        QCOMPARE(face(transport), Theme::colors().transportFace);
        // The lens LED: ring lensRim radius 5, lens radius 4 (lit ledOn, unlit ledOff) at (22, 12).
        const QImage off = view.grab().toImage();
        QCOMPARE(px(off, PanelButton::Margin + 22, PanelButton::Margin + 12), Theme::colors().ledOff);
        view.setLit(true);
        const QImage lit = view.grab().toImage();
        QCOMPARE(px(lit, PanelButton::Margin + 22, PanelButton::Margin + 12), Theme::colors().ledOn);
        const QColor rim = px(lit, PanelButton::Margin + 26, PanelButton::Margin + 12);  // 4.5 px from the centre
        const QColor want = Theme::colors().lensRim;
        QVERIFY2(qAbs(rim.red() - want.red()) < 40 && qAbs(rim.green() - want.green()) < 40
                     && qAbs(rim.blue() - want.blue()) < 40,
                 qPrintable(rim.name()));
    }

    void sectionRuleStyles()
    {
        const auto render = [](const QString &title) {
            QImage img(200, SectionRule::Height, QImage::Format_RGB32);
            img.fill(Theme::colors().panel);
            QPainter p(&img);
            SectionRule::paint(&p, 0, 0, 200, title, true);
            return img;
        };
        Theme::setSkin(QStringLiteral("jp08"));
        QImage img = render(QStringLiteral("BANK"));
        QCOMPARE(px(img, 3, 3), Theme::colors().ruleAccent);  // the upper bar, y 2 to 4
        QCOMPARE(px(img, 3, 5), Theme::colors().panel);       // between the bars
        QCOMPARE(px(img, 3, 8), Theme::colors().ruleAccent);  // the lower bar, y 7 to 9
        QCOMPARE(px(img, 100, 1), Theme::colors().ruleTitleBack);  // the plate behind the title
        Theme::setSkin(QStringLiteral("ju06a-60"));
        img = render(QStringLiteral("PATCH"));
        QCOMPARE(px(img, 3, 0), Theme::colors().ruleBand);
        QCOMPARE(px(img, 3, 11), Theme::colors().ruleBand);
        QImage engine(200, SectionRule::Height, QImage::Format_RGB32);
        QPainter p(&engine);
        SectionRule::paint(&p, 0, 0, 200, QStringLiteral("ENGINE"), true, true);
        QCOMPARE(px(engine, 3, 6), Theme::colors().ruleBand2);
        // Secondary rules stay the hairline on every skin.
        QImage sec(200, SectionRule::Height, QImage::Format_RGB32);
        sec.fill(Theme::colors().panel);
        QPainter ps(&sec);
        SectionRule::paint(&ps, 0, 0, 200, QStringLiteral("MIDI OUTPUT"), false);
        QCOMPARE(px(sec, 3, 8), Theme::colors().ruleSecondary);
        QCOMPARE(px(sec, 3, 3), Theme::colors().panel);
        // The D-05 hairline: ruleAccent at row 8.
        Theme::setSkin(QStringLiteral("d05"));
        img = render(QStringLiteral("PATCH BANK"));
        QCOMPARE(px(img, 3, 8), Theme::colors().ruleAccent);
        QCOMPARE(px(img, 3, 3), Theme::colors().panel);
    }

    // ---------------------------------------------------------------- --screenshot-state
    void newScreenshotStatesRenderInTheirSkins()
    {
        QTemporaryDir out;
        QVERIFY(out.isValid());
        struct Row { const char *state; const char *skin; };
        const Row rows[] = {{"jp08", "jp08"},           {"ju06a-60", "ju06a-60"},   {"ju06a-106", "ju06a-106"},
                            {"flip-prompt", "ju06a-106"}, {"flip-hint", "ju06a-60"}, {"devices", "d05"}};
        QImage shots[6];
        int i = 0;
        for (const Row &row : rows) {
            CliOptions o;
            o.screenshotFile = out.filePath(QString::fromLatin1(row.state) + QStringLiteral(".png"));
            o.screenshotState = QString::fromLatin1(row.state);
            QCOMPARE(runScreenshot(o), 0);
            QVERIFY(shots[i].load(o.screenshotFile));
            const qreal dpr = qApp->devicePixelRatio();
            QCOMPARE(shots[i].width(), int(960 * dpr));
            QCOMPARE(shots[i].height(), int(560 * dpr));
            // The window background is the skin's panel; the process is back on the D-05 afterwards.
            Theme::setSkin(QString::fromLatin1(row.skin));
            QCOMPARE(px(shots[i], 3, 3), Theme::colors().panel);
            Theme::setSkin(QStringLiteral("d05"));
            QCOMPARE(Theme::skin().id, QStringLiteral("d05"));
            ++i;
        }
        Paths::setOverride(QString());
        // Every state is its own picture.
        for (int a = 0; a < 6; ++a)
            for (int b = a + 1; b < 6; ++b)
                QVERIFY2(shots[a] != shots[b], qPrintable(QStringLiteral("%1 == %2").arg(a).arg(b)));
        // The JU-06A prompt shows the readout colour in its context line.
        Theme::setSkin(QStringLiteral("ju06a-106"));
        const QColor lit = Theme::colors().ledSegOn;
        int n = 0;
        const QImage &prompt = shots[3];
        const qreal dpr = qApp->devicePixelRatio();
        for (int y = int(46 * dpr) + int(50 * dpr); y < int(46 * dpr) + int(79 * dpr); ++y)
            for (int x = int(16 * dpr) + int(160 * dpr); x < int(16 * dpr) + int(500 * dpr); ++x)
                n += QColor(prompt.pixel(x, y)) == lit ? 1 : 0;
        QVERIFY2(n > 30, qPrintable(QString::number(n)));
        Theme::setSkin(QStringLiteral("d05"));
    }

    void settingsDevicesRendersTheSkinnedDialog()
    {
        QTemporaryDir out;
        QVERIFY(out.isValid());
        CliOptions o;
        o.screenshotFile = out.filePath(QStringLiteral("settings-devices.png"));
        o.screenshotState = QStringLiteral("settings-devices");
        QCOMPARE(runScreenshot(o), 0);
        QImage img;
        QVERIFY(img.load(o.screenshotFile));
        QVERIFY(img.width() > 400 && img.height() > 300);
        Theme::setSkin(QStringLiteral("ju06a-106"));
        QCOMPARE(px(img, 3, 3), Theme::colors().panel);
        Theme::setSkin(QStringLiteral("d05"));
        Paths::setOverride(QString());
    }
};

QTEST_MAIN(TestSkins)
#include "test_skins.moc"
