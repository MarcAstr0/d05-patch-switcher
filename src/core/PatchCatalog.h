#pragma once

#include "core/PatchAddress.h"

#include <QChar>
#include <QHash>
#include <QString>
#include <array>

namespace d05 {

// The 896-entry catalogue: preset names and key modes from the embedded TSV,
// plus an in-memory overlay of user-slot names (fed by the Library in T1.5).
class PatchCatalog {
public:
    static constexpr int MaxUserNameLength = 18;

    // Loads the preset table. Default is the embedded resource; tests may pass a path.
    explicit PatchCatalog(const QString &tsvPath = QStringLiteral(":/data/d05-presets.tsv"));

    bool isValid() const { return m_presetCount == 6 * 64; }
    int presetCount() const { return m_presetCount; }

    // Preset name, or the user-given name for U slots (empty when unnamed).
    QString name(const PatchAddress &a) const;
    // 'W', 'D' or 'S' for presets; a null QChar for user slots.
    QChar mode(const PatchAddress &a) const;
    // name(a) if non-empty, else the address text.
    QString displayName(const PatchAddress &a) const;

    // User-name overlay. Names are printable ASCII only (0x20..0x7E). Tab, CR, LF and
    // other whitespace controls become a space; every other character outside that
    // range is dropped; runs of spaces collapse to one; the result is trimmed, cut to
    // 18 characters and trimmed again. An empty result clears the name. Returns false
    // (and changes nothing) for preset addresses.
    bool setUserName(const PatchAddress &a, const QString &name);
    bool hasUserName(const PatchAddress &a) const { return m_userNames.contains(a.catalogueIndex()); }
    void clearUserNames() { m_userNames.clear(); }

    // One step in catalogue order across group boundaries (U8-88 -> P1-11, P1-88 -> P2-11).
    // Clamped: at the ends they return the same address (next(P6-88) == P6-88,
    // prev(U1-11) == U1-11); the caller resends it per FR-5.
    static PatchAddress next(const PatchAddress &a);
    static PatchAddress prev(const PatchAddress &a);

private:
    std::array<QString, PatchAddress::CatalogueSize> m_presetNames;
    std::array<QChar, PatchAddress::CatalogueSize> m_modes;
    QHash<int, QString> m_userNames;
    int m_presetCount = 0;
};

}  // namespace d05
