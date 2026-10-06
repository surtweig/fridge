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

entity tb_system is end tb_system;
architecture sim of tb_system is
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
    signal stores, rom_reads, rom_writes, key_events, modes, presents, palettes : integer := 0;
    signal foreign_writes, unknown_reads, reset_reads : integer := 0;
    signal interleaved : boolean := false;
    signal palette_last : std_logic := '0';
    constant TITLE : string := "FRIDGE: ROM + KEYBOARD + GPU";
    constant CONTROLS : string := "SPACE: TEXT/EGA   R: PALETTE";
    constant PROMPT : string := "TYPE BELOW:";
    constant TEXT_STORES : integer := 1600 + 2*(TITLE'length+CONTROLS'length+PROMPT'length);
    constant INIT_STORES : integer := TEXT_STORES + 1024;
    type bytes_t is array (natural range <>) of integer;
    constant EVENTS : bytes_t := (16#E1#,16#61#,16#A0#,16#20#,16#E2#,16#62#,16#F2#,16#72#,16#A0#,16#20#);
    type colors_t is array (0 to 15) of std_logic_vector(23 downto 0);
    constant DEFAULTS : colors_t := (
        x"000000",x"000080",x"008000",x"008080",x"800000",x"800080",x"804000",x"808080",
        x"404040",x"0040FF",x"40FF40",x"40FFFF",x"FF4040",x"FF40FF",x"FFFF40",x"FFD040");
    function rom_byte(i : integer) return integer is
    begin
        case i/256 is
            when 0 => return i mod 256;
            when 1 => return 255 - i mod 256;
            when 2 => if i mod 2 = 0 then return 16#AA#; else return 16#55#; end if;
            when others => return 16#42#;
        end case;
    end function;
    procedure bit_out(signal ck, dt : out std_logic; d : std_logic) is
    begin
        dt <= d; wait for 20 us;
        ck <= '0'; wait for 40 us;
        ck <= '1'; wait for 20 us;
    end procedure;
    procedure send_byte(signal ck, dt : out std_logic; b : std_logic_vector(7 downto 0)) is
        variable parity : std_logic := '1';
    begin
        bit_out(ck,dt,'0');
        for i in 0 to 7 loop
            bit_out(ck,dt,b(i)); parity := parity xor b(i);
        end loop;
        bit_out(ck,dt,parity); bit_out(ck,dt,'1'); dt <= '1';
    end procedure;
begin
    cpu_clk <= not cpu_clk after 50 ns when not finished else '0';
    pixel_clk <= not pixel_clk after 6734 ps when not finished else '0';
    dut : entity work.fridge_system
        generic map (BOOT_IMAGE => RAMBootImage, ROM_INIT => ROM_IMAGE)
        port map (cpu_clk => cpu_clk, pixel_clk => pixel_clk,
                  cpu_reset => reset, pixel_reset => reset, ps2_clk => ps2_clk, ps2_dat => ps2_dat,
                  RED => red, GREEN => green, BLUE => blue, HSYNC => hs, VSYNC => vs, ACTIVE => active,
                  PIXEL_X => px, PIXEL_Y => py, HALTED => halted, ROM_ERROR => rom_error,
                  KBD_OVERFLOW => overflow, KBD_ERROR => rx_error, KBD_ACTIVITY => activity, DEBUG => dbg);

    monitor : process(cpu_clk)
        variable ordinal, addr, value, n : integer;
    begin
        if rising_edge(cpu_clk) then
            if reset = '1' then
                stores <= 0; rom_reads <= 0; rom_writes <= 0; key_events <= 0;
                modes <= 0; presents <= 0; palettes <= 0;
                foreign_writes <= 0; unknown_reads <= 0; reset_reads <= 0;
                interleaved <= false; palette_last <= '0';
            else
                assert halted = '0' and rom_error = '0' and overflow = '0' and rx_error = '0'
                    report "combined system halted or signalled a peripheral error" severity failure;
                assert not (dbg.device_read = '1' and dbg.device_write = '1')
                    report "overlapping I/O strobes" severity failure;
                if dbg.device_read = '1' then
                    assert not is_x(std_logic_vector(dbg.device_in))
                        report "unresolved device read data" severity failure;
                    case to_integer(dbg.device_sel) is
                        when 1 =>
                            assert rom_reads < 1024 and to_integer(dbg.device_in) = rom_byte(rom_reads)
                                report "ROM read sequence corrupted by foreign-device traffic" severity failure;
                            rom_reads <= rom_reads+1;
                        when 3 =>
                            if dbg.device_in /= 0 then
                                assert key_events < EVENTS'length and
                                       to_integer(dbg.device_in) = EVENTS(key_events)
                                    report "keyboard FIFO sequence mismatch" severity failure;
                                if key_events = 0 then
                                    assert rom_reads > 0 and rom_reads < 1024
                                        report "keyboard event was not consumed during ROM streaming" severity failure;
                                    interleaved <= true;
                                end if;
                                key_events <= key_events+1;
                            end if;
                        when 2 =>
                            assert dbg.device_in = 0 report "reset device read must return zero" severity failure;
                            reset_reads <= reset_reads+1;
                        when 99 =>
                            assert dbg.device_in = 0 report "unmapped read returned stale data" severity failure;
                            unknown_reads <= unknown_reads+1;
                        when others => assert false report "unexpected read device" severity failure;
                    end case;
                end if;
                if dbg.device_write = '1' then
                    if dbg.device_sel = 99 then
                        assert dbg.device_out = 16#5A# severity failure;
                        foreign_writes <= foreign_writes+1;
                    elsif dbg.device_sel = 2 then
                        assert rom_writes = 0 and dbg.device_out = 1 severity failure;
                        rom_writes <= rom_writes+1;
                    else
                        assert dbg.device_sel = 1 and rom_writes < 13 severity failure;
                        case (rom_writes-1) mod 3 is
                            when 0 => value := 2;
                            when 1 => value := 0;
                            when others => value := (rom_writes-1)/3;
                        end case;
                        assert to_integer(dbg.device_out) = value report "ROM IOUT protocol mismatch" severity failure;
                        rom_writes <= rom_writes+1;
                    end if;
                end if;
                if dbg.frame_store = '1' then
                    ordinal := stores;
                    if ordinal < 1600 then addr := ordinal; value := 0;
                    elsif ordinal < 1600+TITLE'length*2 then
                        n := ordinal-1600; addr := 164+n;
                        if n mod 2 = 0 then value := character'pos(TITLE(n/2+1)); else value := 15; end if;
                    elsif ordinal < 1600+(TITLE'length+CONTROLS'length)*2 then
                        n := ordinal-1600-TITLE'length*2; addr := 324+n;
                        if n mod 2 = 0 then value := character'pos(CONTROLS(n/2+1)); else value := 15; end if;
                    elsif ordinal < TEXT_STORES then
                        n := ordinal-1600-(TITLE'length+CONTROLS'length)*2; addr := 484+n;
                        if n mod 2 = 0 then value := character'pos(PROMPT(n/2+1)); else value := 15; end if;
                    elsif ordinal < INIT_STORES then
                        addr := ordinal-TEXT_STORES; value := rom_byte(addr);
                    else
                        n := ordinal-INIT_STORES; addr := 644+n;
                        assert n < 4 report "unexpected echo write" severity failure;
                        if n mod 2 = 0 then value := character'pos('a')+n/2; else value := 15; end if;
                    end if;
                    assert to_integer(dbg.frame_addr) = addr and to_integer(dbg.frame_data) = value
                        report "combined program VFSA mismatch at store " & integer'image(stores) &
                               " got " & integer'image(to_integer(dbg.frame_addr)) & "/" & integer'image(to_integer(dbg.frame_data)) &
                               " expected " & integer'image(addr) & "/" & integer'image(value) severity failure;
                    stores <= stores+1;
                end if;
                if dbg.mode_switch(0) = '1' then
                    if modes = 1 then assert dbg.mode_switch(1) = '0' severity failure;
                    else assert dbg.mode_switch(1) = '1' severity failure; end if;
                    modes <= modes+1;
                end if;
                if dbg.present = '1' then
                    assert dbg.frame_offset = 0 severity failure;
                    presents <= presents+1;
                end if;
                palette_last <= dbg.palette_write;
                if dbg.palette_write = '1' and palette_last = '0' then
                    assert dbg.palette_index = 15 and dbg.palette_ready = '1' severity failure;
                    if palettes = 0 then assert dbg.palette_rgb = x"FFD040" severity failure;
                    else assert palettes = 1 and dbg.palette_rgb = x"3F8040" severity failure; end if;
                    palettes <= palettes+1;
                end if;
            end if;
        end if;
    end process;

    watchdog : process
    begin
        wait for 500 ms;
        assert finished report "combined-system test timed out" severity failure;
        wait;
    end process;
    stimulus : process
        procedure frame_start is
        begin
            loop
                wait until falling_edge(pixel_clk);
                exit when px=0 and py=0;
            end loop;
        end procedure;
        procedure check_frame(text_mode : boolean; echoes : integer; animated : boolean) is
            variable ex, ey, ix, iy, cell, ch, value, idx, byteaddr : integer;
            variable fontcolumn : std_logic_vector(0 to 7);
            variable expected : std_logic_vector(23 downto 0);
        begin
            frame_start;
            for sample in 0 to 1650*750-1 loop
                ex := sample mod 1650; ey := sample/1650;
                assert px=ex and py=ey report "integrated raster alignment mismatch" severity failure;
                assert (active='1') = (ex<1280 and ey<720) severity failure;
                assert (hs='1') = (ex>=1390 and ex<1430) severity failure;
                assert (vs='1') = (ey>=725 and ey<730) severity failure;
                if ex>=1280 or ey>=720 then expected := x"000000";
                elsif ex<160 or ex>=1120 or ey<40 or ey>=680 then expected := x"0000FF";
                else
                    ix := (ex-160)/4; iy := (ey-40)/4;
                    if text_mode then
                        cell := (iy/8)*40+ix/6; ch := 0;
                        if cell>=82 and cell<82+TITLE'length then ch:=character'pos(TITLE(cell-82+1));
                        elsif cell>=162 and cell<162+CONTROLS'length then ch:=character'pos(CONTROLS(cell-162+1));
                        elsif cell>=242 and cell<242+PROMPT'length then ch:=character'pos(PROMPT(cell-242+1));
                        elsif cell>=322 and cell<322+echoes then ch:=character'pos('a')+cell-322;
                        end if;
                        fontcolumn := RasterFontData(ch)(ix mod 6);
                        if fontcolumn(iy mod 8)='1' then idx:=15; else idx:=0; end if;
                    else
                        byteaddr := iy*120+ix/2;
                        if byteaddr<1024 then value:=rom_byte(byteaddr); else value:=0; end if;
                        if ix mod 2=0 then idx:=value/16; else idx:=value mod 16; end if;
                    end if;
                    expected := DEFAULTS(idx);
                    if animated and idx=15 then expected:=x"3F8040"; end if;
                end if;
                assert red & green & blue = expected
                    report "combined display mismatch at " & integer'image(ex) & "," & integer'image(ey) severity failure;
                wait until falling_edge(pixel_clk);
            end loop;
        end procedure;
        procedure press_release(code : std_logic_vector(7 downto 0)) is
        begin
            send_byte(ps2_clk,ps2_dat,code);
            send_byte(ps2_clk,ps2_dat,x"F0"); send_byte(ps2_clk,ps2_dat,code);
        end procedure;
        procedure initial_key is
        begin
            wait until rom_reads=1 for 50 ms;
            assert rom_reads=1 report "boot did not start ROM stream" severity failure;
            send_byte(ps2_clk,ps2_dat,x"1C"); -- a, while streaming
            wait until stores=INIT_STORES+2 for 50 ms;
            assert stores=INIT_STORES+2 and interleaved and rom_reads=1024 and rom_writes=13 and
                   unknown_reads=4 and reset_reads=4 and foreign_writes=4
                report "combined initialisation or device isolation failed" severity failure;
        end procedure;
    begin
        wait for 2 us; wait until falling_edge(cpu_clk); reset <= '0';
        initial_key;
        check_frame(true,1,false);
        report "PASS: ROM/keyboard interleaving, unknown-device guards, TEXT echo and full raster" severity note;
        send_byte(ps2_clk,ps2_dat,x"F0"); send_byte(ps2_clk,ps2_dat,x"1C");
        press_release(x"29"); -- space -> EGA
        assert modes=2 report "space did not select EGA" severity failure;
        press_release(x"32"); -- b writes TEXT while EGA is displayed
        press_release(x"2D"); -- r changes shared palette entry 15
        wait for 20 us;
        assert stores=INIT_STORES+4 and palettes=2 and key_events=8
            report "EGA-time typing or palette command failed" severity failure;
        check_frame(false,2,true);
        report "PASS: ROM patterns in EGA with live palette update and hidden TEXT writes" severity note;
        press_release(x"29"); -- back to TEXT
        wait for 20 us;
        assert modes=3 and key_events=10 severity failure;
        check_frame(true,2,true);
        report "PASS: return to TEXT preserves both echoes and uses the updated palette" severity note;

        -- Restart, abort its next ROM stream, then restart again. RAM/frame
        -- contents survive reset; the boot program must restore its own state.
        reset <= '1'; wait for 5 us; reset <= '0';
        wait until rom_reads=128 for 50 ms;
        assert rom_reads=128 report "warm restart failed: ROM reads=" & integer'image(rom_reads) &
            " PC=" & integer'image(to_integer(dbg.pc)) & " stores=" & integer'image(stores) severity failure;
        reset <= '1'; wait for 5 us; reset <= '0';
        initial_key;
        check_frame(true,1,false);
        assert palettes=1 and modes=1 and key_events=1 and rom_error='0' severity failure;
        report "PASS: warm reset and mid-stream reset recover ROM, keyboard, palette and program state" severity note;
        report "PASS: combined Fridge system" severity note;
        finished <= true;
        wait;
    end process;
end sim;
