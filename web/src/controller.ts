// The application: state, actions, keyboard, ports, storage. Wires the panel to the core.
import { fromIndex, next as nextAddr, prev as prevAddr, type PatchAddress } from './core/address';
import { Catalog } from './core/catalog';
import { Library, createLibraryStorage, parseLibraryJson } from './core/library';
import { Entry } from './logic/entry';
import { accessContent, detectEnv, type Env } from './logic/access';
import { isMac, matchKey, capText, comboOf, type Action, type KeyContext } from './logic/keymap';
import { computeMetrics, type Metrics } from './logic/layout';
import { lcdLines } from './logic/lcd';
import { SettingsStore, localStore } from './logic/settings';
import { buildRows, emptyText, emptyTextPhone, rowOf, searchActive, type Row, type View } from './logic/views';
import { FollowParser } from './midi/follow';
import type { MidiPort } from './midi/port';
import { aboutDialog, confirmDialog, modalOpen, settingsDialog, shortcutsDialog } from './ui/dialogs';
import { SEPARATOR, openMenu, type MenuItem } from './ui/menu';
import { Panel } from './ui/panel';

const FLASH_MS = 400;
/** Dialog heights in CSS px (title strip + client area + border), for the sheet decision. */
const SETTINGS_H = 474;
const ABOUT_H = 463;
const START_PATCH = 512; // P1-11

declare const __APP_VERSION__: string;

type Restore = 'keep' | 'first' | 'highlight' | 'beforeSearch';

export class Controller {
  readonly catalog = new Catalog();
  readonly library: Library;
  readonly settings: SettingsStore;
  readonly panel: Panel;
  private readonly entry = new Entry();
  private readonly follow = new FollowParser();
  private readonly mac: boolean;
  private readonly env: Env;
  private current: PatchAddress;
  private view: View = 'all';
  private query = '';
  private rows: Row[] = [];
  private highlightAddr: PatchAddress | null = null;
  private lastHighlight: PatchAddress | null = null;
  private preSearch: PatchAddress | null = null;
  private recentsOrder: number[] | null = null;
  private overlay = false;
  private overlayPushed = false;
  private sentFlash = false;
  private activity = false;
  private flashTimer: ReturnType<typeof setTimeout> | null = null;
  private message: { text: string; error: boolean } | null = null;
  private channel: number;
  private metrics: Metrics | null = null;
  private lastAccess: string;
  private openName: string | null = null;
  private resendBlocked = false;
  private connectPending = false;
  private settingsOpen = false;
  private menu: { close(): void } | null = null;
  private repeatKey: string | null = null;
  private notSaved: string | null = null;
  private readonly storagePersistent: boolean;
  private dprQuery: MediaQueryList | null = null;
  private readonly importInput: HTMLInputElement;

