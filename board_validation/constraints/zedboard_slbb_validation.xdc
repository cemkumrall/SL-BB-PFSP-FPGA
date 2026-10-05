# SL-BB ZedBoard board-validation constraints
# Digilent ZedBoard / XC7Z020-CLG484-1

# 100 MHz on-board oscillator (GCLK, Bank 13, fixed 3.3 V)
set_property PACKAGE_PIN Y9 [get_ports {GCLK}]
set_property IOSTANDARD LVCMOS33 [get_ports {GCLK}]
create_clock -add -name GCLK_100MHz -period 10.000 -waveform {0.000 5.000} [get_ports {GCLK}]

# User LEDs, Bank 33 (fixed 3.3 V)
set_property PACKAGE_PIN T22 [get_ports {LD0}]
set_property PACKAGE_PIN T21 [get_ports {LD1}]
set_property PACKAGE_PIN U22 [get_ports {LD2}]
set_property PACKAGE_PIN U21 [get_ports {LD3}]
set_property IOSTANDARD LVCMOS33 [get_ports {LD0 LD1 LD2 LD3}]
