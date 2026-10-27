#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QPalette>
#include <QPushButton>
#include <QWidget>
#include <QtDebug>

#include <cstring>

namespace d05 {
namespace Theme {

namespace {

bool g_mediumByWeight = true;
bool g_checked = false;
bool g_archivoLoaded = false;
bool g_wordmarkBase = true;
bool g_wordmarkWarned = false;

int g_skinIndex = -1;
Palette g_palette;
Skin g_skin;
QFont g_roleCache[kRoleCount];
bool g_roleFilled[kRoleCount] = {};
QString g_qssText[SkinTables::kSkinCount];  // read from the resources once, kept
bool g_qssLoaded[SkinTables::kSkinCount] = {};
Notifier *g_notifier = nullptr;

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

// DESIGN-2.md 3.1: the skin's wordmark face, with the documented fallbacks. Never a system font.
QFont buildWordmark(const SkinTables::Row &row, bool *usedBase)
{
    *usedBase = true;
    QFont base = makeMedium(15);
    base.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
    if (std::strcmp(row.wordmarkFamily, "Inter") == 0)
        return base;
    if (g_archivoLoaded) {
        const auto usable = [&](const QFont &f, const QString &family, int weight) {
            const QFontInfo info(f);
            return info.family() == family && qAbs(info.weight() - weight) <= 50;
        };
        const QString family = QString::fromLatin1(row.wordmarkFamily);
        QFont f(family);
        f.setPixelSize(row.wordmarkSize);
        f.setWeight(static_cast<QFont::Weight>(row.wordmarkWeight));
        f.setLetterSpacing(QFont::AbsoluteSpacing, row.wordmarkSpacing);
        if (usable(f, family, row.wordmarkWeight)) {
            *usedBase = false;
            return f;
        }
        // A GDI font database lists the SemiBold file under its legacy family name.
        QFont legacy(family + QStringLiteral(" SemiBold"));
        legacy.setPixelSize(row.wordmarkSize);
        legacy.setWeight(QFont::Normal);
        legacy.setLetterSpacing(QFont::AbsoluteSpacing, row.wordmarkSpacing);
        if (row.wordmarkWeight == 600 && QFontInfo(legacy).family() == family + QStringLiteral(" SemiBold")) {
            *usedBase = false;
            return legacy;
        }
    }
    if (!g_wordmarkWarned) {
        g_wordmarkWarned = true;
        qWarning("Theme: the %s nameplate face is not available; using Inter Medium", row.wordmarkFamily);
    }
    return base;
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
    case Role::DisplayName3: f = makeMedium(28); break;
    case Role::DisplayName4: f = makeMedium(37); break;
    case Role::DisplayName5: f = makeMedium(46); break;
    case Role::DisplayContext3: f = makeMedium(13); spacing = 1.0; break;
    case Role::DisplayContext4: f = makeMedium(17); spacing = 1.5; break;
    case Role::DisplayContext5: f = makeMedium(21); spacing = 2.0; break;
    case Role::RuleTitle: f = makeMedium(10); spacing = 1.0; break;
    case Role::SwitcherTab: f = makeMedium(10); spacing = 1.0; break;
    }
    if (spacing != 0)
        f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
    return f;
}

QPalette makeQPalette(const Palette &c)
{
    QPalette p;
    p.setColor(QPalette::Window, c.panel);
    p.setColor(QPalette::Base, c.fieldBack);
    p.setColor(QPalette::AlternateBase, c.well);
    p.setColor(QPalette::Text, c.labelPrimary);
    p.setColor(QPalette::WindowText, c.labelPrimary);
    p.setColor(QPalette::ButtonText, c.labelPrimary);
    p.setColor(QPalette::Button, c.buttonFace);
    p.setColor(QPalette::Highlight, c.matchBack);
    p.setColor(QPalette::HighlightedText, c.labelPrimary);
    p.setColor(QPalette::PlaceholderText, c.labelDim);
    p.setColor(QPalette::ToolTipBase, c.fieldBack);
    p.setColor(QPalette::ToolTipText, c.labelPrimary);
    for (QPalette::ColorRole r : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, r, c.labelDisabled);
    return p;
}

Palette makeColors(const SkinTables::Row &row)
{
    Palette p;
    int i = 0;
#define D05_THEME_FILL(name) p.name = QColor::fromRgb(row.colours[i++]);
    D05_COLOUR_TOKENS(D05_THEME_FILL)
#undef D05_THEME_FILL
    return p;
}

template <typename E>
E pick(const char *value, std::initializer_list<std::pair<const char *, E>> table, E fallback)
{
    for (const auto &e : table) {
        if (std::strcmp(value, e.first) == 0)
            return e.second;
    }
    return fallback;
}

Skin makeSkin(const SkinTables::Row &row)
{
    Skin s;
    s.id = QString::fromLatin1(row.id);
    s.model = QString::fromLatin1(row.model);
    s.engine = row.engine;
    s.display = pick<DisplayKind>(row.display, {{"lcd", DisplayKind::Lcd}, {"led", DisplayKind::Led}},
                                  DisplayKind::Lcd);
    s.groupSlot = pick<GroupSlot>(row.groupSlot,
                                  {{"groupBar", GroupSlot::GroupBar}, {"empty", GroupSlot::Empty},
                                   {"engine", GroupSlot::Engine}},
                                  GroupSlot::GroupBar);
    s.rowTag = pick<RowTag>(row.rowTag,
                            {{"keyMode", RowTag::KeyMode}, {"none", RowTag::None}, {"engine", RowTag::Engine}},
                            RowTag::KeyMode);
    s.sectionRule = pick<RuleStyle>(row.sectionRule,
                                    {{"hairline", RuleStyle::Hairline}, {"doubleBar", RuleStyle::DoubleBar},
                                     {"band", RuleStyle::Band}},
                                    RuleStyle::Hairline);
    s.buttonLed = pick<LedStyle>(row.buttonLed, {{"slit", LedStyle::Slit}, {"lens", LedStyle::Lens}},
                                 LedStyle::Slit);
    s.ledCells = row.ledCells;
    s.lcdShowGhost = row.lcdShowGhost;
    s.keyModeOnLine2 = row.keyModeOnLine2;
    s.displayEntryEcho = row.displayEntryEcho;
    s.ledShowGhost = row.ledShowGhost;
    s.modelName = QString::fromLatin1(row.modelName);
    s.groupTitle = QString::fromLatin1(row.groupTitle);
    s.bankTitle = QString::fromLatin1(row.bankTitle);
    s.numberTitle = QString::fromLatin1(row.numberTitle);
    s.engineTitle = QString::fromLatin1(row.engineTitle);
    s.wordmarkSub = QString::fromLatin1(row.wordmarkSub);
    s.idleContext = QString::fromLatin1(row.idleContext);
    s.searchPlaceholder = QString::fromLatin1(row.searchPlaceholder);
    return s;
}

int indexOf(const QString &id)
{
    for (int i = 0; i < SkinTables::kSkinCount; ++i) {
        if (id == QLatin1String(SkinTables::kSkins[i].id))
            return i;
    }
    return -1;
}

bool loadQss(int i)
{
    if (g_qssLoaded[i])
        return true;
    QFile qss(QStringLiteral(":/theme/%1.qss").arg(QLatin1String(SkinTables::kSkins[i].id)));
    if (!qss.open(QIODevice::ReadOnly))
        return false;
    g_qssText[i] = QString::fromUtf8(qss.readAll());
    g_qssLoaded[i] = true;
    return true;
}

// Swaps everything but the signal. Returns false if the style sheet could not be read (the
// palette still changes).
bool apply(int i)
{
    const SkinTables::Row &row = SkinTables::kSkins[i];
    g_skinIndex = i;
    g_palette = makeColors(row);
    g_skin = makeSkin(row);
    g_wordmarkBase = true;
    g_roleFilled[static_cast<int>(Role::Wordmark)] = false;
    const bool haveQss = loadQss(i);
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance())) {
        app->setPalette(makeQPalette(g_palette));
        if (haveQss)
            app->setStyleSheet(g_qssText[i]);
    }
    return haveQss;
}

}  // namespace

