-- Shared-RTL regression, adapted from examples/palette/tb_access.vhd.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeRAMBootImage.all;
use work.FridgeIRCodes.all;

entity tb_access is
end tb_access;

architecture sim of tb_access is
    signal cv, cr : std_logic;
    signal cc, ca, cb, ccc, c0, c1, result : XCM2_WORD;
    signal chl, cbc : XCM2_DWORD;
    signal cpu_clk : std_logic := '0';
    signal pixel_clk : std_logic := '0';
    signal finished : boolean := false;
    signal reset : std_logic := '1';

    signal cpu_halted, cpu_inte, cpu_int : std_logic := '0';
    signal cpu_int_irq : XCM2_WORD := (others => '0');
    signal device_sel, device_data : XCM2_WORD;
    signal device_read : std_logic;

    signal ram_write_data, ram_read_data : XCM2_WORD;
    signal ram_write_addr, ram_read_addr : XCM2_DWORD;
    signal ram_write_enabled : std_logic;

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

    signal pam16_cmd_enabled : std_logic;
    signal pam16_cmd_ready : std_logic := '1';
    signal pam16_data_write, pam16_data_read : XCM2_DWORD := (others => '0');
    signal pam16_cmd_code : PAM16_COMMAND;

    signal debug_state, debug_ir : XCM2_WORD;
    signal debug_pc : XCM2_DWORD;

    signal red, green, blue : std_logic_vector(7 downto 0);
    signal hs, vs, act : std_logic;
    signal px : integer range 0 to 1649;
    signal py : integer range 0 to 749;


    constant CPU_PERIOD : time := 31 ns;
    constant PIXEL_PERIOD : time := 230 ns;

    signal snapshots : integer := 0;
    signal pal_count : integer := 0;
    signal pal_last : std_logic := '0';
