#!/usr/bin/env bash
# Independent code review by OpenAI Codex (read-only). Second opinion next to the
# Claude `reviewer` agent; the foreman runs both and reconciles (docs/FOREMAN.md).
#
# Usage:
#   scripts/codex-review.sh --commit <sha>      [-o report.md]   # one commit
#   scripts/codex-review.sh --base <branch>     [-o report.md]   # this branch vs <branch>, e.g. origin/main
#   scripts/codex-review.sh --uncommitted       [-o report.md]   # staged + unstaged + untracked
#   scripts/codex-review.sh "<instructions>"    [-o report.md]   # free-form: review what the text says
#
# `codex review` refuses custom instructions together with --commit/--base/--uncommitted,
# so in those modes the project guidance reaches Codex through AGENTS.md at the repo
# root, which Codex reads on its own. In free-form mode the same guidance is prepended
# to the instructions given.
#
# Environment:
#   CODEX_MODEL   model slug the account accepts (default gpt-5.6-sol; see ~/.codex/models_cache.json)
#   CODEX_EFFORT  reasoning effort override (default: whatever ~/.codex/config.toml says)
#
# Notes for this machine (see CLAUDE.md): Codex is installed under nvm's Node 22, and
# ~/.codex/config.toml pins a model the ChatGPT account rejects, so the model is always
# passed explicitly. The API-key environment variable is unset so the run bills to the
# ChatGPT login, which is the path known to work.
set -euo pipefail

for d in "$HOME/.nvm/versions/node"/v22*/bin; do
    [ -d "$d" ] && PATH="$d:$PATH"
done
export PATH
unset OPENAI_API_KEY

MODEL="${CODEX_MODEL:-gpt-5.6-sol}"
OUT=""
ARGS=()
SCOPED=0
while [ $# -gt 0 ]; do
    case "$1" in
        -o) OUT="$2"; shift 2 ;;
        --commit|--base) ARGS+=("$1" "$2"); SCOPED=1; shift 2 ;;
        --uncommitted)   ARGS+=("$1"); SCOPED=1; shift ;;
        *)  ARGS+=("$1"); shift ;;
    esac
done
if [ ${#ARGS[@]} -eq 0 ]; then
    sed -n '2,10p' "$0" >&2
    exit 64
fi

cd "$(git rev-parse --show-toplevel)"

read -r -d '' PROMPT <<'EOF' || true
You are an independent reviewer for d05-patch-switcher, a C++17 / Qt 6 Widgets desktop
app that selects patches on a Roland Boutique D-05 over MIDI. Before judging anything,
read CLAUDE.md, docs/SPEC.md and docs/MIDI.md: they define correct behaviour, and the
hardware facts in them (Bank Select MSB 87, LSB 0-13, Program Change (bank-1)*8+(number-1),
panel digits 1-8 only) are not negotiable.

Report defects, not preferences. For each finding give: severity (blocker, major, minor),
file and line, what is wrong, a concrete input or sequence that triggers it, and the fix.
Look hardest at: wrong MIDI bytes or ordering; address parsing edge cases (digits 0 and 9,
groups P7+ and U9+, lower case, spacing); threading between RtMidi's callback thread and
Qt widgets; error paths (port vanishes, open throws, malformed JSON, missing config dir);
persistence atomicity; repaint and timer discipline against the footprint budget in
docs/SPEC.md section 5; and tests that test something easier than the acceptance
criteria in docs/PLAN.md. Say "no defects found" for an area you checked and found clean.
End with one line: ship or do not ship, and why. No praise, no restating the diff.
EOF

CMD=(codex review -c "model=\"$MODEL\"")
if [ -n "${CODEX_EFFORT:-}" ]; then
    CMD+=(-c "model_reasoning_effort=\"$CODEX_EFFORT\"")
fi
if [ "$SCOPED" -eq 1 ]; then
    CMD+=("${ARGS[@]}")                       # guidance comes from AGENTS.md
else
    CMD+=("$PROMPT"$'\n\n'"${ARGS[*]}")       # free-form: guidance + the caller's instructions
fi

echo "codex review: model=$MODEL args=${ARGS[*]}" >&2
if [ -n "$OUT" ]; then
    "${CMD[@]}" | tee "$OUT"
else
    "${CMD[@]}"
fi
