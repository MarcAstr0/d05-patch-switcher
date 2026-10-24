#include "ui/ShortcutsDialog.h"

#include "ui/SectionRule.h"
#include "ui/Shortcuts.h"
#include "ui/Theme.h"

#include <QFontMetrics>
#include <QLabel>
#include <QShortcut>

namespace d05 {

namespace {

constexpr int kMargin = 20;
constexpr int kColumnW = 310;
constexpr int kColumnGap = 20;
constexpr int kRowH = 26;
constexpr int kTextX = 156;
constexpr int kTextW = 154;
constexpr int kCapH = 18;
constexpr int kCapGap = 4;
constexpr int kWordGap = 4;
constexpr int kSectionGap = 16;
constexpr int kRuleGap = 4;
constexpr int kFooterH = 16;

QLabel *makeCap(const QString &text, QWidget *parent)
{
    auto *cap = new QLabel(text, parent);
    cap->setObjectName(QStringLiteral("keycap"));
    cap->setFont(Theme::font(Theme::Role::Keycap));
    cap->ensurePolished();
    cap->setFixedHeight(kCapH);
    cap->setFixedWidth(qMax(kCapH, cap->sizeHint().width()));
    return cap;
}

// One row at (x, y) in `parent`: caps and joining words from the left, the action at x 156,
// one line, never wrapped (a test checks each one fits).
void placeRow(const Shortcuts::Row &row, QWidget *parent, int x0, int y)
{
    int x = x0;
    for (const Shortcuts::Part &part : row.parts) {
        if (part.kind == Shortcuts::Part::Kind::Key) {
            QLabel *cap = makeCap(Shortcuts::partText(part), parent);
            cap->move(x, y + (kRowH - kCapH) / 2);
            x += cap->width() + kCapGap;
        } else {
            auto *word = new QLabel(part.text, parent);
            word->setProperty("role", QStringLiteral("dim"));
            word->ensurePolished();
            word->adjustSize();
            x += kWordGap - kCapGap;  // the word's own 4 px replace the gap after the cap
            word->move(x, y + (kRowH - word->height()) / 2);
            x += word->width() + kWordGap;
        }
    }
    auto *text = new QLabel(row.text, parent);
    text->setProperty("role", QStringLiteral("body"));
    text->setObjectName(QStringLiteral("rowText"));
    text->setWordWrap(false);
    text->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    text->setGeometry(x0 + kTextX, y, kTextW, kRowH);
}

}  // namespace

ShortcutsDialog::ShortcutsDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("shortcutsDialog"));
    setWindowTitle(QStringLiteral("Keyboard Shortcuts"));
    setModal(true);

    // Everything sits at the design's coordinates (DESIGN 10.3), content origin (20, 20).
    int y[2] = {kMargin, kMargin};
    bool first[2] = {true, true};
    for (const Shortcuts::Section &section : Shortcuts::sections()) {
        const int c = section.column;
        const int x0 = kMargin + c * (kColumnW + kColumnGap);
        if (!first[c])
            y[c] += kSectionGap;
        first[c] = false;
        auto *rule = new SectionRule(section.title, false, this);
        rule->setGeometry(x0, y[c], kColumnW, SectionRule::Height);
        y[c] += SectionRule::Height + kRuleGap;
        for (const Shortcuts::Row &row : section.rows) {
            placeRow(row, this, x0, y[c]);
            y[c] += kRowH;
        }
    }
    const int bottom = qMax(y[0], y[1]) + kSectionGap;
    auto *footer = new QLabel(QStringLiteral("Esc or F1 closes"), this);
    footer->setProperty("role", QStringLiteral("dim"));
    footer->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    footer->setGeometry(kMargin, bottom, 2 * kColumnW + kColumnGap, kFooterH);

    setFixedSize(2 * kMargin + 2 * kColumnW + kColumnGap, bottom + kFooterH + kMargin);

    // F1 closes the sheet it opened; Esc is the dialog's own reject.
    auto *f1 = new QShortcut(QKeySequence(Qt::Key_F1), this);
    f1->setContext(Qt::WindowShortcut);
    connect(f1, &QShortcut::activated, this, &QDialog::reject);
}

QStringList ShortcutsDialog::capTexts() const
{
    QStringList out;
    for (const QLabel *l : findChildren<QLabel *>(QStringLiteral("keycap")))
        out << l->text();
    return out;
}

QStringList ShortcutsDialog::rowTexts() const
{
    QStringList out;
    for (const QLabel *l : findChildren<QLabel *>(QStringLiteral("rowText")))
        out << l->text();
    return out;
}

}  // namespace d05