begin
    cpu_clk <= not cpu_clk after CPU_PERIOD / 2 when not finished else '0';
    pixel_clk <= not pixel_clk after PIXEL_PERIOD / 2 when not finished else '0';

    cpu : entity work.FridgeCPU
        port map (
            CLK_MAIN => cpu_clk,
            CLK_PHI2 => cpu_clk,
            RESET => reset,
            DEBUG_SWITCH => '0',
            HALTED => cpu_halted,
            INTE => cpu_inte,
            INT => cpu_int,
            INT_IRQ => cpu_int_irq,
            DEVICE_SEL => device_sel,
            DEVICE_READ => device_read,
            DEVICE_DATA_IN => device_data,
            DEVICE_DATA_OUT => open,
            DEVICE_WRITE => open,
            RAM_WRITE_DATA => ram_write_data,
            RAM_WRITE_ADDR => ram_write_addr,
            RAM_WRITE_ENABLED => ram_write_enabled,
            RAM_READ_DATA => ram_read_data,
            RAM_READ_ADDR => ram_read_addr,
            GPU_COMMAND_VALID => cv, GPU_COMMAND_READY => cr,
            GPU_COMMAND_CODE => cc, GPU_COMMAND_A => ca, GPU_COMMAND_B => cb, GPU_COMMAND_C => ccc,
            GPU_COMMAND_HL => chl, GPU_COMMAND_BC => cbc, GPU_COMMAND_ARG0 => c0,
            GPU_COMMAND_ARG1 => c1, GPU_COMMAND_RESULT => result,
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

    ram : entity work.FridgeRAM
        generic map (INIT_DATA => RAMBootImage)
        port map (
            CLK => cpu_clk,
            WRITE_DATA => ram_write_data,
            WRITE_ADDR => ram_write_addr,
            WRITE_ENABLED => ram_write_enabled,
            READ_DATA => ram_read_data,
            READ_ADDR => ram_read_addr);

    gpu : entity work.fridge_gpu
        port map (
            CLK => pixel_clk,
            COMMAND_CLK => cpu_clk,
            RESET => reset,
            COMMAND_RESET => reset,
            COMMAND_VALID => cv, COMMAND_READY => cr,
            COMMAND_CODE => cc, COMMAND_A => ca, COMMAND_B => cb, COMMAND_C => ccc,
            COMMAND_HL => chl, COMMAND_BC => cbc, COMMAND_ARG0 => c0,
            COMMAND_ARG1 => c1, COMMAND_RESULT => result,
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
            RED => red,
            GREEN => green,
            BLUE => blue,
            HSYNC => hs,
            VSYNC => vs,
            ACTIVE => act,
            PIXEL_X => px,
            PIXEL_Y => py);



    monitor : process(cpu_clk)
        type expected_t is array(0 to 311) of integer;
        constant expected : expected_t := (145,18,52,86,120,1,1,69,165,18,52,86,120,0,255,69,195,18,52,86,120,1,0,69,92,18,52,86,120,2,2,69,90,18,52,86,120,0,241,69,183,18,52,86,120,3,2,69,87,18,52,86,120,0,241,69,5,18,52,86,120,2,2,69,7,18,52,86,120,3,2,69,196,18,52,86,120,239,159,69,4,18,52,86,120,239,159,69,145,18,52,86,120,75,0,69,145,18,52,86,120,74,255,69,52,18,52,86,120,74,255,69,145,18,52,86,120,75,0,69,145,18,52,86,120,240,0,69,145,18,52,86,120,0,160,69,145,18,52,86,120,128,0,69,241,18,52,86,120,127,254,69,226,18,52,86,120,127,255,69,145,18,52,86,120,127,255,69,226,18,52,86,120,127,255,69,145,18,52,86,120,128,0,69,255,18,52,86,120,128,0,69,0,18,52,86,120,0,0,69,171,18,52,86,120,0,1,69,171,18,52,86,120,0,1,69,145,0,3,86,120,0,1,69,171,18,52,86,120,0,3,69,145,0,3,86,120,128,0,69,171,18,52,86,120,0,3,69,171,18,52,86,120,0,3,69,169,18,52,86,120,0,3,69,63,3,3,86,120,127,251,69,63,1,0,86,120,239,159,69,64,3,3,86,120,0,0,69,0,3,3,86,120,127,252,69,64,1,0,86,120,0,0,69,63,8,0,86,120,0,0,69);
        variable addr : integer;
    begin
        if rising_edge(cpu_clk) then
            if reset = '1' then snapshots <= 0;
            elsif ram_write_enabled = '1' then
                addr := to_integer(ram_write_addr);
                if addr >= 16#F000# and addr < 16#F008# then
                    assert snapshots < expected'length report "extra snapshot" severity failure;
                    assert addr = 16#F000#+snapshots mod 8 and to_integer(ram_write_data) = expected(snapshots)
                        report "GPU ISA/register/flag mismatch at snapshot " & integer'image(snapshots) &
                               " got " & integer'image(to_integer(ram_write_data)) & " expected " & integer'image(expected(snapshots)) severity failure;
                    snapshots <= snapshots+1;
                end if;
            end if;
        end if;
    end process;
    stimulus : process
    begin
        wait for 2 us; wait until falling_edge(cpu_clk); reset <= '0';
        wait until cpu_halted = '1' for 10 ms;
        assert cpu_halted = '1' and snapshots = 312 report "GPU ISA contract timeout/count mismatch" severity failure;
        assert cv = '0' report "CPU left a pending GPU operation" severity failure;
        -- Abort an active immediate store, then replay all ABI checks from reset.
        wait until falling_edge(cpu_clk); reset <= '1'; wait for 2 us;
        wait until falling_edge(cpu_clk); reset <= '0';
        wait until cv = '1' and cc = VFSI for 1 ms;
        wait until falling_edge(cpu_clk); reset <= '1'; wait for 2 us;
        wait until falling_edge(cpu_clk); reset <= '0';
        wait until cpu_halted = '1' for 10 ms;
        assert cpu_halted = '1' and snapshots = 312 report "GPU reset replay timeout/count mismatch" severity failure;
        report "PASS: advanced GPU CPU ABI" severity note;
        finished <= true; wait;
    end process;
end sim;