  constructor(root: HTMLElement, private readonly win: Window, private readonly port: MidiPort) {
    const nav = win.navigator as Navigator & { userAgentData?: { platform?: string; brands?: { brand: string }[]; mobile?: boolean } };
    this.mac = isMac(nav);
    this.env = detectEnv(nav, typeof nav.permissions?.query === 'function');
    const { storage, persistent } = createLibraryStorage();
    this.storagePersistent = persistent;
    this.library = new Library(this.catalog, storage);
    this.settings = new SettingsStore(localStore(win));
    this.channel = this.settings.value.channel;
    this.current = fromIndex(this.settings.value.lastPatch) ?? fromIndex(START_PATCH)!;
    this.lastAccess = port.access;
    this.highlightAddr = this.current;

    this.panel = new Panel(root, {
      group: (g) => this.pressGroup(g),
      bank: (b) => this.pressBank(b),
      number: (n) => this.pressNumber(n),
      step: (d) => this.send(d < 0 ? prevAddr(this.current) : nextAddr(this.current)),
      view: (v) => this.setView(v),
      autoSend: () => this.setAutoSend(!this.settings.value.autoSend),
      system: (a) => this.toggleMenu(a),
      chip: (a) => this.send(a),
      query: (t) => this.setQuery(t),
      openSearch: () => this.openOverlay(true),
      closeSearch: () => this.closeOverlay(),
      connect: () => port.requestAccess(),
      onRowClick: (row) => {
        this.highlightAddr = row.address;
        this.send(row.address);
      },
      onStar: (a) => this.toggleFavorite(a),
      onRenameRequest: (a) => this.renameAddress(a),
      onRenameDone: (a, text) => {
        if (text !== null) this.library.setUserName(a, text);
      },
    }, this.mac);

    this.importInput = document.createElement('input');
    this.importInput.type = 'file';
    this.importInput.accept = '.json,application/json';
    this.importInput.id = 'importFile';
    this.importInput.hidden = true;
    this.importInput.addEventListener('change', () => void this.onImportFile());
    document.body.appendChild(this.importInput);

    // Storage state first, so the message survives the first render.
    const loaded = this.library.load();
    if (loaded === 'recovered') this.warn('The saved library could not be read. It was set aside and a new one started.');
    else if (loaded === 'unreadable' || loaded === 'sidelineFailed') this.warn(this.library.saveError ?? 'The saved library could not be read.');
    else if (!persistent || !this.settings.persistent) this.warn('Storage is blocked: favourites and settings last only until this page closes.');
    this.library.onSaveStatus((e) => {
      this.notSaved = e;
      if (e) this.warn(`Library not saved: ${e}`);
      this.render();
    });
    this.notSaved = this.library.saveError;
    this.library.onChange(() => {
      this.rebuild('keep');
      this.render();
    });

    port.onChange(() => this.onPortChange());
    port.onMessage((b) => this.onMessage(b));
    this.applyPorts();
    this.bindEvents();
    this.measure();
    this.rebuild('highlight');
    this.render();
    // Initial focus (DESIGN-WEB 10): the search field on the desktop layout; on phones none (a focused
    // field opens the soft keyboard), except the access panel button on the landscape layout.
    const kind = this.metrics?.choice.kind;
    if (port.access !== 'granted') {
      if (kind !== 'portrait') this.panel.access.focusButton();
    } else if (kind === 'desktop') this.panel.search.focus({ preventScroll: true });
    this.panel.scrollGroupIntoView(this.current.group);
  }

  // ------------------------------------------------------------------ events and layout

  private bindEvents(): void {
    const w = this.win;
    w.addEventListener('keydown', (e) => this.onKeyDown(e));
    w.addEventListener('keyup', (e) => this.onKeyUp(e));
    w.addEventListener('blur', () => (this.repeatKey = null));
    w.addEventListener('resize', () => this.measure());
    w.addEventListener('orientationchange', () => this.measure());
    w.addEventListener('pagehide', () => this.flushAll());
    w.document.addEventListener('visibilitychange', () => {
      if (w.document.visibilityState === 'hidden') this.flushAll();
    });
    w.addEventListener('storage', (e) => {
      if (e.key === 'd05.settings' || e.key === null) {
        // Another tab changed the settings: take them over (never write them back from here).
        this.settings.reload();
        this.channel = this.settings.value.channel;
        this.applyPorts();
        this.render();
      }
      if (e.key === null || e.key === 'd05.library') {
        this.library.reload();
        this.rebuild('keep');
        this.render();
      }
    });
    w.addEventListener('popstate', () => {
      if (this.overlay && !modalOpen()) {
        this.overlayPushed = false;
        this.closeOverlay();
      }
    });
    w.document.addEventListener(
      'pointerdown',
      (e) => {
        if (this.entry.pending && !this.panel.inEntryArea(e.target)) this.cancelEntry();
      },
      true,
    );
    this.panel.stage.addEventListener('focusout', (e) => {
      if (this.entry.pending && !this.panel.inEntryArea((e as FocusEvent).relatedTarget)) this.cancelEntry();
    });
    this.armDpr();
  }

  private armDpr(): void {
    this.dprQuery?.removeEventListener('change', this.onDpr);
    this.dprQuery = this.win.matchMedia(`(resolution: ${this.win.devicePixelRatio}dppx)`);
    this.dprQuery.addEventListener('change', this.onDpr);
  }

