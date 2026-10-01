# Blinky on Digilent Atlys (XC6SLX45-3CSG324)
# Tools run inside the ise:14.7 Docker container via ../../bin wrappers.

PART  = xc6slx45-3-csg324
TOP   = blinky
TOOLS = ../../bin

# Digilent Adept: the Atlys onboard USB-JTAG is a Digilent FX2 device
# (VID:PID 1443:0007, DJTG protocol) - openFPGALoader cannot drive it.
DJTGCFG = ../../bin/djtgcfg
BOARD   = Atlys

.PHONY: all load timing clean

all: $(TOP).bit

$(TOP).ngc: $(TOP).vhd $(TOP).prj $(TOP).xst
	$(TOOLS)/xst -ifn $(TOP).xst -ofn $(TOP).syr

$(TOP).ngd: $(TOP).ngc $(TOP).ucf
	$(TOOLS)/ngdbuild -p $(PART) -uc $(TOP).ucf $(TOP).ngc $@

$(TOP)_map.ncd: $(TOP).ngd
	$(TOOLS)/map -p $(PART) -w -o $@ $(TOP).ngd $(TOP).pcf

$(TOP).ncd: $(TOP)_map.ncd
	$(TOOLS)/par -w $< $@ $(TOP).pcf

$(TOP).bit: $(TOP).ncd
	$(TOOLS)/bitgen -w $(TOP).ncd $@

timing: $(TOP).ncd
	$(TOOLS)/trce -v 10 $(TOP).ncd $(TOP).pcf -o $(TOP).twr

# Load into FPGA SRAM via the onboard Digilent USB-JTAG (volatile, lost on
# power-off). Flash programming is not supported via djtgcfg on the Atlys.
load: $(TOP).bit
	$(DJTGCFG) prog -d $(BOARD) -i 0 --file $(TOP).bit

clean:
	rm -rf $(TOP).ngc $(TOP).ngd $(TOP).ncd $(TOP).pcf $(TOP).bit $(TOP).syr \
		$(TOP).bld $(TOP).map $(TOP).mrp $(TOP).ngm $(TOP).pad $(TOP).par \
		$(TOP).ptwx $(TOP).twr $(TOP).twx $(TOP).xpi $(TOP).lso $(TOP).srp \
		$(TOP)_map.ncd $(TOP)_summary.html $(TOP)_usage.xml \
		*.xrpt *.unroutes _xmsgs xst iseconfig xlnx_auto_0_xdb
