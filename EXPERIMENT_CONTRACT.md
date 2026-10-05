# Experiment contract

This repository distinguishes three evaluation stages used in the manuscript.

## 1. State-lifetime profiling
- Benchmark: Taillard ta001-ta120.
- Profiling workload: 20,000 parent expansions per instance.
- Purpose: characterize transient/persistent candidate-state behaviour and derive the analytical state-volume model.
- Final profiling outputs: `results/profiling/`.

## 2. Matched RTL architectural comparison
- Architectures: Conventional B&B, Unpipelined SL-BB, Pipelined SL-BB.
- Benchmark: Taillard ta001-ta120.
- Matched workload: 50 parent expansions per instance.
- Same processing-time matrices, initial upper bounds, lower-bound formulation, branching/pruning policy, candidate ordering, tie-breaking, and LIFO search semantics.
- Raw outputs: `results/rtl/raw/`.

## 3. Physical board validation
- ZedBoard / Xilinx Zynq-7000 XC7Z020, 100 MHz.
- PL-only ILA-enabled debug build.
- Representative workload: ta001 (20x5), `max_parents_popped=50`; the run completes after 7 parent expansions.
- Board telemetry matched the HLS reference signature: 7 parents, 128 candidates, 122 pruned, 6 committed survivors, max LB1 = 1450, complete = 1, overflow = 0.
- The debug build is separate from the clean implementations used for manuscript resource, timing, and power metrics.

## Toolchain
- Vitis HLS 2021.1
- Vivado 2021.1
- Device: xc7z020clg484-1
- Target clock: 100 MHz (10 ns)
