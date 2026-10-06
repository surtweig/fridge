-- Independent pixel reference: packed odd-width sprites, every mode, four
-- overlaps, descriptor 63, clipping, offsets, CDC, and the worst-case 64 hits.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeIRCodes.all;
entity tb_sprites is end;
architecture sim of tb_sprites is
    signal clk, cmd_clk : std_logic := '0';
    signal reset : std_logic := '1'; signal finished : boolean := false;
    signal valid, ready, store, present : std_logic := '0';
    signal code, a, b, c, arg0, arg1, result, data, pm : XCM2_WORD := X"00";
    signal hl, bc, addr, offs : XCM2_DWORD := X"0000";
    signal mode : std_logic_vector(0 to 1) := "00";
    signal red, green, blue : std_logic_vector(7 downto 0);
    signal hs, vs, act : std_logic;
    signal px : integer range 0 to 1649; signal py : integer range 0 to 749;
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
begin
    clk <= not clk after 6734 ps when not finished else '0';
    cmd_clk <= not cmd_clk after 50 ns when not finished else '0';
    dut : entity work.fridge_gpu
        port map(CLK => clk, COMMAND_CLK => cmd_clk, RESET => reset, COMMAND_RESET => reset,
            COMMAND_VALID => valid, COMMAND_READY => ready, COMMAND_CODE => code,
            COMMAND_A => a, COMMAND_B => b, COMMAND_C => c, COMMAND_HL => hl, COMMAND_BC => bc,
            COMMAND_ARG0 => arg0, COMMAND_ARG1 => arg1, COMMAND_RESULT => result,
            FRAME_STORE => store, FRAME_ADDR => addr, FRAME_DATA => data,
            PRESENT_TRIGGER => present, PRESENT_MODE => pm, FRAME_OFFSET => offs, MODE_SWITCH => mode,
            PALETTE_READY => open, RED => red, GREEN => green, BLUE => blue, HSYNC => hs,
            VSYNC => vs, ACTIVE => act, PIXEL_X => px, PIXEL_Y => py);
    stimulus : process
        procedure command(op : XCM2_WORD; av, bv, cv, hv : integer) is
        begin
            wait until falling_edge(cmd_clk);
            code <= op; a <= to_unsigned(av,8); b <= to_unsigned(bv,8); c <= to_unsigned(cv,8);
            hl <= to_unsigned(hv,16); valid <= '1';
            wait until ready = '1' for 20 us;
            assert ready = '1' report "GPU command timeout" severity failure;
            wait until falling_edge(cmd_clk); valid <= '0';
            wait until falling_edge(cmd_clk); wait until falling_edge(cmd_clk);
        end;
        procedure capture(offset_x, offset_y : integer; worst : boolean := false; blank : boolean := false) is
            variable x, y, sx, sy, index : integer;
            variable expected : std_logic_vector(23 downto 0);
        begin
            -- Allow a complete refresh after descriptors or offsets change.
            wait until rising_edge(clk) and px = 0 and py = 0;
            wait until rising_edge(clk) and px = 0 and py = 0;
            for n in 0 to 1650*750-1 loop
                wait until falling_edge(clk);
                x := px; y := py;
                expected := x"000000";
                if x < 1280 and y < 720 then
                    expected := x"0000FF";
                    if x >= 160 and x < 1120 and y >= 40 and y < 680 then
                        sx := ((x-160)/4+offset_x) mod 240; sy := ((y-40)/4+offset_y) mod 160;
                        expected := colors(7);
                        if blank then null;
                        elsif worst then
                            -- Alternating row colors catch stale/missed cache rows.
                            -- Four additive sprites saturate; later opaque hits
                            -- would produce a different result if the cap failed.
                            index := 1+sy mod 2;
                            for i in 0 to 3 loop expected := blend(expected,index,3); end loop;
                        else
                            for m in 1 to 7 loop
                                if sx >= 8+24*(m-1) and sx < 25+24*(m-1) and sy >= 12 and sy < 21 then
                                    index := ((sy-12)*17+sx-(8+24*(m-1))) mod 16;
                                    expected := blend(expected,index,m);
                                end if;
                            end loop;
                            if sx >= 8 and sx < 25 and sy >= 40 and sy < 49 then
                                index := ((sy-40)*17+sx-8) mod 16;
                                -- Three transparent-zero sprites still consume
                                -- slots; the fourth is XOR, the fifth opaque.
                                for i in 0 to 2 loop expected := blend(expected,index,2); end loop;
                                expected := blend(expected,index,7);
                            end if;
                            if sx = 239 and sy = 159 then expected := colors(0); end if;
                        end if;
                    end if;
                end if;
                assert red & green & blue = expected
                    report "sprite pixel mismatch x=" & integer'image(x) & " y=" & integer'image(y) &
                           " expected=" & integer'image(to_integer(unsigned(expected))) &
                           " got=" & integer'image(to_integer(unsigned(std_logic_vector'(red & green & blue)))) severity failure;
                assert (act = '1') = (x < 1280 and y < 720) report "sprite pipeline raster alignment" severity failure;
                wait until rising_edge(clk);
            end loop;
        end;
    begin
        wait for 2 us; wait until falling_edge(cmd_clk); reset <= '0';
        -- Frame 1 active/visible, EGA.
        pm <= to_unsigned(5,8); present <= '1'; mode <= "10";
        wait until falling_edge(cmd_clk); present <= '0'; mode <= "00";
        data <= X"77"; store <= '1';
        for i in 0 to 19199 loop addr <= to_unsigned(i,16); wait until falling_edge(cmd_clk); end loop;
        store <= '0';
        -- Sprite pixels are a continuous packed stream, including across odd rows.
        for i in 0 to 76 loop command(VSSA,(i*2 mod 16)*16+(i*2+1) mod 16,0,0,256+i); end loop;
        for m in 1 to 7 loop
            command(VSS,m,17,9,256); command(VSD,m,m,0,(8+24*(m-1))*256+12);
        end loop;
        for i in 8 to 12 loop
            command(VSS,i,17,9,256);
            if i < 11 then command(VSD,i,2,0,8*256+40);
            elsif i = 11 then command(VSD,i,7,0,8*256+40);
            else command(VSD,i,1,0,8*256+40); end if;
        end loop;
        command(VSS,63,17,9,256); command(VSD,63,1,0,239*256+159);
        command(VSS,64,240,160,256); command(VSD,64,1,0,0); -- Cannot alias descriptor 0.
        command(VSS,63,17,9,32767); command(VSD,63,8,0,0); -- Must retain valid descriptor.
        capture(0,0);
        wait until falling_edge(cmd_clk); offs <= X"EB9B"; pm <= X"00"; present <= '1';
        wait until falling_edge(cmd_clk); present <= '0';
        capture(235,155);
        -- Worst case: every pixel intersects all 64 descriptors. The cache
        -- must finish each row before scanout despite four RAM reads/pixel.
        for i in 0 to 19199 loop command(VSSA,17*(1+(i/120) mod 2),0,0,1024+i); end loop;
        for i in 0 to 63 loop
            command(VSS,i,240,160,1024);
            if i < 4 then command(VSD,i,3,0,0); else command(VSD,i,1,0,0); end if;
        end loop;
        capture(235,155,true);
        -- Reset removes all descriptors even though sprite RAM is retained.
        wait until falling_edge(cmd_clk); reset <= '1'; wait for 2 us;
        wait until falling_edge(cmd_clk); reset <= '0';
        pm <= to_unsigned(5,8); present <= '1'; mode <= "10"; offs <= X"0000";
        wait until falling_edge(cmd_clk); present <= '0'; mode <= "00";
        capture(0,0,false,true);
        report "PASS: sprite scanout and compositing" severity note;
        finished <= true; wait;
    end process;
end;
