#include "ui/BankGrid.h"

#include "ui/SectionRule.h"
#include "ui/Theme.h"

#include <QPaintDevice>
#include <QKeyEvent>
#include <QPainter>

namespace d05 {

namespace {
constexpr int kBankFaceY = 27;
constexpr int kNumberFaceY = 90;
}  // namespace

BankGrid::BankGrid(QWidget *parent)
    : PanelButtonField(parent)
{
    setObjectName(QStringLiteral("bankGrid"));
    setFixedSize(sizeHint());
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 8; ++col) {
            Slot s;
            s.row = row;
            s.col = col;
            s.face = QPoint(4 + 50 * col, row == 0 ? kBankFaceY : kNumberFaceY);
            m_slots.append(s);
        }
    }
    m_current = *PatchAddress::fromParts(8, 1, 1);
}

void BankGrid::setDevice(Model model, int engine)
{
    const bool changed = model != m_model || engine != m_engine || m_hasCurrent || m_stage != Stage::Idle;
    m_model = model;
    m_engine = engine;
    m_stage = Stage::Idle;
    m_hasCurrent = false;
    if (changed)
        update();
}

void BankGrid::setCurrent(const PatchAddress &current)
{
    const bool changed = !(current == m_current) || m_stage != Stage::Idle || !m_hasCurrent;
    m_current = current;
    m_hasCurrent = true;
    m_stage = Stage::Idle;
    setCursor(bankSlot(current.bank()));
    if (changed)
        update();
    emit entryChanged();
}

void BankGrid::clearCurrent()
{
    const bool changed = m_hasCurrent || m_stage != Stage::Idle;
    m_hasCurrent = false;
    m_stage = Stage::Idle;
    setCursor(bankSlot(1));
    if (changed)
        update();
    emit entryChanged();
}

void BankGrid::beginEntry(int group)
{
    m_stage = Stage::AwaitBank;
    m_entryGroup = m_model == Model::D05 ? qBound(0, group, PatchAddress::GroupCount - 1) : 0;
    m_entryBank = 0;
    setCursor(bankSlot(currentBank()));
    update();
    emit entryChanged();
}

void BankGrid::cancelEntry()
{
    if (m_stage == Stage::Idle)
        return;
    m_stage = Stage::Idle;
    update();
    emit entryChanged();
}

QString BankGrid::entryText() const
{
    if (m_stage == Stage::Idle)
        return QString();
    if (m_model != Model::D05) {
        const QString bank = m_stage == Stage::AwaitNumber ? QString::number(m_entryBank) : QStringLiteral("_");
        return m_model == Model::JP08 ? bank + QStringLiteral("_") : bank + QStringLiteral("-_");
    }
    const QString g = PatchAddress::fromParts(m_entryGroup, 1, 1)->groupLabel();
    return g + QStringLiteral("-") + (m_stage == Stage::AwaitNumber ? QString::number(m_entryBank) : QStringLiteral("_"))
           + QStringLiteral("_");
}

void BankGrid::pressBank(int bank)
{
    if (bank < 1 || bank > 8)
        return;
    if (m_stage == Stage::Idle)
        m_entryGroup = m_model == Model::D05 ? m_current.group() : 0;
    m_stage = Stage::AwaitNumber;
    m_entryBank = bank;
    setCursor(bankSlot(bank));
    update();
    emit entryChanged();
}

void BankGrid::pressNumber(int number)
{
    if (number < 1 || number > 8 || m_stage == Stage::AwaitBank)
        return;
    const int group = m_stage == Stage::Idle ? m_current.group() : m_entryGroup;
    const int bank = m_stage == Stage::Idle ? currentBank() : m_entryBank;
    const auto address = m_model == Model::D05 ? PatchAddress::fromParts(group, bank, number)
                                               : PatchAddress::fromBankNumber(m_model, bank, number, m_engine);
    if (!address)
        return;
    m_stage = Stage::Idle;
    setCursor(numberSlot(number));
    update();
    emit patchChosen(*address);
    emit entryChanged();
}

void BankGrid::handleDigit(int digit)
{
    if (m_stage == Stage::Idle)
        beginEntry(m_current.group());  // the group is ignored on the 64-patch models
    if (m_stage == Stage::AwaitBank)
        pressBank(digit);
    else
        pressNumber(digit);
}

PanelButton::Look BankGrid::lookFor(int slot) const
{
    PanelButton::Look look;
    look.kind = PanelButton::Kind::Led;
    look.faces = slot < 8 ? PanelButton::FaceSet::Bank : PanelButton::FaceSet::Button;
    if (slot < 8) {
        const int bank = slot + 1;
        look.lit = m_stage == Stage::Idle ? (m_hasCurrent && bank == m_current.bank())
                                          : (m_stage == Stage::AwaitNumber && bank == m_entryBank);
    } else {
        look.lit = m_stage == Stage::Idle && m_hasCurrent && slot - 8 + 1 == m_current.number();
    }
    return look;
}

QString BankGrid::tipFor(int slot) const
{
    return slot < 8 ? QStringLiteral("Bank %1").arg(slot + 1) : QStringLiteral("Number %1").arg(slot - 8 + 1);
}

void BankGrid::activate(int slot)
{
    if (slot < 8)
        pressBank(slot + 1);
    else
        pressNumber(slot - 8 + 1);
}

void BankGrid::paintNumerals(QPainter *p, int y, bool bright)
{
    static const QString numerals[8] = {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                                        QStringLiteral("4"), QStringLiteral("5"), QStringLiteral("6"),
                                        QStringLiteral("7"), QStringLiteral("8")};
    const QFont font = Theme::font(Theme::Role::PanelLabel);
    const qreal dpr = p->device()->devicePixelRatioF();
    p->setFont(font);
    p->setPen(bright ? Theme::colors().labelPrimary : Theme::colors().labelNumeral);
    for (int i = 0; i < 8; ++i) {
        const int cx = 4 + 50 * i + PanelButton::FaceW / 2;
        p->drawText(QPoint(qRound(cx - PanelButton::textAdvance(numerals[i], font, dpr) / 2), y + 9), numerals[i]);
    }
}

void BankGrid::paintBackdrop(QPainter *p)
{
    const Theme::Skin &skin = Theme::skin();
    SectionRule::paint(p, 4, 0, 394, skin.bankTitle, true);
    paintNumerals(p, 14, m_stage == Stage::AwaitBank);
    SectionRule::paint(p, 4, 63, 394, skin.numberTitle, true);
    paintNumerals(p, 77, m_stage == Stage::AwaitNumber);
}

void BankGrid::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    const Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;
    if (key >= Qt::Key_1 && key <= Qt::Key_8 && mods == Qt::NoModifier) {
        // A held key must not stream selections: only the first press counts.
        if (!event->isAutoRepeat())
            handleDigit(key - Qt::Key_0);
        event->accept();
        return;
    }
    if (key == Qt::Key_Escape && m_stage != Stage::Idle) {
        cancelEntry();
        event->accept();
        return;
    }
    PanelButtonField::keyPressEvent(event);
}

void BankGrid::focusOutEvent(QFocusEvent *event)
{
    PanelButtonField::focusOutEvent(event);
    cancelEntry();  // focus leaving the grid cancels a pending entry (DESIGN 6.4)
}

}  // namespace d05
