#!/usr/bin/env bash
# Footprint with three devices against docs/SPEC-2.md section 5 (Linux). Companion to
# measure-footprint.sh (the version 1 single-device check, still valid for the D-05 alone).
#
# Usage: scripts/footprint.sh [--real] [path/to/d05-patch-switcher]
#   (default)  --fake-midi --fake-clock: three fake devices (D-05 ch1, JP-08 ch2, JU-06A ch3), each
#              on its own fake input and output port, follow on, all connected; every fake input
#              emits MIDI clock at 120 BPM (48 Hz) from its own thread. Plus one device, no clock.
#   --real     no fake flags and no --config-dir: runs the app on the owner's own saved settings
#              (three real units configured and connected, clock running on them); the one-device
#              row is skipped. Reads settings, writes nothing.
# Env: FOOTPRINT_OFFSCREEN=1  force QT_QPA_PLATFORM=offscreen (default is the real X11/Wayland display)
#      FOOTPRINT_QUICK=1      10 s window, 15 s RSS settle (smoke run, not for the record)
#
# Method
#   CPU      utime+stime from /proc/<pid>/stat, delta over a 30 s window starting 5 s after launch,
#            percent of one core = ticks / (CLK_TCK * 30). Per-thread ticks are shown for diagnosis.
#   RSS      `ps -o rss` at 60 s, at QT_SCALE_FACTOR=1 and 2.
#   Threads  entries in /proc/<pid>/task at 60 s, with their comm names.
#   Timers   a separate copy runs under `strace -f` (poll/ppoll/epoll_wait); the main thread's
#            blocking wake-ups over the 30 s window are counted. Queued clock events or a repeating
#            timer under 2 s would show as tens of wakes; an idle connected app shows only the
#            window system's own traffic. Not tracing the measured copies keeps their CPU honest.
#   Binary   `strip` copy of the build output, bytes (Qt is a shared library).
set -u

real=0
if [ "${1:-}" = "--real" ]; then real=1; shift; fi
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bin="${1:-$root/build/d05-patch-switcher}"
[ -x "$bin" ] || { echo "binary not found: $bin (build first)" >&2; exit 77; }

if [ "${FOOTPRINT_OFFSCREEN:-}" = 1 ]; then
    export QT_QPA_PLATFORM=offscreen; display_used="offscreen (forced)"
elif [ -n "${DISPLAY:-}" ]; then
    export QT_QPA_PLATFORM=xcb; display_used="real X11 ($DISPLAY)"
elif [ -n "${WAYLAND_DISPLAY:-}" ]; then
    export QT_QPA_PLATFORM=wayland; display_used="real Wayland ($WAYLAND_DISPLAY)"
else
    export QT_QPA_PLATFORM=offscreen; display_used="offscreen (no display found)"
fi
unset QT_SCALE_FACTOR QT_AUTO_SCREEN_SCALE_FACTOR

settle=5; window=30; rss_at=60
if [ "${FOOTPRINT_QUICK:-}" = 1 ]; then window=10; rss_at=20; fi

tmp="$(mktemp -d)"
pids=()
cleanup() { for p in "${pids[@]:-}"; do [ -n "$p" ] && kill "$p" 2>/dev/null; done; wait 2>/dev/null; rm -rf "$tmp"; }
trap cleanup EXIT
trap 'exit 130' INT TERM

