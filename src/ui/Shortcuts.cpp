#include "ui/Shortcuts.h"

namespace d05 {
namespace Shortcuts {

const QList<Binding> &bindings()
{
    static const QList<Binding> list = {
        {QStringLiteral("actionFocusSearch"), QKeySequence(Qt::Key_Slash)},
        {QStringLiteral("actionFocusSearchCtrlF"), QKeySequence(Qt::CTRL | Qt::Key_F)},
        {QStringLiteral("actionEscape"), QKeySequence(Qt::Key_Escape)},
        {QStringLiteral("actionPrev"), QKeySequence(Qt::Key_PageUp)},
        {QStringLiteral("actionNext"), QKeySequence(Qt::Key_PageDown)},
        {QStringLiteral("actionGridEntry"), QKeySequence(Qt::CTRL | Qt::Key_G)},
        {QStringLiteral("actionViewAll"), QKeySequence(Qt::CTRL | Qt::Key_1)},
        {QStringLiteral("actionViewFavourites"), QKeySequence(Qt::CTRL | Qt::Key_2)},
        {QStringLiteral("actionViewRecents"), QKeySequence(Qt::CTRL | Qt::Key_3)},
        {QStringLiteral("actionToggleFavourite"), QKeySequence(Qt::CTRL | Qt::Key_D)},
        {QStringLiteral("actionRename"), QKeySequence(Qt::Key_F2)},
        {QStringLiteral("actionAutoSend"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A)},
        {QStringLiteral("actionSettings"), QKeySequence(Qt::CTRL | Qt::Key_Comma)},
        {QStringLiteral("actionShortcuts"), QKeySequence(Qt::Key_F1)},
        {QStringLiteral("actionQuit"), QKeySequence(Qt::CTRL | Qt::Key_Q)},
    };
    return list;
}

QKeySequence key(const QString &action)
{
    for (const Binding &b : bindings())
        if (b.action == action)
            return b.key;
    return QKeySequence();
}

namespace {
Part lit(const char *text)
{
    return {Part::Kind::Key, QString::fromLatin1(text), QString()};
}
Part bound(const char *action)
{
    return {Part::Kind::Key, QString(), QString::fromLatin1(action)};
}
Part word(const char *text)
{
    return {Part::Kind::Word, QString::fromLatin1(text), QString()};
}
Row row(const QList<Part> &parts, const char *text)
{
    return {parts, QString::fromLatin1(text)};
}
}  // namespace

const QList<Section> &sections()
{
    static const QList<Section> list = {
        {QStringLiteral("FIND"), 0,
         {row({bound("actionFocusSearch"), word("or"), bound("actionFocusSearchCtrlF")}, "Focus the search field"),
          row({bound("actionEscape")}, "Clear the search"),
          row({bound("actionEscape"), word("again")}, "Back to the list")}},
        {QStringLiteral("LIST"), 0,
         {row({lit("Up"), lit("Down")}, "Move the highlight"),
          row({lit("Home"), lit("End")}, "First or last row"),
          row({lit("Enter")}, "Send highlighted patch"),
          row({lit("Space"), word("or"), bound("actionToggleFavourite")}, "Toggle favourite"),
          row({bound("actionRename")}, "Rename a user slot")}},
        {QStringLiteral("PLAY"), 1,
         {row({bound("actionPrev")}, "Previous patch"), row({bound("actionNext")}, "Next patch"),
          row({bound("actionGridEntry"), word("then"), lit("1-8"), lit("1-8")}, "Enter bank and number"),
          row({bound("actionAutoSend")}, "Auto-send on or off")}},
        {QStringLiteral("VIEW"), 1,
         {row({bound("actionViewAll")}, "All"), row({bound("actionViewFavourites")}, "Favourites"),
          row({bound("actionViewRecents")}, "Recents")}},
        {QStringLiteral("APP"), 1,
         {row({bound("actionSettings")}, "Settings"), row({bound("actionShortcuts")}, "This sheet"),
          row({bound("actionQuit")}, "Quit")}},
    };
    return list;
}

QString capText(const QKeySequence &key, Platform platform)
{
    if (platform == Platform::Mac) {
        if (key == QKeySequence(Qt::Key_Escape))
            return QStringLiteral("Esc");
        if (key == QKeySequence(Qt::Key_PageUp))
            return QStringLiteral("fn \u2191");
        if (key == QKeySequence(Qt::Key_PageDown))
            return QStringLiteral("fn \u2193");
        return key.toString(QKeySequence::NativeText);
    }
    // PortableText, not NativeText: the result must not depend on the platform running it.
    QString t = key.toString(QKeySequence::PortableText);
    t.replace(QStringLiteral("PgDown"), QStringLiteral("PgDn"));
    return t;
}

QString capText(const QKeySequence &key)
{
#ifdef Q_OS_MACOS
    return capText(key, Platform::Mac);
#else
    return capText(key, Platform::Other);
#endif
}

QString partText(const Part &part)
{
    return part.action.isEmpty() ? part.text : capText(key(part.action));
}

QString settingsHintKey()
{
#ifdef Q_OS_MACOS
    return QStringLiteral("CMD+,");
#else
    return QStringLiteral("CTRL+,");
#endif
}

}  // namespace Shortcuts
}  // namespace d05