// SPEC-2 section 5: a skin switch repaints each window once. Qt's repaint manager paints a top-level
// window twice (once as a dirty widget, once as the dirty region) when the window itself is the first
// widget marked dirty after a paint. A style sheet change sends StyleChange to every widget in the
// order of a hash of widget pointers, so the window is first in about one switch in 25 (measured with
// gdb on addDirtyWidget: every double paint had the window first, no single paint did). Marking one
// visible child of each window first, which every switch repaints anyway, makes the order fixed.
void markChildFirst()
{
    const QWidgetList windows = QApplication::topLevelWidgets();
    for (QWidget *w : windows) {
        if (!w->isVisible())
            continue;
        const QList<QWidget *> kids = w->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget *c : kids) {
            if (c->isVisible()) {
                c->update();
                break;
            }
        }
    }
}

const Palette &colors()
{
    if (g_skinIndex < 0)
        apply(0);
    return g_palette;
}

const Skin &skin()
{
    if (g_skinIndex < 0)
        apply(0);
    return g_skin;
}

QColor wordmarkColour()
{
    const int i = g_skinIndex < 0 ? 0 : g_skinIndex;
    return QColor::fromRgb(SkinTables::kSkins[i].colours[SkinTables::kSkins[i].wordmarkColour]);
}

