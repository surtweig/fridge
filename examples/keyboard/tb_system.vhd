-- CPU + RAM + GPU + keyboard integration: runs the real boot image (event
-- tape demo), types real PS/2 scan-code-set-2 traffic into the bridge pins,
-- and verifies the VFSA stream (19200 fill bytes, then the FRIDGE_KEYBOARD_*
-- event bytes in arrival order), the VMODE/VPRE traffic, the IIN-driven
-- tape updates and their on-screen pixel colors.

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
    signal ps2_clk : std_logic := '1';
    signal ps2_dat : std_logic := '1';

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

    signal kbd_overflow, kbd_rx_error, kbd_rx_activity : std_logic;

    signal write_count : integer := 0;
    signal vpre_count : integer := 0;
    signal vmode_count : integer := 0;
    signal read_count : integer := 0;
    signal tape_done : boolean := false;

    constant CPU_PERIOD : time := 100 ns;
    constant PIXEL_PERIOD : time := 13468 ps;
    constant TAPE_EXPECTED : integer := 6;

    -- Expected FRIDGE_KEYBOARD_* event bytes for the typing script below:
    -- press a, release a, press B (shift), release B, press 1, press Enter.
    function tape_byte(i : integer) return XCM2_WORD is
    begin
        case i is
            when 0 => return X"E1";
            when 1 => return X"61";
            when 2 => return X"C2";
            when 3 => return X"42";
            when 4 => return X"B1";
            when 5 => return X"8A";
            when others => return X"00";
        end case;
    end function;

    procedure send_bit(signal ps2_clk : out std_logic;
                       signal ps2_dat : out std_logic;
                       d : std_logic; t : time) is
    begin
        ps2_dat <= d;
        wait for t / 4;
        ps2_clk <= '0';
        wait for t / 2;
        ps2_clk <= '1';
        wait for t / 4;
    end procedure;

    procedure send_byte(signal ps2_clk : out std_logic;
                        signal ps2_dat : out std_logic;
                        b : std_logic_vector(7 downto 0); t : time) is
        variable par : std_logic;
    begin
        par := '1';
        for i in 0 to 7 loop
            par := par xor b(i);
        end loop;
        send_bit(ps2_clk, ps2_dat, '0', t);
        for i in 0 to 7 loop
            send_bit(ps2_clk, ps2_dat, b(i), t);
        end loop;
        send_bit(ps2_clk, ps2_dat, par, t);
        send_bit(ps2_clk, ps2_dat, '1', t);
        ps2_dat <= '1';
    end procedure;
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

    kbd : entity work.fridge_keyboard
        port map (
            CLK => cpu_clk,
            RESET => reset,
            PS2_CLK => ps2_clk,
            PS2_DAT => ps2_dat,
            DEVICE_SEL => device_sel,
            DEVICE_READ => device_read,
            DEVICE_DATA => device_data,
            OVERFLOW => kbd_overflow,
            RX_ERROR => kbd_rx_error,
            RX_ACTIVITY => kbd_rx_activity);

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

    -- CPU-side bus monitor: fill + tape VFSA stream, VMODE/VPRE traffic,
    -- and IIN reads of the keyboard device.
    monitor : process (cpu_clk)
    begin
        if rising_edge(cpu_clk) then
            if reset = '0' then
                if gpu_back_store = '1' then
                    if write_count < 19200 then
                        assert to_integer(gpu_back_addr) = write_count
                            report "fill write out of order at " &
                                   integer'image(to_integer(gpu_back_addr))
                            severity failure;
                        assert gpu_back_data = X"00"
                            report "fill write data mismatch at " &
                                   integer'image(write_count)
                            severity failure;
                    else
                        assert to_integer(gpu_back_addr) = write_count - 19200
                            report "tape write out of order at " &
                                   integer'image(to_integer(gpu_back_addr))
                            severity failure;
                        assert gpu_back_data = tape_byte(write_count - 19200)
                            report "tape write data mismatch at " &
                                   integer'image(write_count - 19200)
                            severity failure;
                    end if;
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
                if device_read = '1' and to_integer(device_sel) = 3 then
                    read_count <= read_count + 1;
                end if;
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

        procedure send(x : std_logic_vector(7 downto 0)) is
        begin
            send_byte(ps2_clk, ps2_dat, x, 4 us);
            wait for 2 us;
        end procedure;
    begin
        reset <= '1';
        wait for 10 * CPU_PERIOD;
        wait until falling_edge(cpu_clk);
        reset <= '0';

        -- Type: 'a' down/up, Shift+'b' down/up, '1' down, Up arrow
        -- (dropped), Enter down. 6 events must reach the tape in order.
        send(X"1C");
        send(X"F0");
        send(X"1C");
        send(X"12");
        send(X"32");
        send(X"F0");
        send(X"32");
        send(X"F0");
        send(X"12");
        send(X"16");
        send(X"E0");
        send(X"75");
        send(X"E0");
        send(X"F0");
        send(X"75");
        send(X"5A");

        wait until tape_done for 200 ms;
        assert tape_done
            report "tape writes did not complete: " & integer'image(write_count)
            severity failure;
        assert write_count = 19200 + TAPE_EXPECTED
            report "expected " & integer'image(19200 + TAPE_EXPECTED) &
                   " VFSA writes, got " & integer'image(write_count)
            severity failure;
        report "PASS: 19200 fill writes and 6 tape writes verified" severity note;

        assert vmode_count = 2
            report "expected exactly two VMODE, got " & integer'image(vmode_count)
            severity failure;
        assert vpre_count = 1
            report "expected exactly one VPRE, got " & integer'image(vpre_count)
            severity failure;
        report "PASS: VMODE TEXT->EGA (A bit 0) and VPRE MANUAL_11 traffic" severity note;

        -- Let the poll loop spin through many empty IIN reads before
        -- sampling the read count (tape_done fires within a few cycles of
        -- the last event).
        wait for 1 ms;
        assert read_count > 500
            report "CPU did not keep polling the keyboard device: " &
                   integer'image(read_count) severity failure;
        assert kbd_overflow = '0'
            report "unexpected FIFO overflow" severity failure;
        assert kbd_rx_error = '0'
            report "unexpected PS/2 receive error" severity failure;
        report "PASS: IIN polling without overflow or receive errors" severity note;

        -- Two vblanks so the presented frame (frame 1) is on screen, then
        -- check tape pixel pairs: byte 0 = E1 (press a), byte 1 = 61
        -- (release a), byte 2 = C2 (press B).
        wait until vs = '1' for 100 ms;
        wait until vs = '0' for 100 ms;
        wait until vs = '1' for 100 ms;
        wait until vs = '0' for 100 ms;
        check_rgb(160, 40, X"FF", X"FF", X"40");   -- 0xE left  = bright yellow
        check_rgb(164, 40, X"00", X"00", X"80");   -- 0xE1 right = blue
        check_rgb(168, 40, X"80", X"40", X"00");   -- 0x6 left  = brown
        check_rgb(172, 40, X"00", X"00", X"80");   -- 0x61 right = blue
        check_rgb(176, 40, X"FF", X"40", X"40");   -- 0xC left  = bright red
        check_rgb(180, 40, X"00", X"80", X"00");   -- 0xC2 right = green
        check_rgb(160, 44, X"00", X"00", X"00");   -- fill row below tape
        check_rgb(1120, 40, X"00", X"00", X"FF");  -- margin stays blue
        report "PASS: event tape pixels on display" severity note;

        report "PASS: CPU/keyboard/HDMI integration" severity note;
        finished <= true;
        wait;
    end process;

    -- End-of-tape flag once all expected VFSA writes are seen.
    done_watch : process (cpu_clk)
    begin
        if rising_edge(cpu_clk) then
            if write_count >= 19200 + TAPE_EXPECTED then
                tape_done <= true;
            end if;
        end if;
    end process;
end sim;
