#!/usr/bin/env bash
# Footprint check against docs/SPEC.md section 5 (Linux). Launches the real GUI against a
# throwaway --config-dir and measures it; prints a table with PASS/FAIL; exits 1 on any FAIL.
#
# Usage: scripts/measure-footprint.sh [path/to/d05-patch-switcher]
# Env:   FOOTPRINT_OFFSCREEN=1  force QT_QPA_PLATFORM=offscreen (numbers differ from X11)
#        FOOTPRINT_XVFB=1       run under Xvfb (needs xvfb-run) instead of the real display
#        FOOTPRINT_QUICK=1      10 s CPU window and 15 s RSS settle (smoke run, not for the record)
#
# Method
#   CPU      utime+stime ticks from /proc/<pid>/stat, delta over a 30 s window starting 5 s after
#            launch; percent = ticks / (CLK_TCK * 30). `ps -o %cpu` (lifetime average) is shown too.
#   Wakes    the app is launched under `strace -f -e trace=poll,ppoll,epoll_wait,epoll_pwait`
#            (child of strace, so no ptrace privilege is needed); a wake is a return from a poll
#            with a non-zero timeout, any thread, counted over the same window. The ALSA port
#            scan of a poll also costs kernel context switches, so voluntary_ctxt_switches
#            (summed over /proc/<pid>/task/*/status) are shown as info only: they count the
#            scan's ioctls, not wake-ups (about 34 per poll untraced).
#   RSS      `ps -o rss` at 60 s idle, 1x and QT_SCALE_FACTOR=2.
#   Threads  entries in /proc/<pid>/task at 60 s, with their comm names.
#   Startup  wall clock from just before exec to the window's first Paint event, stamped by the
#            app itself when D05_FIRST_PAINT_STAMP is set (no effect otherwise). Median of 3.
#   Binary   `strip` copy of the build output, size in bytes (Qt is a shared library).
# The idle scenarios run side by side so windows stay on screen for about 65 s in total.
set -u

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bin="${1:-$root/build/d05-patch-switcher}"
[ -x "$bin" ] || { echo "binary not found: $bin (build first)" >&2; exit 77; }

if [ "${FOOTPRINT_XVFB:-}" = 1 ] && [ -z "${FOOTPRINT_INSIDE_XVFB:-}" ]; then
    command -v xvfb-run >/dev/null || { echo "xvfb-run not found" >&2; exit 77; }
    exec env FOOTPRINT_INSIDE_XVFB=1 xvfb-run -a -s "-screen 0 1600x1200x24" "$0" "$@"
fi

if [ "${FOOTPRINT_OFFSCREEN:-}" = 1 ]; then
    export QT_QPA_PLATFORM=offscreen
    display_used="offscreen (forced)"
elif [ -n "${FOOTPRINT_INSIDE_XVFB:-}" ]; then
    export QT_QPA_PLATFORM=xcb
    display_used="Xvfb ($DISPLAY)"
elif [ -n "${DISPLAY:-}" ]; then
    export QT_QPA_PLATFORM=xcb
    display_used="real X11 ($DISPLAY)"
elif [ -n "${WAYLAND_DISPLAY:-}" ]; then
    export QT_QPA_PLATFORM=wayland
    display_used="real Wayland ($WAYLAND_DISPLAY)"
else
    export QT_QPA_PLATFORM=offscreen
    display_used="offscreen (no display found)"
fi
unset QT_SCALE_FACTOR QT_AUTO_SCREEN_SCALE_FACTOR

settle=5; window=30; rss_at=60
if [ "${FOOTPRINT_QUICK:-}" = 1 ]; then window=10; rss_at=20; fi

