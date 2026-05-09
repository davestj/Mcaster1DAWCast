#!/bin/bash
# Mcaster1DAWCast — symbolicate a macOS crash report.
#
# Usage:   scripts/symbolicate.sh <crash.ips>
#          scripts/symbolicate.sh  (reads from stdin)
#
# macOS crash reports from ~/Library/Logs/DiagnosticReports/ contain raw
# instruction addresses. Given our dSYM, atos maps each address back to
# file:line. This script extracts the stack frames, resolves each one,
# and prints a readable backtrace alongside the original.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BINARY="$REPO_ROOT/src/DAWCast/mcaster1dawcast"
DSYM="$REPO_ROOT/src/DAWCast/mcaster1dawcast.dSYM/Contents/Resources/DWARF/mcaster1dawcast"

if [[ ! -e "$DSYM" ]]; then
  echo "→ regenerating dSYM…"
  dsymutil "$BINARY"
fi

INPUT="${1:-/dev/stdin}"

# Find the base load address of mcaster1dawcast in the crash report.
BASE=$(awk '/mcaster1dawcast \(\*\)/ {
  match($0, /0x[0-9a-fA-F]+/, a); print a[0]; exit
}' "$INPUT")

if [[ -z "$BASE" ]]; then
  echo "warning: couldn't find mcaster1dawcast load address in crash; using 0x100000000"
  BASE="0x100000000"
fi

echo "# Using dSYM: $DSYM"
echo "# Binary loaded at: $BASE"
echo ""

# Pull every instruction address attributed to mcaster1dawcast and
# symbolicate it. Output preserves the thread grouping from the crash.
awk -v bin="mcaster1dawcast" '
  /^Thread [0-9]+/ || /Crashed:/ { print; next }
  $0 ~ bin && match($0, /0x[0-9a-fA-F]+/, a) {
    print $0 " -> " a[0]
    next
  }
  NF == 0 { print; next }
' "$INPUT" | while read -r line; do
  if [[ "$line" == *" -> 0x"* ]]; then
    addr="${line##* -> }"
    resolved=$(atos -o "$DSYM" -arch arm64 -l "$BASE" "$addr" 2>/dev/null || echo "$addr")
    echo "    $resolved"
  else
    echo "$line"
  fi
done
