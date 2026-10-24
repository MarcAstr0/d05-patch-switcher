// Tiny DOM helpers. No framework.
export const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => (c === '&' ? '&amp;' : c === '<' ? '&lt;' : c === '>' ? '&gt;' : '&quot;'));

export function el<K extends keyof HTMLElementTagNameMap>(tag: K, cls = '', text?: string): HTMLElementTagNameMap[K] {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
}

export function q<T extends Element = HTMLElement>(root: ParentNode, sel: string): T {
  const e = root.querySelector<T>(sel);
  if (!e) throw new Error(`missing ${sel}`);
  return e;
}

/** The star of DESIGN.md 7.2: five points, outer radius 7, inner radius 3, one point up. */
export const STAR_SVG = (() => {
  const pts: string[] = [];
  for (let i = 0; i < 10; i++) {
    const r = i % 2 === 0 ? 7 : 3;
    const a = (-90 + i * 36) * (Math.PI / 180);
    pts.push(`${(7 + r * Math.cos(a)).toFixed(2)},${(7 + r * Math.sin(a)).toFixed(2)}`);
  }
  return `<svg class="st" viewBox="0 0 14 14" width="14" height="14" aria-hidden="true"><polygon points="${pts.join(' ')}"/></svg>`;
})();
