-- CPU + RAM + GPU + ROM integration: runs the real boot image (ROM demo),
-- verifies the IOUT-driven ROM protocol traffic (device reset, LOAD mode +
-- segment selects), the 1024-byte VFSA stream of ROM contents in order,
-- the VMODE/VPRE traffic, the halt, and the on-screen pixel colors of the
-- painted ROM image (including the untouched black area and blue margins).

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeRAMBootImage.all;
use work.FridgeROMImage.all;

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
    signal device_read, device_write : std_logic;

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

    signal rom_error : std_logic;

    signal write_count : integer := 0;
    signal vpre_count : integer := 0;
    signal vmode_count : integer := 0;
    signal rom_writes : integer := 0;
    signal rom_reads : integer := 0;
    signal rom_resets : integer := 0;
    signal frame_done : boolean := false;

    constant CPU_PERIOD : time := 100 ns;
    constant PIXEL_PERIOD : time := 13468 ps;
    constant FRAME_BYTES : integer := 1024;

    -- Independent re-implementation of the FridgeROMImage demo patterns.
    function rom_byte(a : integer) return XCM2_WORD is
        variable seg, i : integer;
    begin
        seg := a / 256;
        i := a mod 256;
        case seg is
            when 0 => return to_unsigned(i, 8);
            when 1 => return to_unsigned(255 - i, 8);
            when 2 =>
                if i mod 2 = 0 then
                    return X"AA";
                else
                    return X"55";
                end if;
            when others => return X"42";
        end case;
    end function;

    -- Expected OUT sequence on the ROM data device (device 1): three
    -- writes per segment (LOAD, hi, lo) for segments 0..3 in order.
    function rom_write_val(n : integer) return XCM2_WORD is
    begin
        case n mod 3 is
            when 0 => return X"02";                 -- LOAD
            when 1 => return X"00";                 -- segment hi
            when others => return to_unsigned(n / 3, 8);  -- segment lo
        end case;
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
            DEVICE_WRITE => device_write,
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

    romdev : entity work.fridge_rom
        generic map (IMAGE => ROM_IMAGE)
        port map (
            CLK => cpu_clk,
            RESET => reset,
            DEVICE_SEL => device_sel,
            DEVICE_READ => device_read,
            DEVICE_WRITE => device_write,
            DEVICE_DATA => device_data,
            ERROR => rom_error);

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

    -- CPU-side bus monitor: ROM protocol traffic (IOUT/IIN), the VFSA
    -- stream of ROM contents, and VMODE/VPRE traffic.
    monitor : process (cpu_clk)
    begin
        if rising_edge(cpu_clk) then
            if reset = '0' then
                if gpu_back_store = '1' then
                    assert to_integer(gpu_back_addr) = write_count
                        report "frame write out of order at " &
                               integer'image(to_integer(gpu_back_addr))
                        severity failure;
                    assert gpu_back_data = rom_byte(write_count)
                        report "frame write data mismatch at " &
                               integer'image(write_count)
                        severity failure;
                    write_count <= write_count + 1;
                end if;
                if gpu_present_trigger = '1' then
                    assert to_integer(gpu_present_mode) = 5
                        report "VPRE mode mismatch" severity failure;
                    assert gpu_frame_offset = X"0000"
                        report "VPRE offset mismatch" severity failure;
                    vpre_count <= vpre_count + 1;
                end if;
                if gpu_mode_switch(0) = '1' then
                    if vmode_count = 0 then
                        assert gpu_mode_switch(1) = '1'
                            report "first VMODE did not select TEXT (A bit 0)"
                            severity failure;
                    else
                        assert gpu_mode_switch(1) = '0'
                            report "second VMODE did not select EGA"
                            severity failure;
                    end if;
                    vmode_count <= vmode_count + 1;
                end if;
                if device_write = '1' and to_integer(device_sel) = 2 then
                    assert device_data = X"01"
                        report "ROM reset command must carry value 1"
                        severity failure;
                    rom_resets <= rom_resets + 1;
                end if;
                if device_write = '1' and to_integer(device_sel) = 1 then
                    assert device_data = rom_write_val(rom_writes)
                        report "ROM protocol write mismatch at " &
                               integer'image(rom_writes)
                        severity failure;
                    rom_writes <= rom_writes + 1;
                end if;
                if device_read = '1' and to_integer(device_sel) = 1 then
                    rom_reads <= rom_reads + 1;
                end if;
                assert rom_error = '0'
                    report "unexpected ROM ERROR flag" severity failure;
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

        wait until cpu_halted = '1' for 200 ms;
        assert cpu_halted = '1'
            report "CPU did not halt: " & integer'image(write_count)
            severity failure;
        assert write_count = FRAME_BYTES
            report "expected " & integer'image(FRAME_BYTES) &
                   " frame writes, got " & integer'image(write_count)
            severity failure;
        assert rom_writes = 12
            report "expected 12 ROM protocol writes, got " &
                   integer'image(rom_writes) severity failure;
        assert rom_resets = 1
            report "expected 1 ROM reset command, got " &
                   integer'image(rom_resets) severity failure;
        assert rom_reads = FRAME_BYTES
            report "expected " & integer'image(FRAME_BYTES) &
                   " ROM stream reads, got " & integer'image(rom_reads)
            severity failure;
        report "PASS: 1024 ROM stream writes verified (IOUT path included)"
            severity note;

        assert vmode_count = 2
            report "expected exactly two VMODE, got " & integer'image(vmode_count)
            severity failure;
        assert vpre_count = 1
            report "expected exactly one VPRE, got " & integer'image(vpre_count)
            severity failure;
        assert rom_error = '0'
            report "unexpected ROM ERROR flag" severity failure;
        report "PASS: VMODE TEXT->EGA (A bit 0) and VPRE MANUAL_11 traffic" severity note;

        -- Two vblanks so the presented frame (frame 1) is on screen.
        wait until vs = '1' for 100 ms;
        wait until vs = '0' for 100 ms;
        wait until vs = '1' for 100 ms;
        wait until vs = '0' for 100 ms;

        -- Segment 0 ramp 00 01 .. FF at frame bytes 0..255.
        check_rgb(160, 40, X"00", X"00", X"00");   -- 0x00 left  = black
        check_rgb(164, 40, X"00", X"00", X"00");   -- 0x00 right = black
        check_rgb(168, 40, X"00", X"00", X"00");   -- 0x01 left  = black
        check_rgb(172, 40, X"00", X"00", X"80");   -- 0x01 right = blue
        check_rgb(280, 48, X"FF", X"FF", X"FF");   -- 0xFF left  = white
        check_rgb(284, 48, X"FF", X"FF", X"FF");   -- 0xFF right = white
        -- Segment 1 ramp FF .. 00 at frame bytes 256..511.
        check_rgb(288, 48, X"FF", X"FF", X"FF");   -- 0xFF left  = white
        check_rgb(292, 48, X"FF", X"FF", X"FF");   -- 0xFF right = white
        check_rgb(408, 56, X"00", X"00", X"00");   -- 0x00 left  = black
        check_rgb(412, 56, X"00", X"00", X"00");   -- 0x00 right = black
        -- Segment 2 AA 55 .. at frame bytes 512..767.
        check_rgb(416, 56, X"40", X"FF", X"40");   -- 0xAA left  = bright green
        check_rgb(420, 56, X"40", X"FF", X"40");   -- 0xAA right = bright green
        check_rgb(424, 56, X"80", X"00", X"80");   -- 0x55 left  = magenta
        check_rgb(428, 56, X"80", X"00", X"80");   -- 0x55 right = magenta
        -- Segment 3 fill 42 at frame bytes 768..1023.
        check_rgb(544, 64, X"80", X"00", X"00");   -- 0x42 left  = red
        check_rgb(548, 64, X"00", X"80", X"00");   -- 0x42 right = green
        check_rgb(664, 72, X"80", X"00", X"00");   -- 0x42 left  = red
        check_rgb(668, 72, X"00", X"80", X"00");   -- 0x42 right = green
        -- Untouched frame stays black; margins stay blue.
        check_rgb(672, 72, X"00", X"00", X"00");
        check_rgb(1120, 40, X"00", X"00", X"FF");
        report "PASS: ROM image pixels on display" severity note;

        report "PASS: CPU/ROM/HDMI integration" severity note;
        finished <= true;
        wait;
    end process;

    done_watch : process (cpu_clk)
    begin
        if rising_edge(cpu_clk) then
            if write_count >= FRAME_BYTES then
                frame_done <= true;
            end if;
        end if;
    end process;
end sim;