  private readonly onDpr = (): void => {
    this.measure();
    this.armDpr();
  };

  private measure(): void {
    const st = this.panel.stage;
    const W = st.clientWidth;
    const H = st.clientHeight;
    const dpr = this.win.devicePixelRatio || 1;
    const m = this.metrics;
    if (m && m.W === W && m.H === H && m.dpr === dpr) return;
    this.metrics = computeMetrics(W, H, dpr);
    this.panel.applyLayout(this.metrics);
    if (this.metrics.choice.kind !== 'portrait' && this.overlay) {
      this.overlay = false;
      if (this.overlayPushed) {
        this.overlayPushed = false;
        history.back(); // pop the entry the overlay pushed, so Back does not hit a dead one
      }
    }
    this.rebuild('keep');
    this.render();
  }

  private get kind(): 'desktop' | 'landscape' | 'portrait' {
    return this.metrics?.choice.kind ?? 'desktop';
  }

  // ------------------------------------------------------------------ rendering

  private outputOpen(): boolean {
    return this.port.output.state === 'open';
  }

  private render(): void {
    const kind = this.kind;
    const settings = this.settings.value;
    const granted = this.port.access === 'granted';
    const columns = kind === 'desktop' ? 40 : 20;
    const active = searchActive(this.query);
    const [l1, l2] = lcdLines({
      columns,
      current: this.current,
      name: this.catalog.name(this.current),
      mode: this.catalog.mode(this.current),
      entryText: this.entry.text(),
      access: this.port.access,
      outputConfigured: settings.output !== null,
      outputOpen: this.outputOpen(),
      sentFlash: this.sentFlash,
      showCount: active && (kind !== 'portrait' || this.overlay),
      matches: this.rows.length,
    });
    this.panel.display.setLines(l1, l2);
    this.panel.update({
      current: this.current,
      entry: { phase: this.entry.phase, group: this.entry.group, bank: this.entry.bank },
      accessGranted: granted,
      autoSend: settings.autoSend,
      view: this.view,
      query: this.query,
      overlay: this.overlay,
      chips: this.library.favorites().map((a) => ({ address: a, name: this.catalog.name(a) })),
    });
    const empty = kind === 'desktop' ? emptyText(this.view, this.query, this.mac) : emptyTextPhone(this.view, this.query);
    this.panel.list.update({
      rows: this.rows,
      current: this.current,
      highlight: rowOf(this.rows, this.highlightAddr),
      isFavorite: (a) => this.library.isFavorite(a),
      showCounts: this.view === 'recents',
      empty,
    });
    const p = this.pendingScroll;
    this.pendingScroll = null;
    if (p) {
      if (p.top) this.panel.list.scrollToTop();
      else this.panel.list.scrollToRow(p.row, p.nearTop);
    }
    const wantsInput = settings.follow && settings.input !== null;
    this.panel.status.update(
      {
        activity: this.sentFlash || this.activity,
        outName: settings.output?.name ?? null,
        outOpen: this.outputOpen(),
        channel: this.channel,
        inName: settings.input?.name ?? null,
        wantsInput,
        inputOpen: this.port.input.state === 'open',
        message: this.message,
        notSaved: this.notSavedText(),
        accessGranted: granted,
      },
      kind !== 'portrait' || (this.metrics?.W ?? 0) >= 480,
    );
    this.panel.access.update(accessContent(this.port.access, this.env));
    this.pumpCards();
  }

  private notSavedText(): string | null {
    if (this.notSaved) return this.notSaved;
    if (!this.storagePersistent || !this.settings.persistent) return 'Storage is blocked: changes last only until this page closes.';
    return null;
  }

  private say(text: string, error = false): void {
    this.message = { text, error };
  }

  private cards: { title: string; text: string; red: boolean }[] = [];
  private seenWarnings = new Set<string>();

