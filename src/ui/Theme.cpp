#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QPalette>
#include <QPushButton>

namespace d05 {
namespace Theme {

namespace {

bool g_mediumByWeight = true;
bool g_checked = false;

QFont makeMedium(int pixelSize)
{
    QFont f;
    if (g_mediumByWeight) {
        f.setFamily(QStringLiteral("Inter"));
        f.setWeight(QFont::Medium);
    } else {
        f.setFamily(QStringLiteral("Inter Medium"));
        f.setWeight(QFont::Normal);
    }
    f.setPixelSize(pixelSize);
    return f;
}

QFont makeRegular(int pixelSize)
{
    QFont f(QStringLiteral("Inter"));
    f.setWeight(QFont::Normal);
    f.setPixelSize(pixelSize);
    return f;
}

void checkMedium()
{
    QFont probe(QStringLiteral("Inter"));
    probe.setWeight(QFont::Medium);
    probe.setPixelSize(10);
    g_mediumByWeight = (QFontInfo(probe).weight() == 500);
    g_checked = true;
}

QFont build(Role role)
{
    QFont f;
    qreal spacing = 0;
    switch (role) {
    case Role::PanelLabel: f = makeMedium(10); spacing = 0.5; break;
    case Role::ListName: f = makeRegular(14); break;
    case Role::ListAddress: f = makeMedium(13); break;
    case Role::ListMeta: f = makeMedium(10); spacing = 0.5; break;
    case Role::Field: f = makeRegular(14); break;
    case Role::StatusLabel: f = makeMedium(10); spacing = 0.5; break;
    case Role::StatusValue: f = makeRegular(11); break;
    case Role::Body: f = makeRegular(13); break;
    case Role::Note: f = makeRegular(12); break;
    case Role::EmptyTitle: f = makeMedium(11); spacing = 1.0; break;
    case Role::Keycap: f = makeMedium(10); spacing = 0.5; break;
    case Role::Wordmark: f = makeMedium(15); spacing = 3.0; break;
    case Role::WordmarkSub: f = makeMedium(10); spacing = 1.0; break;
    }
    if (spacing != 0)
        f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
    return f;
}

QPalette makePalette()
{
    QPalette p;
    p.setColor(QPalette::Window, panel);
    p.setColor(QPalette::Base, fieldBack);
    p.setColor(QPalette::AlternateBase, well);
    p.setColor(QPalette::Text, labelPrimary);
    p.setColor(QPalette::WindowText, labelPrimary);
    p.setColor(QPalette::ButtonText, labelPrimary);
    p.setColor(QPalette::Button, buttonFace);
    p.setColor(QPalette::Highlight, matchBack);
    p.setColor(QPalette::HighlightedText, labelPrimary);
    p.setColor(QPalette::PlaceholderText, labelDim);
    p.setColor(QPalette::ToolTipBase, fieldBack);
    p.setColor(QPalette::ToolTipText, labelPrimary);
    for (QPalette::ColorRole r : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, r, labelDisabled);
    return p;
}

}  // namespace

bool mediumByWeight()
{
    return g_mediumByWeight;
}

QFont font(Role role)
{
    // Roles are looked up after install(); before it, the medium check has not run
    // and Inter may not be registered, so nothing is cached then.
    static QFont cache[13];
    static bool filled[13] = {};
    const int i = static_cast<int>(role);
    if (!g_checked)
        return build(role);
    if (!filled[i]) {
        cache[i] = build(role);
        filled[i] = true;
    }
    return cache[i];
}

bool install(QApplication &app)
{
    bool ok = true;
    const int a = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Regular.ttf"));
    const int b = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Medium.ttf"));
    ok = ok && a >= 0 && b >= 0;
    checkMedium();

    app.setFont(makeRegular(13));
    app.setPalette(makePalette());

    QFile qss(QStringLiteral(":/theme/app.qss"));
    if (qss.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    else
        ok = false;
    return ok;
}

void applyDialogButtonFonts(QWidget *root)
{
    const QFont f = font(Role::PanelLabel);
    const auto buttons = root->findChildren<QPushButton *>();
    for (QPushButton *b : buttons)
        b->setFont(f);
}

}  // namespace Theme
}  // namespace d05
