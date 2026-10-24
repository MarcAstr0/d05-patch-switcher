#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>

namespace d05 {

// The one table of keyboard bindings (SPEC FR-13). MainWindow registers its actions from
// bindings(), and the shortcut sheet (DESIGN 10.3) is generated from sections(), so the
// sheet cannot list a key the window does not have. Keys that belong to a widget and not to
// an action (arrows, Enter, Space, the digits of a grid entry) are literal parts of the rows.
namespace Shortcuts {

struct Binding {
    QString action;  // the QAction object name
    QKeySequence key;
};

struct Part {
    enum class Kind { Key, Word };
    Kind kind;
    QString text;    // the literal key cap text, or the joining word
    QString action;  // non-empty: the cap shows this binding's key, text is unused
};

struct Row {
    QList<Part> parts;
    QString text;  // the action, as the sheet words it
};

struct Section {
    QString title;
    int column;  // 0 left, 1 right
    QList<Row> rows;
};

const QList<Binding> &bindings();
// The key bound to an action name (empty when the action has no key).
QKeySequence key(const QString &action);
const QList<Section> &sections();

// Cap text for a key: the platform's native text (symbols on macOS), with Qt's "PgDown"
// written "PgDn" as in the design.
QString capText(const QKeySequence &key);
// The cap text of a part (resolves a binding).
QString partText(const Part &part);

// The first-run line 2 hint: the Settings key as the LCD can show it ("CTRL+," or "CMD+," on macOS).
QString settingsHintKey();

}  // namespace Shortcuts
}  // namespace d05
