#pragma once

#include <QColor>
#include <QFont>

class QApplication;
class QWidget;

namespace d05 {

// Colour tokens (DESIGN.md section 2), type roles (section 3) and the one-time
// application setup: fonts, palette, style sheet. Everything is compiled into the
// executable as Qt resources; nothing is read from the source tree at run time.
namespace Theme {

// 2.1 Panel and surfaces
inline const QColor panel(0x202326);
inline const QColor band(0x26292B);
inline const QColor bandEdge(0x3A3E41);
inline const QColor well(0x141617);
inline const QColor wellEdgeDark(0x0B0C0D);
inline const QColor wellEdgeLight(0x34383B);
inline const QColor fieldBack(0x0E0F10);
inline const QColor fieldBorder(0x3A3D40);
inline const QColor fieldBorderHover(0x4C5053);
inline const QColor rowHover(0x1B1D1F);
inline const QColor selection(0x33373B);
inline const QColor selectionBarIdle(0x6B6E70);
inline const QColor matchBack(0x2E4366);
// 2.2 Display
inline const QColor bezel(0x050606);
inline const QColor bezelRim(0x2E3134);
inline const QColor bezelRimTop(0x45494D);
inline const QColor lcdBack(0x071104);
inline const QColor lcdOn(0x98CF1C);
inline const QColor lcdGhost(0x0F1D09);
// 2.3 Lettering and rules
inline const QColor labelPrimary(0xE2E0D4);
inline const QColor labelSecondary(0xA3A293);
inline const QColor labelDim(0x6E6D64);
inline const QColor labelDisabled(0x5F5F58);
inline const QColor labelAccent(0x86A6D4);
inline const QColor ruleAccent(0x6E8EBA);
inline const QColor ruleSecondary(0x8C8B7E);
inline const QColor labelRed(0xE8565C);
inline const QColor arrow(0xBDB9A0);
inline const QColor starOff(0x5C5E5A);
inline const QColor tagBorder(0x4A4C48);
// 2.4 Buttons and LEDs
inline const QColor buttonSlot(0x121415);
inline const QColor buttonFace(0x3F4244);
inline const QColor buttonFaceHover(0x4A4D50);
inline const QColor buttonPressed(0x2F3133);
inline const QColor buttonHighlight(0x56595C);
inline const QColor buttonFaceLit(0x4B3B3C);
inline const QColor buttonDisabled(0x2C2E30);
inline const QColor ledOn(0xF0383F);
inline const QColor ledCore(0xFF9C8C);
inline const QColor ledOff(0x2A1A1A);
inline const QColor focusRing(0x9DBBE8);

// 3.2 Type roles.
enum class Role {
    PanelLabel,
    ListName,
    ListAddress,
    ListMeta,
    Field,
    StatusLabel,
    StatusValue,
    Body,
    Note,
    EmptyTitle,
    Keycap,
    Wordmark,
    WordmarkSub,
};

// The font for a role: Inter, pixel size and letter spacing per the table, Medium
// where the role says so. Cheap to call (cached after the first use).
QFont font(Role role);

// DESIGN 10.2: QSS cannot set letter spacing, so every QPushButton under `root` (a dialog or
// message box, with its buttons already added) gets the PanelLabel font. Call before show.
void applyDialogButtonFonts(QWidget *root);

// One-time setup, call once after the QApplication exists: registers the embedded
// Inter fonts, sets the application font (Inter Regular 13 px), the QPalette from
// the tokens and the style sheet from :/theme/app.qss. Returns false if a font or
// the style sheet could not be loaded (the app still runs with fallbacks).
bool install(QApplication &app);

// True when Medium resolves through the family "Inter" at weight 500; false when
// the "Inter Medium" family name is used at normal weight instead (DESIGN 3.1).
bool mediumByWeight();

}  // namespace Theme
}  // namespace d05
