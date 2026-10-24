// Layout choice and geometry (docs/design/DESIGN-WEB.md 4, 5.1, 7.1, 8.1). Pure functions.

export type LayoutKind = 'desktop' | 'landscape' | 'portrait';

export interface LayoutChoice {
  kind: LayoutKind;
  /** Landscape with H <= 379. */
  compact: boolean;
}

export function chooseLayout(W: number, H: number): LayoutChoice {
  if (W >= 800 && H >= 480) return { kind: 'desktop', compact: false };
  if (W >= 680 && W > H) return { kind: 'landscape', compact: H <= 379 };
  return { kind: 'portrait', compact: false };
}

export interface DisplayGeometry {
  columns: number;
  /** Dot pitch and gap in device px; dot = pitch - gap. */
  P: number;
  g: number;
  /** CSS px. */
  fieldW: number;
  fieldH: number;
  padX: number;
  padY: number;
  bezelW: number;
  bezelH: number;
}

const EPS = 1e-9;
const clamp = (v: number, lo: number, hi: number): number => Math.max(lo, Math.min(hi, v));

export const gap = (P: number, dpr: number): number => Math.max(1, Math.min(Math.round(dpr), Math.floor(P / 3)));

function geometry(columns: number, P: number, dpr: number, padX: number, padY: number): DisplayGeometry {
  const g = gap(P, dpr);
  const fieldW = Math.ceil(((6 * columns + 3) * P) / dpr - EPS);
  const fieldH = Math.ceil((19 * P) / dpr - EPS);
  return { columns, P, g, fieldW, fieldH, padX, padY, bezelW: fieldW + 2 * padX, bezelH: fieldH + 2 * padY };
}

/** DESIGN.md 5.1 "Choosing P" with the step p from the window width. */
export function desktopDisplay(W: number, dpr: number): DisplayGeometry {
  const p = W < 1042 ? 3 : W < 1285 ? 4 : 5;
  const T = 243 * p + 38;
  const Pa = Math.floor(p * dpr + EPS);
  const integral = p * dpr - Pa < EPS;
  const bezelW = (P: number): number => Math.ceil((243 * P) / dpr - EPS) + 38;
  let P = Pa;
  if (!integral && bezelW(Pa + 1) + 32 <= W && Math.abs(bezelW(Pa + 1) - T) < Math.abs(bezelW(Pa) - T)) P = Pa + 1;
  return geometry(40, Math.max(P, 1), dpr, 19, 11);
}

/** Twenty columns on a phone: the largest whole device pitch that fits the face span (landscape: capped). */
export function phoneDisplay(kind: LayoutKind, compact: boolean, spanW: number, dpr: number): DisplayGeometry {
  let P = Math.floor((dpr * (spanW - 20)) / 123 + EPS);
  if (kind === 'landscape') P = Math.min(P, Math.floor((compact ? 2 : 3) * dpr + EPS));
  return geometry(20, Math.max(P, 1), dpr, 10, 7);
}

export interface PortraitMetrics {
  CW: number;
  cx0: number;
  m: number;
  /** Pad cell width and the left of the pad grid. */
  cw: number;
  gx: number;
  spanX: number;
  spanW: number;
}

export function portraitMetrics(W: number): PortraitMetrics {
  const CW = Math.min(W, 560);
  const cx0 = Math.floor((W - CW) / 2);
  const m = 12;
  const G = CW - 2 * m;
  const cw = Math.floor(G / 4);
  const gx = cx0 + m + Math.floor((G - 4 * cw) / 2);
  return { CW, cx0, m, cw, gx, spanX: gx + 3, spanW: 4 * cw - 6 };
}

/** The top of the band (below the display) in portrait, and the pad row height. */
export function portraitRows(H: number, bezelH: number): { B: number; hp: number } {
  const B = 8 + bezelH + 10;
  const hp = clamp(Math.floor((H - 222 - (B + 128)) / 4), 52, 88);
  return { B, hp };
}

export interface LandscapeMetrics {
  /** Button cell width and the left column width. */
  c: number;
  L: number;
  spanX: number;
  spanW: number;
  /** vw: width of a view cell in the right column. */
  rw: number;
  vw: number;
}

export function landscapeMetrics(W: number): LandscapeMetrics {
  const c = clamp(Math.floor((W / 2 - 24) / 8), 44, 56);
  const L = 8 * c + 24;
  const rw = W - L - 24;
  return { c, L, spanX: 14, spanW: 8 * c - 4, rw, vw: Math.floor((rw + 6) / 3) };
}

/** Everything the layout needs in one object; applied to the page as CSS custom properties. */
export interface Metrics {
  choice: LayoutChoice;
  W: number;
  H: number;
  dpr: number;
  display: DisplayGeometry;
  vars: Record<string, number>;
}

export function computeMetrics(W: number, H: number, dpr: number): Metrics {
  const choice = chooseLayout(W, H);
  const vars: Record<string, number> = {};
  let display: DisplayGeometry;
  if (choice.kind === 'desktop') {
    display = desktopDisplay(W, dpr);
    vars['B'] = 46 + display.bezelH + 14;
    vars['bezelX'] = 16;
    vars['bezelY'] = 46;
  } else if (choice.kind === 'portrait') {
    const pm = portraitMetrics(W);
    display = phoneDisplay('portrait', false, pm.spanW, dpr);
    const { B, hp } = portraitRows(H, display.bezelH);
    Object.assign(vars, { B, hp, cw: pm.cw, gx: pm.gx, spanX: pm.spanX, spanW: pm.spanW, vw: Math.floor((pm.spanW + 6) / 3), ledxPad: Math.floor((pm.cw - 6) / 2) - 8, ledxView: Math.floor((Math.floor((pm.spanW + 6) / 3) - 6) / 2) - 8, bezelX: pm.spanX + Math.floor((pm.spanW - display.bezelW) / 2), bezelY: 8 });
  } else {
    const lm = landscapeMetrics(W);
    display = phoneDisplay('landscape', choice.compact, lm.spanW, dpr);
    Object.assign(vars, { c: lm.c, L: lm.L, spanW: lm.spanW, rw: lm.rw, vw: lm.vw, ledxPad: Math.floor((lm.c - 4) / 2) - 8, ledxView: Math.floor((lm.vw - 6) / 2) - 8, B: 8 + display.bezelH + 4, bezelX: lm.spanX + Math.floor((lm.spanW - display.bezelW) / 2), bezelY: 8 });
  }
  Object.assign(vars, { bezelW: display.bezelW, bezelH: display.bezelH, fieldW: display.fieldW, fieldH: display.fieldH, padX: display.padX, padY: display.padY });
  return { choice, W, H, dpr, display, vars };
}
