# ============================================================================
# Create SL-BB ZedBoard validation project (Vivado 2021.1)
# Usage from Vivado Tcl Console:
#   cd <folder-containing-this-package>
#   source ./tcl/create_slbb_validation_project.tcl
# ============================================================================

set SCRIPT_DIR [file normalize [file dirname [info script]]]
set ROOT_DIR   [file normalize [file join $SCRIPT_DIR ..]]
set PROJ_DIR   [file join $ROOT_DIR vivado_project]
set OUT_DIR    [file join $ROOT_DIR output]
file mkdir $OUT_DIR

create_project -force SLBB_ZedBoard_Validation $PROJ_DIR -part xc7z020clg484-1
set_property target_language Verilog [current_project]
set_property simulator_language Mixed [current_project]

# Local HLS IP repository
set_property ip_repo_paths [list [file join $ROOT_DIR ip_repo]] [current_project]
update_ip_catalog

# Final exported HLS accelerator
create_ip -vlnv xilinx.com:hls:fd_bb_slbb:1.0 -module_name fd_bb_slbb_0
generate_target all [get_ips fd_bb_slbb_0]

# Integrated Logic Analyzer for the validation screenshot
create_ip -name ila -vendor xilinx.com -library ip -module_name ila_0
set_property -dict [list \
    CONFIG.C_NUM_OF_PROBES {13} \
    CONFIG.C_DATA_DEPTH {8192} \
    CONFIG.C_PROBE0_WIDTH {1} \
    CONFIG.C_PROBE1_WIDTH {1} \
    CONFIG.C_PROBE2_WIDTH {1} \
    CONFIG.C_PROBE3_WIDTH {1} \
    CONFIG.C_PROBE4_WIDTH {1} \
    CONFIG.C_PROBE5_WIDTH {14} \
    CONFIG.C_PROBE6_WIDTH {32} \
    CONFIG.C_PROBE7_WIDTH {32} \
    CONFIG.C_PROBE8_WIDTH {32} \
    CONFIG.C_PROBE9_WIDTH {32} \
    CONFIG.C_PROBE10_WIDTH {16} \
    CONFIG.C_PROBE11_WIDTH {32} \
    CONFIG.C_PROBE12_WIDTH {4} \
] [get_ips ila_0]
generate_target all [get_ips ila_0]

# RTL wrapper and board constraints
add_files -norecurse [file join $ROOT_DIR rtl slbb_board_validation_top.v]
add_files -fileset constrs_1 -norecurse [file join $ROOT_DIR constraints zedboard_slbb_validation.xdc]
set_property top slbb_board_validation_top [current_fileset]
update_compile_order -fileset sources_1

puts "\n\[INFO\] Project created successfully."
puts "\[INFO\] Top: slbb_board_validation_top"
puts "\[INFO\] Device: xc7z020clg484-1"
puts "[INFO] You may now click Generate Bitstream, or run:"
puts "       launch_runs impl_1 -to_step write_bitstream -jobs 4"
puts "\n[INFO] After bitstream generation, open Hardware Manager, program the device,"
puts "       and use the automatically inserted ila_0 core."