  /** A storage or library warning: the status message, and in portrait one card per distinct warning. */
  private warn(text: string): void {
    this.say(text, true);
    if (this.seenWarnings.has(text)) return;
    this.seenWarnings.add(text);
    this.cards.push({ title: 'WARNING', text, red: false });
  }

  /** Portrait has no status message slot (DESIGN-WEB 7.7, 7.10): import results, refusals and warnings come as a card, one at a time. */
  private pumpCards(): void {
    if (this.kind !== 'portrait' || !this.metrics) return;
    if (document.querySelector('dialog.note')) return;
    const n = this.cards.shift();
    if (!n) return;
    const d = confirmDialog(document.body, { title: n.title, text: n.text, red: n.red, confirm: '', sheet: true }, () => {});
    d.dlg.classList.add('note');
    d.dlg.addEventListener('close', () => this.pumpCards());
  }

  // ------------------------------------------------------------------ list model

  /** Rebuilds the rows. `restore` says where the highlight goes (the desktop's Restore modes). */
  private rebuild(restore: Restore): void {
    const saved = this.highlightAddr;
    if (saved) this.lastHighlight = saved;
    const oldIndex = Math.max(0, rowOf(this.rows, saved));
    let rows = buildRows(this.view, this.query, this.catalog, this.library);
    if (this.view === 'recents') {
      // Sends update counts in place; the order refreshes when the view is entered again.
      if (this.recentsOrder === null || restore !== 'keep') this.recentsOrder = rows.map((r) => r.address.index);
      else {
        const pos = new Map(this.recentsOrder.map((v, i) => [v, i] as const));
        const fresh = rows.filter((r) => !pos.has(r.address.index));
        const kept = rows.filter((r) => pos.has(r.address.index)).sort((a, b) => pos.get(a.address.index)! - pos.get(b.address.index)!);
        rows = [...fresh, ...kept];
        this.recentsOrder = rows.map((r) => r.address.index);
      }
    } else this.recentsOrder = null;
    this.rows = rows;
    if (rows.length === 0) {
      this.highlightAddr = null;
      return;
    }
    const has = (a: PatchAddress | null): boolean => rowOf(rows, a) >= 0;
    const near = (a: PatchAddress): void => {
      this.highlightAddr = a;
      this.pendingScroll = { row: rowOf(rows, a), nearTop: true };
    };
    switch (restore) {
      case 'first':
        this.highlightAddr = rows[0]!.address;
        this.pendingScroll = { row: 0, nearTop: false, top: true };
        break;
      case 'highlight':
        if (saved && has(saved)) near(saved);
        else if (has(this.current)) near(this.current);
        else {
          this.highlightAddr = rows[0]!.address;
          this.pendingScroll = { row: 0, nearTop: false, top: true };
        }
        break;
      case 'beforeSearch':
        if (this.preSearch && has(this.preSearch)) near(this.preSearch);
        else if (has(this.current)) near(this.current);
        else {
          this.highlightAddr = rows[0]!.address;
          this.pendingScroll = { row: 0, nearTop: false, top: true };
        }
        break;
      case 'keep':
        if (saved && has(saved)) this.highlightAddr = saved;
        else {
          // A starred row left the Favourites view: stay at the same place.
          const i = Math.min(oldIndex, rows.length - 1);
          this.highlightAddr = this.view === 'favorites' && saved ? rows[i]!.address : rows[0]!.address;
        }
        break;
    }
  }

  private pendingScroll: { row: number; nearTop: boolean; top?: boolean } | null = null;

  private highlightRow(i: number, scroll = true): void {
    const row = this.rows[i];
    if (!row) return;
    this.highlightAddr = row.address;
    this.render();
    if (scroll) this.panel.list.scrollToRow(i);
  }

  private highlightIndex(): number {
    return rowOf(this.rows, this.highlightAddr);
  }

  private moveHighlight(delta: number | 'home' | 'end'): boolean {
    const n = this.rows.length;
    if (n === 0) return false;
    const cur = this.highlightIndex();
    const to = delta === 'home' ? 0 : delta === 'end' ? n - 1 : Math.max(0, Math.min(n - 1, (cur < 0 ? -1 : cur) + delta));
    if (to === cur) return false;
    this.highlightRow(to);
    return true;
  }

