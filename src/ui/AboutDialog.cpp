#include "ui/AboutDialog.h"

#include "ui/Nameplate.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace d05 {

QString AboutDialog::trademarkNotice()
{
    return QStringLiteral(
        "Roland, Boutique, D-05 and D-50 are trademarks of Roland Corporation. This app is not "
        "affiliated with or endorsed by Roland.");
}

AboutDialog::AboutDialog(const QString &version, QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("aboutDialog"));
    setWindowTitle(QStringLiteral("About"));
    setModal(true);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(20, 20, 20, 20);
    column->setSpacing(8);

    // The wordmark, never the owner's logo: this is the app's own identity.
    auto *mark = new Nameplate(QString(), this);
    mark->setObjectName(QStringLiteral("aboutWordmark"));
    mark->setFixedSize(Nameplate::wordmarkWidth(), Nameplate::SlotHeight);
    column->addWidget(mark, 0, Qt::AlignLeft);
    column->addSpacing(8);  // 8 + the 8 spacing on each side = 16 px gap

    const auto paragraph = [&](const QString &text, const char *role) {
        auto *l = new QLabel(text, this);
        l->setObjectName(QStringLiteral("aboutParagraph"));
        l->setProperty("role", QString::fromLatin1(role));
        l->setWordWrap(true);
        l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        l->setFixedWidth(ClientWidth - 40);
        l->ensurePolished();  // the style sheet's font decides the wrap
        l->setFixedHeight(l->heightForWidth(ClientWidth - 40));
        column->addWidget(l);
    };
    paragraph(QStringLiteral("Version %1").arg(version), "body");
    paragraph(QStringLiteral("Selects patches on a Roland Boutique D-05 over MIDI."), "body");
    paragraph(QStringLiteral("MIT licence. Copyright 2026 Mario Castro Squella."), "note");
    paragraph(trademarkNotice(), "note");
    paragraph(QStringLiteral("Inter typeface, SIL Open Font License 1.1."), "note");

    column->addSpacing(12);  // 20 px with the spacing
    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addStretch(1);
    auto *ok = new QPushButton(QStringLiteral("OK"), this);
    ok->setObjectName(QStringLiteral("okButton"));
    ok->setDefault(true);
    row->addWidget(ok);
    column->addLayout(row);
    Theme::applyDialogButtonFonts(this);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    ok->setFocus();

    setFixedWidth(ClientWidth);
    layout()->activate();
    setFixedHeight(layout()->totalHeightForWidth(ClientWidth));
}

QStringList AboutDialog::paragraphs() const
{
    QStringList out;
    for (const QLabel *l : findChildren<QLabel *>(QStringLiteral("aboutParagraph")))
        out << l->text();
    return out;
}

}  // namespace d05
