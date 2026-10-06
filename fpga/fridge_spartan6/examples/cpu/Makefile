PART = xc6slx45-3-csg324
TOP = cpu_smoke
TOOLS = ../../bin
BOARD = Atlys
SOURCES = FridgeGlobals.vhd FridgeIRCodes.vhd FridgeRAMBootImage.vhd FridgeRAM.vhd FridgeCPU.vhd cpu_smoke.vhd

.PHONY: all timing load test clean
all: $(TOP).bit

$(TOP).ngc: $(SOURCES) $(TOP).prj $(TOP).xst
	$(TOOLS)/xst -ifn $(TOP).xst -ofn $(TOP).syr
$(TOP).ngd: $(TOP).ngc $(TOP).ucf
	$(TOOLS)/ngdbuild -p $(PART) -uc $(TOP).ucf $< $@
$(TOP)_map.ncd: $(TOP).ngd
	$(TOOLS)/map -p $(PART) -w -o $@ $< $(TOP).pcf
$(TOP).ncd: $(TOP)_map.ncd
	$(TOOLS)/par -w $< $@ $(TOP).pcf
$(TOP).bit: $(TOP).ncd
	$(TOOLS)/bitgen -w $< $@
timing: $(TOP).ncd
	$(TOOLS)/trce -v 10 -u 10 $< $(TOP).pcf -o $(TOP).twr
load: $(TOP).bit
	$(TOOLS)/djtgcfg prog -d $(BOARD) -i 0 --file $<
test: tb_cpu.vhd tb_cpu.prj $(SOURCES)
	$(TOOLS)/fuse work.tb_cpu -prj tb_cpu.prj -o tb_cpu.exe
	$(TOOLS)/ise-run ./tb_cpu.exe -tclbatch run.tcl
	grep -q 'PASS: CPU/RAM smoke test' isim.log
clean:
	rm -rf $(TOP).ngc $(TOP).ngd $(TOP).ncd $(TOP).pcf $(TOP).bit $(TOP).syr \
		$(TOP).bld $(TOP)_map.ncd $(TOP)_map.mrp $(TOP)_map.map $(TOP)_map.ngm \
		$(TOP).pad $(TOP).par $(TOP).ptwx $(TOP).twr $(TOP).twx $(TOP).xpi \
		$(TOP).lso $(TOP)_summary.html $(TOP)_usage.xml *.xrpt *.unroutes \
		_xmsgs xst iseconfig xlnx_auto_0_xdb netlist.lst \
		par_usage_statistics.html tb_cpu.exe tb_cpu.exe.sim \
		isim isim.log fuse.log fuse.xmsgs fuseRelaunch.cmd \
		$(TOP).bgn $(TOP).drc $(TOP)_bitgen.xwbt $(TOP)_map.map \
		$(TOP)_pad.csv $(TOP)_pad.txt $(TOP)_summary.xml isim*.wdb
