-- Canonical computer. Clocks and HDMI pins belong to atlys_top.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeROMImage.all;
use work.FridgeSystemDebug.all;

entity fridge_system is
    generic (BOOT_IMAGE : XCM2_RAM; ROM_INIT : ROM_IMAGE_T);
    port (
        cpu_clk, pixel_clk, cpu_reset, pixel_reset : in std_logic;
        ps2_clk, ps2_dat : in std_logic;
        RED, GREEN, BLUE : out std_logic_vector(7 downto 0);
        HSYNC, VSYNC, ACTIVE : out std_logic;
        PIXEL_X : out integer range 0 to 1649;
        PIXEL_Y : out integer range 0 to 749;
        HALTED, ROM_ERROR, KBD_OVERFLOW, KBD_ERROR, KBD_ACTIVITY : out std_logic;
        DEBUG : out system_debug_t
    );
end fridge_system;

architecture rtl of fridge_system is
    signal cpu_halted, cpu_inte : std_logic;
    signal cpu_int : std_logic := '0';
    signal cpu_int_irq : XCM2_WORD := (others => '0');
    signal device_sel, device_in, device_out, rom_data, keyboard_data : XCM2_WORD;
    signal device_read, device_write : std_logic;

    signal ram_write_data, ram_read_data : XCM2_WORD;
    signal ram_write_addr, ram_read_addr : XCM2_DWORD;
    signal ram_write_enabled : std_logic;

    signal gpu_command_valid, gpu_command_ready : std_logic;
    signal gpu_command_code, gpu_command_a, gpu_command_b, gpu_command_c : XCM2_WORD;
    signal gpu_command_arg0, gpu_command_arg1, gpu_command_result : XCM2_WORD;
    signal gpu_command_hl, gpu_command_bc : XCM2_DWORD;
    signal gpu_mode_switch : std_logic_vector(0 to 1);
    signal gpu_palette_switch, gpu_present_trigger : std_logic;
    signal gpu_palette_index : XCM2_WORD;
    signal gpu_palette_rgb : std_logic_vector(23 downto 0);
    signal gpu_palette_ready : std_logic;
    signal gpu_present_mode : XCM2_WORD;
    signal gpu_frame_offset : XCM2_DWORD;
    signal gpu_back_store, gpu_back_load, gpu_back_clr : std_logic;
    signal gpu_back_addr, gpu_vmem_addr : XCM2_DWORD;
    signal gpu_back_data, gpu_vmem_data : XCM2_WORD;
    signal gpu_vmem_store, gpu_vmem_load : std_logic;

    signal pam16_cmd_enabled, pam16_cmd_ready : std_logic;
    signal pam16_data_write, pam16_data_read : XCM2_DWORD := (others => '0');
    signal pam16_cmd_code : PAM16_COMMAND;

    signal debug_state, debug_ir : XCM2_WORD;
    signal debug_pc : XCM2_DWORD;

    signal rom_error_i, kbd_overflow_i, kbd_error_i, kbd_activity_i : std_logic;