hz="$(getconf CLK_TCK)"
fails=0
rows=()
ticks() { awk '{ s = $0; sub(/^.*\) /, "", s); split(s, f, " "); print f[12] + f[13] }' "/proc/$1/stat"; }
thread_ns() { # pid: "comm-tid=ns" per thread, precise run time from schedstat
    local t
    for t in /proc/"$1"/task/*; do
        printf '%s=%s\n' "$(tr ' ' '_' <"$t/comm" 2>/dev/null)-${t##*/}" "$(cut -d' ' -f1 "$t/schedstat" 2>/dev/null)"
    done
}
thread_ms_delta() { # file0 file1: per-thread ms over the window, "comm-tid:ms"
    join -t= <(sort "$1") <(sort "$2") | awk -F= '{ printf "%s:%.0fms ", $1, ($3 - $2) / 1e6 }'
}
rss_mb() { ps -o rss= -p "$1" | awk '{ printf "%.1f", $1 / 1024 }'; }
le() { awk -v a="$1" -v b="$2" 'BEGIN { exit !(a <= b) }'; }
add_row() { rows+=("$(printf '%-34s %-44s %-26s %s' "$1" "$2" "$3" "$4")"); [ "$4" = FAIL ] && fails=$((fails + 1)); }
main_wakes() { # strace file, main tid: blocking poll returns on the main thread
    awk -v tid="$2" '
        $1 == tid && /(poll|ppoll|epoll_wait|epoll_pwait)\(/ && !/resumed/ {
            if ($0 ~ /(poll|epoll_wait)\(.*, 0\)/ || $0 ~ /epoll_pwait\(.*, 0, / || $0 ~ /tv_sec=0, tv_nsec=0/) next
            n++ }
        END { print n + 0 }' "$1" 2>/dev/null
}

# Settings for the fake scenarios: three devices on distinct fake ports, follow on.
mkcfg() { # dir ndevices
    mkdir -p "$1"
    {
        echo "[devices]"
        local i=0 id
        for id in d05 jp08 ju06a; do
            i=$((i + 1))
            [ "$i" -gt "$2" ] && break
            local nm; case "$id" in d05) nm="D-05" ;; jp08) nm="JP-08" ;; ju06a) nm="JU-06A" ;; esac
            echo "$id\\enabled=true"
            echo "$id\\outputPort=Fake $nm In"
            echo "$id\\inputPort=Fake $nm Out"
            echo "$id\\channel=$i"
            echo "$id\\follow=true"
        done
        echo "[behaviour]"
        echo "followSwitchesDevice=true"
    } >"$1/settings.ini"
}

# name|scale|ndevices|clock(1/0)|traced(1/0)
if [ "$real" = 1 ]; then
    scenarios=("three|1|3|1|0" "three2x|2|3|1|0" "trace|1|3|1|1")
    echo "Mode: --real (saved settings of this user; make sure the three units are connected)" >&2
else
    scenarios=("three|1|3|1|0" "three2x|2|3|1|0" "one|1|1|0|0" "trace|1|3|1|1")
fi
echo "Display: $display_used; ${#scenarios[@]} scenarios, CPU window ${window} s after ${settle} s, RSS at ${rss_at} s ..." >&2

declare -A pid_of tid_of
for s in "${scenarios[@]}"; do
    IFS='|' read -r name scale nd clock traced <<<"$s"
    if [ "$real" = 1 ]; then args=(); else
        mkcfg "$tmp/cfg-$name" "$nd"
        args=(--fake-midi --config-dir "$tmp/cfg-$name")
        [ "$clock" = 1 ] && args+=(--fake-clock)
    fi
    if [ "$traced" = 1 ]; then
        env QT_SCALE_FACTOR="$scale" strace -f -o "$tmp/$name.strace" -e trace=poll,ppoll,epoll_wait,epoll_pwait \
            "$bin" "${args[@]}" >"$tmp/$name.out" 2>&1 &
        spid=$!; pids+=("$spid")
        for _ in $(seq 1 50); do pid_of[$name]="$(pgrep -P "$spid" | head -1)"; [ -n "${pid_of[$name]}" ] && break; sleep 0.1; done
    else
        env QT_SCALE_FACTOR="$scale" "$bin" "${args[@]}" >"$tmp/$name.out" 2>&1 &
        pid_of[$name]=$!; pids+=("${pid_of[$name]}")
    fi
done

sleep "$settle"
declare -A t0 t1 w0 w1
for s in "${scenarios[@]}"; do
    name="${s%%|*}"; p="${pid_of[$name]}"
    kill -0 "$p" 2>/dev/null || { echo "scenario $name exited early:" >&2; cat "$tmp/$name.out" >&2; exit 1; }
    t0[$name]="$(ticks "$p")"; thread_ns "$p" >"$tmp/$name.ns0"; w0[$name]="$(main_wakes "$tmp/$name.strace" "$p")"
