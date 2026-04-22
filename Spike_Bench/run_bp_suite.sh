#!/usr/bin/env bash
set -euo pipefail

ROOT=/scratch/eecs251b-aak
BENCH_ROOT="$ROOT/CS252/benchmarks"
SPIKE_BUILD_DIR="$ROOT/CS252/chipyard/252A_perceptron/riscv-isa-sim/build"

BENCHMARKS=(
  "embench-iot/src/huffbench/libhuffbench.c"
  "embench-iot/src/nsichneu/libnsichneu.c"
  "wrappers/patricia_baremetal.c"
  "embench-iot/src/picojpeg/picojpeg_test.c"
  "embench-iot/src/slre/libslre.c"
  "embench-iot/src/statemate/libstatemate.c"
  "wrappers/stringsearch_baremetal.c"
  "embench-iot/src/tarfind/tarfind.c"
  "embench-iot/src/wikisort/libwikisort.c"
  "embench-iot/src/xgboost/testbench.c"
)

echo "[1/2] Rebuilding custom Spike"
cd "$SPIKE_BUILD_DIR"
make -j2 spike

echo "[2/2] Running benchmark suite"
cd "$BENCH_ROOT"

for bench in "${BENCHMARKS[@]}"; do
  echo
  echo "=== Running $bench ==="
  ./bp_test.sh "$bench"
done

echo
echo "Suite complete."
