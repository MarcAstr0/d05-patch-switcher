#include "ui/ShortcutsDialog.h"

#include "ui/SectionRule.h"
#include "ui/Shortcuts.h"
#include "ui/Theme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QShortcut>
#include <QVBoxLayout>

namespace d05 {

namespace {

constexpr int kMargin = 20;
constexpr int kColumnW = 270;
constexpr int kColumnGap = 20;
constexpr int kRowH = 26;
constexpr int kKeyColumnW = 120;
constexpr int kCapH = 18;
constexpr int kCapGap = 4;
constexpr int kWordGap = 4;
constexpr int kSectionGap = 14;
constexpr int kRuleGap = 4;

QLabel *makeCap(const QString &text, QWidget *parent)
{
    auto *cap = new QLabel(text, parent);
    cap->setObjectName(QStringLiteral("keycap"));
    cap->setFont(Theme::font(Theme::Role::Keycap));
    cap->adjustSize();
    cap->setFixedHeight(kCapH);
    cap->setFixedWidth(qMax(kCapH, cap->sizeHint().width()));
    return cap;
}

// One row: caps (and joining words) from the left, the action text after the key column.
// The action wraps to a second line when it does not fit; the row then grows.
QWidget *makeRow(const Shortcuts::Row &row, QWidget *parent)
{
    auto *w = new QWidget(parent);
    int x = 0;
    for (const Shortcuts::Part &part : row.parts) {
        if (part.kind == Shortcuts::Part::Kind::Key) {
            QLabel *cap = makeCap(Shortcuts::partText(part), w);
            cap->move(x, (kRowH - kCapH) / 2);
            x += cap->width() + kCapGap;
        } else {
            auto *word = new QLabel(part.text, w);
            word->setProperty("role", QStringLiteral("dim"));
            word->adjustSize();
            x += kWordGap - kCapGap;  // the gap after the previous cap is replaced by the word's own
            word->move(x, (kRowH - word->sizeHint().height()) / 2);
            x += word->sizeHint().width() + kWordGap;
        }
    }
    x -= kCapGap;
    const int textX = qMax(kKeyColumnW, x + 8);
    auto *text = new QLabel(row.text, w);
    text->setProperty("role", QStringLiteral("body"));
    text->setObjectName(QStringLiteral("rowText"));
    text->setWordWrap(true);
    text->ensurePolished();
    const int textW = kColumnW - textX;
    const int textH = qMax(kRowH, text->heightForWidth(textW));
    text->setGeometry(textX, 0, textW, textH);
    text->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    w->setFixedSize(kColumnW, textH);
    return w;
}

}  // namespace

ShortcutsDialog::ShortcutsDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("shortcutsDialog"));
    setWindowTitle(QStringLiteral("Keyboard Shortcuts"));
    setModal(true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
    outer->setSpacing(0);
    auto *columns = new QHBoxLayout;
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(kColumnGap);
    QVBoxLayout *column[2];
    for (QVBoxLayout *&c : column) {
        c = new QVBoxLayout;
        c->setContentsMargins(0, 0, 0, 0);
        c->setSpacing(0);
        columns->addLayout(c);
    }
    for (const Shortcuts::Section &section : Shortcuts::sections()) {
        QVBoxLayout *c = column[section.column];
        if (c->count() > 0)
            c->addSpacing(kSectionGap);
        auto *rule = new SectionRule(section.title, false, this);
        rule->setFixedSize(kColumnW, SectionRule::Height);
        c->addWidget(rule);
        c->addSpacing(kRuleGap);
        for (const Shortcuts::Row &row : section.rows)
            c->addWidget(makeRow(row, this));
    }
    for (QVBoxLayout *c : column)
        c->addStretch(1);
    outer->addLayout(columns);
    outer->addSpacing(kSectionGap);

    auto *footer = new QLabel(QStringLiteral("Esc or F1 closes"), this);
    footer->setProperty("role", QStringLiteral("dim"));
    footer->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    outer->addWidget(footer);

    setFixedWidth(ClientWidth);
    layout()->activate();
    setFixedHeight(layout()->sizeHint().height());

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
