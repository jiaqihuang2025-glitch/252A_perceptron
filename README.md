# SPIKE for Perceptron

This repository packages a Spike `riscv-isa-sim` source tree with a lightweight C++ perceptron branch predictor model.

The predictor is integrated into Spike's conditional branch execution path and can be enabled with:

```bash
--perceptron-stats
```

When enabled, Spike prints:

```text
branches
mispredictions
miss_rate
```

## Layout

```text
benchmarks/
  nested_loop.c
  data_dependent.c
riscv-isa-sim/
  Spike source with perceptron predictor support
```

The main predictor files are:

```text
riscv-isa-sim/riscv/perceptron_predictor.h
riscv-isa-sim/riscv/perceptron_predictor.cc
```

The command-line option is wired through:

```text
riscv-isa-sim/spike_main/spike.cc
```

## Build Spike

From the repository root:

```bash
cd riscv-isa-sim
mkdir -p build
cd build
../configure --prefix=$PWD/../install
make -j2 spike
```

On the EECS 251B Chipyard environment, use the course toolchain path:

```bash
export PATH=/scratch/eecs251b-aba/chipyard-jiaqihuang2025-glitch/.conda-env/bin:/scratch/eecs251b-aba/chipyard-jiaqihuang2025-glitch/.conda-env/riscv-tools/bin:$PATH
```

## Build Benchmarks

From the repository root:

```bash
riscv64-unknown-elf-gcc -O0 -static -o benchmarks/nested_loop.riscv benchmarks/nested_loop.c
riscv64-unknown-elf-gcc -O0 -static -o benchmarks/data_dependent.riscv benchmarks/data_dependent.c
```

## Run With Proxy Kernel

On the EECS 251B Chipyard environment, the proxy kernel is:

```text
/scratch/eecs251b-aba/chipyard-jiaqihuang2025-glitch/toolchains/riscv-tools/riscv-pk/build/pk
```

Nested loop:

```bash
riscv-isa-sim/build/spike --perceptron-stats /scratch/eecs251b-aba/chipyard-jiaqihuang2025-glitch/toolchains/riscv-tools/riscv-pk/build/pk benchmarks/nested_loop.riscv
```

Data-dependent branch benchmark:

```bash
riscv-isa-sim/build/spike --perceptron-stats /scratch/eecs251b-aba/chipyard-jiaqihuang2025-glitch/toolchains/riscv-tools/riscv-pk/build/pk benchmarks/data_dependent.riscv
```

Example outputs from the course environment:

```text
bbl loader
done: 990000
Perceptron stats hart=0: branches=98971 mispredictions=4674 miss_rate=4.723%
```

```text
bbl loader
done: 23923 4058831531
Perceptron stats hart=0: branches=1732024 mispredictions=413941 miss_rate=23.899%
```

## Notes

This is an online predictor model inside Spike. It is useful for branch-prediction experiments, but it is not a cycle-accurate BOOM frontend model.

The printed percentage is branch miss rate:

```text
miss_rate = mispredictions / branches
```

It is not MPKI. To compute MPKI, also count committed instructions:

```text
MPKI = mispredictions * 1000 / committed_instructions
```
