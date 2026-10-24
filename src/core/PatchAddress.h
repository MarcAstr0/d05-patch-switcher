#pragma once

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

    // Default: U1-11 (index 0).
    constexpr PatchAddress() = default;

    // Returns nullopt when any part is out of range.
    static std::optional<PatchAddress> fromParts(int group, int bank, int number);
    // Inverse of catalogueIndex(); nullopt outside 0..895.
    static std::optional<PatchAddress> fromCatalogueIndex(int index);
    // Accepts "P3-45", "p345", "3-45", "345" (missing letter means P), "U3-45", "u345".
    // The '-' is accepted only directly after the group digit. Surrounding whitespace is ignored. Rejects anything else (P1-19, P7-11, U9-11, X1-11).
    static std::optional<PatchAddress> parse(const QString &text);

    int group() const { return m_group; }    // 0..13
    int bank() const { return m_bank; }      // 1..8
    int number() const { return m_number; }  // 1..8
    bool isUser() const { return m_group < 8; }
    bool isPreset() const { return m_group >= 8; }
    // Group label number as printed: 1..8 for U, 1..6 for P.
    int groupNumber() const { return isUser() ? m_group + 1 : m_group - 7; }

    int catalogueIndex() const { return m_group * 64 + (m_bank - 1) * 8 + (m_number - 1); }
    MidiTriplet midiTriplet() const;

    // Canonical text, e.g. "P3-45".
    QString toString() const;
    // Group only, e.g. "P3".
    QString groupLabel() const;

    bool operator==(const PatchAddress &o) const { return catalogueIndex() == o.catalogueIndex(); }
    bool operator!=(const PatchAddress &o) const { return !(*this == o); }
    bool operator<(const PatchAddress &o) const { return catalogueIndex() < o.catalogueIndex(); }

private:
    PatchAddress(int g, int b, int n) : m_group(g), m_bank(b), m_number(n) {}
    int m_group = 0;
    int m_bank = 1;
    int m_number = 1;
};

}  // namespace d05
