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
  if [[ "$SOURCE_FILE" == "$BENCH_ROOT/embench-iot/src/"* ]]; then
    BENCH_NAME=$(basename "$(dirname "$SOURCE_FILE")")
  else
    BENCH_NAME=$(basename "${SOURCE_FILE%.c}")
  fi
fi

BUILD_DIR="$BENCH_ROOT/build/$BENCH_NAME"
RISCV_TEST_DIR="$BENCH_ROOT/riscv_test"
RESULT_DIR="$BENCH_ROOT/results/$BENCH_NAME"
ELF_FILE="$RISCV_TEST_DIR/$BENCH_NAME.riscv"
SUMMARY_FILE="$RESULT_DIR/summary.txt"
SUPPORT_OBJS=()
EXTRA_CFLAGS=()
BENCH_OBJS=()
EXTRA_LIBS=()

mkdir -p "$BUILD_DIR" "$RESULT_DIR" "$RISCV_TEST_DIR"

if [[ "$SOURCE_FILE" == "$BENCH_ROOT/mibench/automotive/susan/"* ]]; then
  EXTRA_LIBS+=(-lm)
fi

if [[ "$SOURCE_FILE" == "$BENCH_ROOT/embench-iot/src/wikisort/"* ]]; then
  EXTRA_LIBS+=(-lm)
fi

if [[ "$SOURCE_FILE" == "$BENCH_ROOT/wrappers/stringsearch_baremetal.c" ]]; then
  EXTRA_CFLAGS+=(-I"$BENCH_ROOT/mibench/office/stringsearch")
fi

if [[ "$SOURCE_FILE" == "$BENCH_ROOT/wrappers/patricia_baremetal.c" ]]; then
  EXTRA_CFLAGS+=(-I"$BENCH_ROOT/mibench/network/patricia")
fi

if [[ "$SOURCE_FILE" == "$BENCH_ROOT/embench-iot/src/"* ]]; then
  EMBENCH_ROOT="$BENCH_ROOT/embench-iot"
  EMBENCH_SUPPORT_DIR="$EMBENCH_ROOT/support"
  EMBENCH_BOARD_DIR="$EMBENCH_ROOT/examples/riscv64-spike"

  EXTRA_CFLAGS+=(
    -I"$EMBENCH_SUPPORT_DIR"
    -I"$EMBENCH_BOARD_DIR"
    -DWARMUP_HEAT=1
    -DGLOBAL_SCALE_FACTOR=1
  )

  for support_src in main.c beebsc.c; do
    support_obj="$BUILD_DIR/${support_src%.c}.o"
    PATH="$RISCV_TOOLS:$PATH" \
      "$CC" "${CFLAGS[@]}" "${EXTRA_CFLAGS[@]}" \
      -I"$(dirname "$SOURCE_FILE")" \
      -I"$TEST_SUPPORT_DIR" \
      -c "$EMBENCH_SUPPORT_DIR/$support_src" -o "$support_obj"
    SUPPORT_OBJS+=("$support_obj")
  done

  boardsupport_obj="$BUILD_DIR/boardsupport.o"
  PATH="$RISCV_TOOLS:$PATH" \
    "$CC" "${CFLAGS[@]}" "${EXTRA_CFLAGS[@]}" \
    -I"$(dirname "$SOURCE_FILE")" \
    -I"$TEST_SUPPORT_DIR" \
    -c "$EMBENCH_BOARD_DIR/boardsupport.c" -o "$boardsupport_obj"
  SUPPORT_OBJS+=("$boardsupport_obj")
fi

echo "[1/4] Compiling $SOURCE_FILE"
if [[ "$SOURCE_FILE" == "$BENCH_ROOT/embench-iot/src/"* ]]; then
  for bench_src in "$(dirname "$SOURCE_FILE")"/*.c; do
    bench_obj="$BUILD_DIR/$(basename "${bench_src%.c}").o"
    PATH="$RISCV_TOOLS:$PATH" \
      "$CC" "${CFLAGS[@]}" \
      "${EXTRA_CFLAGS[@]}" \
      -I"$(dirname "$SOURCE_FILE")" \
      -I"$TEST_SUPPORT_DIR" \
      -c "$bench_src" -o "$bench_obj"
    BENCH_OBJS+=("$bench_obj")
  done
else
  bench_obj="$BUILD_DIR/$BENCH_NAME.o"
  PATH="$RISCV_TOOLS:$PATH" \
    "$CC" "${CFLAGS[@]}" \
    "${EXTRA_CFLAGS[@]}" \
    -I"$(dirname "$SOURCE_FILE")" \
    -I"$TEST_SUPPORT_DIR" \
    -c "$SOURCE_FILE" -o "$bench_obj"
  BENCH_OBJS+=("$bench_obj")

  if [[ "$SOURCE_FILE" == "$BENCH_ROOT/wrappers/stringsearch_baremetal.c" ]]; then
    for extra_src in "$BENCH_ROOT/mibench/office/stringsearch/bmhasrch.c"; do
      extra_obj="$BUILD_DIR/$(basename "${extra_src%.c}").o"
      PATH="$RISCV_TOOLS:$PATH" \
        "$CC" "${CFLAGS[@]}" \
        "${EXTRA_CFLAGS[@]}" \
        -I"$(dirname "$SOURCE_FILE")" \
        -I"$TEST_SUPPORT_DIR" \
        -c "$extra_src" -o "$extra_obj"
      BENCH_OBJS+=("$extra_obj")
    done
  fi

  if [[ "$SOURCE_FILE" == "$BENCH_ROOT/wrappers/patricia_baremetal.c" ]]; then
    for extra_src in "$BENCH_ROOT/mibench/network/patricia/patricia.c"; do
      extra_obj="$BUILD_DIR/$(basename "${extra_src%.c}").o"
      PATH="$RISCV_TOOLS:$PATH" \
        "$CC" "${CFLAGS[@]}" \
        "${EXTRA_CFLAGS[@]}" \
        -I"$(dirname "$SOURCE_FILE")" \
        -I"$TEST_SUPPORT_DIR" \
        -c "$extra_src" -o "$extra_obj"
      BENCH_OBJS+=("$extra_obj")
    done
  fi
fi

PATH="$RISCV_TOOLS:$PATH" \
  "$CC" "${LDFLAGS[@]}" "${BENCH_OBJS[@]}" "${SUPPORT_OBJS[@]}" "${EXTRA_LIBS[@]}" -o "$ELF_FILE"

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
