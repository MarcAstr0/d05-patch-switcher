# Plan

Milestones in order. Each task names its agent (see `.claude/agents/`), its acceptance
criteria, and whether it ends at a **gate** where the owner (Mario) has to act. The
foreman (`docs/FOREMAN.md`) dispatches tasks, verifies, commits, and ticks the boxes
here. Nothing is "done" until its box is ticked by the foreman after running the checks.

Legend: ☐ not started · ◐ in progress · ☑ done · ⛔ blocked (say why in the Decisions log).

## M0 · Scaffold ☑ (2026-10-24, Fable)

- ☑ Repository, MIT licence, `.gitignore`, folder layout.
- ☑ `CMakeLists.txt` building a Qt 6 Widgets window with RtMidi 6.0.0 via FetchContent; verified on Ubuntu 24.04 / Qt 6.4.2 / GCC 13.
- ☑ `data/d05-presets.tsv`: 384 preset names extracted from the guide and verified; `tests/test_presets_table.cpp` guards it.
- ☑ Docs: `SPEC.md`, `MIDI.md`, `DESIGN-BRIEF.md`, this plan, `FOREMAN.md`, `CLAUDE.md`, `README.md`.
- ☑ Agents: builder (Sonnet), designer (Opus), reviewer (Sonnet).
- ☑ `.github/workflows/build.yml` (untested until first push; M4 owns it).

## M1 · Core and command line (builder) ☑

No GUI beyond the scaffold window. Everything here is testable headless.

- ☑ **T1.1 Split the build.** `d05core` and `d05ui` static libraries, the executable, tests linking `d05core`. `cmake --build` and `ctest` green; `--list-ports` still works.
- ☑ **T1.2 `PatchAddress`.** Parse/format every form in SPEC §4 FR-3 and the canonical `P3-45`; `catalogueIndex()` 0…895 and its inverse; `midiTriplet()`. Tests: round-trip over all 896; rejects `P1-19`, `P1-90`, `P7-11`, `U9-11`, `X1-11`; the five worked examples from `docs/MIDI.md`.
- ☑ **T1.3 `PatchCatalog`.** Loads the TSV from the resource; `name(addr)`, `mode(addr)`, `displayName(addr)` (user name or address for U groups); user-name overlay; `prev/next` in catalogue order with the ends clamped. Tests: 384 preset names present, `P1-11` = Fantasia, `U1-11` displays as `U1-11` until named, `P1-88`→next = `P2-11`, `P6-88`→next stays.
- ☑ **T1.4 `SearchQuery`.** The FR-3 grammar as a predicate plus match ranges. Tests: a table with at least 25 cases copied from the spec, including the "bare digits are text" rule, accent-insensitivity (`Cafe` finds *Venetian Cafe*; a query with an accent also matches), and multi-token AND.
- ☑ **T1.5 `Library`.** JSON load/save per SPEC §3.3, `QSaveFile`, 500 ms debounce, corruption handling, `--config-dir`. Tests in a temp dir: round-trip, recents ordering and counts, cap at 50, broken file is sidelined and the app proceeds.
- ☑ **T1.6 MIDI layer.** `MidiBackend`, `FakeMidiBackend`, `RtMidiBackend`, `MidiLink` (connect by name, substring match, 2 s poll while disconnected, `sendPatch`, status text, follow-mode parsing with 300 ms echo suppression). Tests on the fake: exact bytes for channels 1, 10 and 16; a vanished port flips status and the poll reconnects when the fake port "reappears"; incoming CC0/CC32/PC updates `current` without a send; the echo of our own send is ignored.
- ☑ **T1.7 CLI.** `--list-ports`, `--port`, `--in-port`, `--channel`, `--send`, `--fake-midi`, `--config-dir` with the exit codes from FR-15. `scripts/midi-smoke.sh` passes on the Linux box against `Midi Through` (install `alsa-utils` if `aseqdump` is missing). Record the test command in the README's developer section.
- ☑ **R1 Review** (reviewer + Codex): all of M1, the agent on the commit range and `scripts/codex-review.sh --base origin/main` after the push; foreman reconciles per `docs/FOREMAN.md`. Builder fixes anything rated above style.
- ☑ **Push and CI**: the build-and-test steps green on all three runners (packaging steps may still fail).

