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
# Wait (up to 5 s) until aseqdump has subscribed and printed its header.
for _ in $(seq 50); do
    tr -d '\000' <"$dump" | grep -q "Waiting for data" && break
    kill -0 "$pid" 2>/dev/null || break
    sleep 0.1
done
if ! tr -d '\000' <"$dump" | grep -q "Waiting for data"; then
    cat "$dump"
    echo "aseqdump could not attach to Midi Through (no ALSA sequencer?); skipping" >&2
    exit 77
fi

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

# check_pc DEVICE ENGINE-ARGS... : sends with --device, expects exactly one Program Change.
# check_pc LABEL CHANNEL PROGRAM ARGS...  (ARGS go before --send's address, which is the last one)
check_pc() {
    local label="$1" ch="$2" prog="$3" c=$(($2 - 1)) got want
    shift 3
    : >"$dump"
    "$bin" --config-dir "$tmp/cfg" --port "Midi Through" --channel "$ch" "$@" >"$tmp/out.txt" 2>"$tmp/err.txt"
    local rc=$?
    [ "$rc" -eq 0 ] || { fail "$label: exit $rc ($(cat "$tmp/err.txt"))"; return; }
    for _ in $(seq 20); do
        tr -d "\000" <"$dump" | grep -q "Program change" && break
        sleep 0.1
    done
    sleep 0.2  # a stray second message would arrive by now
    got="$(tr -d "\000" <"$dump" | grep -E "Control change|Program change" | sed -E 's/^ *[0-9]+:[0-9]+ +//; s/  +/ /g')"
    want="Program change $c, program $prog"
    if [ "$got" = "$want" ]; then
        echo "ok   $label ch$ch"
    else
        fail "$label ch$ch: aseqdump saw:"$'\n'"$got"$'\n'"expected:"$'\n'"$want"
    fi
}

check_send P1-11 1 8 0
check_send P3-45 10 10 28
check_send U8-88 16 7 63

# JP-08 (docs/MIDI-JP08.md) and JU-06A (docs/MIDI-JU06A.md): a lone Program Change.
check_pc "jp08 4-5" 5 28 --device jp08 --send 4-5
check_pc "jp08 8-8" 1 63 --device jp08 --send 88
check_pc "ju06a --engine 106 4-5" 4 28 --device ju06a --engine 106 --send 4-5
check_pc "ju06a --engine 60 60:1-1" 16 0 --device ju06a --engine 60 --send 60:1-1
check_pc "ju06a --engine 106 106:8-8" 2 63 --device ju06a --engine 106 --send 106:8-8

# Exit codes: bad address -> 1, port not found -> 2, nothing reaches the port.
: >"$dump"  # aseqdump keeps its file offset; NULs fill the gap, hence tr -d below
"$bin" --config-dir "$tmp/cfg" --port "Midi Through" --send P1-19 >/dev/null 2>&1
rc=$?; [ "$rc" -eq 1 ] && echo "ok   P1-19 exits 1" || fail "P1-19 exit $rc, expected 1"
"$bin" --config-dir "$tmp/cfg" --port "No Such Port" --send P1-11 >/dev/null 2>&1
rc=$?; [ "$rc" -eq 2 ] && echo "ok   missing port exits 2" || fail "missing port exit $rc, expected 2"
# JU-06A engine mismatch: exit 5, one line on stderr, nothing on the wire.
"$bin" --config-dir "$tmp/cfg" --device ju06a --engine 60 --port "Midi Through" --channel 4 --send 106:4-5 >"$tmp/out.txt" 2>"$tmp/err.txt"
rc=$?
[ "$rc" -eq 5 ] && echo "ok   ju06a engine mismatch exits 5" || fail "engine mismatch exit $rc, expected 5"
[ "$(wc -l <"$tmp/err.txt")" -eq 1 ] && [ ! -s "$tmp/out.txt" ] || fail "engine mismatch: expected one stderr line and no stdout"
# A bare JU-06A address with no engine known exits 1; --engine with another device is a usage error.
"$bin" --config-dir "$tmp/cfg" --device ju06a --port "Midi Through" --channel 4 --send 4-5 >/dev/null 2>&1
rc=$?; [ "$rc" -eq 1 ] && echo "ok   bare ju06a address without engine exits 1" || fail "bare ju06a exit $rc, expected 1"
"$bin" --config-dir "$tmp/cfg" --device jp08 --engine 60 --port "Midi Through" --channel 4 --send 4-5 >/dev/null 2>&1
rc=$?; [ "$rc" -eq 64 ] && echo "ok   --engine with jp08 exits 64" || fail "--engine with jp08 exit $rc, expected 64"
sleep 0.3
[ -z "$(tr -d "\000" <"$dump" | grep -E "Control change|Program change")" ] || fail "bytes were sent for a failing command"
[ ! -e "$tmp/cfg" ] || fail "--send created the --config-dir directory"

if [ "$fails" -eq 0 ]; then echo "midi-smoke: all passed"; exit 0; fi
echo "midi-smoke: $fails failure(s)"
exit 1
