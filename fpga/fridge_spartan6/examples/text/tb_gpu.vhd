library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeRasterFont.all;

entity tb_gpu is
end tb_gpu;

architecture sim of tb_gpu is
    signal clk : std_logic := '0';
    signal finished : boolean := false;
    signal reset : std_logic := '1';
    signal command_reset : std_logic := '1';

    signal frame_store : std_logic := '0';
    signal frame_addr : XCM2_DWORD := (others => '0');
    signal frame_data : XCM2_WORD := (others => '0');
    signal present_trigger : std_logic := '0';
    signal present_mode : XCM2_WORD := (others => '0');
    signal frame_offset : XCM2_DWORD := (others => '0');
    signal mode_switch : std_logic_vector(0 to 1) := "00";

    signal red, green, blue : std_logic_vector(7 downto 0);
    signal hs, vs, act : std_logic;
    signal px : integer range 0 to 1649;
    signal py : integer range 0 to 749;

    constant CLK_PERIOD : time := 13468 ps;
    constant MARGIN : std_logic_vector(23 downto 0) := x"0000FF";

    type palette_t is array (0 to 15) of std_logic_vector(23 downto 0);
    constant PALETTE : palette_t := (
        x"000000", x"000080", x"008000", x"008080",
        x"800000", x"800080", x"804000", x"808080",
        x"404040", x"0040FF", x"40FF40", x"40FFFF",
        x"FF4040", x"FF40FF", x"FFFF40", x"FFFFFF");
