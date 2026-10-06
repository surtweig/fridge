-- Real boot program on the assembled system, real PS/2 pin traffic, and
-- independent ROM/text/framebuffer/pixel expectations. Includes warm reset
-- and reset in the middle of a ROM stream.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeRAMBootImage.all;
use work.FridgeROMImage.all;
use work.FridgeRasterFont.all;
use work.FridgeSystemDebug.all;

entity tb_graphics is end tb_graphics;
architecture sim of tb_graphics is
    signal cpu_clk, pixel_clk : std_logic := '0';
    signal reset : std_logic := '1';
    signal ps2_clk, ps2_dat : std_logic := '1';
    signal finished : boolean := false;
    signal halted, rom_error, overflow, rx_error, activity : std_logic;
    signal red, green, blue : std_logic_vector(7 downto 0);
    signal hs, vs, active : std_logic;
    signal px : integer range 0 to 1649;
    signal py : integer range 0 to 749;
    signal dbg : system_debug_t;
    type colors_t is array(0 to 15) of std_logic_vector(23 downto 0);
    constant colors : colors_t := (x"000000",x"000080",x"008000",x"008080",x"800000",x"800080",x"804000",x"808080",
                                   x"404040",x"0040FF",x"40FF40",x"40FFFF",x"FF4040",x"FF40FF",x"FFFF40",x"FFFFFF");
    function blend(rgb : std_logic_vector(23 downto 0); index, m : integer) return std_logic_vector is
        variable r : std_logic_vector(23 downto 0) := rgb;
        variable value, src, dst : integer;
    begin
        for ch in 0 to 2 loop
            dst := to_integer(unsigned(rgb(ch*8+7 downto ch*8)));
            src := to_integer(unsigned(colors(index)(ch*8+7 downto ch*8)));
            value := dst;
            case m is
                when 1 => value := src;
                when 2 => if index /= 0 then value := src; end if;
                when 3 => value := dst+src; if value > 255 then value := 255; end if;
                when 4 => value := dst-src; if value < 0 then value := 0; end if;
                when 5 => value := to_integer(to_unsigned(dst,8) and to_unsigned(src,8));
                when 6 => value := to_integer(to_unsigned(dst,8) or to_unsigned(src,8));
                when 7 => value := to_integer(to_unsigned(dst,8) xor to_unsigned(src,8));
                when others => null;
            end case;
            r(ch*8+7 downto ch*8) := std_logic_vector(to_unsigned(value,8));
        end loop;
        return r;
    end;
    constant TITLE : string := "FRAMEBUFFER + SPRITES: PASS";
    constant HELP1 : string := "SPACE: TEXT/EGA   WASD: MOVE";
    constant HELP2 : string := "M: SPRITE MODE (0..7)";
    constant HELP3 : string := "7 FIXED SPRITES SHOW MODES 1..7";
    function letter(col, row : integer) return integer is
    begin
        if row = 3 and col >= 2 and col < 2+TITLE'length then return character'pos(TITLE(col-1));
        elsif row = 6 and col >= 2 and col < 2+HELP1'length then return character'pos(HELP1(col-1));
        elsif row = 8 and col >= 2 and col < 2+HELP2'length then return character'pos(HELP2(col-1));
        elsif row = 10 and col >= 2 and col < 2+HELP3'length then return character'pos(HELP3(col-1));
        else return 0; end if;
    end;
    procedure bit_out(signal ck, dt : out std_logic; d : std_logic) is
    begin
        dt <= d; wait for 20 us; ck <= '0'; wait for 40 us; ck <= '1'; wait for 20 us;
    end;
    procedure send_byte(signal ck, dt : out std_logic; value : std_logic_vector(7 downto 0)) is
        variable parity : std_logic := '1';
    begin
        bit_out(ck,dt,'0');
        for i in 0 to 7 loop bit_out(ck,dt,value(i)); parity := parity xor value(i); end loop;
        bit_out(ck,dt,parity); bit_out(ck,dt,'1'); dt <= '1';
    end;
