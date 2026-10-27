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

    // Which device's patches the grid addresses (SPEC-2 FR-4): the D-05's group, bank and number, or
    // the 64-patch models' bank and number (the JU-06A's on `engine`, its believed one). Ends any
    // entry and lights nothing until setCurrent(); the D-05 is the default.
    void setDevice(Model model, int engine = 0);
    Model deviceModel() const { return m_model; }
    int deviceEngine() const { return m_engine; }

    // The current patch: lights its bank and number, ends any entry.
    void setCurrent(const PatchAddress &current);
    PatchAddress current() const { return m_current; }
    // The JU-06A with no patch on its engine: nothing lit, the bank to enter in is 1.
    void clearCurrent();
    bool hasCurrent() const { return m_hasCurrent; }

    // Starts an entry in `group`, awaiting the bank. Lights nothing in the grid.
    void beginEntry(int group);
    void cancelEntry();
    Stage stage() const { return m_stage; }
    int entryGroup() const { return m_entryGroup; }
    int entryBank() const { return m_entryBank; }
    // "P3-__" or "P3-4_" while an entry is pending, empty otherwise. The 64-patch models: "__" or
    // "4_" (JP-08), "_-_" or "4-_" (JU-06A).
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

    int currentBank() const { return m_hasCurrent ? m_current.bank() : 1; }

    PatchAddress m_current;
    bool m_hasCurrent = true;
    Model m_model = Model::D05;
    int m_engine = 0;
    Stage m_stage = Stage::Idle;
    int m_entryGroup = 0;
    int m_entryBank = 0;
};

}  // namespace d05
