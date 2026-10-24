#pragma once

#include "core/PatchAddress.h"
#include "ui/PanelButtonField.h"

namespace d05 {

// The PATCH BANK and PATCH NUMBER rows (DESIGN.md 6.4): 8 + 8 LED buttons with numerals,
// always showing the current patch, plus the panel-style two-step entry (group, bank,
// number). Local coordinates: the widget starts 4 px left of the column and at the bank
// rule; faces at x = 4 + 50 col, y = 27 (bank row) and 90 (number row).
class BankGrid : public PanelButtonField {
    Q_OBJECT
public:
    static constexpr int WidgetWidth = 402;
    static constexpr int WidgetHeight = 118;

    enum class Stage { Idle, AwaitBank, AwaitNumber };

    explicit BankGrid(QWidget *parent = nullptr);

    // The current patch: lights its bank and number, ends any entry.
    void setCurrent(const PatchAddress &current);
    PatchAddress current() const { return m_current; }

    // Starts an entry in `group`, awaiting the bank. Lights nothing in the grid.
    void beginEntry(int group);
    void cancelEntry();
    Stage stage() const { return m_stage; }
    int entryGroup() const { return m_entryGroup; }
    int entryBank() const { return m_entryBank; }
    // "P3-__" or "P3-4_" while an entry is pending, empty otherwise.
    QString entryText() const;

    // A bank (1..8) or number (1..8) button pressed by click, Enter or Space, or a digit.
    void pressBank(int bank);
    void pressNumber(int number);

    // Test access: the slot of a bank / number button and its lit state.
    static int bankSlot(int bank) { return bank - 1; }
    static int numberSlot(int number) { return 8 + number - 1; }
    bool bankLit(int bank) const { return lookFor(bankSlot(bank)).lit; }
    bool numberLit(int number) const { return lookFor(numberSlot(number)).lit; }
    // True when the numerals of that row are drawn in labelPrimary (awaiting it).
    bool bankNumeralsBright() const { return m_stage == Stage::AwaitBank; }
    bool numberNumeralsBright() const { return m_stage == Stage::AwaitNumber; }

    QSize sizeHint() const override { return {WidgetWidth, WidgetHeight}; }

signals:
    // The entry stage or the entry's group/bank changed (display echo and group bar follow).
    void entryChanged();
    // An entry or a number click completed: send this patch.
    void patchChosen(const d05::PatchAddress &address);

protected:
    PanelButton::Look lookFor(int slot) const override;
    QString tipFor(int slot) const override;
    void activate(int slot) override;
    void paintBackdrop(QPainter *p) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void handleDigit(int digit);
    void paintNumerals(QPainter *p, int y, bool bright);

    PatchAddress m_current;
    Stage m_stage = Stage::Idle;
    int m_entryGroup = 0;
    int m_entryBank = 0;
};

}  // namespace d05
