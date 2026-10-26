#pragma once

#include "core/Model.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <array>
#include <optional>

namespace d05 {

// A patch address: group (0..13: U1..U8, P1..P6), bank 1..8, number 1..8.
// Catalogue order is group, then bank, then number: U1-11 ... U8-88, P1-11 ... P6-88.
class PatchAddress {
public:
    static constexpr int GroupCount = 14;
    static constexpr int CatalogueSize = GroupCount * 64;  // 896
    static constexpr int BankSelectMsb = 87;

    struct MidiTriplet {
        int msb;      // always 87
        int lsb;      // group index 0..13
        int program;  // (bank-1)*8 + (number-1), 0..63
        bool operator==(const MidiTriplet &o) const {
            return msb == o.msb && lsb == o.lsb && program == o.program;
        }
    };

    // Model-aware construction (SPEC-2 3.2). Digits 1..8 only. engine is 60/106 for the JU-06A
    // and ignored (stored as 0) for the other models.
    static std::optional<PatchAddress> fromBankNumber(Model model, int bank, int number, int engine = 0);
    static std::optional<PatchAddress> fromCatalogueIndex(Model model, int index);
    // D-05: as parse(text). JP-08: "4-5", "45". JU-06A: "106:4-5", "60:45", or bare "4-5"/"45"
    // which takes `currentEngine` (fails when it is absent or not 60/106).
    static std::optional<PatchAddress> parse(const QString &text, Model model,
                                             std::optional<int> currentEngine = std::nullopt);

    // Default: U1-11 (index 0).
    constexpr PatchAddress() = default;

    // Returns nullopt when any part is out of range.
    static std::optional<PatchAddress> fromParts(int group, int bank, int number);
    // Inverse of catalogueIndex(); nullopt outside 0..895.
    static std::optional<PatchAddress> fromCatalogueIndex(int index);
    // Accepts "P3-45", "p345", "3-45", "345" (missing letter means P), "U3-45", "u345".
    // The '-' is accepted only directly after the group digit. Surrounding whitespace is ignored. Rejects anything else (P1-19, P7-11, U9-11, X1-11).
    static std::optional<PatchAddress> parse(const QString &text);

    Model model() const { return m_model; }
    int engine() const { return m_engine; }  // 60/106 for the JU-06A, else 0
    int program() const { return (m_bank - 1) * 8 + (m_number - 1); }  // 0..63

    int group() const { return m_group; }    // 0..13 (D-05; 0 for other models)
    int bank() const { return m_bank; }      // 1..8
    int number() const { return m_number; }  // 1..8
    bool isUser() const { return m_model == Model::D05 && m_group < 8; }
    bool isPreset() const { return m_model == Model::D05 && m_group >= 8; }
    // Group label number as printed: 1..8 for U, 1..6 for P. Always 0 for JP-08 and JU-06A
    // addresses (they have no groups; use bank(), number() and engine()).
    int groupNumber() const {
        return m_model != Model::D05 ? 0 : (isUser() ? m_group + 1 : m_group - 7);
    }

    int catalogueIndex() const {
        return (m_model == Model::D05 ? m_group * 64 : (m_engine == 106 ? 64 : 0)) + program();
    }
    // D-05 only (unchanged).
    MidiTriplet midiTriplet() const;
    // Raw MIDI messages for `channel` (1..16), in send order (SPEC-2 3.2). D-05: Bank Select
    // MSB, LSB, Program Change. JP-08: Program Change. JU-06A: Program Change only when
    // `deviceEngine` equals this address's engine; otherwise (including nullopt) empty.
    // The deviceEngine argument is ignored for the D-05 and JP-08. Bad channel: empty.
    QList<QByteArray> midiMessages(int channel, std::optional<int> deviceEngine = std::nullopt) const;

    // Canonical text: "P3-45" (D-05), "4-5" (JP-08), "106:4-5" (JU-06A).
    QString toString() const;
    // Group only, e.g. "P3". Empty for JP-08 and JU-06A addresses (use engine() for the latter).
    QString groupLabel() const;

    bool operator==(const PatchAddress &o) const {
        return m_model == o.m_model && catalogueIndex() == o.catalogueIndex();
    }
    bool operator!=(const PatchAddress &o) const { return !(*this == o); }
    bool operator<(const PatchAddress &o) const {
        return m_model != o.m_model ? m_model < o.m_model : catalogueIndex() < o.catalogueIndex();
    }

private:
    PatchAddress(int g, int b, int n) : m_group(g), m_bank(b), m_number(n) {}
    Model m_model = Model::D05;
    int m_engine = 0;
    int m_group = 0;
    int m_bank = 1;
    int m_number = 1;
};

}  // namespace d05