  private setView(v: View): void {
    if (v === this.view) return;
    this.panel.list.finishEdit(true);
    this.view = v;
    this.recentsOrder = null;
    this.rebuild('highlight');
    this.render();
  }

  private setQuery(text: string): void {
    const was = searchActive(this.query);
    this.query = text;
    const now = searchActive(text);
    if (now && !was) this.preSearch = this.highlightAddr;
    this.recentsOrder = null;
    this.rebuild(now ? 'first' : 'beforeSearch');
    this.render();
  }

  // ------------------------------------------------------------------ sending

  private statusText(): string {
    const o = this.port.output;
    if (this.port.access !== 'granted') return 'MIDI access is not allowed';
    if (o.state === 'none') return 'No output port chosen';
    if (o.state === 'missing') return 'Port not found';
    return o.text ?? 'Port error';
  }

  /** The one place a patch is sent. Returns true when all three messages went out. */
  private send(a: PatchAddress, quiet = false): boolean {
    this.panel.list.finishEdit(true);
    const err = this.outputOpen() ? this.port.sendPatch(a, this.channel) : this.statusText();
    if (err !== null) {
      this.say(`Not sent: ${err}`, true);
      this.entry.cancel(); // back to the current patch
      this.render();
      return false;
    }
    this.follow.noteSent(a);
    this.entry.cancel();
    this.current = a;
    if (!quiet) {
      this.library.recordSend(a);
      this.highlightAddr = a;
    }
    this.resendBlocked = false;
    this.flash(true);
    this.render();
    if (!quiet) this.panel.list.scrollToRow(rowOf(this.rows, a));
    return true;
  }

  private flash(sent: boolean): void {
    if (sent) this.sentFlash = true;
    else {
      this.sentFlash = false; // a follow move ends a running SENT flash
      this.activity = true;
    }
    if (this.flashTimer !== null) clearTimeout(this.flashTimer);
    this.flashTimer = setTimeout(() => {
      this.flashTimer = null;
      this.sentFlash = false;
      this.activity = false;
      this.render();
    }, FLASH_MS);
  }

  private sendHighlighted(): void {
    if (this.highlightAddr) this.send(this.highlightAddr);
  }

  private resendCurrent(): void {
    if (this.resendBlocked) return;
    if (!this.send(this.current, true)) this.resendBlocked = true;
  }

  // ------------------------------------------------------------------ entry

  private pressGroup(g: number): void {
    this.panel.grid.focus({ preventScroll: true });
    this.entry.begin(g);
    this.render();
  }

  private pressBank(b: number): void {
    this.entry.pressBank(b, this.current);
    this.render();
  }

  private pressNumber(n: number): void {
    const a = this.entry.pressNumber(n, this.current);
    if (a) this.send(a);
    else this.render();
  }

  private cancelEntry(): void {
    this.entry.cancel();
    this.render();
  }

  // ------------------------------------------------------------------ library actions

  private toggleFavorite(a: PatchAddress): void {
    this.panel.list.finishEdit(true);
    this.library.toggleFavorite(a);
  }

  private toggleHighlightedFavorite(): void {
    this.toggleFavorite(this.highlightAddr ?? this.lastHighlight ?? this.current);
  }

  private renameHighlighted(): void {
    const a = this.highlightAddr;
    if (!a) return;
    this.renameAddress(a);
  }

  private renameAddress(a: PatchAddress): void {
    if (a.group >= 8) {
      this.say('Only user slots can be renamed');
      this.render();
      return;
    }
    this.highlightAddr = a;
    this.render();
    this.panel.list.startEdit(a, this.catalog.name(a));
  }

  // ------------------------------------------------------------------ search overlay (portrait)

  private openOverlay(focus: boolean): void {
    if (this.overlay) return;
    this.overlay = true;
    history.pushState({ d05: 'overlay' }, '');
    this.overlayPushed = true;
    this.render();
    if (focus) this.panel.search.focus();
  }

