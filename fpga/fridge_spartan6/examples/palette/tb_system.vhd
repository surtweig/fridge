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

    signal write_count : integer := 0;
    signal vpre_count : integer := 0;
    signal vmode_count : integer := 0;
    signal vmode_text : std_logic := '0';

    constant CPU_PERIOD : time := 100 ns;
    constant PIXEL_PERIOD : time := 13468 ps;

    type colors_t is array (0 to 15) of std_logic_vector(23 downto 0);
    constant COLORS : colors_t := (
        x"102030", x"203050", x"20A040", x"20A0A0",
        x"C02030", x"A030B0", x"C08020", x"B0B0B0",
        x"505050", x"4080FF", x"80FF40", x"40E0FF",
        x"FF6040", x"E040D0", x"FFD040", x"FF8040");
    constant TITLE : string := "VPAL SHARED PALETTE";
    constant INFO : string := "TEXT + EGA  16 RGB888 COLORS";
    constant TITLE_WRITES : integer := TITLE'length*2;
    constant INFO_WRITES : integer := INFO'length*2;
    constant TEXT_WRITES : integer := TITLE_WRITES + INFO_WRITES + 64;
    constant TOTAL_WRITES : integer := TEXT_WRITES + 19200;
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
            DEVICE_DATA => device_data,
            RAM_WRITE_DATA => ram_write_data,
            RAM_WRITE_ADDR => ram_write_addr,
            RAM_WRITE_ENABLED => ram_write_enabled,
            RAM_READ_DATA => ram_read_data,
            RAM_READ_ADDR => ram_read_addr,
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


    monitor : process (cpu_clk)
        variable ordinal, expected_addr, expected_data : integer;
        variable textpos : integer;
    begin
        if rising_edge(cpu_clk) and reset = '0' then
            pal_last <= gpu_palette_switch;
            if gpu_palette_switch = '1' and pal_last = '0' then
                assert gpu_palette_ready = '1' report "CPU VPAL issued while busy" severity failure;
                if pal_count < 16 then
                    assert to_integer(gpu_palette_index) = pal_count and gpu_palette_rgb = COLORS(pal_count)
                        report "VPAL setup index/RGB mismatch" severity failure;
                else
                    assert gpu_palette_index = 15 and
                           gpu_palette_rgb = std_logic_vector(to_unsigned((255+(pal_count-15)*32) mod 256,8)) & x"8040"
                        report "VPAL animation index/RGB mismatch" severity failure;
                end if;
                pal_count <= pal_count + 1;
            end if;
            if gpu_back_store = '1' then
                ordinal := write_count;
                if ordinal < TITLE_WRITES then
                    expected_addr := (2*40+3)*2 + ordinal;
                    textpos := ordinal/2 + 1;
                    if ordinal mod 2 = 1 then expected_data := 16#1F#;
                    elsif textpos <= TITLE'length then expected_data := character'pos(TITLE(textpos));
                    else expected_data := 0; end if;
                elsif ordinal < TITLE_WRITES + INFO_WRITES then
                    ordinal := ordinal - TITLE_WRITES;
                    expected_addr := (4*40+3)*2 + ordinal;
                    textpos := ordinal/2 + 1;
                    if ordinal mod 2 = 1 then expected_data := 16#1F#;
                    elsif textpos <= INFO'length then expected_data := character'pos(INFO(textpos));
                    else expected_data := 0; end if;
                elsif ordinal < TEXT_WRITES then
                    ordinal := ordinal - TITLE_WRITES - INFO_WRITES;
                    expected_addr := 644 + ordinal;
                    if ordinal mod 2 = 0 then expected_data := 16#41#;
                    else expected_data := (ordinal/4)*16+15; end if;
                else
                    expected_addr := ordinal - TEXT_WRITES;
                    expected_data := (expected_addr/1200)*17;
                end if;
                assert write_count < TOTAL_WRITES and to_integer(gpu_back_addr) = expected_addr and
                       to_integer(gpu_back_data) = expected_data
                    report "demo VFSA mismatch at write " & integer'image(write_count) &
                           ": got " & integer'image(to_integer(gpu_back_addr)) & "/" & integer'image(to_integer(gpu_back_data)) &
                           ", expected " & integer'image(expected_addr) & "/" & integer'image(expected_data) severity failure;
                write_count <= write_count + 1;
            end if;
            if gpu_present_trigger = '1' then
                assert gpu_frame_offset = X"0000" report "demo VPRE offset mismatch" severity failure;
                if vpre_count = 0 or vpre_count = 2 then
                    assert gpu_present_mode = 2 severity failure;
                else
                    assert gpu_present_mode = 5 severity failure;
                end if;
                vpre_count <= vpre_count + 1;
            end if;
            if gpu_mode_switch(0) = '1' then
                if vmode_count mod 2 = 0 then
                    assert gpu_mode_switch(1) = '1' severity failure;
                else
                    assert gpu_mode_switch(1) = '0' severity failure;
                end if;
                vmode_text <= gpu_mode_switch(1);
                vmode_count <= vmode_count + 1;
            end if;
        end if;
    end process;

    watchdog : process
    begin
        wait for 500 ms;
        assert finished report "palette demo integration timed out" severity failure;
        wait;
    end process;

    stimulus : process
        procedure next_frame is
        begin
            loop
                wait until falling_edge(pixel_clk);
                exit when px = 0 and py = 0;
            end loop;
        end procedure;
        procedure check_rgb(ex, ey : integer; color : std_logic_vector(23 downto 0)) is
        begin
            loop
                wait until falling_edge(pixel_clk);
                exit when px = ex and py = ey;
            end loop;
            assert red & green & blue = color
                report "demo pixel mismatch at " & integer'image(ex) & "," & integer'image(ey) severity failure;
        end procedure;
    begin
        wait for 10 * CPU_PERIOD;
        wait until falling_edge(cpu_clk);
        reset <= '0';
        wait until vpre_count = 2 for 100 ms;
        assert vpre_count = 2 and pal_count = 16 and write_count = TOTAL_WRITES
            report "demo initialization failed" severity failure;
        next_frame;
        check_rgb(10,10,x"0000FF");
        -- Swatch 'A' at cell (2,8): bg palette 0, fg palette 15.
        check_rgb(208,296,COLORS(0));
        check_rgb(212,296,COLORS(15));
        check_rgb(256,296,COLORS(1));
        check_rgb(928,296,COLORS(15));
        report "PASS: real demo image writes both framebuffers byte-exactly; custom TEXT palette on display" severity note;

        wait until vpre_count = 3 for 200 ms;
        assert vpre_count = 3 and pal_count = 17 and vmode_text = '0'
            report "demo did not animate then select EGA" severity failure;
        next_frame;
        for i in 0 to 14 loop
            check_rgb(160,40+i*40,COLORS(i));
        end loop;
        check_rgb(160,640,x"1F8040");
        check_rgb(1120,640,x"0000FF");
        report "PASS: animated palette entry 15 and all 16 EGA bars share the TEXT palette" severity note;
        assert write_count = TOTAL_WRITES and cpu_halted = '0'
            report "animation rewrote the framebuffer or halted" severity failure;
        report "PASS: CPU/GPU/HDMI palette integration" severity note;
        finished <= true;
        wait;
    end process;
end sim;
