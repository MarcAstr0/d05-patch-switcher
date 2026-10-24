#!/usr/bin/env bash
# Hardware-free MIDI check (Linux): sends patches to the ALSA "Midi Through" port
# and asserts the exact messages aseqdump sees. See docs/MIDI.md.
# Usage: scripts/midi-smoke.sh [path/to/d05-patch-switcher]
set -u

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bin="${1:-$root/build/d05-patch-switcher}"
export QT_QPA_PLATFORM=offscreen

command -v aseqdump >/dev/null || { echo "aseqdump not found (sudo apt install alsa-utils)" >&2; exit 77; }
[ -x "$bin" ] || { echo "binary not found: $bin (build first)" >&2; exit 77; }

tmp="$(mktemp -d)"
dump="$tmp/dump.txt"
pid=""
cleanup() {
    [ -n "$pid" ] && kill "$pid" 2>/dev/null && wait "$pid" 2>/dev/null
    rm -rf "$tmp"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

fails=0
fail() { echo "FAIL: $*"; fails=$((fails + 1)); }

: >"$dump"  # aseqdump keeps its file offset; NULs fill the gap, hence tr -d below
stdbuf -oL aseqdump -p "Midi Through" >"$dump" 2>&1 &
pid=$!
sleep 0.5
kill -0 "$pid" 2>/dev/null || { cat "$dump"; echo "aseqdump did not start" >&2; exit 1; }

# check_send ADDR CHANNEL(1-based) MSB-line is always 87; LSB; PROGRAM
check_send() {
    local addr="$1" ch="$2" lsb="$3" prog="$4" c=$(($2 - 1)) want got
    : >"$dump"
    "$bin" --config-dir "$tmp/cfg" --port "Midi Through" --channel "$ch" --send "$addr" >"$tmp/out.txt" 2>"$tmp/err.txt"
    local rc=$?
    [ "$rc" -eq 0 ] || { fail "$addr ch$ch: exit $rc ($(cat "$tmp/err.txt"))"; return; }
    # wait up to 2 s for the third message
    for _ in $(seq 20); do
        tr -d "\000" <"$dump" | grep -q "Program change" && break
        sleep 0.1
    done
    got="$(tr -d "\000" <"$dump" | grep -E "Control change|Program change" | sed -E 's/^ *[0-9]+:[0-9]+ +//; s/  +/ /g')"
    want="Control change $c, controller 0, value 87
Control change $c, controller 32, value $lsb
Program change $c, program $prog"
    if [ "$got" = "$want" ]; then
        echo "ok   $addr ch$ch"
    else
        fail "$addr ch$ch: aseqdump saw:"$'\n'"$got"$'\n'"expected:"$'\n'"$want"
    fi
}

check_send P1-11 1 8 0
check_send P3-45 10 10 28
check_send U8-88 16 7 63

# Exit codes: bad address -> 1, port not found -> 2, nothing reaches the port.
: >"$dump"  # aseqdump keeps its file offset; NULs fill the gap, hence tr -d below
"$bin" --config-dir "$tmp/cfg" --port "Midi Through" --send P1-19 >/dev/null 2>&1
rc=$?; [ "$rc" -eq 1 ] && echo "ok   P1-19 exits 1" || fail "P1-19 exit $rc, expected 1"
"$bin" --config-dir "$tmp/cfg" --port "No Such Port" --send P1-11 >/dev/null 2>&1
rc=$?; [ "$rc" -eq 2 ] && echo "ok   missing port exits 2" || fail "missing port exit $rc, expected 2"
sleep 0.3
[ -z "$(tr -d "\000" <"$dump" | grep -E "Control change|Program change")" ] || fail "bytes were sent for a failing command"
[ ! -e "$tmp/cfg/library.json" ] || fail "--send wrote library.json"

if [ "$fails" -eq 0 ]; then echo "midi-smoke: all passed"; exit 0; fi
echo "midi-smoke: $fails failure(s)"
exit 1
