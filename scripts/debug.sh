#!/bin/bash
# Mcaster1DAWCast — launch under lldb with auto-backtrace on crash.
#
# Usage:   scripts/debug.sh
#          scripts/debug.sh --wait     # attach to a running process
#          scripts/debug.sh <file.au>  # set a breakpoint before AU load
#
# On crash, lldb will print a full symbolicated backtrace of every thread
# and then drop you at the (lldb) prompt so you can inspect state, step
# around, or print variables. The dSYM is loaded automatically because it
# lives next to the binary.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BINARY="$REPO_ROOT/src/DAWCast/mcaster1dawcast"
DSYM="$REPO_ROOT/src/DAWCast/mcaster1dawcast.dSYM"

if [[ ! -x "$BINARY" ]]; then
  echo "error: $BINARY not found — run 'make -C src/DAWCast' first" >&2
  exit 1
fi

# Regenerate the dSYM if it's missing or older than the binary.
if [[ ! -d "$DSYM" ]] || [[ "$BINARY" -nt "$DSYM/Contents/Resources/DWARF/mcaster1dawcast" ]]; then
  echo "→ regenerating dSYM…"
  dsymutil "$BINARY" -o "$DSYM"
fi

LLDB_CMDS=$(mktemp)
cat > "$LLDB_CMDS" <<'EOF'
# Stop on every crash-class signal so we can see the state at the moment
# of failure. Qt swallows some of these itself; we catch them first.
process handle SIGSEGV --stop true  --pass true  --notify true
process handle SIGBUS  --stop true  --pass true  --notify true
process handle SIGILL  --stop true  --pass true  --notify true
process handle SIGABRT --stop true  --pass true  --notify true

# Macro: print every frame of every thread at the current stop, fully
# symbolicated against our dSYM.
command alias stacks thread backtrace all

# Useful breakpoints for AU diagnosis. Enable with `br enable <n>`:
breakpoint set --name dawcast::plugins::AuPluginInstance::create
breakpoint set --name dawcast::plugins::Vst3PluginInstance::create
breakpoint disable 1
breakpoint disable 2

process launch --stop-at-entry=false
EOF

echo "→ launching $BINARY under lldb"
echo "  (on crash, type 'stacks' to see every thread's backtrace)"
echo "  (type 'c' to continue, 'q' to quit)"
echo ""

lldb -s "$LLDB_CMDS" "$BINARY"
rm -f "$LLDB_CMDS"