begin
    clk <= not clk after CLK_PERIOD / 2 when not finished else '0';

    dut : entity work.fridge_gpu
        port map (
            CLK => clk,
            COMMAND_CLK => clk,
            RESET => reset,
            COMMAND_RESET => command_reset,
            FRAME_STORE => frame_store,
            FRAME_ADDR => frame_addr,
            FRAME_DATA => frame_data,
            PRESENT_TRIGGER => present_trigger,
            PRESENT_MODE => present_mode,
            FRAME_OFFSET => frame_offset,
            MODE_SWITCH => mode_switch,
            RED => red,
            GREEN => green,
            BLUE => blue,
            HSYNC => hs,
            VSYNC => vs,
            ACTIVE => act,
            PIXEL_X => px,
            PIXEL_Y => py);

    stimulus : process
        type model_fb_t is array (0 to 19199) of integer range 0 to 255;
        variable model0, model1 : model_fb_t := (others => 0);
        variable tb_visible : std_logic := '0';
        variable tb_active : std_logic := '1';

        procedure expected_color(ex, ey : integer; which : std_logic;
                                 hoff, voff : integer; text : std_logic;
                                 result : out std_logic_vector(23 downto 0)) is
            variable ix, iy, pxi, pyi, addr, b, idx : integer;
            variable ch, at : integer;
            variable glyph : std_logic_vector(0 to 7);
        begin
            if ex >= 1280 or ey >= 720 then
                result := x"000000";
            elsif ex < 160 or ex >= 1120 or ey < 40 or ey >= 680 then
                result := MARGIN;
            elsif text = '1' then
                -- TEXT mode: 40x20 cells of two bytes at (row*40 + col)*2,
                -- glyph code then attribute (background high, foreground low).
                -- Frame offsets are ignored, matching the emulator.
                ix := (ex - 160) / 4;
                iy := (ey - 40) / 4;
                addr := (iy / 8) * 80 + (ix / 6) * 2;
                if which = '1' then
                    ch := model1(addr);
                    at := model1(addr + 1);
                else
                    ch := model0(addr);
                    at := model0(addr + 1);
                end if;
                glyph := RasterFontData(ch)(ix mod 6);
                if glyph(iy mod 8) = '1' then
                    idx := at mod 16;
                else
                    idx := at / 16;
                end if;
                result := PALETTE(idx);
            else
                ix := (ex - 160) / 4;
                iy := (ey - 40) / 4;
                pxi := ix + hoff;
                if pxi >= 240 then
                    pxi := pxi - 240;
                end if;
                pyi := iy + voff;
                if pyi >= 160 then
                    pyi := pyi - 160;
                end if;
                addr := pyi * 120 + pxi / 2;
                if which = '1' then
                    b := model1(addr);
                else
                    b := model0(addr);
                end if;
                if pxi mod 2 = 0 then
                    idx := b / 16;
                else
                    idx := b mod 16;
                end if;
                result := PALETTE(idx);
            end if;
        end procedure;

        procedure write_byte(addr, val : integer) is
        begin
            wait until falling_edge(clk);
            frame_store <= '1';
            frame_addr <= to_unsigned(addr, 16);
            frame_data <= to_unsigned(val, 8);
            wait until falling_edge(clk);
            frame_store <= '0';
            if tb_active = '1' then
                model1(addr) := val;
            else
                model0(addr) := val;
            end if;
        end procedure;

        procedure write_raw(addr, val : integer) is
        begin
            wait until falling_edge(clk);
            frame_store <= '1';
            frame_addr <= to_unsigned(addr, 16);
            frame_data <= to_unsigned(val, 8);
            wait until falling_edge(clk);
            frame_store <= '0';
        end procedure;

        procedure do_vpre(mode, hoff, voff : integer) is
        begin
            wait until falling_edge(clk);
            present_mode <= to_unsigned(mode, 8);
            frame_offset <= to_unsigned(hoff, 8) & to_unsigned(voff, 8);
            present_trigger <= '1';
            wait until falling_edge(clk);
            present_trigger <= '0';
            if mode = 1 then
                if tb_visible = '0' then
                    tb_visible := '1';
                    tb_active := '0';
                else
                    tb_visible := '0';
                    tb_active := '1';
                end if;
            elsif mode = 2 then
                tb_visible := '0';
                tb_active := '0';
            elsif mode = 3 then
                tb_visible := '0';
                tb_active := '1';
            elsif mode = 4 then
                tb_visible := '1';
                tb_active := '0';
            elsif mode = 5 then
                tb_visible := '1';
                tb_active := '1';
            end if;
        end procedure;

        procedure do_vmode(text : std_logic) is
        begin
            wait until falling_edge(clk);
            mode_switch(0) <= '1';
            mode_switch(1) <= text;
            wait until falling_edge(clk);
            mode_switch(0) <= '0';
            mode_switch(1) <= '0';
        end procedure;

        procedure wait_frame_start is
        begin
            loop
                wait until falling_edge(clk);
                exit when px = 0 and py = 0;
            end loop;
        end procedure;

        procedure check_pixel(ex, ey : integer; which : std_logic;
                              hoff, voff : integer; text : std_logic) is
            variable exp : std_logic_vector(23 downto 0);
        begin
            loop
                wait until falling_edge(clk);
                exit when px = ex and py = ey;
            end loop;
            expected_color(ex, ey, which, hoff, voff, text, exp);
            assert red & green & blue = exp
                report "pixel mismatch at " & integer'image(ex) & "," &
                       integer'image(ey) severity failure;
        end procedure;

        -- Fixed expectation, independent of the reference model: used to lock
        -- the font's bit order and column indexing against a hand-decoded glyph.
        procedure check_rgb(ex, ey : integer;
                            er, eg, eb : std_logic_vector(7 downto 0)) is
        begin
            loop
                wait until falling_edge(clk);
                exit when px = ex and py = ey;
            end loop;
            assert red = er and green = eg and blue = eb
                report "pixel mismatch at " & integer'image(ex) & "," &
                       integer'image(ey) severity failure;
        end procedure;

        procedure check_byte(addr : integer; which : std_logic;
                             hoff, voff : integer; text : std_logic) is
            variable pxi, pyi, ix, iy : integer;
        begin
            pxi := (addr mod 120) * 2;
            pyi := addr / 120;
            ix := (pxi - hoff) mod 240;
            iy := (pyi - voff) mod 160;
            check_pixel(160 + ix * 4, 40 + iy * 4, which, hoff, voff, text);
        end procedure;

        procedure check_rows(y0, y1 : integer; which : std_logic;
                             hoff, voff : integer; text : std_logic) is
            variable ex, ey : integer;
            variable exp : std_logic_vector(23 downto 0);
        begin
            loop
                wait until falling_edge(clk);
                exit when px = 0 and py = 0;
            end loop;
            for i in 0 to 1650 * (y1 + 1) - 1 loop
                ex := i mod 1650;
                ey := i / 1650;
                assert px = ex and py = ey
                    report "raster position mismatch at sample " &
                           integer'image(i) severity failure;
                if ey >= y0 then
                    assert (hs = '1') = (ex >= 1390 and ex < 1430)
                        report "HSYNC mismatch at " & integer'image(ex) severity failure;
                    assert (vs = '1') = (ey >= 725 and ey < 730)
                        report "VSYNC mismatch at " & integer'image(ey) severity failure;
                    assert (act = '1') = (ex < 1280 and ey < 720)
                        report "ACTIVE mismatch at " & integer'image(ex) & "," &
                               integer'image(ey) severity failure;
                    expected_color(ex, ey, which, hoff, voff, text, exp);
                    assert red & green & blue = exp
                        report "pixel mismatch at " & integer'image(ex) & "," &
                               integer'image(ey) severity failure;
                end if;
                wait until falling_edge(clk);
            end loop;
            if y1 = 749 then
                assert px = 0 and py = 0
                    report "frame did not wrap" severity failure;
            end if;
        end procedure;

        procedure fill_frame(pattern_id : integer) is
            variable v : integer;
        begin
            for a in 0 to 19199 loop
                if pattern_id = 1 then
                    v := (a mod 256 + a / 256) mod 256;
                else
                    v := (a mod 256 + 96) mod 256;
                end if;
                write_byte(a, v);
            end loop;
        end procedure;
    begin
        reset <= '1';
        command_reset <= '1';
        wait for 10 * CLK_PERIOD;
        wait until falling_edge(clk);
        reset <= '0';
        command_reset <= '0';

        -- Reset state: TEXT mode, empty frames, zero offsets.
        wait_frame_start;
        check_pixel(10, 10, '0', 0, 0, '1');
        check_pixel(160, 40, '0', 0, 0, '1');
        check_pixel(200, 100, '0', 0, 0, '1');
        report "PASS: reset state (TEXT, empty frames)" severity note;

        do_vmode('0');
        fill_frame(1);
        report "PASS: frame1 filled via write port" severity note;

        -- AUTO from (visible=0, active=1) -> (visible=1, active=0) at vblank.
        wait_frame_start;
        do_vpre(1, 0, 0);
        check_byte(256, '0', 0, 0, '0');
        check_byte(300, '0', 0, 0, '0');
        check_pixel(200, 300, '0', 0, 0, '0');
        report "PASS: no display change before vblank" severity note;
        check_rows(0, 749, '1', 0, 0, '0');
        report "PASS: full frame after AUTO swap (margins, 4x scaling, nibble order, palette, raster)" severity note;

        fill_frame(2);

        -- AUTO from (visible=1, active=0) -> (visible=0, active=1) at vblank.
        wait_frame_start;
        do_vpre(1, 0, 0);
        check_byte(500, '1', 0, 0, '0');
        check_pixel(400, 200, '1', 0, 0, '0');
        check_pixel(800, 400, '1', 0, 0, '0');
        report "PASS: old frame stays visible until vblank" severity note;
        check_rows(40, 103, '0', 0, 0, '0');
        report "PASS: second AUTO swap shows the other buffer" severity note;

        -- MANUAL modes and write targeting.
        do_vpre(5, 0, 0);
        wait_frame_start;
        check_byte(10, '1', 0, 0, '0');
        write_byte(7, 16#5A#);
        do_vpre(3, 0, 0);
        wait_frame_start;
        check_byte(10, '0', 0, 0, '0');
        write_byte(9, 16#A5#);
        do_vpre(2, 0, 0);
        wait_frame_start;
        check_byte(10, '0', 0, 0, '0');
        write_byte(11, 16#3C#);
        do_vpre(4, 0, 0);
        wait_frame_start;
        check_byte(7, '1', 0, 0, '0');
        check_byte(9, '1', 0, 0, '0');
        check_byte(11, '1', 0, 0, '0');
        do_vpre(3, 0, 0);
        wait_frame_start;
        check_byte(11, '0', 0, 0, '0');
        report "PASS: swap modes 2..5 and write targeting" severity note;

        -- Frame offsets with wrap.
        do_vpre(0, 1, 2);
        check_rows(40, 103, '0', 1, 2, '0');
        report "PASS: frame offsets 1,2 with wrap" severity note;
        do_vpre(0, 0, 0);
        wait_frame_start;
        check_byte(11, '0', 0, 0, '0');
        check_pixel(800, 500, '0', 0, 0, '0');
        report "PASS: offsets restored" severity note;

        -- Out-of-range writes are ignored.
        write_raw(19200, 16#EE#);
        write_raw(32767, 16#BB#);
        write_raw(65535, 16#CC#);
        wait_frame_start;
        check_byte(0, '0', 0, 0, '0');
        check_byte(19199, '0', 0, 0, '0');
        check_pixel(1119, 679, '0', 0, 0, '0');
        report "PASS: out-of-range writes ignored" severity note;

        -- TEXT mode: 40x20 cells of two bytes at (row*40 + col)*2, glyph code
        -- then attribute (background in the high nibble, foreground in the low).
        do_vpre(2, 0, 0);
        wait_frame_start;
        do_vmode('1');

        -- cell (5,3): 'A' (0x41), white 15 on dark blue 1 -> attribute 0x1F
        write_byte((3*40 + 5)*2, 16#41#);
        write_byte((3*40 + 5)*2 + 1, 16#1F#);
        -- cell (0,0): '0' (0x30), black 0 on bright green 10 -> attribute 0xA0
        write_byte(0, 16#30#);
        write_byte(1, 16#A0#);
        -- cell (39,19): '_' (0x5F), bright magenta 13 on black 0 -> 0x0D
        write_byte((19*40 + 39)*2, 16#5F#);
        write_byte((19*40 + 39)*2 + 1, 16#0D#);

        wait_frame_start;
        -- 'A' is 7E A0 A0 A0 7E 00, one byte per column with bit 7 in the top
        -- row. Cell (5,3) starts at internal (30,24) -> screen (280,136).
        check_rgb(280, 136, X"00", X"00", X"80");  -- col 0 row 0 clear -> bg
        check_rgb(280, 140, X"FF", X"FF", X"FF");  -- col 0 row 1 set   -> fg
        check_rgb(284, 136, X"FF", X"FF", X"FF");  -- col 1 row 0 set   -> fg
        check_rgb(300, 164, X"00", X"00", X"80");  -- col 5 row 7 clear -> bg
        report "PASS: glyph bit order and column index ('A')" severity note;

        check_rows(0, 749, '0', 0, 0, '1');
        report "PASS: TEXT mode full-frame glyph/attribute scan" severity note;

        -- TEXT scanout ignores the frame offsets, matching the emulator.
        do_vpre(0, 5, 7);
        wait_frame_start;
        check_pixel(280, 136, '0', 5, 7, '1');
        check_pixel(280, 140, '0', 5, 7, '1');
        check_pixel(160, 40, '0', 5, 7, '1');
        check_pixel(1119, 679, '0', 5, 7, '1');
        report "PASS: TEXT mode ignores VPRE frame offsets" severity note;

        do_vmode('0');
        wait_frame_start;
        check_pixel(280, 136, '0', 5, 7, '0');
        check_pixel(200, 100, '0', 5, 7, '0');
        report "PASS: return to EGA after TEXT" severity note;

        report "PASS: GPU framebuffer/scan-out tests" severity note;
        finished <= true;
        wait;
    end process;
end sim;
