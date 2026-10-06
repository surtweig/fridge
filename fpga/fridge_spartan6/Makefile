PROGRAM ?= integration
PART = xc6slx45-3-csg324
BOARD = Atlys
BUILD = .local/build/$(PROGRAM)
RTL = $(wildcard rtl/*.vhd rtl/core/*.vhd rtl/video/*.vhd rtl/devices/*.vhd)
HOST = $(wildcard ../../falc/*.cpp ../../falc/*.h ../../falc/CMakeLists.txt ../../include/*.h)
STDLIB = $(wildcard ../../x2al_std/*.inc)
INPUTS = programs/$(PROGRAM)/boot.falc $(wildcard programs/$(PROGRAM)/rom/*.hex)
TOOLS = $(abspath bin)

.PHONY: all regen timing load test test-system test-units regressions clean
all: $(BUILD)/fridge.bit

$(BUILD)/images.stamp: $(INPUTS) $(HOST) $(STDLIB) tools/build-falc.sh tools/falc tools/prepare-build.py tools/rom2vhd.py
	mkdir -p $(BUILD)
	tools/build-falc.sh
	python3 tools/prepare-build.py $(PROGRAM) $(BUILD)
	touch $@

regen:
	mkdir -p $(BUILD)
	tools/build-falc.sh
	python3 tools/prepare-build.py $(PROGRAM) $(BUILD)
	touch $(BUILD)/images.stamp

$(BUILD)/fridge.ngc: $(BUILD)/images.stamp $(RTL)
	cd $(BUILD) && $(TOOLS)/xst -ifn fridge.xst -ofn fridge.syr
$(BUILD)/fridge.ngd: $(BUILD)/fridge.ngc constraints/fridge_atlys.ucf
	cd $(BUILD) && $(TOOLS)/ngdbuild -p $(PART) -uc ../../../constraints/fridge_atlys.ucf fridge.ngc fridge.ngd
$(BUILD)/fridge_map.ncd: $(BUILD)/fridge.ngd
	cd $(BUILD) && $(TOOLS)/map -p $(PART) -w -o fridge_map.ncd fridge.ngd fridge.pcf
$(BUILD)/fridge.ncd: $(BUILD)/fridge_map.ncd
	cd $(BUILD) && $(TOOLS)/par -w fridge_map.ncd fridge.ncd fridge.pcf
$(BUILD)/fridge.bit: $(BUILD)/fridge.ncd
	cd $(BUILD) && $(TOOLS)/bitgen -w fridge.ncd fridge.bit
timing: $(BUILD)/fridge.ncd
	cd $(BUILD) && $(TOOLS)/trce -v 10 -u 10 fridge.ncd fridge.pcf -o fridge.twr
load: $(BUILD)/fridge.bit
	$(TOOLS)/djtgcfg prog -d $(BOARD) -i 0 --file $<

# Each test has a separate ISim work library and image package set.
test:
	python3 tools/run-tests.py
test-system:
	python3 tools/run-tests.py system
test-units:
	python3 tools/run-tests.py units
regressions:
	$(MAKE) -C examples/hdmi test
	$(MAKE) -C examples/cpu test
	$(MAKE) -C examples/gpu test
	$(MAKE) -C examples/keyboard test
	$(MAKE) -C examples/rom test
	$(MAKE) -C examples/text test
	$(MAKE) -C examples/palette test
clean:
	rm -rf $(BUILD)
