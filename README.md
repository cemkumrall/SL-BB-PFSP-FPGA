# SL-BB: State-Lifetime-Aware Branch-and-Bound FPGA Architecture

Research artifacts for the manuscript **“SL-BB: A State-Lifetime-Aware Branch-and-Bound FPGA Architecture for High-Throughput Exact Permutation Flow Shop Scheduling.”**

SL-BB reorganizes FPGA-based exact PFSP branch-and-bound execution around measured candidate-state lifetime. Candidate information remains compact during evaluation, and persistent search state is materialized only for candidates that survive pruning.

## Repository contents
- `src/` — final Pipelined SL-BB HLS source.
- `tb/` — final HLS/host testbench.
- `scripts/` — minimal HLS and batch C-simulation scripts.
- `data/` — benchmark metadata and instructions; raw Taillard files are not redistributed.
- `results/profiling/` — final 120-instance state-lifetime profiling outputs.
- `results/rtl/` — raw RTL transaction outputs and manuscript-level RTL summary.
- `results/post_route/` — clean Vivado post-route utilization, timing, and power reports plus a compact summary.
- `results/power_energy_summary.csv` — derived throughput/energy values reported in the manuscript.
- `board_validation/` — ZedBoard PL-only validation wrapper, constraints, project Tcl, frozen exported HLS IP, expected ta001 signature, and validation evidence.
- `EXPERIMENT_CONTRACT.md` — exact separation of profiling, matched RTL comparison, and board-validation workloads.

## Key reported results
Across 120 Taillard PFSP instances, profiling identified 93.91% of generated candidates as transient. Relative to Conventional B&B, final Pipelined SL-BB reduced post-route BRAM utilization by 90.87%, achieved an average RTL speedup of approximately 8.21x, and reduced estimated per-instance energy by 89.12%. The design met the 100 MHz timing target and was physically validated on a Zynq-7000 XC7Z020 FPGA.

## Reproducing the final HLS source
Requirements: Vitis HLS 2021.1, part `xc7z020clg484-1`, 10 ns target clock, and Taillard files under `data/taillard/`.

From the repository root:
```text
vitis_hls -f scripts/run_hls.tcl
```

## Physical ZedBoard validation
The `board_validation/` directory contains the exact PL-only validation assets used for the manuscript demonstration. The wrapper embeds ta001, checks the expected telemetry signature, and exposes runtime signals through ILA.

**Important:** the ILA-enabled validation build is a debug build. Its resource utilization is not used for the manuscript resource/timing/power comparison. The authoritative clean reports are under `results/post_route/raw/`.

## Workload distinction
State-lifetime profiling uses 20,000 parent expansions per instance. The matched RTL architectural comparison uses 50 parent expansions per instance. The ta001 physical validation run also uses a maximum of 50 and completes after 7. See `EXPERIMENT_CONTRACT.md`.

## Citation
A versioned Zenodo DOI will be added after the public GitHub release is archived.