begin
    cpu : entity work.FridgeCPU
        port map (
            CLK_MAIN => cpu_clk,
            CLK_PHI2 => cpu_clk,
            RESET => cpu_reset,
            DEBUG_SWITCH => '0',
            HALTED => cpu_halted,
            INTE => cpu_inte,
            INT => cpu_int,
            INT_IRQ => cpu_int_irq,
            DEVICE_SEL => device_sel,
            DEVICE_READ => device_read,
            DEVICE_WRITE => device_write,
            DEVICE_DATA_IN => device_in,
            DEVICE_DATA_OUT => device_out,
            RAM_WRITE_DATA => ram_write_data,
            RAM_WRITE_ADDR => ram_write_addr,
            RAM_WRITE_ENABLED => ram_write_enabled,
            RAM_READ_DATA => ram_read_data,
            RAM_READ_ADDR => ram_read_addr,
            GPU_COMMAND_VALID => gpu_command_valid, GPU_COMMAND_READY => gpu_command_ready,
            GPU_COMMAND_CODE => gpu_command_code, GPU_COMMAND_A => gpu_command_a,
            GPU_COMMAND_B => gpu_command_b, GPU_COMMAND_C => gpu_command_c,
            GPU_COMMAND_HL => gpu_command_hl, GPU_COMMAND_BC => gpu_command_bc,
            GPU_COMMAND_ARG0 => gpu_command_arg0, GPU_COMMAND_ARG1 => gpu_command_arg1,
            GPU_COMMAND_RESULT => gpu_command_result,
            GPU_MODE_SWITCH => gpu_mode_switch,
            GPU_PALETTE_SWITCH => gpu_palette_switch,
            GPU_PALETTE_INDEX => gpu_palette_index,
            GPU_PALETTE_RGB => gpu_palette_rgb,
            GPU_PALETTE_READY => gpu_palette_ready,
            GPU_PRESENT_TRIGGER => gpu_present_trigger,
            GPU_PRESENT_MODE => gpu_present_mode,
            GPU_FRAME_OFFSET => gpu_frame_offset,
            GPU_BACK_STORE => gpu_back_store,
            GPU_BACK_LOAD => gpu_back_load,
            GPU_BACK_ADDR => gpu_back_addr,
            GPU_BACK_DATA => gpu_back_data,
            GPU_BACK_CLR => gpu_back_clr,
            GPU_VMEM_STORE => gpu_vmem_store,
            GPU_VMEM_LOAD => gpu_vmem_load,
            GPU_VMEM_ADDR => gpu_vmem_addr,
            GPU_VMEM_DATA => gpu_vmem_data,
            PAM16_COMMAND_ENABLED => pam16_cmd_enabled,
            PAM16_COMMAND_READY => pam16_cmd_ready,
            PAM16_DATA_WRITE => pam16_data_write,
            PAM16_DATA_READ => pam16_data_read,
            PAM16_COMMAND_CODE => pam16_cmd_code,
            DEBUG_STEP => '0',
            DEBUG_STATE => debug_state,
            DEBUG_CURRENT_IR => debug_ir,
            DEBUG_PC => debug_pc);

    pam16_cmd_ready <= '1';

    ram : entity work.FridgeRAM
        generic map (INIT_DATA => BOOT_IMAGE)
        port map (
            CLK => cpu_clk,
            WRITE_DATA => ram_write_data,
            WRITE_ADDR => ram_write_addr,
            WRITE_ENABLED => ram_write_enabled,
            READ_DATA => ram_read_data,
            READ_ADDR => ram_read_addr);

    video : entity work.fridge_gpu
        port map (
            CLK => pixel_clk,
            COMMAND_CLK => cpu_clk,
            RESET => pixel_reset,
            COMMAND_RESET => cpu_reset,
            COMMAND_VALID => gpu_command_valid, COMMAND_READY => gpu_command_ready,
            COMMAND_CODE => gpu_command_code, COMMAND_A => gpu_command_a,
            COMMAND_B => gpu_command_b, COMMAND_C => gpu_command_c,
            COMMAND_HL => gpu_command_hl, COMMAND_BC => gpu_command_bc,
            COMMAND_ARG0 => gpu_command_arg0, COMMAND_ARG1 => gpu_command_arg1,
            COMMAND_RESULT => gpu_command_result,
            FRAME_STORE => gpu_back_store,
            FRAME_ADDR => gpu_back_addr,
            FRAME_DATA => gpu_back_data,
            PRESENT_TRIGGER => gpu_present_trigger,
            PRESENT_MODE => gpu_present_mode,
            FRAME_OFFSET => gpu_frame_offset,
            MODE_SWITCH => gpu_mode_switch,
            PALETTE_WRITE => gpu_palette_switch,
            PALETTE_INDEX => gpu_palette_index,
            PALETTE_RGB => gpu_palette_rgb,
            PALETTE_READY => gpu_palette_ready,
            RED => RED,
            GREEN => GREEN,
            BLUE => BLUE,
            HSYNC => HSYNC,
            VSYNC => VSYNC,
            ACTIVE => ACTIVE,
            PIXEL_X => PIXEL_X,
            PIXEL_Y => PIXEL_Y);


    -- Firmware device map. Unmapped reads return zero; writes are ignored.
    -- There are no internal tri-state buses or multiple data drivers.
    device_in <= rom_data when device_sel = 1 else
                 keyboard_data when device_sel = 3 else X"00";
    rom : entity work.fridge_rom
        generic map (IMAGE => ROM_INIT)
        port map (CLK => cpu_clk, RESET => cpu_reset,
                  DEVICE_SEL => device_sel, DEVICE_READ => device_read,
                  DEVICE_WRITE => device_write, DEVICE_DATA_IN => device_out,
                  DEVICE_DATA_OUT => rom_data, ERROR => rom_error_i);
    keyboard : entity work.fridge_keyboard
        port map (CLK => cpu_clk, RESET => cpu_reset,
                  PS2_CLK => ps2_clk, PS2_DAT => ps2_dat,
                  DEVICE_SEL => device_sel, DEVICE_READ => device_read,
                  DEVICE_DATA => keyboard_data, OVERFLOW => kbd_overflow_i,
                  RX_ERROR => kbd_error_i, RX_ACTIVITY => kbd_activity_i);

    HALTED <= cpu_halted;
    ROM_ERROR <= rom_error_i;
    KBD_OVERFLOW <= kbd_overflow_i;
    KBD_ERROR <= kbd_error_i;
    KBD_ACTIVITY <= kbd_activity_i;
    DEBUG <= (pc => debug_pc, device_sel => device_sel,
              device_in => device_in, device_out => device_out,
              device_read => device_read, device_write => device_write,
              frame_store => gpu_back_store, frame_addr => gpu_back_addr,
              frame_data => gpu_back_data, mode_switch => gpu_mode_switch,
              present => gpu_present_trigger, present_mode => gpu_present_mode,
              frame_offset => gpu_frame_offset, palette_write => gpu_palette_switch,
              palette_ready => gpu_palette_ready, palette_index => gpu_palette_index,
              palette_rgb => gpu_palette_rgb);
end rtl;
