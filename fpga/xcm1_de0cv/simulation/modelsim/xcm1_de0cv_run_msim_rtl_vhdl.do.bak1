transcript on
if {[file exists rtl_work]} {
	vdel -lib rtl_work -all
}
vlib rtl_work
vmap work rtl_work

vlib xcm_clk_pll
vmap xcm_clk_pll xcm_clk_pll
vlog -vlog01compat -work xcm_clk_pll +incdir+T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/xcm_clk_pll/synthesis/submodules {T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/xcm_clk_pll/synthesis/submodules/xcm_clk_pll_pll_0.v}
vlib vga_pll
vmap vga_pll vga_pll
vlog -vlog01compat -work vga_pll +incdir+T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/vga_pll/synthesis/submodules {T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/vga_pll/synthesis/submodules/vga_pll_pll_0.v}
vcom -2008 -work work {T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/xcm_clk_pll/synthesis/xcm_clk_pll.vhd}
vcom -2008 -work work {T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/xcm_clk_pll/synthesis/xcm_clk_pll.vhd}
vcom -2008 -work vga_pll {T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/vga_pll/synthesis/vga_pll.vhd}
vcom -2008 -work work {T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/XCMMain.vhd}
vcom -2008 -work work {T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/VGAController.vhd}
vcom -2008 -work work {T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/XCMIRCodes.vhd}
vcom -2008 -work work {T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/ROMImage.vhd}
vcom -2008 -work work {T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/XCMCPU.vhd}
vcom -2008 -work work {T:/svn/work/cpp/xcm2/fpga/xcm1_de0cv/XCMROM.vhd}

