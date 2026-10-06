PART = xc6slx45-3-csg324
TOP = keyboard
TOOLS = ../../bin
BOARD = Atlys
SOURCES = FridgeGlobals.vhd FridgeIRCodes.vhd FridgeRAMBootImage.vhd FridgeRAM.vhd \
	FridgeCPU.vhd video_timing.vhd fridge_gpu.vhd tmds_encoder.vhd tmds_serializer.vhd \
	ps2_receiver.vhd fridge_keyboard.vhd keyboard.vhd

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
test: tb_ps2_receiver.vhd tb_ps2_receiver.prj tb_keyboard.vhd tb_keyboard.prj \
	tb_system.vhd tb_system.prj $(SOURCES)
	$(TOOLS)/fuse work.tb_ps2_receiver -prj tb_ps2_receiver.prj -o tb_ps2_receiver.exe
	$(TOOLS)/ise-run ./tb_ps2_receiver.exe -tclbatch run.tcl
	grep -q 'PASS: PS/2 receiver tests' isim.log
	$(TOOLS)/fuse work.tb_keyboard -prj tb_keyboard.prj -o tb_keyboard.exe
	$(TOOLS)/ise-run ./tb_keyboard.exe -tclbatch run.tcl
	grep -q 'PASS: keyboard decoder/FIFO tests' isim.log
	$(TOOLS)/fuse work.tb_system -prj tb_system.prj -o tb_system.exe
	$(TOOLS)/ise-run ./tb_system.exe -tclbatch run.tcl
	grep -q 'PASS: CPU/keyboard/HDMI integration' isim.log
clean:
	rm -rf $(TOP).ngc $(TOP).ngd $(TOP).ncd $(TOP).pcf $(TOP).bit $(TOP).syr \
		$(TOP).bld $(TOP)_map.ncd $(TOP)_map.mrp $(TOP)_map.map $(TOP)_map.ngm \
		$(TOP).pad $(TOP).par $(TOP).ptwx $(TOP).twr $(TOP).twx $(TOP).xpi \
		$(TOP).lso $(TOP)_summary.html $(TOP)_usage.xml *.xrpt *.unroutes \
		_xmsgs xst iseconfig xlnx_auto_0_xdb netlist.lst \
		par_usage_statistics.html tb_ps2_receiver.exe tb_ps2_receiver.exe.sim \
		tb_keyboard.exe tb_keyboard.exe.sim tb_system.exe tb_system.exe.sim \
		isim isim.log fuse.log fuse.xmsgs fuseRelaunch.cmd \
		$(TOP).bgn $(TOP).drc $(TOP)_bitgen.xwbt $(TOP)_map.map \
		$(TOP)_pad.csv $(TOP)_pad.txt $(TOP)_summary.xml isim*.wdb