  private closeOverlay(): void {
    if (!this.overlay) return;
    this.overlay = false;
    this.render();
    if (this.overlayPushed) {
      this.overlayPushed = false;
      history.back();
    }
  }

  // ------------------------------------------------------------------ ports and settings

  private applyPorts(): void {
    const s = this.settings.value;
    this.port.setOutput(s.output);
    this.port.setInput(s.follow ? s.input : null);
  }

  private onPortChange(): void {
    const access = this.port.access;
    const was = this.lastAccess;
    this.lastAccess = access;
    if (access === 'granted' && was !== 'granted') {
      this.applyPorts();
      // First change to granted with no port ever saved: Settings opens by itself (DESIGN-WEB 6.1).
      if (this.settings.value.output === null && !this.settingsOpen) {
        this.render();
        this.openSettings();
        return;
      }
    }
    const open = this.outputOpen() ? (this.settings.value.output?.name ?? null) : null;
    if (open !== null && open !== this.openName) {
      this.openName = open;
      if (this.settingsOpen) this.connectPending = true;
      else if (this.settings.value.resendOnConnect) this.resendCurrent();
    } else if (open === null) this.openName = null;
    this.render();
  }

  private setAutoSend(on: boolean): void {
    this.settings.value.autoSend = on;
    this.settings.save();
    this.render();
  }

  private openSettings(): void {
    if (this.settingsOpen) return;
    this.settingsOpen = true;
    const before = { output: this.settings.value.output, channel: this.channel };
    const sheet = this.useSheet(520, SETTINGS_H);
    settingsDialog(document.body, {
      port: this.port,
      settings: this.settings.value,
      sheet,
      storageNote: this.notSavedText(),
      onTest: (p) => {
        this.port.setOutput(p.output);
        this.channel = p.channel;
        this.resendQuietly();
      },
      onOk: (next) => {
        next.lastPatch = this.settings.value.lastPatch;
        this.settings.value = next;
        this.channel = next.channel;
        this.settings.save();
        this.resendBlocked = false;
        this.applyPorts();
      },
      onCancel: () => {
        this.channel = before.channel;
        this.port.setOutput(before.output);
        this.connectPending = false; // the restored port is the one that was open before: nothing to resend
      },
      onClosed: () => {
        this.settingsOpen = false;
        if (this.connectPending) {
          this.connectPending = false;
          if (this.settings.value.resendOnConnect) this.resendCurrent();
        }
        this.render();
      },
    });
  }

  /** TEST sends the current patch once; it is not a selection, so no Recents entry. */
  private resendQuietly(): void {
    this.send(this.current, true);
  }

  /** Phone sheet when portrait, or when the dialog does not fit the window in either direction (DESIGN-WEB 11.1). */
  private useSheet(width: number, height: number): boolean {
    this.measure(); // the resize event may not have been handled yet: decide from the current viewport
    const m = this.metrics;
    if (!m) return false;
    return m.choice.kind === 'portrait' || (m.choice.kind === 'landscape' && (m.W < width + 16 || m.H < height + 16));
  }

  // ------------------------------------------------------------------ system menu

  private toggleMenu(anchor: HTMLElement): void {
    if (this.menu) {
      this.menu.close();
      return;
    }
    const phone = this.kind !== 'desktop';
    const sc = (a: Action): string | undefined => (phone ? undefined : capText(comboOf(a), this.mac));
    const items: (MenuItem | typeof SEPARATOR)[] = [
      { act: 'settings', label: 'Settings...', shortcut: this.mac ? undefined : sc('settings') },
      ...(this.kind === 'desktop' ? [{ act: 'shortcuts', label: 'Keyboard Shortcuts', shortcut: sc('shortcuts') }] : []),
      { act: 'clear', label: 'Clear Recents...' },
      SEPARATOR,
      { act: 'import', label: 'Import Library...' },
      { act: 'export', label: 'Export Library' },
      SEPARATOR,
      { act: 'about', label: 'About D-05 Patch Switcher' },
    ];
    this.menu = openMenu(
      document.body,
      anchor,
      items,
      phone,
      (act) => this.runMenu(act),
      () => (this.menu = null),
    );
  }