QStringList skinIds()
{
    QStringList ids;
    for (int i = 0; i < SkinTables::kSkinCount; ++i)
        ids << QString::fromLatin1(SkinTables::kSkins[i].id);
    return ids;
}

QString skinIdFor(const QString &model, int engine)
{
    if (model == QLatin1String("jp08"))
        return QStringLiteral("jp08");
    if (model == QLatin1String("ju06a"))
        return engine == 106 ? QStringLiteral("ju06a-106") : QStringLiteral("ju06a-60");
    return QStringLiteral("d05");
}

Notifier *notifier()
{
    if (!g_notifier)
        g_notifier = new Notifier;  // lives for the process; created on the GUI thread
    return g_notifier;
}

bool setSkin(const QString &id)
{
    const int i = indexOf(id);
    if (i < 0)
        return false;
    if (i == g_skinIndex)
        return true;
    if (!loadQss(i))  // the style sheet cannot be read: keep the previous skin untouched
        return false;
    markChildFirst();
    apply(i);
    emit notifier()->skinChanged();
    return true;
}

bool mediumByWeight()
{
    return g_mediumByWeight;
}

bool wordmarkIsBaseRole()
{
    font(Role::Wordmark);  // resolves the face for the current skin
    return g_wordmarkBase;
}

QFont font(Role role)
{
    // Roles are looked up after install(); before it, the medium check has not run
    // and Inter may not be registered, so nothing is cached then.
    const int i = static_cast<int>(role);
    if (role == Role::Wordmark) {
        if (!g_checked)
            return build(role);
        if (!g_roleFilled[i]) {
            const int s = g_skinIndex < 0 ? 0 : g_skinIndex;
            g_roleCache[i] = buildWordmark(SkinTables::kSkins[s], &g_wordmarkBase);
            g_roleFilled[i] = true;
        }
        return g_roleCache[i];
    }
    if (!g_checked)
        return build(role);
    if (!g_roleFilled[i]) {
        g_roleCache[i] = build(role);
        g_roleFilled[i] = true;
    }
    return g_roleCache[i];
}

bool install(QApplication &app)
{
    bool ok = true;
    const int a = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Regular.ttf"));
    const int b = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Medium.ttf"));
    ok = ok && a >= 0 && b >= 0;
    // The two nameplate faces (DESIGN-2.md 3.1): used for the wordmark only.
    const int c = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Archivo-SemiBold-nameplate.ttf"));
    const int d = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Archivo-Bold-nameplate.ttf"));
    g_archivoLoaded = c >= 0 && d >= 0;
    ok = ok && g_archivoLoaded;
    checkMedium();
    for (bool &f : g_roleFilled)
        f = false;

    app.setFont(makeRegular(13));
    ok = apply(0) && ok;
    emit notifier()->skinChanged();
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