**Gate G1 (owner, optional).** If the D-05 can be brought to the Linux box now, plug it
in over USB, confirm it shows in `--list-ports` (see `docs/MIDI.md`, "Hardware tests on
the Linux box"; if it does not, the Hapax is the fallback), then run checklist steps 1–4
with `--send`. Otherwise defer to G3. Either answer unblocks M3.

## M2 · Design (designer) ☑ — runs in parallel with M1

Touches only `docs/design/`, `assets/`. No C++.

- ☑ **T2.1 References.** `docs/design/REFERENCES.md`: what the D-50 and D-05 panels actually look like, with the sources looked at; the display's true character grid and colour; decisions taken from them.
- ☑ **T2.2 Design document.** `docs/design/DESIGN.md` covering everything in `docs/DESIGN-BRIEF.md` §Deliverables: sizes, tokens, type, every component and state, resize rules, keyboard focus visuals, the Settings and Shortcuts dialogs, the logo slot.
- ☑ **T2.3 Assets.** `assets/fonts/` (OFL, with licences), `assets/icons/d05-patch-switcher.svg` + `.png` 256 px (AppImage needs it) + the `.icns`/`.ico` or the commands to make them, `assets/theme/app.qss` with the tokens as comments at the top.
- ☑ **T2.4 Mockups.** `docs/design/mockup-main.png` and `mockup-settings.png` at 1× (and 2× if cheap), produced from `docs/design/mockup/` (SVG rendered with Inkscape, or HTML rendered by the foreman through the Playwright MCP; not the system Chrome, see the brief).

**Gate G2 (owner).** The foreman shows the mockups (publish them as an artifact page
when the Artifact tool is available, otherwise give the paths) and asks for approval or
changes. One round of changes is budgeted. M3 starts only after approval.

## M3 · GUI (builder, designer reviews) ☑ — after G1 answered and G2 approved

- ☑ **T3.1 Theme and DisplayWidget.** `Theme` loads the QSS and tokens; `DisplayWidget` per DESIGN.md with a pre-rendered glyph atlas per size, repaint only on text change, `flashSent()`. `--screenshot` implemented (fake backend, canned library, every `--screenshot-state`). Test: construction + screenshot of three states under `offscreen`.
- ☑ **T3.2 Main window.** Layout per DESIGN.md: display, group bar, bank/number grid, search, list with delegate, status bar, toolbar actions, every shortcut in FR-13. Highlight does not send; Enter and clicks do; prev/next works across groups.
- ☑ **T3.3 Views and library.** All / Favourites / Recents; star toggle; recents counts; `F2` rename for U slots with the 18-character limit; clear-recents with confirmation.
- ☑ **T3.4 Settings dialog and connection.** FR-9 and FR-10 wired to `MidiLink`; disconnected state on the display; auto-send; follow-the-panel; resend-on-connect; Test button.
- ☑ **T3.5 Footprint.** `scripts/measure-footprint.sh`; numbers recorded in the PR/commit message; fix until within SPEC §5.
- ☑ **D3 Design review** (designer): screenshots of every state at 1× and 2× (`QT_SCALE_FACTOR=2`) against DESIGN.md; numbered deviations; builder fixes; one more round at most.
- ☑ **R3 Review** (reviewer + Codex): all of M3, with emphasis on threading, error paths and repaint discipline; both reviews, reconciled.
- ☑ **Push and CI** green on build-and-test.

**Gate G3 (owner).** Hardware test on the Linux box, D-05 on USB directly if it
enumerates, otherwise through the Hapax: `docs/MIDI.md` checklist steps 1–6. Then, if convenient, build from source on the MacBook
(`brew install qt cmake`, then the README commands) and repeat steps 3–5. Report what
the D-05 display showed. Fixes go back to the builder.

## M4 · Packaging and CI (builder) ☑

- ☑ **T4.1 CI green** on all three runners including the packaging steps; artifacts downloadable from the Actions run. Fix the workflow, not the symptom (pin runner images and action versions when a step is flaky).
- ☑ **T4.2 Linux AppImage** launches on the dev box from `~/Downloads` with no extra packages; desktop file and icon integrated.
- ☑ **T4.3 macOS .dmg** universal (`lipo -info` shows x86_64 and arm64), ad-hoc signed, opens after right-click → Open. The README documents the Gatekeeper step.
- ☑ **T4.4 Windows zip** runs on a clean Windows 10/11 (CI-only proof: the exe starts with `--list-ports` on the runner).
- ☑ **T4.5 Pre-release** `v0.9.0` tag → GitHub release with the three files and generated notes.

**Gate G4 (owner).** Download the `.dmg` on the MacBook, run it against the real rig
(checklist steps 3–5). Report.

## M5 · Polish and v1.0.0 ◐

- ☑ **T5.1** `F1` shortcut sheet; About box with version, licence and the trademark notice; first-run hint on the display's second line when no port is configured (`OPEN SETTINGS: CTRL+,`).
- ☐ **T5.2** Everything the owner reported at G3/G4.
- ☐ **T5.3** `CHANGELOG.md`; README screenshots from `--screenshot` at 2×.
- ☐ **R5 Final review** (reviewer + Codex; the foreman may run the agent on Opus).
- ☐ **T5.4** Tag `v1.0.0`; release green; the vault's project page updated by the owner.

## Decisions log

Append-only. Date, who, what changed and why. The foreman writes here whenever a spec
detail is changed, a task is dropped, or a gate answer changes the plan.

- 2026-10-24, owner: user banks are unused today; v1 shows them unnamed and editable, SysEx name reading deferred.
- 2026-10-24, owner: public repository, MIT, no Roland artwork; favourites are a flat list; no Hapax export in v1; the owner runs the foreman on Opus.
- 2026-10-24, Fable: the owner's path is D-05 → Hapax (thru) → computer over USB, so the app opens the Hapax's port; follow-the-panel is optional.
- 2026-10-24, Fable: `Bank.S` is not a requirement. The owner's unit has no such switch (verified on the device 2026-08-31) and the guide gates only Program Change. Docs corrected before the first push.
- 2026-10-24, foreman: `docs/MIDI.md` worked example for `P3-45` said `C0 24` (PC 36); the formula it quotes gives 28. Corrected to `C0 1C`. Found by the builder in T1.2; no behaviour change.
- 2026-10-24, owner via Fable: hardware tests will use the D-05 plugged into the Linux box over USB directly (the Hapax stays in the synth room). Unverified whether the D-05 enumerates on Linux without a driver; gates G1/G3 now say to check `--list-ports` first and fall back to the Hapax. A web version is specified in `docs/WEB.md`, to be scheduled after M3.
- 2026-10-24, foreman: SPEC FR-6 "printable text" for user names is narrowed to printable ASCII (0x20 to 0x7E); tabs and newlines become a space, runs of spaces collapse. The D-05 and the LCD-style display cannot show arbitrary Unicode. Preset names are up to 20 characters (`Alpha Omega Ensemble`); the display design (T2.2) must fit 20.
- 2026-10-24, foreman: DESIGN.md §13.2 readings accepted: display line 1 shows the current (last sent or followed) patch, not the list highlight; line 2 says `NO MATCHES` / `1 MATCH` / `<n> MATCHES` (refines FR-12); an unnamed user slot shows its address and a blank name on the display; a group click starts panel-style entry and a number click while idle sends at once in the current group and bank; recents counts show only in the Recents view; the activity LED shares the 400 ms `SENT` timer; helper widgets `SectionRule`, `PanelButton`, `Nameplate` are added to SPEC §6's `ui/` list.
- 2026-10-24, foreman: FR-10 "vanishes" detection on the real backends. RtMidi gives no hot-plug event and SPEC §5 forbids a timer while connected, so a vanished output (or input) is detected at the next send (port list re-checked before sending, RtMidi warnings treated as failures) and whenever `MidiLink::checkConnection()` runs; M3 calls it on window activation. An idle unplug therefore shows `NO MIDI OUTPUT` at the next send or focus, not instantly. Port names are saved and matched without ALSA's trailing `client:port` numbers, which change across replug.
- 2026-10-24, foreman: FR-15 exit codes extended with 64 for usage errors (bad `--channel`, unknown option, `--screenshot` before M3), kept apart from 1 "bad address". `--send` without `--port` exits 2 until `AppSettings` exists; in M3 it falls back to the saved output port and channel, as FR-15's "overrides the saved one" implies.
- 2026-10-24, designer (T2.3), accepted by foreman: the combo-box arrow is an 8x4 PNG plus @2x instead of SVG, because QSS SVG needs the QtSvg plugin (a new dependency) and a border-drawn wedge renders flat in Qt 6.4. The only chrome raster; DESIGN.md §10.2, §11.4, §14.1.
- 2026-10-24, foreman: verified on the Linux box: the D-05 over USB is class-compliant with no driver (USB `0582:01ff`), ALSA client `Boutique`, port `Boutique:Boutique MIDI 1 24:0`. The port name is "Boutique", not "D-05"; `--port Boutique` matches it. `docs/MIDI.md` "Hardware tests on the Linux box" can drop "unverified" (owner's file).
- 2026-10-24, G1 passed (owner, foreman ran the sends): D-05 on USB as `Boutique`, unit on MIDI channel 3. Checklist steps 1, 3 (via `--send`) and 4: `P1-11`, `P2-11`, `P6-88`, `U1-11` each shown on the unit in order. A first run on channel 1 changed nothing, as expected. The D-05 transmits MIDI clock continuously on its USB port; the input path must keep ignoring timing messages.
- 2026-10-24, G2 approved (owner): mockups approved without changes. Switches: `lcdShowGhost` on (faint unlit dots), `keyModeOnLine2` off (key mode at the right of line 1, per FR-12), LCD colours kept as sampled. "Hapax MIDI 1" in the mockups is sample text; the port combo lists whatever the OS reports and nothing is hard-coded.
- 2026-10-24, foreman: M1 and M2 closed. CI run 40101413905 green on Linux, macOS and Windows including the packaging steps. T1.1 to T1.4 had no per-task Codex pass (Codex joined the process at T1.5); R1's Codex run over `b0fb2a5..HEAD` covers them.
- 2026-10-24, foreman: DESIGN.md §5.1 device-pitch rule revised after review of T3.1 (`floor(p*dpr)` made the display about 20% narrow at 125% and 150%): P is floor(p*dpr) or one more, whichever bezel is closer to the design width and still fits the window; gap g = clamp(round(dpr), 1, P/3). Unchanged at 1x and 2x. Ties keep the smaller pitch; at 1.25 between 848 and 1041 px the display is 49 px wider than its 1x design. Decided by the foreman (owner's machines run 1x and 2x).
- 2026-10-24, foreman (T3.2 readings): a send that fails while disconnected leaves the current patch, display and list unchanged and shows "Not sent: <status>" (the display mirrors the synth). A number click while the grid awaits a bank is ignored. Digits typed into the focused, idle grid start an entry in the current group. A click on empty window background cancels a pending entry. In the Recents view a send reorders the list; the highlight stays on the address.
- 2026-10-24, foreman (T3.3 readings): the 18-character name limit lives in a validator (not maxLength) so pasted non-ASCII is stripped before the cut; F2 on a preset row says "Only user slots can be renamed"; clicking away from the name editor commits (Qt default; Esc cancels); Clear Recents always asks, even when empty; the star hit area is the right 30 px of a row; Ctrl+D works window-wide, Space only in the list.
- 2026-10-24, foreman (T3.4 readings): Settings applies on OK only; the connection line is a preview from the port list (no second port is opened). TEST applies the dialog's pending port and channel to the live link and sends the current patch once; Cancel restores them. Auto-send has no timer: a keyboard highlight move sends, and a held arrow sends the first step and the final row only. Follow moves blink the activity LED but do not show `SENT`. Resend-on-connect fires on start-up connect, poll reconnect and a port switch. The last patch is saved and restored at start without sending. On first run the output combo preselects the first listed port; nothing is saved until OK.
- 2026-10-24, foreman (T3.4 review): resend-on-connect and TEST do not count as sends in Recents (they still flash `SENT`); they are not patch choices. A resend that fails disables resend-on-connect until a send succeeds or Settings change. `lastPatch` is written on quit, not on every send or follow step. The Settings dialog lists ports through `MidiLink`, not the backend (SPEC §6).
- 2026-10-24, foreman (T3.5): the text caret does not blink (`QApplication::setCursorFlashTime(0)`); Qt's 500 ms caret timer broke the "no timer under 2 s" rule while the window was active. Footprint measured on the real X11 display, all within SPEC §5 (numbers in the T3.5 commit message).
- 2026-10-24, foreman (R3): supersedes the T3.3 reading "in the Recents view a send reorders the list". While the Recents view is shown, sends update counts in place; the order refreshes when the view is entered again. Reordering under the cursor made auto-send in Recents bounce between two rows. Codex's R3 run hit the account's usage limit and returned no findings; it is re-run over the M3 range after the limit resets.
- 2026-10-24, G3 passed (owner): D-05 on USB directly (`Boutique`, channel 3), preview build of f2ccb92. Checklist steps 1, 3 (TEST), 4 (P1-11, P2-11, P6-88, U1-11), 5 (P1-88 NEXT lands on P2-11) and 6 (follow the panel, no re-select) all passed. The Hapax (thru) path is untested; it is covered at G4 on the MacBook, where the app talks to the Hapax.
- 2026-10-24, foreman (M4): T4.1 and T4.4 proven by CI run 40113475789. Pinned Qt 6.8.3, linuxdeploy 1-alpha-20251107-1, plugin-qt 1-alpha-20250213-1, actions by SHA. Verify steps: macOS dmg universal (x86_64 arm64), codesign valid, slices 744 KB stripped; Windows zip runs from a fresh directory (`--version`, `--list-ports`), exe 601 KB; Linux AppImage runs; the runner has no ALSA sequencer (no snd-seq module), so the real backend is checked for exit 3 and the packaged binary is exercised with `--fake-midi`.
- 2026-10-24, foreman (T4.3): proven in CI run 40114572432 (universal, ad-hoc signed, bundle `D-05 Patch Switcher.app` with icon, `NSHighResolutionCapable`, `LSMinimumSystemVersion` 12.0, dmg with an Applications link). "Opens after right-click → Open" is checked by the owner at G4; macOS 15 needs Privacy & Security → Open Anyway instead (README "macOS notes", SPEC §8).
- 2026-10-24, foreman (T4.5): `v0.9.0` tagged at b4e813e; release run 40115379566 published https://github.com/MarcAstr0/d05-patch-switcher/releases/tag/v0.9.0 as a pre-release with the AppImage, the universal dmg, the Windows zip and SHA256SUMS.txt.
- 2026-10-24, G4 passed (owner, MacBook via the Hapax): v0.9.0 dmg installed and opened after the Gatekeeper step; checklist steps 3 to 5 passed with output `HAPAX Puerto 3`. The Hapax exposes 16 USB virtual ports, routes incoming MIDI through the selected track (input ALL ACTIVE) and rewrites the channel to the track's output channel; Program Change and Bank Select pass. Recorded in `docs/MIDI.md`. First attempt failed only because a different Hapax port was chosen.
- 2026-10-24, owner: macOS notarization (Developer ID + notarytool + stapler, Apple Developer Program at US$99/year) is deferred; revisit after 2026-11-06. Until then releases stay ad-hoc signed and the README's macOS notes cover first launch. When adopted, the workflow notarizes only when the signing secrets exist, so forks and pull requests still build unsigned.
- 2026-10-24, foreman (R3, Codex findings after the limit reset): a follow move ends a running SENT flash; input listing/open/loss errors now reach the UI (`MidiLink::inputStatusChanged`); the main window relayouts when the display's size changes on a DPR change. Two visuals were added by the builder without a DESIGN.md slot and are accepted provisionally: an input-error line right of the input combo in Settings (380, 144, 100, 26; type.note, labelRed) and an input `NOT FOUND` marker in the status bar (statusError role). The designer records or revises both in DESIGN.md during M5. R3 now has both reviews.
- 2026-10-24, foreman (T5.1): the first-run hint reads `CMD+,` on macOS and `CTRL+,` elsewhere, and shows only when no output port was ever configured (a configured but missing port still shows `NO MIDI OUTPUT`). The subset Inter has ⌘ ⇧ ⌥ for macOS key caps; other key symbols fall back to the system font. The Shortcuts sheet layout (wrapping, key column width) goes to the designer.