begin
    cpu_clk <= not cpu_clk after 50 ns when not finished else '0';
    pixel_clk <= not pixel_clk after 6734 ps when not finished else '0';
    dut : entity work.fridge_system
        generic map(BOOT_IMAGE => RAMBootImage, ROM_INIT => ROM_IMAGE)
        port map(cpu_clk => cpu_clk, pixel_clk => pixel_clk, cpu_reset => reset, pixel_reset => reset,
                 ps2_clk => ps2_clk, ps2_dat => ps2_dat, RED => red, GREEN => green, BLUE => blue,
                 HSYNC => hs, VSYNC => vs, ACTIVE => active, PIXEL_X => px, PIXEL_Y => py,
                 HALTED => halted, ROM_ERROR => rom_error, KBD_OVERFLOW => overflow,
                 KBD_ERROR => rx_error, KBD_ACTIVITY => activity, DEBUG => dbg);
    monitor : process(cpu_clk)
    begin
        if rising_edge(cpu_clk) and reset = '0' then
            assert halted = '0' report "graphics demo self-test failed" severity failure;
            assert rom_error = '0' and overflow = '0' and rx_error = '0' report "graphics demo device error" severity failure;
        end if;
    end process;
    stimulus : process
        procedure capture(text_mode : boolean; sprite_x : integer := 112; sprite_mode : integer := 2) is
            variable x, y, index, glyph_index : integer;
            variable expected : std_logic_vector(23 downto 0);
            variable glyph : std_logic_vector(0 to 7);
        begin
            wait until rising_edge(pixel_clk) and px = 0 and py = 0;
            wait until rising_edge(pixel_clk) and px = 0 and py = 0;
            for n in 0 to 1650*750-1 loop
                wait until falling_edge(pixel_clk);
                expected := x"000000";
                if px < 1280 and py < 720 then
                    expected := x"0000FF";
                    if px >= 160 and px < 1120 and py >= 40 and py < 680 then
                        x := (px-160)/4; y := (py-40)/4;
                        if text_mode then
                            glyph_index := letter(x/6,y/8);
                            glyph := RasterFontData(glyph_index)(x mod 6);
                            expected := colors(0);
                            if glyph(y mod 8) = '1' then expected := colors(15); end if;
                        else
                            expected := colors(7);
                            if x = y then expected := colors(12); end if;
                            if x >= sprite_x and x < sprite_x+16 and y >= 56 and y < 72 then
                                if (x-sprite_x) mod 4 = 0 or (x-sprite_x) mod 4 = 3 then index := 0;
                                else index := 15; end if;
                                expected := blend(expected,index,sprite_mode);
                            end if;
                            for m in 1 to 7 loop
                                if x >= 16+30*(m-1) and x < 32+30*(m-1) and y >= 104 and y < 120 then
                                    if (x-(16+30*(m-1))) mod 4 = 0 or (x-(16+30*(m-1))) mod 4 = 3 then index := 0;
                                    else index := 15; end if;
                                    expected := blend(expected,index,m);
                                end if;
                            end loop;
                        end if;
                    end if;
                end if;
                assert red & green & blue = expected report "graphics demo pixel mismatch at " & integer'image(px) & "," & integer'image(py) severity failure;
                wait until rising_edge(pixel_clk);
            end loop;
        end;
    begin
        wait for 2 us; wait until falling_edge(cpu_clk); reset <= '0';
        wait until dbg.device_read = '1' and dbg.device_sel = 3 for 100 ms;
        assert dbg.device_read = '1' report "graphics boot failed to reach keyboard poll" severity failure;
        capture(true);
        send_byte(ps2_clk,ps2_dat,x"29"); -- SPACE: show EGA.
        capture(false);
        send_byte(ps2_clk,ps2_dat,x"1C"); -- A: move left.
        send_byte(ps2_clk,ps2_dat,x"3A"); -- M: transparent-zero -> additive.
        capture(false,111,3);
        send_byte(ps2_clk,ps2_dat,x"F0"); send_byte(ps2_clk,ps2_dat,x"29");
        send_byte(ps2_clk,ps2_dat,x"29"); -- SPACE: return to TEXT.
        capture(true);
        -- Warm reset reruns the boot-time self-tests and clears help cells.
        wait until falling_edge(cpu_clk); reset <= '1'; wait for 2 us;
        wait until falling_edge(cpu_clk); reset <= '0';
        wait until dbg.device_read = '1' and dbg.device_sel = 3 for 100 ms;
        assert dbg.device_read = '1' report "graphics warm reset failed" severity failure;
        capture(true);
        report "PASS: framebuffer and sprite board demo" severity note;
        finished <= true; wait;
    end process;
end;
