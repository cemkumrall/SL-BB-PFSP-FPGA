# SL-BB ZedBoard PL-only Board Validation

This package is built specifically around the final exported `fd_bb_slbb` HLS IP found in the supplied project. The accelerator IP itself is not modified. The wrapper supplies a deterministic Taillard `ta001` ROM, fixed inputs, automatic repeated starts, LED status, a golden C-simulation signature check, and an ILA core.

## What the LEDs mean

- **LD0**: heartbeat (board clock is alive)
- **LD1**: SL-BB accelerator is currently running
- **LD2**: **PASS** — the returned telemetry exactly matches the golden `ta001` C-simulation signature and `complete_flag=1`, `overflow_flag=0`
- **LD3**: post-run wait window (~0.25 s); the test automatically repeats

## Fastest route in Vivado 2021.1

1. Extract this folder to a short path, e.g. `C:/FPGA_BB/SL-BB-PFSP-FPGA/board_validation`.
2. Open **Vivado 2021.1**.
3. In the Tcl Console:
   ```tcl
   cd C:/FPGA_BB/SL-BB-PFSP-FPGA/board_validation
   source ./tcl/create_slbb_validation_project.tcl
   ```
4. When the project opens, click **Generate Bitstream**.
5. Connect the ZedBoard JTAG USB cable, power the board, then open **Hardware Manager -> Open target -> Auto Connect**.
6. Program the device with the generated `.bit`. Vivado should associate the ILA debug probes automatically; if it asks for an `.ltx`, select the one generated in the project run directory.
7. In `hw_ila_1` / `ila_0`, use a trigger such as `probe0 (ap_start) == 1`. Arm the trigger. If the end-of-run pulse is outside the captured window, trigger on `probe2 (ap_done) == 1` instead. The wrapper repeats the test every ~0.25 s, so you do not need a push button or PS software.
8. For the publication screenshot, rename/display the important probes as:
   - `ap_start`
   - `ap_idle`
   - `ap_done`
   - `p_ce0`
   - `p_address0`
   - `exec_cycle_counter`
   - `parents_latched`
   - `cand_latched`
   - `committed_latched`
   - `max_lb1_latched`
   - `pruned_latched`
   - `status_flags`
9. After a correct run, **LD2 must be ON**. `status_flags[3]` is the same PASS condition.

## Figure 9 photo

Recommended scene:
- ZedBoard in the foreground, with LD2 visible ON.
- Laptop on one side with Vivado Hardware Manager showing the programmed XC7Z020.
- Large TV/monitor behind it showing the ILA capture.

This is a debug/validation build. Keep it separate from the clean post-route implementation used for the paper's resource, timing, and power metrics.

## ILA probe order

| Probe | Width | Signal |
|---|---:|---|
| probe0 | 1 | ap_start |
| probe1 | 1 | ap_idle |
| probe2 | 1 | ap_done |
| probe3 | 1 | ap_ready |
| probe4 | 1 | p_ce0 |
| probe5 | 14 | p_address0 |
| probe6 | 32 | exec_cycle_counter |
| probe7 | 32 | parents_latched |
| probe8 | 32 | cand_latched |
| probe9 | 32 | committed_latched |
| probe10 | 16 | max_lb1_latched |
| probe11 | 32 | pruned_latched |
| probe12 | 4 | {PASS, out_valid, complete, overflow} |
