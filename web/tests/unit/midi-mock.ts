// A hand-written stand-in for navigator.requestMIDIAccess and the objects it hands out.
export class MockPort {
  state: 'connected' | 'disconnected' = 'connected';
  sent: number[][] = [];
  opened = 0;
  failSend: Error | null = null;
  onmidimessage: ((e: { data: Uint8Array }) => void) | null = null;
  constructor(public id: string, public name: string, public manufacturer: string) {}
  open(): Promise<MockPort> {
    this.opened++;
    return Promise.resolve(this);
  }
  send(data: number[]): void {
    if (this.failSend) throw this.failSend;
    this.sent.push([...data]);
  }
  receive(...bytes: number[]): void {
    this.onmidimessage?.({ data: Uint8Array.from(bytes) });
  }
}

export class MockAccess {
  outputs = new Map<string, MockPort>();
  inputs = new Map<string, MockPort>();
  onstatechange: (() => void) | null = null;
  add(kind: 'out' | 'in', p: MockPort): void {
    (kind === 'out' ? this.outputs : this.inputs).set(p.id, p);
    this.onstatechange?.();
  }
  remove(kind: 'out' | 'in', id: string): void {
    (kind === 'out' ? this.outputs : this.inputs).delete(id);
    this.onstatechange?.();
  }
}

export interface MockNav {
  nav: Navigator;
  access: MockAccess;
  requests: unknown[];
  /** Settle the pending requestMIDIAccess. */
  settle(ok: boolean): void;
  setPermission(state: PermissionState): void;
}

export function mockNavigator(opts: { permission?: PermissionState | 'unsupported'; auto?: boolean } = {}): MockNav {
  const access = new MockAccess();
  const requests: unknown[] = [];
  let pending: { ok: () => void; fail: () => void } | null = null;
  const status = { state: (opts.permission === 'unsupported' ? 'prompt' : (opts.permission ?? 'prompt')) as PermissionState, onchange: null as null | (() => void) };
  const nav = {
    requestMIDIAccess(o: unknown) {
      requests.push(o);
      return new Promise((resolve, reject) => {
        pending = { ok: () => resolve(access), fail: () => reject(new DOMException('denied', 'SecurityError')) };
        if (opts.auto) pending.ok();
      });
    },
    permissions:
      opts.permission === 'unsupported'
        ? { query: () => Promise.reject(new TypeError('unsupported')) }
        : { query: () => Promise.resolve(status) },
  } as unknown as Navigator;
  return {
    nav,
    access,
    requests,
    settle: (ok) => (ok ? pending?.ok() : pending?.fail()),
    setPermission: (s) => {
      status.state = s;
      status.onchange?.();
    },
  };
}

export const flush = (): Promise<void> => new Promise((r) => setTimeout(r, 0));
