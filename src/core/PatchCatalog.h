#pragma once

#include "core/PatchAddress.h"

#include <QChar>
#include <QHash>
#include <QString>
#include <array>

class TestPatchCatalog;

namespace d05 {

class Library;

// A device's catalogue: factory names (and D-05 key modes) from the embedded TSV for the
// model, plus an in-memory overlay of user names (fed by the Library).
// D-05: 896 slots, user names on U slots only. JP-08 (64) and JU-06A (128, both engines in
// catalogue order, 60 then 106): user names on any slot (SPEC-2 3.3). Addresses of another
// model are never matched: name queries return empty, mode() is null, next/prev use the
// address's own model.
class PatchCatalog {
public:
    static constexpr int MaxUserNameLength = 18;

    // Loads the preset table. Default is the embedded D-05 resource; tests may pass a path.
    explicit PatchCatalog(const QString &tsvPath = QStringLiteral(":/data/d05-presets.tsv"));
    // Loads the embedded table of `model`.
    explicit PatchCatalog(Model model);
    // Loads `model`'s table from `tsvPath` (the file must use that model's column layout).
    PatchCatalog(Model model, const QString &tsvPath);

    Model model() const { return m_model; }
    // True when every factory slot of the model is named (384, 64, 128).
    bool isValid() const { return m_presetCount == expectedFactoryCount(m_model); }
    // Number of factory names loaded (named D-05 presets, or 64 / 128).
    int presetCount() const { return m_presetCount; }

    // What is shown: the user name when set, else the factory name (empty for an unnamed
    // D-05 U slot).
    QString name(const PatchAddress &a) const;
    // Factory name only; empty for D-05 U slots and for foreign-model addresses.
    QString factoryName(const PatchAddress &a) const;
    // User name only; empty when none.
    QString userName(const PatchAddress &a) const;
    // Tooltip name: the factory name when the slot has a user name and a factory name
    // (a JP-08 or JU-06A slot the owner overwrote); otherwise empty (nothing extra to show).
    QString tooltipName(const PatchAddress &a) const;
    // Key mode: 'W', 'D' or 'S' for D-05 presets; a null QChar for D-05 user slots and for
    // every JP-08 / JU-06A slot (no key-mode tag there; the JU-06A row shows a.engine() as its
    // tag instead).
    QChar mode(const PatchAddress &a) const;
    // name(a) if non-empty, else the address text.
    QString displayName(const PatchAddress &a) const;

    // User-name overlay (read side). Mutation goes through Library, the only writer.
    // True for a D-05 U slot, or any JP-08 / JU-06A slot, that has a user name.
    bool hasUserName(const PatchAddress &a) const {
        return a.model() == m_model && m_userNames.contains(a.catalogueIndex());
    }
    // True when the slot has a factory name (D-05 presets, every JP-08 / JU-06A slot).
    bool hasFactoryName(const PatchAddress &a) const { return !factoryName(a).isEmpty(); }

    // One step in catalogue order of the address's own model. D-05 crosses group boundaries
    // (U8-88 -> P1-11, P1-88 -> P2-11). The JU-06A never crosses engines (106:1-1 does not
    // step back to 60:8-8). Clamped at the ends: they return the same address; the caller
    // resends it per FR-5.
    static PatchAddress next(const PatchAddress &a);
    static PatchAddress prev(const PatchAddress &a);

private:
    friend class Library;
    friend class ::TestPatchCatalog;

    // Names are printable ASCII only (0x20..0x7E). Tab, CR, LF and
    // other whitespace controls become a space; every other character outside that
    // range is dropped; runs of spaces collapse to one; the result is trimmed, cut to
    // 18 characters and trimmed again. An empty result clears the name. Returns false
    // (and changes nothing) for D-05 preset addresses and for addresses of another model.
    bool setUserName(const PatchAddress &a, const QString &name);
    void clearUserNames() { m_userNames.clear(); }

    static int expectedFactoryCount(Model m);
    void load(const QString &tsvPath);
    static PatchAddress step(const PatchAddress &a, int delta);

    Model m_model = Model::D05;
    std::array<QString, PatchAddress::CatalogueSize> m_presetNames;
    std::array<QChar, PatchAddress::CatalogueSize> m_modes;
    QHash<int, QString> m_userNames;
    int m_presetCount = 0;
};

}  // namespace d05
