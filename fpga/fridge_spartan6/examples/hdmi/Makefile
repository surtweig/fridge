PART = xc6slx45-3-csg324
TOP = hdmi
TOOLS = ../../bin
BOARD = Atlys
SOURCES = video_pattern.vhd tmds_encoder.vhd tmds_serializer.vhd hdmi.vhd

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
test: tb_video.vhd tb_video.prj tb_serializer.vhd tb_serializer.prj tb_hdmi.vhd tb_hdmi.prj $(SOURCES)
	$(TOOLS)/fuse work.tb_video -prj tb_video.prj -o tb_video.exe
	$(TOOLS)/ise-run ./tb_video.exe -tclbatch run.tcl
	grep -q 'PASS: full 1650x750 raster' isim.log
	grep -q 'PASS: TMDS controls' isim.log
	$(TOOLS)/fuse work.tb_serializer -prj tb_serializer.prj -o tb_serializer.exe
	$(TOOLS)/ise-run ./tb_serializer.exe -tclbatch run.tcl
	grep -q 'PASS: OSERDES2/BUFPLL' isim.log
	$(TOOLS)/fuse work.tb_hdmi -prj tb_hdmi.prj -o tb_hdmi.exe
	$(TOOLS)/ise-run ./tb_hdmi.exe -tclbatch run_clocks.tcl
	grep -q 'PASS: DCM/PLL/BUFPLL' isim.log
clean:
	rm -rf hdmi.ngc hdmi.ngd hdmi.ncd hdmi.pcf hdmi.bit hdmi.syr hdmi.bld \
		hdmi_map.ncd hdmi.mrp hdmi.map hdmi.ngm hdmi.pad hdmi.par hdmi.ptwx \
		hdmi.twr hdmi.twx hdmi.xpi hdmi.lso hdmi.srp hdmi_summary.html \
		hdmi_usage.xml *.xrpt *.unroutes _xmsgs xst iseconfig xlnx_auto_0_xdb \
		tb_video.exe tb_video.exe.sim isim isim.log fuse.log fuse.xmsgs \
		fuseRelaunch.cmd tb_serializer.exe tb_serializer.exe.sim tb_hdmi.exe tb_hdmi.exe.sim \
		hdmi.bgn hdmi.drc hdmi_bitgen.xwbt hdmi_map.map hdmi_map.ngm \
		hdmi_pad.csv hdmi_pad.txt hdmi_summary.xml netlist.lst \
		par_usage_statistics.html isim*.wdb
