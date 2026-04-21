#!/usr/bin/env bash
set -euo pipefail

COMPILE_SPIKE=0

usage() {
  cat <<'EOF'
usage: bp_test.sh [-compile] <benchmark.c> [output-name]

Examples:
  ./bp_test.sh mybench.c
  ./bp_test.sh -compile mybench.c
  ./bp_test.sh embench-iot/src/xgboost/testbench.c xgboost_test

This script:
  1. Compiles the given C source into a bare-metal RISC-V ELF.
  2. Stores the ELF in CS252/benchmarks/riscv_test/.
  3. Optionally rebuilds Spike when -compile is provided.
  4. Runs perceptron, tage, and hybrid BP experiments.
  5. Stores logs and a short summary in CS252/benchmarks/results/<name>/.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -compile|--compile-spike)
      COMPILE_SPIKE=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      break
      ;;
  esac
done

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage
  exit 1
fi

ROOT=/scratch/eecs251b-aak
BENCH_ROOT="$ROOT/CS252/benchmarks"
TEST_SUPPORT_DIR="$ROOT/chipyard-SMLEric/tests"
SPIKE_DIR="$ROOT/CS252/chipyard/252A_perceptron/riscv-isa-sim"
RISCV_TOOLS="$ROOT/chipyard-SMLEric/.conda-env/riscv-tools/bin"

TARGET=riscv64-unknown-elf
CC="$TARGET-gcc"

ARCH=rv64imafdc
ABI=lp64d
CFLAGS=(
  -std=gnu99
  -O2
  -fno-common
  -fno-builtin-printf
  -Wall
  -march="$ARCH"
  -mabi="$ABI"
  -specs=htif_nano.specs
)
LDFLAGS=(
  -static
  -march="$ARCH"
  -mabi="$ABI"
  -specs=htif_nano.specs
)

SOURCE_ARG=$1
if [[ "$SOURCE_ARG" = /* ]]; then
  SOURCE_FILE="$SOURCE_ARG"
else
  SOURCE_FILE="$BENCH_ROOT/$SOURCE_ARG"
fi

if [[ ! -f "$SOURCE_FILE" ]]; then
  echo "error: benchmark source not found: $SOURCE_FILE" >&2
  exit 1
fi

if [[ $# -eq 2 ]]; then
  BENCH_NAME=$2
else
  BENCH_NAME=$(basename "${SOURCE_FILE%.c}")
fi

BUILD_DIR="$BENCH_ROOT/build/$BENCH_NAME"
RISCV_TEST_DIR="$BENCH_ROOT/riscv_test"
RESULT_DIR="$BENCH_ROOT/results/$BENCH_NAME"
OBJ_FILE="$BUILD_DIR/$BENCH_NAME.o"
ELF_FILE="$RISCV_TEST_DIR/$BENCH_NAME.riscv"
SUMMARY_FILE="$RESULT_DIR/summary.txt"

mkdir -p "$BUILD_DIR" "$RESULT_DIR" "$RISCV_TEST_DIR"

echo "[1/4] Compiling $SOURCE_FILE"
PATH="$RISCV_TOOLS:$PATH" \
  "$CC" "${CFLAGS[@]}" \
  -I"$(dirname "$SOURCE_FILE")" \
  -I"$TEST_SUPPORT_DIR" \
  -c "$SOURCE_FILE" -o "$OBJ_FILE"

PATH="$RISCV_TOOLS:$PATH" \
  "$CC" "${LDFLAGS[@]}" "$OBJ_FILE" -o "$ELF_FILE"

echo "[2/4] ELF stored at $ELF_FILE"

if [[ "$COMPILE_SPIKE" -eq 1 ]]; then
  echo "[3/4] Rebuilding custom Spike"
  cd "$SPIKE_DIR/build"
  make -j2 spike
else
  echo "[3/4] Skipping Spike rebuild"
  cd "$SPIKE_DIR/build"
fi

echo "[4/4] Running predictor experiments"
PREDICTORS=(perceptron tage hybrid)
: > "$SUMMARY_FILE"

for predictor in "${PREDICTORS[@]}"; do
  LOG_FILE="$RESULT_DIR/${BENCH_NAME}.${predictor}.log"
  echo "Running $predictor ..."
  ./spike --perceptron-stats --bp="$predictor" "$ELF_FILE" \
    > "$LOG_FILE" 2>&1

  STATS_LINE=$(grep "Branch predictor stats" "$LOG_FILE" | tail -n 1 || true)
  if [[ -z "$STATS_LINE" ]]; then
    echo "$predictor: FAILED (no stats line found)" | tee -a "$SUMMARY_FILE"
  else
    echo "$predictor: $STATS_LINE" | tee -a "$SUMMARY_FILE"
  fi
done

echo
echo "Done."
echo "ELF:     $ELF_FILE"
echo "Logs:    $RESULT_DIR"
echo "Summary: $SUMMARY_FILE"