tmp="$(mktemp -d)"
pids=()
declare -A pid_of
cleanup() {
    for p in "${pid_of[@]:-}" "${pids[@]:-}"; do [ -n "$p" ] && kill "$p" 2>/dev/null; done
    wait 2>/dev/null
    rm -rf "$tmp"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

hz="$(getconf CLK_TCK)"
fails=0
rows=()

ticks() { awk '{ s = $0; sub(/^.*\) /, "", s); split(s, f, " "); print f[12] + f[13] }' "/proc/$1/stat"; }
ctxsw() {
    local sum=0 n f
    for f in /proc/"$1"/task/*/status; do
        n="$(awk '/^voluntary_ctxt_switches/ { print $2 }' "$f" 2>/dev/null)"
        sum=$((sum + ${n:-0}))
    done
    echo "$sum"
}
rss_mb() { ps -o rss= -p "$1" | awk '{ printf "%.1f", $1 / 1024 }'; }
pollwakes() { # strace file: blocking poll returns so far
    awk '
        / <\.\.\. [a-z_0-9]+ resumed>/ { next }
        /(^|[0-9] )(poll|ppoll|epoll_wait|epoll_pwait)\(/ {
            if ($0 ~ /^[0-9]+ +/ || 1) {
                z = 0
                if ($0 ~ /(poll|epoll_wait)\(.*, 0\)/) z = 1
                if ($0 ~ /epoll_pwait\(.*, 0, /) z = 1
                if ($0 ~ /tv_sec=0, tv_nsec=0/) z = 1
                if (!z) n++
            }
        }
        END { print n + 0 }' "$1" 2>/dev/null
}
add_row() { # name measured budget verdict
    rows+=("$(printf '%-36s %-40s %-24s %s' "$1" "$2" "$3" "$4")")
    [ "$4" = FAIL ] && fails=$((fails + 1))
}
le() { awk -v a="$1" -v b="$2" 'BEGIN { exit !(a <= b) }'; }

# name|scale|extra args
scenarios=("fake|1|--fake-midi" "disconnected|1|--port NoSuchPort-footprint" "fake2x|2|--fake-midi" "disconnected2x|2|--port NoSuchPort-footprint")
have_through=0
if "$bin" --list-ports 2>/dev/null | grep -q "Midi Through"; then
    scenarios+=("through|1|--port Midi\ Through")
    have_through=1
fi

echo "Display: $display_used" >&2
echo "Launching ${#scenarios[@]} idle scenarios; measuring ${window} s after ${settle} s, RSS at ${rss_at} s ..." >&2
for s in "${scenarios[@]}"; do
    IFS='|' read -r name scale extra <<<"$s"
    mkdir -p "$tmp/cfg-$name"
    eval exec strace -f -o "\"$tmp/$name.strace\"" -e trace=poll,ppoll,epoll_wait,epoll_pwait \
        env QT_SCALE_FACTOR="$scale" "\"$bin\"" --config-dir "\"$tmp/cfg-$name\"" $extra >"$tmp/$name.out" 2>&1 &
    spid=$!
    pids+=("$spid")
    pid_of[$name]=""
    for _ in $(seq 1 50); do
        pid_of[$name]="$(pgrep -P "$spid" | head -1)"
        [ -n "${pid_of[$name]}" ] && break
        sleep 0.1
    done
done

sleep "$settle"
declare -A t0 w0 t1 w1 c0 c1
for s in "${scenarios[@]}"; do
    name="${s%%|*}"; p="${pid_of[$name]}"
    kill -0 "$p" 2>/dev/null || { echo "scenario $name exited early:" >&2; cat "$tmp/$name.out" >&2; exit 1; }
    t0[$name]="$(ticks "$p")"; w0[$name]="$(pollwakes "$tmp/$name.strace")"; c0[$name]="$(ctxsw "$p")"
done
sleep "$window"
for s in "${scenarios[@]}"; do
    name="${s%%|*}"; p="${pid_of[$name]}"
    t1[$name]="$(ticks "$p")"; w1[$name]="$(pollwakes "$tmp/$name.strace")"; c1[$name]="$(ctxsw "$p")"
done
rest=$((rss_at - settle - window)); [ "$rest" -gt 0 ] && sleep "$rest"

declare -A rss thr psc
for s in "${scenarios[@]}"; do
    name="${s%%|*}"; p="${pid_of[$name]}"
    rss[$name]="$(rss_mb "$p")"
    thr[$name]="$(ls /proc/"$p"/task | wc -l) ($(cat /proc/"$p"/task/*/comm 2>/dev/null | paste -sd, -))"
    psc[$name]="$(ps -o %cpu= -p "$p" | tr -d ' ')"
done
for p in "${pid_of[@]}" "${pids[@]}"; do kill "$p" 2>/dev/null; done
wait 2>/dev/null
pids=()

