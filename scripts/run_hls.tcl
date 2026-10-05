set project_name fd_bb_slbb_hls
set root [file normalize [pwd]]
open_project -reset $project_name
set_top fd_bb_slbb
add_files [file join $root src fd_bb_slbb.cpp]
add_files [file join $root src fd_bb_slbb.h]
add_files -tb [file join $root tb tb_slbb.cpp]
open_solution -reset solution1
set_part xc7z020clg484-1
create_clock -period 10.0 -name default
config_compile -pipeline_loops 0
set data_dir [file join $root data taillard]
set csim_out [file join $root results rtl host_csim.csv]
csim_design -clean -argv "$data_dir $csim_out"
csynth_design
export_design -format ip_catalog
close_project
exit
