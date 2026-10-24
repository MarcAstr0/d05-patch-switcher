#pragma once

#include <QtGlobal>

namespace d05 {

// The display's 5 x 7 dot-matrix glyphs, codes 0x20 to 0x7E (DESIGN.md Appendix A),
// indexed by `code - 0x20`. Seven row bytes top to bottom; bit 4 (0x10) is the
// leftmost column and bit 0 the rightmost.
constexpr int kGlyphCount = 95;
constexpr int kGlyphRows = 7;
extern const quint8 kGlyphs[kGlyphCount][kGlyphRows];

}  // namespace d05
