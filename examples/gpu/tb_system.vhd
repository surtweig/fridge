library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeRAMBootImage.all;

entity tb_system is
end tb_system;

architecture sim of tb_system is
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

    signal write_count : integer := 0;
    signal vpre_count : integer := 0;
    signal vmode_count : integer := 0;
    signal vpre_seen : std_logic := '0';

    constant CPU_PERIOD : time := 100 ns;
    constant PIXEL_PERIOD : time := 13468 ps;

    function bar_byte(addr : integer) return integer is
    begin
        return (addr / 1200) * 17;
    end function;
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
            DEVICE_DATA => device_data,
            RAM_WRITE_DATA => ram_write_data,
            RAM_WRITE_ADDR => ram_write_addr,
            RAM_WRITE_ENABLED => ram_write_enabled,
            RAM_READ_DATA => ram_read_data,
            RAM_READ_ADDR => ram_read_addr,
            GPU_MODE_SWITCH => gpu_mode_switch,
            GPU_PALETTE_SWITCH => gpu_palette_switch,
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
            FRAME_STORE => gpu_back_store,
            FRAME_ADDR => gpu_back_addr,
            FRAME_DATA => gpu_back_data,
            PRESENT_TRIGGER => gpu_present_trigger,
            PRESENT_MODE => gpu_present_mode,
            FRAME_OFFSET => gpu_frame_offset,
            MODE_SWITCH => gpu_mode_switch,
            RED => red,
            GREEN => green,
            BLUE => blue,
            HSYNC => hs,
            VSYNC => vs,
            ACTIVE => act,
            PIXEL_X => px,
            PIXEL_Y => py);

    -- CPU-side bus monitor: VFSA write stream, VPRE and VMODE traffic.
    monitor : process (cpu_clk)
    begin
        if rising_edge(cpu_clk) then
            if reset = '0' then
                if gpu_back_store = '1' then
                    assert to_integer(gpu_back_addr) = write_count
                        report "VFSA write out of order at " &
                               integer'image(to_integer(gpu_back_addr)) severity failure;
                    assert to_integer(gpu_back_data) = bar_byte(write_count)
                        report "VFSA write data mismatch at " &
                               integer'image(write_count) severity failure;
                    write_count <= write_count + 1;
                end if;
                if gpu_present_trigger = '1' then
                    assert to_integer(gpu_present_mode) = 1
                        report "VPRE mode mismatch" severity failure;
                    assert gpu_frame_offset = X"0000"
                        report "VPRE offset mismatch" severity failure;
                    vpre_count <= vpre_count + 1;
                    vpre_seen <= '1';
                end if;
                if gpu_mode_switch(0) = '1' then
                    assert gpu_mode_switch(1) = '0'
                        report "VMODE did not select EGA" severity failure;
                    vmode_count <= vmode_count + 1;
                end if;
            end if;
        end if;
    end process;

    -- Display monitor: bar 15 spot must be black until the present has been
    -- requested, then white; white before the request is a contract violation.
    spot : process (pixel_clk)
    begin
        if falling_edge(pixel_clk) then
            if px = 160 and py = 676 then
                assert not (red = X"FF" and green = X"FF" and blue = X"FF")
                       or vpre_seen = '1'
                    report "display showed the new frame before VPRE" severity failure;
            end if;
        end if;
    end process;

    stimulus : process
        procedure check_rgb(ex, ey : integer;
                            er, eg, eb : std_logic_vector(7 downto 0)) is
        begin
            loop
                wait until falling_edge(pixel_clk);
                exit when px = ex and py = ey;
            end loop;
            assert red = er and green = eg and blue = eb
                report "pixel mismatch at " & integer'image(ex) & "," &
                       integer'image(ey) severity failure;
        end procedure;
    begin
        reset <= '1';
        wait for 10 * CPU_PERIOD;
        wait until falling_edge(cpu_clk);
        reset <= '0';

        wait until cpu_halted = '1' for 100 ms;
        assert cpu_halted = '1'
            report "CPU did not halt" severity failure;
        assert debug_pc = X"0029"
            report "unexpected PC at halt, got " &
                   integer'image(to_integer(debug_pc)) severity failure;
        report "PASS: CPU halted at 0x0029" severity note;

        assert write_count = 19200
            report "expected 19200 VFSA writes, got " &
                   integer'image(write_count) severity failure;
        report "PASS: 19200 VFSA writes with expected addresses and data" severity note;

        assert vmode_count >= 1
            report "no VMODE seen" severity failure;
        assert vpre_count = 1
            report "expected exactly one VPRE, got " &
                   integer'image(vpre_count) severity failure;
        report "PASS: VMODE and VPRE traffic" severity note;

        check_rgb(10, 10, X"00", X"00", X"FF");
        check_rgb(160, 40, X"00", X"00", X"00");
        check_rgb(200, 80, X"00", X"00", X"80");
        check_rgb(1120, 100, X"00", X"00", X"FF");
        check_rgb(200, 440, X"40", X"FF", X"40");
        check_rgb(200, 636, X"FF", X"FF", X"40");
        check_rgb(200, 640, X"FF", X"FF", X"FF");
        check_rgb(1112, 676, X"FF", X"FF", X"FF");
        check_rgb(1119, 679, X"FF", X"FF", X"FF");
        check_rgb(160, 680, X"00", X"00", X"FF");
        report "PASS: color bars on display after present (margins blue)" severity note;

        report "PASS: CPU/GPU/HDMI integration" severity note;
        finished <= true;
        wait;
    end process;
end sim;
