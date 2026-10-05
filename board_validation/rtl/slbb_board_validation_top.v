`timescale 1ns/1ps

// ============================================================================
// SL-BB ZedBoard PL-only validation wrapper
// Target  : ZedBoard / XC7Z020-CLG484-1
// Clock   : 100 MHz on-board GCLK (Y9)
// Workload: Taillard ta001, n=20, m=5, initial_ub=1278, max_parents=50
//
// This wrapper DOES NOT modify the exported HLS accelerator. It only provides:
//   1) a synchronous ROM containing ta001 processing times,
//   2) deterministic constants for the HLS inputs,
//   3) an automatic periodic start generator,
//   4) a golden-signature checker and LEDs,
//   5) an ILA instance for board-level runtime observation.
//
// IMPORTANT: this is a DEBUG / VALIDATION build. Do not use its resource or
// power numbers as the clean implementation metrics reported in the paper.
// ============================================================================

module slbb_board_validation_top (
    input  wire GCLK,
    output wire LD0,
    output wire LD1,
    output wire LD2,
    output wire LD3
);

    // ------------------------------------------------------------------------
    // Power-on reset: 128 cycles after FPGA configuration.
    // No PS, software, external reset, or AXI control is required.
    // ------------------------------------------------------------------------
    reg [7:0] por_count = 8'd0;
    always @(posedge GCLK) begin
        if (!por_count[7])
            por_count <= por_count + 1'b1;
    end
    wire hls_rst = ~por_count[7];

    // ------------------------------------------------------------------------
    // HLS control and memory interface
    // ------------------------------------------------------------------------
    reg         ap_start = 1'b0;
    wire        ap_done;
    wire        ap_idle;
    wire        ap_ready;
    wire [13:0] p_address0;
    wire        p_ce0;
    reg  [15:0] p_q0 = 16'd0;
    wire [463:0] out_r;
    wire         out_r_ap_vld;

    // ------------------------------------------------------------------------
    // Taillard ta001 ROM. HLS flattened p[MAX_N][MAX_M] using MAX_M=20,
    // therefore address = job*20 + machine. Unused addresses return zero.
    // Registered output reproduces the one-cycle ap_memory read behavior.
    // ------------------------------------------------------------------------
    function [15:0] ta001_rom;
        input [13:0] addr;
        begin
            case (addr)
            14'd0: ta001_rom = 16'd54; // p[0][0]
            14'd1: ta001_rom = 16'd79; // p[0][1]
            14'd2: ta001_rom = 16'd16; // p[0][2]
            14'd3: ta001_rom = 16'd66; // p[0][3]
            14'd4: ta001_rom = 16'd58; // p[0][4]
            14'd20: ta001_rom = 16'd83; // p[1][0]
            14'd21: ta001_rom = 16'd3; // p[1][1]
            14'd22: ta001_rom = 16'd89; // p[1][2]
            14'd23: ta001_rom = 16'd58; // p[1][3]
            14'd24: ta001_rom = 16'd56; // p[1][4]
            14'd40: ta001_rom = 16'd15; // p[2][0]
            14'd41: ta001_rom = 16'd11; // p[2][1]
            14'd42: ta001_rom = 16'd49; // p[2][2]
            14'd43: ta001_rom = 16'd31; // p[2][3]
            14'd44: ta001_rom = 16'd20; // p[2][4]
            14'd60: ta001_rom = 16'd71; // p[3][0]
            14'd61: ta001_rom = 16'd99; // p[3][1]
            14'd62: ta001_rom = 16'd15; // p[3][2]
            14'd63: ta001_rom = 16'd68; // p[3][3]
            14'd64: ta001_rom = 16'd85; // p[3][4]
            14'd80: ta001_rom = 16'd77; // p[4][0]
            14'd81: ta001_rom = 16'd56; // p[4][1]
            14'd82: ta001_rom = 16'd89; // p[4][2]
            14'd83: ta001_rom = 16'd78; // p[4][3]
            14'd84: ta001_rom = 16'd53; // p[4][4]
            14'd100: ta001_rom = 16'd36; // p[5][0]
            14'd101: ta001_rom = 16'd70; // p[5][1]
            14'd102: ta001_rom = 16'd45; // p[5][2]
            14'd103: ta001_rom = 16'd91; // p[5][3]
            14'd104: ta001_rom = 16'd35; // p[5][4]
            14'd120: ta001_rom = 16'd53; // p[6][0]
            14'd121: ta001_rom = 16'd99; // p[6][1]
            14'd122: ta001_rom = 16'd60; // p[6][2]
            14'd123: ta001_rom = 16'd13; // p[6][3]
            14'd124: ta001_rom = 16'd53; // p[6][4]
            14'd140: ta001_rom = 16'd38; // p[7][0]
            14'd141: ta001_rom = 16'd60; // p[7][1]
            14'd142: ta001_rom = 16'd23; // p[7][2]
            14'd143: ta001_rom = 16'd59; // p[7][3]
            14'd144: ta001_rom = 16'd41; // p[7][4]
            14'd160: ta001_rom = 16'd27; // p[8][0]
            14'd161: ta001_rom = 16'd5; // p[8][1]
            14'd162: ta001_rom = 16'd57; // p[8][2]
            14'd163: ta001_rom = 16'd49; // p[8][3]
            14'd164: ta001_rom = 16'd69; // p[8][4]
            14'd180: ta001_rom = 16'd87; // p[9][0]
            14'd181: ta001_rom = 16'd56; // p[9][1]
            14'd182: ta001_rom = 16'd64; // p[9][2]
            14'd183: ta001_rom = 16'd85; // p[9][3]
            14'd184: ta001_rom = 16'd13; // p[9][4]
            14'd200: ta001_rom = 16'd76; // p[10][0]
            14'd201: ta001_rom = 16'd3; // p[10][1]
            14'd202: ta001_rom = 16'd7; // p[10][2]
            14'd203: ta001_rom = 16'd85; // p[10][3]
            14'd204: ta001_rom = 16'd86; // p[10][4]
            14'd220: ta001_rom = 16'd91; // p[11][0]
            14'd221: ta001_rom = 16'd61; // p[11][1]
            14'd222: ta001_rom = 16'd1; // p[11][2]
            14'd223: ta001_rom = 16'd9; // p[11][3]
            14'd224: ta001_rom = 16'd72; // p[11][4]
            14'd240: ta001_rom = 16'd14; // p[12][0]
            14'd241: ta001_rom = 16'd73; // p[12][1]
            14'd242: ta001_rom = 16'd63; // p[12][2]
            14'd243: ta001_rom = 16'd39; // p[12][3]
            14'd244: ta001_rom = 16'd8; // p[12][4]
            14'd260: ta001_rom = 16'd29; // p[13][0]
            14'd261: ta001_rom = 16'd75; // p[13][1]
            14'd262: ta001_rom = 16'd41; // p[13][2]
            14'd263: ta001_rom = 16'd41; // p[13][3]
            14'd264: ta001_rom = 16'd49; // p[13][4]
            14'd280: ta001_rom = 16'd12; // p[14][0]
            14'd281: ta001_rom = 16'd47; // p[14][1]
            14'd282: ta001_rom = 16'd63; // p[14][2]
            14'd283: ta001_rom = 16'd56; // p[14][3]
            14'd284: ta001_rom = 16'd47; // p[14][4]
            14'd300: ta001_rom = 16'd77; // p[15][0]
            14'd301: ta001_rom = 16'd14; // p[15][1]
            14'd302: ta001_rom = 16'd47; // p[15][2]
            14'd303: ta001_rom = 16'd40; // p[15][3]
            14'd304: ta001_rom = 16'd87; // p[15][4]
            14'd320: ta001_rom = 16'd32; // p[16][0]
            14'd321: ta001_rom = 16'd21; // p[16][1]
            14'd322: ta001_rom = 16'd26; // p[16][2]
            14'd323: ta001_rom = 16'd54; // p[16][3]
            14'd324: ta001_rom = 16'd58; // p[16][4]
            14'd340: ta001_rom = 16'd87; // p[17][0]
            14'd341: ta001_rom = 16'd86; // p[17][1]
            14'd342: ta001_rom = 16'd75; // p[17][2]
            14'd343: ta001_rom = 16'd77; // p[17][3]
            14'd344: ta001_rom = 16'd18; // p[17][4]
            14'd360: ta001_rom = 16'd68; // p[18][0]
            14'd361: ta001_rom = 16'd5; // p[18][1]
            14'd362: ta001_rom = 16'd77; // p[18][2]
            14'd363: ta001_rom = 16'd51; // p[18][3]
            14'd364: ta001_rom = 16'd68; // p[18][4]
            14'd380: ta001_rom = 16'd94; // p[19][0]
            14'd381: ta001_rom = 16'd77; // p[19][1]
            14'd382: ta001_rom = 16'd40; // p[19][2]
            14'd383: ta001_rom = 16'd31; // p[19][3]
            14'd384: ta001_rom = 16'd28; // p[19][4]
                default: ta001_rom = 16'd0;
            endcase
        end
    endfunction

    always @(posedge GCLK) begin
        if (p_ce0)
            p_q0 <= ta001_rom(p_address0);
    end

    // ------------------------------------------------------------------------
    // Final exported HLS accelerator IP -- unchanged.
    // ------------------------------------------------------------------------
    fd_bb_slbb_0 u_slbb (
        .ap_clk             (GCLK),
        .ap_rst             (hls_rst),
        .ap_start           (ap_start),
        .ap_done            (ap_done),
        .ap_idle            (ap_idle),
        .ap_ready           (ap_ready),
        .p_address0         (p_address0),
        .p_ce0              (p_ce0),
        .p_q0               (p_q0),
        .n                  (32'd20),
        .m                  (32'd5),
        .initial_ub         (16'd1278),
        .max_parents_popped (32'd50),
        .out_r              (out_r),
        .out_r_ap_vld       (out_r_ap_vld)
    );

    // ------------------------------------------------------------------------
    // Telemetry field mapping of the HLS-flattened Telemetry struct.
    // ------------------------------------------------------------------------
    wire [31:0] out_parents      = out_r[31:0];
    wire [31:0] out_candidates   = out_r[63:32];
    wire [31:0] out_lb_evals     = out_r[95:64];
    wire [31:0] out_dir_probes   = out_r[127:96];
    wire [31:0] out_pruned       = out_r[159:128];
    wire [31:0] out_committed    = out_r[191:160];
    wire [31:0] out_peak_stack   = out_r[223:192];
    wire [15:0] out_max_cfwd     = out_r[239:224];
    wire [15:0] out_max_cbwd     = out_r[255:240];
    wire [15:0] out_max_lb1      = out_r[271:256];
    wire [15:0] out_max_obs      = out_r[287:272];
    wire [31:0] out_max_sum_lb1  = out_r[319:288];
    wire [63:0] out_bram_words   = out_r[383:320];
    wire [63:0] out_cycles_model = out_r[447:384];
    wire        out_complete     = out_r[448];
    wire        out_overflow     = out_r[456];

    // Golden C-simulation signature for ta001 with max_parents=50:
    // parents=7, candidates=128, lb=256, dir=14, pruned=122, committed=6,
    // peak=2, max_cfwd=368, max_cbwd=529, max_lb1=1450, max_obs=1450,
    // max_sum_lb1=26452, bram_words=114, total_cycles_model=1576,
    // complete=1, overflow=0.
    wire golden_match_now =
        (out_parents      == 32'd7)     &&
        (out_candidates   == 32'd128)   &&
        (out_lb_evals     == 32'd256)   &&
        (out_dir_probes   == 32'd14)    &&
        (out_pruned       == 32'd122)   &&
        (out_committed    == 32'd6)     &&
        (out_peak_stack   == 32'd2)     &&
        (out_max_cfwd     == 16'd368)   &&
        (out_max_cbwd     == 16'd529)   &&
        (out_max_lb1      == 16'd1450)  &&
        (out_max_obs      == 16'd1450)  &&
        (out_max_sum_lb1  == 32'd26452) &&
        (out_bram_words   == 64'd114)   &&
        (out_cycles_model == 64'd1576)  &&
        (out_complete     == 1'b1)      &&
        (out_overflow     == 1'b0);

    // ------------------------------------------------------------------------
    // Automatic repeated execution.
    // After each run, wait ~0.25 s so Hardware Manager can be re-armed easily.
    // ------------------------------------------------------------------------
    localparam [1:0] ST_LAUNCH = 2'd0,
                     ST_RUN    = 2'd1,
                     ST_WAIT   = 2'd2;

    localparam integer WAIT_CYCLES = 25_000_000;

    reg [1:0]  run_state = ST_LAUNCH;
    reg [24:0] wait_counter = 25'd0;
    reg [31:0] exec_cycle_counter = 32'd0;
    reg [31:0] run_counter = 32'd0;

    reg [31:0] parents_latched   = 32'd0;
    reg [31:0] cand_latched      = 32'd0;
    reg [31:0] committed_latched = 32'd0;
    reg [31:0] pruned_latched    = 32'd0;
    reg [15:0] max_lb1_latched   = 16'd0;
    reg        complete_latched  = 1'b0;
    reg        overflow_latched  = 1'b0;
    reg        pass_latched      = 1'b0;

    always @(posedge GCLK) begin
        if (hls_rst) begin
            ap_start             <= 1'b0;
            run_state            <= ST_LAUNCH;
            wait_counter         <= 25'd0;
            exec_cycle_counter   <= 32'd0;
            run_counter          <= 32'd0;
            parents_latched      <= 32'd0;
            cand_latched         <= 32'd0;
            committed_latched    <= 32'd0;
            pruned_latched       <= 32'd0;
            max_lb1_latched      <= 16'd0;
            complete_latched     <= 1'b0;
            overflow_latched     <= 1'b0;
            pass_latched         <= 1'b0;
        end else begin
            ap_start <= 1'b0;

            if (out_r_ap_vld) begin
                parents_latched   <= out_parents;
                cand_latched      <= out_candidates;
                committed_latched <= out_committed;
                pruned_latched    <= out_pruned;
                max_lb1_latched   <= out_max_lb1;
                complete_latched  <= out_complete;
                overflow_latched  <= out_overflow;
                pass_latched      <= golden_match_now;
            end

            case (run_state)
                ST_LAUNCH: begin
                    wait_counter       <= 25'd0;
                    exec_cycle_counter <= 32'd0;
                    if (ap_idle) begin
                        ap_start  <= 1'b1;
                        run_state <= ST_RUN;
                    end
                end

                ST_RUN: begin
                    exec_cycle_counter <= exec_cycle_counter + 1'b1;
                    if (ap_done) begin
                        run_counter  <= run_counter + 1'b1;
                        wait_counter <= 25'd0;
                        run_state    <= ST_WAIT;
                    end
                end

                ST_WAIT: begin
                    if (wait_counter == WAIT_CYCLES-1) begin
                        wait_counter <= 25'd0;
                        run_state    <= ST_LAUNCH;
                    end else begin
                        wait_counter <= wait_counter + 1'b1;
                    end
                end

                default: run_state <= ST_LAUNCH;
            endcase
        end
    end

    // ------------------------------------------------------------------------
    // Human-visible LEDs
    // LD0 : heartbeat (~0.75 Hz)
    // LD1 : accelerator busy
    // LD2 : golden-signature PASS (stays ON after a correct run)
    // LD3 : post-run wait window (easy to photograph)
    // ------------------------------------------------------------------------
    reg [26:0] heartbeat = 27'd0;
    always @(posedge GCLK)
        heartbeat <= heartbeat + 1'b1;

    assign LD0 = heartbeat[26];
    assign LD1 = (run_state == ST_RUN);
    assign LD2 = pass_latched;
    assign LD3 = (run_state == ST_WAIT);

    // ------------------------------------------------------------------------
    // ILA probes. Trigger suggestion in Hardware Manager:
    //   ap_start == 1   (first choice)
    // or
    //   ap_done  == 1   (if you prefer the end of the run).
    // ------------------------------------------------------------------------
    wire [3:0] status_flags = {pass_latched, out_r_ap_vld, complete_latched, overflow_latched};

    ila_0 u_ila (
        .clk    (GCLK),
        .probe0 (ap_start),
        .probe1 (ap_idle),
        .probe2 (ap_done),
        .probe3 (ap_ready),
        .probe4 (p_ce0),
        .probe5 (p_address0),
        .probe6 (exec_cycle_counter),
        .probe7 (parents_latched),
        .probe8 (cand_latched),
        .probe9 (committed_latched),
        .probe10(max_lb1_latched),
        .probe11(pruned_latched),
        .probe12(status_flags)
    );

endmodule
