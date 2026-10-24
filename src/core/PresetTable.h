#pragma once

namespace d05 {

// Number of data rows in the embedded preset table (:/data/d05-presets.tsv),
// or -1 if the resource cannot be read. Placeholder until PatchCatalog (T1.3).
int presetRowCount();

} // namespace d05
