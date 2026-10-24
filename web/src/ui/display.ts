// The LCD on a <canvas> (docs/design/DESIGN-WEB.md 5): a 5 x 7 glyph atlas built once per pitch,
// whole device pixels, cells redrawn only when their character changes.
import { glyphRows } from './glyphs';
import type { DisplayGeometry } from '../logic/layout';

type Ctx = CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D;

interface Colors {
  back: string;
  on: string;
  ghost: string;
}

export class Display {
  readonly canvas: HTMLCanvasElement;
  /** Counts: full redraws, cells drawn, atlas builds (tests read them from data attributes). */
  stats = { frames: 0, cells: 0, atlases: 0 };
  private readonly ctx: CanvasRenderingContext2D;
  private geom: DisplayGeometry | null = null;
  private atlas: HTMLCanvasElement | OffscreenCanvas | null = null;
  private atlasKey = '';
  private wantW = 0;
  private wantH = 0;
  private bw = 0;
  private bh = 0;
  private lines: [string, string] = ['', ''];
  private pending: [string, string] = ['', ''];
  private colors: Colors;
  private readonly observer: ResizeObserver | null;

  constructor(private readonly host: HTMLElement) {
    this.canvas = document.createElement('canvas');
    this.canvas.setAttribute('role', 'img');
    this.canvas.setAttribute('aria-label', 'Display');
    this.host.appendChild(this.canvas);
    this.ctx = this.canvas.getContext('2d', { alpha: false })!;
    const cs = getComputedStyle(document.documentElement);
    this.colors = { back: cs.getPropertyValue('--lcdBack').trim(), on: cs.getPropertyValue('--lcdOn').trim(), ghost: cs.getPropertyValue('--lcdGhost').trim() };
    this.observer =
      typeof ResizeObserver === 'function'
        ? new ResizeObserver((entries) => {
            const e = entries[entries.length - 1];
            const box = e?.devicePixelContentBoxSize?.[0];
            // Trust the device box only when it is close to what the geometry says (some engines report CSS px).
            if (box && this.geom && Math.abs(box.inlineSize - this.wantW) <= 2 && Math.abs(box.blockSize - this.wantH) <= 2) this.setBacking(box.inlineSize, box.blockSize);
          })
        : null;
    try {
      this.observer?.observe(this.canvas, { box: 'device-pixel-content-box' as ResizeObserverBoxOptions });
    } catch {
      /* the fallback below sizes the backing store */
    }
  }

  /** New geometry (layout switch, resize, zoom). Rebuilds the atlas only when the pitch or gap changed. */
  setGeometry(g: DisplayGeometry, dpr: number): void {
    const old = this.geom;
    this.geom = g;
    this.canvas.style.width = `${g.fieldW}px`;
    this.canvas.style.height = `${g.fieldH}px`;
    const pitchChanged = !old || old.P !== g.P || old.g !== g.g || old.columns !== g.columns;
    const sizeChanged = !old || old.fieldW !== g.fieldW || old.fieldH !== g.fieldH;
    if (pitchChanged) this.buildAtlas();
    this.wantW = Math.round(g.fieldW * dpr);
    this.wantH = Math.round(g.fieldH * dpr);
    // The observer refines this fallback to the exact device box right after layout.
    if (pitchChanged || sizeChanged || this.bw === 0) this.setBacking(Math.round(g.fieldW * dpr), Math.round(g.fieldH * dpr), pitchChanged);
  }

  private setBacking(w: number, h: number, force = false): void {
    if (!force && w === this.bw && h === this.bh) return;
    this.bw = w;
    this.bh = h;
    this.canvas.width = w; // clears the canvas
    this.canvas.height = h;
    this.lines = ['', ''];
    this.redraw();
  }

  /** Sets both lines (padded or cut to the column count). Nothing happens if neither line changed. */
  setLines(line1: string, line2: string): void {
    this.pending = [line1, line2];
    if (!this.geom) return;
    const n = this.geom.columns;
    const a = line1.slice(0, n).padEnd(n);
    const b = line2.slice(0, n).padEnd(n);
    if (a === this.lines[0] && b === this.lines[1]) return;
    const prev = this.lines;
    this.lines = [a, b];
    this.host.dataset['line1'] = line1.trimEnd();
    this.host.dataset['line2'] = line2.trimEnd();
    this.canvas.setAttribute('aria-label', `${line1.trimEnd()} ${line2.trimEnd()}`.trim() || 'Display');
    this.drawCells(prev);
  }

  private redraw(): void {
    if (!this.geom) return;
    this.stats.frames++;
    this.host.dataset['frames'] = String(this.stats.frames);
    const ctx = this.ctx;
    ctx.fillStyle = this.colors.back;
    ctx.fillRect(0, 0, this.bw, this.bh);
    const n = this.geom.columns;
    const a = this.pending[0].slice(0, n).padEnd(n);
    const b = this.pending[1].slice(0, n).padEnd(n);
    this.host.dataset['line1'] = this.pending[0].trimEnd();
    this.host.dataset['line2'] = this.pending[1].trimEnd();
    this.lines = [a, b];
    this.drawCells(null);
  }

  private drawCells(prev: [string, string] | null): void {
    const g = this.geom;
    if (!g || !this.atlas) return;
    const P = g.P;
    const n = g.columns;
    const ox = Math.floor((this.bw - (6 * n - 1) * P) / 2);
    const oy = Math.floor((this.bh - 15 * P) / 2);
    this.ctx.imageSmoothingEnabled = false;
    for (let l = 0; l < 2; l++) {
      const cur = this.lines[l]!;
      const old = prev ? prev[l]! : null;
      for (let k = 0; k < n; k++) {
        const ch = cur[k]!;
        if (old !== null && old[k] === ch) continue;
        const code = ch.charCodeAt(0);
        const idx = code >= 0x20 && code <= 0x7e ? code - 0x20 : 0x3f - 0x20;
        this.ctx.drawImage(this.atlas, idx * 5 * P, 0, 5 * P, 7 * P, ox + 6 * P * k, oy + 8 * P * l, 5 * P, 7 * P);
        this.stats.cells++;
      }
    }
    this.host.dataset['cells'] = String(this.stats.cells);
  }

  private buildAtlas(): void {
    const g = this.geom!;
    const key = `${g.P}/${g.g}`;
    if (key === this.atlasKey && this.atlas) return;
    this.atlasKey = key;
    this.stats.atlases++;
    this.host.dataset['atlases'] = String(this.stats.atlases);
    const P = g.P;
    const d = P - g.g;
    const w = 95 * 5 * P;
    const h = 7 * P;
    const atlas: HTMLCanvasElement | OffscreenCanvas = typeof OffscreenCanvas === 'function' ? new OffscreenCanvas(w, h) : Object.assign(document.createElement('canvas'), { width: w, height: h });
    const a = atlas.getContext('2d') as Ctx;
    a.fillStyle = this.colors.back;
    a.fillRect(0, 0, w, h);
    for (let i = 0; i < 95; i++) {
      const rows = glyphRows(String.fromCharCode(0x20 + i));
      for (let r = 0; r < 7; r++) {
        for (let c = 0; c < 5; c++) {
          a.fillStyle = rows[r]! & (0x10 >> c) ? this.colors.on : this.colors.ghost;
          a.fillRect(i * 5 * P + c * P, r * P, d, d);
        }
      }
    }
    this.atlas = atlas;
  }
}
