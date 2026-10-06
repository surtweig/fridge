PART = xc6slx45-3-csg324
TOP = rom
TOOLS = ../../bin
HOSTTOOLS = ../../tools
BOARD = Atlys
SOURCES = FridgeGlobals.vhd FridgeIRCodes.vhd FridgeROMImage.vhd FridgeRAMBootImage.vhd \
	FridgeRAM.vhd FridgeCPU.vhd video_timing.vhd fridge_gpu.vhd tmds_encoder.vhd \
	tmds_serializer.vhd fridge_rom.vhd rom.vhd

ROM_SECTIONS = src/rom_image/seg0.hex src/rom_image/seg1.hex \
	src/rom_image/seg2.hex src/rom_image/seg3.hex

.PHONY: all timing load test regen clean
all: $(TOP).bit

# Program and ROM image are generated from the sources in src/ by the host
# tools; both VHDL files are checked in so a plain build needs no extra step.
FridgeRAMBootImage.vhd: src/rom_paint.falc $(HOSTTOOLS)/falc $(HOSTTOOLS)/build-falc.sh
	$(HOSTTOOLS)/falc src/rom_paint.falc rom_paint.bin -vhdl-aggregate
	mv rom_paint.bin.vhd $@

FridgeROMImage.vhd: $(ROM_SECTIONS) $(HOSTTOOLS)/rom2vhd.py
	$(HOSTTOOLS)/rom2vhd.py --raw -o $@ $(ROM_SECTIONS)

regen: FridgeRAMBootImage.vhd FridgeROMImage.vhd

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
test: tb_rom.vhd tb_rom.prj tb_system.vhd tb_system.prj $(SOURCES)
	$(TOOLS)/fuse work.tb_rom -prj tb_rom.prj -o tb_rom.exe
	$(TOOLS)/ise-run ./tb_rom.exe -tclbatch run.tcl
	grep -q 'PASS: ROM device tests' isim.log
	$(TOOLS)/fuse work.tb_system -prj tb_system.prj -o tb_system.exe
	$(TOOLS)/ise-run ./tb_system.exe -tclbatch run.tcl
	grep -q 'PASS: CPU/ROM/HDMI integration' isim.log
clean:
	rm -rf $(TOP).ngc $(TOP).ngd $(TOP).ncd $(TOP).pcf $(TOP).bit $(TOP).syr \
		$(TOP).bld $(TOP)_map.ncd $(TOP)_map.mrp $(TOP)_map.map $(TOP)_map.ngm \
		$(TOP).pad $(TOP).par $(TOP).ptwx $(TOP).twr $(TOP).twx $(TOP).xpi \
		$(TOP).ngm $(TOP).lso $(TOP).drc $(TOP).xwbt $(TOP).xrpt $(TOP)_ngdbuild.xrpt \
		$(TOP)_map.xrpt $(TOP)_pad.xrpt $(TOP)_par.xrpt $(TOP)_xst.xrpt \
		rom_paint.bin \
		*.exe *.wdb xst xlnx_auto_0_xdb _xmsgs fuse.log fuse.xmsgs fuseRelaunch.cmd \
		isim.log netlist.lst par_usage_statistics.html