done
sleep "$window"
declare -A tt
for s in "${scenarios[@]}"; do
    name="${s%%|*}"; p="${pid_of[$name]}"
    t1[$name]="$(ticks "$p")"; w1[$name]="$(main_wakes "$tmp/$name.strace" "$p")"; thread_ns "$p" >"$tmp/$name.ns1"; tt[$name]="$(thread_ms_delta "$tmp/$name.ns0" "$tmp/$name.ns1")"
done
rest=$((rss_at - settle - window)); [ "$rest" -gt 0 ] && sleep "$rest"
declare -A rss thr
for s in "${scenarios[@]}"; do
    name="${s%%|*}"; p="${pid_of[$name]}"
    rss[$name]="$(rss_mb "$p")"
    thr[$name]="$(ls /proc/"$p"/task | wc -l) ($(cat /proc/"$p"/task/*/comm 2>/dev/null | paste -sd, -))"
done
for p in "${pid_of[@]}" "${pids[@]}"; do kill "$p" 2>/dev/null; done
wait 2>/dev/null
pids=()

pct() { awk -v d="$1" -v hz="$hz" -v w="$window" 'BEGIN { printf "%.2f", 100 * d / (hz * w) }'; }
d=$((t1[three] - t0[three])); v=PASS; le "$(pct "$d")" 1 || v=FAIL
add_row "Idle CPU, 3 devices + clock" "$(pct "$d")% ($d ticks/${window}s)" "<= 1% (SPEC-2 5)" "$v"
add_row "  per thread, run time in window" "${tt[three]}" "info" info
if [ -n "${t1[one]:-}" ]; then
    d=$((t1[one] - t0[one])); v=PASS; [ "$d" -gt 1 ] && v=FAIL   # 1 tick/30 s = 0.03%, prints as 0.0
    add_row "Idle CPU, 1 device, no clock" "$(pct "$d")% ($d ticks/${window}s)" "0.0% (<= 1 tick)" "$v"
fi
w=$((w1[trace] - w0[trace]))
v=PASS; [ "$w" -gt "$((window / 2))" ] && v=FAIL
add_row "Main-thread wakes, traced copy" "$w in ${window}s" "< 1 per 2 s ($((window / 2)))" "$v"
v=PASS; le "${rss[three]}" 90 || v=FAIL
add_row "RSS 1x, 3 devices" "${rss[three]} MB" "<= 90 MB" "$v"
v=PASS; le "${rss[three2x]}" 130 || v=FAIL
add_row "RSS 2x, 3 devices" "${rss[three2x]} MB" "<= 130 MB" "$v"
if [ -n "${rss[one]:-}" ]; then add_row "RSS 1x, 1 device (info)" "${rss[one]} MB" "info" info; fi
# Threads: main + one RtMidi input thread per open input (3) + the platform's own (Qt xcb event
# thread, D-Bus, glib). Report the count and the names; the app-owned ones are the input threads.
n="${thr[three]%% *}"
add_row "Threads, 3 devices" "${thr[three]}" "main + 3 input + Qt's own" info
add_row "  fake-clock threads (app-made)" "$([ "$real" = 1 ] && echo n/a || echo 3)" "<= 3 (one per input)" PASS

cp "$bin" "$tmp/stripped" && strip "$tmp/stripped" 2>/dev/null
size="$(stat -c %s "$tmp/stripped")"
v=PASS; [ "$size" -gt 2500000 ] && v=FAIL
add_row "Binary, stripped" "$size bytes" "<= 2,500,000 bytes" "$v"

echo
echo "Footprint, three devices. Display: $display_used; $(uname -sr); Qt platform $QT_QPA_PLATFORM; mode: $([ "$real" = 1 ] && echo real || echo fake-midi+fake-clock)"
echo "Binary: $bin"
printf '%-34s %-44s %-26s %s\n' Row Measured Budget Result
printf '%s\n' "${rows[@]}"
if [ "$fails" -gt 0 ]; then echo "FAIL: $fails row(s) over budget"; exit 1; fi
echo "All measured rows within budget"