report_cpu() { # name label
    local n="$1" d w pct v limit
    d=$((t1[$n] - t0[$n])); w=$((w1[$n] - w0[$n]))
    pct="$(awk -v d="$d" -v hz="$hz" -v w="$window" 'BEGIN { printf "%.2f", 100 * d / (hz * w) }')"
    if [[ "$n" == disconnected* ]]; then
        limit=$((window / 2 + 1))  # one wake per 2 s, plus one for the window edges
        v=PASS; [ "$w" -gt "$limit" ] && v=FAIL
        add_row "Wakes, disconnected $2" "$w in ${window}s" "<= 1 per 2 s ($limit)" "$v"
        v=PASS; [ "$d" -gt "$((window * hz / 100))" ] && v=FAIL  # sanity: under 1 %
        add_row "  ctx switches, disconnected $2" "$((c1[$n] - c0[$n])) in ${window}s" "info" "info"
        add_row "CPU, disconnected $2" "${pct}% ($d ticks); ps ${psc[$n]}%" "poll only, < 1%" "$v"
    else
        v=PASS; [ "$d" -gt 1 ] && v=FAIL  # 1 tick over 30 s is 0.03%, which prints as 0.0
        add_row "Idle CPU, connected $2" "${pct}% ($d ticks); ps ${psc[$n]}%" "0.0% over ${window}s" "$v"
        add_row "  wakes, connected $2" "$w in ${window}s" "info" "info"
    fi
}

report_cpu fake "(fake)"
if [ "$have_through" = 1 ]; then
    report_cpu through "(Midi Through)"
else
    add_row "Idle CPU, connected (Midi Through)" "port not present" "-" "skipped"
fi
report_cpu fake2x "(fake, 2x)"
report_cpu disconnected "(1x)"
report_cpu disconnected2x "(2x)"

for n in fake disconnected through; do
    [ "$n" = through ] && [ "$have_through" = 0 ] && continue
    v=PASS; le "${rss[$n]}" 80 || v=FAIL
    add_row "RSS 1x after ${rss_at}s, $n" "${rss[$n]} MB" "<= 80 MB" "$v"
done
for n in fake2x disconnected2x; do
    v=PASS; le "${rss[$n]}" 120 || v=FAIL
    add_row "RSS 2x after ${rss_at}s, $n" "${rss[$n]} MB" "<= 120 MB" "$v"
done
for n in fake disconnected through; do
    [ "$n" = through ] && [ "$have_through" = 0 ] && continue
    # Qt's xcb and D-Bus helper threads are the platform's; the app adds at most RtMidi's input thread.
    v=PASS; [ "${thr[$n]%% *}" -gt 4 ] && v=FAIL
    add_row "Threads, $n" "${thr[$n]}" "<= 4 (main+Qt+RtMidi)" "$v"
done

# Startup: wall clock to first paint, median of 3 (new process each, killed at the stamp).
ms=()
for i in 1 2 3; do
    mkdir -p "$tmp/cfg-start$i"
    start="$(date +%s%3N)"
    D05_FIRST_PAINT_STAMP=1 "$bin" --fake-midi --config-dir "$tmp/cfg-start$i" 2>"$tmp/start$i.err" &
    p=$!; pids+=("$p")
    stamp=""
    for _ in $(seq 1 100); do
        stamp="$(awk '/^first-paint-ms/ { print $2 }' "$tmp/start$i.err" 2>/dev/null)"
        [ -n "$stamp" ] && break
        sleep 0.05
    done
    kill "$p" 2>/dev/null; wait "$p" 2>/dev/null
    [ -n "$stamp" ] && ms+=($((stamp - start)))
done
if [ "${#ms[@]}" -eq 3 ]; then
    med="$(printf '%s\n' "${ms[@]}" | sort -n | sed -n 2p)"
    v=PASS; [ "$med" -gt 1000 ] && v=FAIL
    add_row "Startup to first paint" "${med} ms (runs: ${ms[*]})" "<= 1000 ms" "$v"
else
    add_row "Startup to first paint" "no first-paint stamp" "<= 1000 ms" FAIL
fi

# Binary size, stripped copy.
cp "$bin" "$tmp/stripped" && strip "$tmp/stripped" 2>/dev/null
size="$(stat -c %s "$tmp/stripped")"
v=PASS; [ "$size" -gt 2000000 ] && v=FAIL
add_row "Binary, stripped" "$size bytes" "<= 2,000,000 bytes" "$v"

echo
echo "Footprint. Display: $display_used; $(uname -sr); Qt platform $QT_QPA_PLATFORM"
echo "Binary: $bin"
printf '%-36s %-40s %-24s %s\n' Row Measured Budget Result
printf '%s\n' "${rows[@]}"
echo "(Repaints, timers and threads-by-design are code-review rows in SPEC section 5.)"
if [ "$fails" -gt 0 ]; then echo "FAIL: $fails row(s) over budget"; exit 1; fi
echo "All measured rows within budget"
