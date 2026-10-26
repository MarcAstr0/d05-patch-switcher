# `test-vectors.json`

Hand-written from `docs/SPEC.md` (FR-3 search grammar, 3.3 library file) and `docs/MIDI.md`
(byte mapping and worked examples). Read by `tests/test_shared_vectors.cpp` and, later,
by the web Vitest suite. If an implementation disagrees with a vector, the spec decides;
do not edit the vector to match the code.

| Section | Shape |
|---|---|
| `addresses` | `{text, group, bank, number, index, msb, lsb, program}`. `group` 0..13 (U1..U8, P1..P6), `index` is the catalogue index 0..895, `text` is canonical. |
| `parse` | `{input, canonical}`: spellings that must parse, and the canonical text they give. |
| `invalid` | Strings that must not parse as an address. |
| `search` | `{query, ...}` over the real catalogue (384 presets from `d05-presets.tsv`, 512 unnamed user slots). Results are in catalogue order. Expectations, any combination: `expect` (exact list of canonical addresses), `expectCount`, `expectFirst`, `expectLast`, `expectContains`, `expectNot`, `expectOnlyPresets` (no U slot in the result). |
| `models` | `{jp08: {search: [...]}, ju06a: {search: [...]}, library: {...}}` (SPEC-2 FR-3, T6.4; `library` is T6.5, below). Each case has the `search` shape over that model's 64 or 128 factory names (`jp08-patches.tsv`, `ju06a-patches.tsv`); canonical addresses are `4-5` (JP-08) and `106:4-5` (JU-06A), results in catalogue order (JU-06A: engine 60 then 106). Extra optional field `userNames`: `{canonical address: name}` applied on a fresh catalogue before the query; text tokens match the user name and the factory name. |
| `library` | `file` is a library.json as it may appear on disk; `expect` is the state after load: `favorites` (catalogue order, deduplicated), `recents` (most recent first, no-zone timestamps read as UTC), `userNames` (sanitised: printable ASCII, whitespace runs collapsed, max 18 characters), `names` (catalogue name per address, empty if none) and `searches` (same shape as above, run with the library's user names applied). `invalidEntries` is a second file whose bad entries must be skipped. |

The library `recents` also cover timestamp spellings (space separator, hour only, `T24:00:00`, lowercase `t`/`z`, offsets, fractional seconds that order but are not printed, year 0050, leading or trailing whitespace read as the epoch), and `userNames` holds two spellings of one address (`U4-44` and `u444`): keys apply in sorted order, so `u444` wins.

`models.library` (SPEC-2 3.5, T6.5) holds two library files and what they must do. `v1`: `file` is a version 1 file (read as `devices.d05`), `expect.d05` is the state after load (`favorites` in catalogue order, `recents` as addresses most recent first, sanitised `userNames`), and `written` is the version 2 object the file becomes at the next save; `file` itself is kept once as `library.json.v1`. `v2`: `file` is a version 2 file with all three devices, an unknown device id (`xyz`) and an unknown top-level key (`note`); `expect` gives each device's state after load; after a save with no net change the file written equals `file` (unknown parts preserved untouched). JSON object key order is not significant.
