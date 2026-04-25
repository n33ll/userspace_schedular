#!/usr/bin/env bash
set -euo pipefail

# One-shot flamegraph generator for uxsched_runner
# Usage:
#   bash benchmark/flamegraph/run_uxsched_flamegraph.sh
# Optional env:
#   FLAMEGRAPH_DIR=$HOME/FlameGraph
#   PERF_FREQ=199
#   RUN_ARGS="..."

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$ROOT_DIR/benchmark/flamegraph/uxsched_runner.cpp"
OUT_DIR="$ROOT_DIR/benchmark/flamegraph/results/uxsched_flame"
BIN="$OUT_DIR/../uxsched_runner"
PERF_OUT="$OUT_DIR/uxsched_runner.perf"
FOLDED_OUT="$OUT_DIR/uxsched_runner.folded"
SVG_OUT="$OUT_DIR/uxsched_runner.svg"

FLAMEGRAPH_DIR="${FLAMEGRAPH_DIR:-$HOME/FlameGraph}"
PERF_FREQ="${PERF_FREQ:-199}"
RUN_ARGS="${RUN_ARGS:-}"

STACKCOLLAPSE="$FLAMEGRAPH_DIR/stackcollapse-perf.pl"
FLAMEGRAPH_PL="$FLAMEGRAPH_DIR/flamegraph.pl"

mkdir -p "$OUT_DIR"

if [[ ! -f "$SRC" ]]; then
  echo "[error] source not found: $SRC"
  exit 1
fi

if [[ ! -x "$(command -v perf)" ]]; then
  echo "[error] perf not found in PATH"
  exit 1
fi

if [[ ! -f "$STACKCOLLAPSE" || ! -f "$FLAMEGRAPH_PL" ]]; then
  echo "[error] FlameGraph scripts not found in: $FLAMEGRAPH_DIR"
  echo "        expected: stackcollapse-perf.pl and flamegraph.pl"
  exit 1
fi

echo "[1/4] Building profiling binary..."
g++ -std=c++17 -O3 -march=native -g -fno-omit-frame-pointer -fno-optimize-sibling-calls \
  "$SRC" -I"$ROOT_DIR" -lboost_context -lnuma -pthread -o "$BIN"

echo "[2/4] Recording perf samples..."
if [[ -n "$RUN_ARGS" ]]; then
  # shellcheck disable=SC2086
  perf record -F "$PERF_FREQ" --call-graph dwarf -- "$BIN" $RUN_ARGS
else
  perf record -F "$PERF_FREQ" --call-graph dwarf -- "$BIN"
fi

echo "[3/4] Collapsing stacks..."
perf script > "$PERF_OUT"
"$STACKCOLLAPSE" "$PERF_OUT" > "$FOLDED_OUT"

if [[ ! -s "$FOLDED_OUT" ]]; then
  echo "[error] folded stack file is empty: $FOLDED_OUT"
  echo "        try: lower perf_event_paranoid or ensure workload runs long enough"
  exit 1
fi

echo "[4/4] Rendering SVG..."
"$FLAMEGRAPH_PL" \
  --title "uxsched_runner (profiling build)" \
  --countname samples \
  "$FOLDED_OUT" > "$SVG_OUT"

echo
echo "[done] Flamegraph generated:"
echo "  $SVG_OUT"
echo "Other artifacts:"
echo "  $PERF_OUT"
echo "  $FOLDED_OUT"
echo "  perf.data (in current working dir)"