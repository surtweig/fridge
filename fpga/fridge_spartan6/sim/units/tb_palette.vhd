-- Shared-RTL regression, adapted from examples/palette/tb_palette.vhd.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeRasterFont.all;

entity tb_palette is
end tb_palette;

architecture sim of tb_palette is
    signal clk : std_logic := '0';
    signal command_clk : std_logic := '0';
    signal pixel_period : time := 13468 ps;
    signal command_period : time := 100 ns;
    signal palette_write : std_logic := '0';
    signal palette_index : XCM2_WORD := X"00";
    signal palette_rgb : std_logic_vector(23 downto 0) := (others => '0');
    signal palette_ready : std_logic;
    signal atomic_check : boolean := false;
    signal atomic_old_seen, atomic_new_seen : boolean := false;
    signal atomic_old, atomic_new : std_logic_vector(23 downto 0) := x"000000";
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
    clk <= not clk after pixel_period / 2 when not finished else '0';

    command_clk <= not command_clk after command_period / 2 when not finished else '0';

    dut : entity work.fridge_gpu
        port map (
            CLK => clk,
            COMMAND_CLK => command_clk,
            RESET => reset,
            COMMAND_RESET => command_reset,
            FRAME_STORE => frame_store,
            FRAME_ADDR => frame_addr,
            FRAME_DATA => frame_data,
            PRESENT_TRIGGER => present_trigger,
            PRESENT_MODE => present_mode,
            FRAME_OFFSET => frame_offset,
            MODE_SWITCH => mode_switch,
            PALETTE_WRITE => palette_write,
            PALETTE_INDEX => palette_index,
            PALETTE_RGB => palette_rgb,
            PALETTE_READY => palette_ready,
            RED => red,
            GREEN => green,
            BLUE => blue,
            HSYNC => hs,
            VSYNC => vs,
            ACTIVE => act,
            PIXEL_X => px,
            PIXEL_Y => py);


    watchdog : process
    begin
        wait for 1 sec;
        assert finished report "palette test timed out" severity failure;
        wait;
    end process;

    atomic_monitor : process (clk)
    begin
        if falling_edge(clk) and atomic_check and px >= 160 and px < 1120 and
           py >= 40 and py < 44 then
            assert red & green & blue = atomic_old or red & green & blue = atomic_new
                report "torn or unexpected palette RGB" severity failure;
            if red & green & blue = atomic_old then atomic_old_seen <= true; end if;
            if red & green & blue = atomic_new then atomic_new_seen <= true; end if;
        end if;
    end process;

    stimulus : process
        type colors_t is array (0 to 15) of std_logic_vector(23 downto 0);
        variable colors : colors_t;

        procedure write_byte(addr, val : integer) is
        begin
            wait until falling_edge(command_clk);
            frame_store <= '1';
            frame_addr <= to_unsigned(addr,16);
            frame_data <= to_unsigned(val,8);
            wait until falling_edge(command_clk);
            frame_store <= '0';
        end procedure;

        procedure vmode(text : std_logic) is
        begin
            wait until falling_edge(command_clk);
            mode_switch <= '1' & text;
            wait until falling_edge(command_clk);
            mode_switch <= "00";
        end procedure;

        procedure present is
        begin
            wait until falling_edge(command_clk);
            present_mode <= X"05";  -- visible=active=1
            present_trigger <= '1';
            wait until falling_edge(command_clk);
            present_trigger <= '0';
        end procedure;

        procedure set_color(idx : integer; color : std_logic_vector(23 downto 0)) is
        begin
            assert palette_ready = '1' report "mailbox not ready before request" severity failure;
            wait until falling_edge(command_clk);
            palette_index <= to_unsigned(idx,8);
            palette_rgb <= color;
            palette_write <= '1';
            wait until falling_edge(command_clk);
            palette_write <= '0';
            -- The acceptance edge has passed; a valid request must go busy.
            if idx < 16 then
                assert palette_ready = '0' report "palette request was not accepted" severity failure;
                -- Change the input payload immediately: the accepted value
                -- must be held in the mailbox, independently of these pins.
                palette_index <= X"00";
                palette_rgb <= x"ABCDEF";
                wait until palette_ready = '1' for 10 us;
                assert palette_ready = '1' report "palette acknowledgement timed out" severity failure;
            else
                assert palette_ready = '1' report "invalid palette index accepted" severity failure;
            end if;
            wait until falling_edge(command_clk);
        end procedure;

        procedure check_rgb(ex,ey : integer; color : std_logic_vector(23 downto 0)) is
        begin
            loop
                wait until falling_edge(clk);
                exit when px = ex and py = ey;
            end loop;
            assert red & green & blue = color
                report "palette pixel mismatch at " & integer'image(ex) & "," & integer'image(ey)
                severity failure;
        end procedure;

        procedure next_frame is
        begin
            loop
                wait until falling_edge(clk);
                exit when px = 0 and py = 0;
            end loop;
        end procedure;
    begin
        wait for 1 us;
        wait until falling_edge(command_clk);
        reset <= '0'; command_reset <= '0';
        wait until falling_edge(command_clk);
        for i in 0 to 15 loop
            colors(i) := std_logic_vector(to_unsigned(16+i*11,8)) &
                         std_logic_vector(to_unsigned(240-i*7,8)) &
                         std_logic_vector(to_unsigned(8+i*13,8));
            set_color(i,colors(i));
        end loop;
        -- Every invalid full-byte index must be ignored; this includes 86
        -- and 255, which expose the emulator's 8-bit 3*A overflow.
        for i in 16 to 255 loop
            set_color(i,x"ABCDEF");
        end loop;
        vmode('0');
        for i in 0 to 7 loop
            write_byte(i, (2*i)*16 + (2*i+1));
        end loop;
        present;
        next_frame;
        for i in 0 to 15 loop
            check_rgb(160+i*4,40,colors(i));
        end loop;
        check_rgb(1120,40,x"0000FF");
        check_rgb(1280,40,x"000000");
        report "PASS: all 16 RGB888 entries, EGA nibble order, all 240 invalid indices, margins and blanking" severity note;

        vmode('1');
        write_byte(0,16#41#); write_byte(1,16#1F#);
        next_frame;
        check_rgb(160,40,colors(1)); -- 'A' col0 row0 clear
        check_rgb(164,40,colors(15)); -- 'A' col1 row0 set
        set_color(1,x"123456");
        set_color(15,x"FEDCBA");
        check_rgb(160,40,x"123456");
        check_rgb(164,40,x"FEDCBA");
        report "PASS: TEXT foreground/background share the EGA palette; live updates need no VPRE" severity note;

        -- Stress the mailbox with a pixel clock slower than the command
        -- clock: attempts made while busy must not overwrite held RGB/index.
        pixel_period <= 230 ns; command_period <= 31 ns;
        wait for 1 us;
        wait until falling_edge(command_clk);
        palette_index <= X"0F"; palette_rgb <= x"A15C37"; palette_write <= '1';
        wait until falling_edge(command_clk);
        palette_write <= '0';
        assert palette_ready = '0' report "slow receiver did not apply backpressure" severity failure;
        wait until falling_edge(command_clk);
        palette_index <= X"01"; palette_rgb <= x"BADBAD"; palette_write <= '1';
        wait until falling_edge(command_clk);
        palette_write <= '0';
        wait until palette_ready = '1' for 10 us;
        assert palette_ready = '1' report "slow receiver acknowledgement failed" severity failure;
        wait until falling_edge(command_clk);
        -- A held strobe is accepted once even after the acknowledgement.
        palette_index <= X"0F"; palette_rgb <= x"E31B75"; palette_write <= '1';
        wait until falling_edge(command_clk);
        palette_rgb <= x"BADBAD";
        wait until palette_ready = '1' for 10 us;
        assert palette_ready = '1' severity failure;
        for i in 0 to 100 loop
            wait until falling_edge(command_clk);
            assert palette_ready = '1' report "held strobe repeated a palette write" severity failure;
        end loop;
        palette_write <= '0';
        pixel_period <= 13468 ps; command_period <= 100 ns;
        wait for 1 us;
        check_rgb(160,40,x"123456");
        check_rgb(164,40,x"E31B75");
        report "PASS: asynchronous backpressure, busy rejection, payload hold and sustained strobe" severity note;

        -- Exercise atomic RGB updates during an active EGA row.
        vmode('0');
        for i in 0 to 119 loop write_byte(i,16#FF#); end loop;
        atomic_old <= x"E31B75"; atomic_new <= x"19D46B";
        next_frame;
        check_rgb(160,40,x"E31B75");
        atomic_check <= true;
        set_color(15,x"19D46B");
        check_rgb(1100,40,x"19D46B");
        check_rgb(160,41,x"19D46B");
        assert atomic_old_seen and atomic_new_seen
            report "atomic monitor did not observe both sides of the active-row update" severity failure;
        atomic_check <= false;
        report "PASS: live RGB update remains atomic on active pixels" severity note;

        -- Reset both clock domains with a transaction in flight. The board
        -- uses a common reset assertion and independently synchronized release.
        pixel_period <= 230 ns; command_period <= 31 ns;
        wait for 1 us;
        wait until falling_edge(command_clk);
        palette_index <= X"00"; palette_rgb <= x"FFFFFF"; palette_write <= '1';
        wait until falling_edge(command_clk);
        assert palette_ready = '0' severity failure;
        reset <= '1'; command_reset <= '1'; palette_write <= '0';
        wait for 2 us;
        pixel_period <= 13468 ps; command_period <= 100 ns;
        wait for 1 us;
        reset <= '0'; command_reset <= '0';
        wait for 1 us;
        assert palette_ready = '1' report "mailbox did not recover after reset" severity failure;
        vmode('0'); present;
        for i in 0 to 7 loop
            write_byte(i, (2*i)*16 + (2*i+1));
        end loop;
        next_frame;
        for i in 0 to 15 loop
            check_rgb(160+i*4,40,PALETTE(i));
        end loop;
        set_color(0,x"13579B");
        check_rgb(160,40,x"13579B");
        report "PASS: reset restores the entire default palette, discards in-flight work and permits new writes" severity note;
        report "PASS: programmable palette/CDC tests" severity note;
        finished <= true;
        wait;
    end process;
end sim;