  private runMenu(act: string): void {
    switch (act) {
      case 'settings':
        this.openSettings();
        break;
      case 'shortcuts':
        this.toggleShortcuts();
        break;
      case 'clear':
        this.confirmClear();
        break;
      case 'import':
        this.importInput.value = '';
        this.importInput.click();
        break;
      case 'export':
        this.exportLibrary();
        break;
      case 'about':
        aboutDialog(document.body, __APP_VERSION__, this.useSheet(440, ABOUT_H));
        break;
    }
  }

  private toggleShortcuts(): void {
    if (this.kind !== 'desktop') return;
    const open = document.querySelector('dialog.shortcuts');
    if (open) return;
    shortcutsDialog(document.body, this.mac);
  }

  private confirmClear(): void {
    confirmDialog(document.body, { title: 'CLEAR RECENTS', text: 'Clear the list of recently sent patches?', confirm: 'CLEAR', sheet: this.kind !== 'desktop' }, () => {
      this.panel.list.finishEdit(true);
      this.library.clearRecents();
    });
  }

  private exportLibrary(): void {
    const blob = new Blob([this.library.exportJson()], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob); // not revoked: a few KB until the page closes, and some browsers cancel the download if it is
    a.download = 'library.json';
    a.hidden = true;
    document.body.appendChild(a);
    a.click();
    a.remove();
    this.announce('Library exported');
  }

  private async onImportFile(): Promise<void> {
    const file = this.importInput.files?.[0];
    if (!file) return;
    let text: string;
    try {
      text = await file.text();
    } catch (e) {
      this.announce(`Not a library file: ${e instanceof Error ? e.message : 'could not read it'}`, true, true);
      return;
    }
    if (parseLibraryJson(text) === null) {
      this.announce('Not a library file: expected a library.json written by this app (version 1)', true, true);
      return;
    }
    confirmDialog(document.body, { title: 'IMPORT LIBRARY', text: `Replace favourites, recents and user names with the ones in ${file.name}?`, confirm: 'IMPORT', sheet: this.kind !== 'desktop' }, () => {
      this.panel.list.finishEdit(true);
      this.recentsOrder = null; // the imported order is most-recent-first
      const r = this.library.importJson(text);
      if (r.ok) {
        this.library.flush();
        const n = (k: number, one: string, many: string): string => `${k} ${k === 1 ? one : many}`;
        this.announce(`Imported ${n(r.favorites, 'favourite', 'favourites')}, ${n(r.userNames, 'name', 'names')}${r.skipped > 0 ? ` (${n(r.skipped, 'entry', 'entries')} skipped)` : ''}`, false, true);
      } else this.announce(`Not a library file: ${r.error}`, true, true);
    });
  }

  /** A status message. `card`: in portrait it also opens the message card (import results and refusals only). */
  private announce(text: string, error = false, card = false): void {
    this.say(text, error);
    if (card) this.cards.push({ title: 'IMPORT LIBRARY', text, red: error });
    this.render();
  }

  // ------------------------------------------------------------------ MIDI input (follow)

  private onMessage(bytes: readonly number[]): void {
    const s = this.settings.value;
    if (!s.follow) return;
    const a = this.follow.feed(bytes, this.channel, this.current);
    if (!a) return;
    this.panel.list.finishEdit(true);
    this.current = a;
    this.flash(false);
    this.render();
  }

  // ------------------------------------------------------------------ keyboard

  private context(t: EventTarget | null): KeyContext {
    if (t === this.panel.search) return 'search';
    if (t === this.panel.list.host) return 'list';
    return 'other';
  }

