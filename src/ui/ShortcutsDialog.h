#pragma once

#include <QDialog>

namespace d05 {

// The keyboard shortcut sheet (DESIGN.md 10.3), opened with F1 and generated from
// Shortcuts::sections(). Window 680 x 412, modal, closed by Esc or F1.
class ShortcutsDialog : public QDialog {
    Q_OBJECT
public:
    static constexpr int ClientWidth = 680;

    explicit ShortcutsDialog(QWidget *parent = nullptr);

    // The text of every key cap, in table order (tests).
    QStringList capTexts() const;
    // The action text of every row, in table order (tests).
    QStringList rowTexts() const;
};

}  // namespace d05