  private onKeyDown(e: KeyboardEvent): void {
    if (e.isComposing || e.defaultPrevented) return;
    if (modalOpen()) return;
    const t = e.target as HTMLElement | null;
    // A held Enter or Space on a native button would click it on every repeat.
    if (e.repeat && (e.key === 'Enter' || e.key === ' ') && t instanceof HTMLButtonElement) {
      e.preventDefault();
      return;
    }
    if (t && t.classList.contains('nameEditor')) return;
    const isField = t instanceof HTMLInputElement || t instanceof HTMLSelectElement || t instanceof HTMLTextAreaElement;
    const plain = !e.ctrlKey && !e.metaKey && !e.altKey;
    if (!e.repeat) this.repeatKey = null;

    // Digits: grid entry (DESIGN.md 6.4), in the group strip and the grid or while an entry is pending.
    if (plain && !e.shiftKey && e.key >= '1' && e.key <= '8' && !isField && (this.entry.pending || this.panel.inEntryArea(t))) {
      e.preventDefault();
      if (e.repeat) return;
      const d = Number(e.key);
      const phase = this.entry.phase;
      this.panel.setGridCursor(phase === 'number' ? 1 : 0, d);
      const a = this.entry.digit(d, this.current);
      if (a) this.send(a);
      else this.render();
      return;
    }

    const action = matchKey(e, this.mac, this.context(t));
    if (!action) return;
    if (action === 'focusSearch' && e.key === '/' && isField) return; // typing a slash
    if (this.menu) return;
    e.preventDefault();
    this.runAction(action, e);
  }

  private runAction(action: Action, e: KeyboardEvent): void {
    switch (action) {
      case 'focusSearch':
        if (this.kind === 'portrait') this.openOverlay(true);
        else if (this.port.access === 'granted') {
          this.panel.search.focus();
          this.panel.search.select();
        }
        break;
      case 'escape':
        this.onEscape();
        break;
      case 'up':
      case 'down':
      case 'home':
      case 'end': {
        const before = this.highlightIndex();
        const moved = this.moveHighlight(action === 'up' ? -1 : action === 'down' ? 1 : action);
        if (moved && this.settings.value.autoSend && this.highlightIndex() !== before) {
          // As on the desktop: a held key reports its first step and, on release, its last.
          if (e.repeat) this.repeatKey = e.key;
          else this.sendHighlighted();
        }
        break;
      }
      case 'send':
        if (!e.repeat) this.sendHighlighted();
        break;
      case 'toggleFavorite':
      case 'toggleFavoriteSpace':
        if (!e.repeat) this.toggleHighlightedFavorite();
        break;
      case 'prev':
        if (!e.repeat) this.send(prevAddr(this.current));
        break;
      case 'next':
        if (!e.repeat) this.send(nextAddr(this.current));
        break;
      case 'gridEntry':
        this.panel.grid.focus({ preventScroll: true });
        this.entry.begin(this.current.group);
        this.render();
        break;
      case 'viewAll':
        this.setView('all');
        break;
      case 'viewFavorites':
        this.setView('favorites');
        break;
      case 'viewRecents':
        this.setView('recents');
        break;
      case 'rename':
        this.renameHighlighted();
        break;
      case 'autoSend':
        this.setAutoSend(!this.settings.value.autoSend);
        break;
      case 'settings':
        this.openSettings();
        break;
      case 'shortcuts':
        this.toggleShortcuts();
        break;
    }
  }

  private onKeyUp(e: KeyboardEvent): void {
    if (!e.repeat && this.repeatKey !== null && e.key === this.repeatKey) {
      this.repeatKey = null;
      if (this.settings.value.autoSend && !modalOpen()) this.sendHighlighted();
    }
  }

  private onEscape(): void {
    if (this.entry.pending) {
      this.cancelEntry();
      this.panel.list.host.focus({ preventScroll: true });
    } else if (this.query !== '') {
      this.panel.search.value = '';
      this.setQuery('');
    } else if (this.overlay) this.closeOverlay();
    else this.panel.list.host.focus({ preventScroll: true });
  }

  private flushAll(): void {
    this.library.flush();
    this.settings.saveLastPatch(this.current.index); // only this field: settings are saved when they change
  }
}
