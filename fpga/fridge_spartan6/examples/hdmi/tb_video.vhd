library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity tb_video is end tb_video;

architecture test of tb_video is
    signal clk : std_logic := '0';
    signal finished : boolean := false;
    signal reset : std_logic := '1';
    signal red, green, blue : std_logic_vector(7 downto 0);
    signal hs, vs, active : std_logic;
    signal test_data : std_logic_vector(7 downto 0) := (others => '0');
    signal test_control : std_logic_vector(1 downto 0) := "00";
    signal test_active : std_logic := '0';
    signal symbol : std_logic_vector(9 downto 0);
    type controls is array (0 to 3) of std_logic_vector(9 downto 0);
    constant control_symbols : controls := ("1101010100", "0010101011", "0101010100", "1010101011");
begin
    clk <= not clk after 5 ns when not finished else '0';
    pattern : entity work.video_pattern port map (clk, reset, red, green, blue, hs, vs, active);
    encoder : entity work.tmds_encoder port map (clk, reset, test_active, test_data, test_control, symbol);

    process
        variable expected_active, expected_rectangle, expected_hs, expected_vs : boolean;
        variable white_count, blue_count, active_count : integer := 0;
    begin
        wait until falling_edge(clk);
        reset <= '0';
        -- At this edge the reset-held raster is still at (0,0).
        for y in 0 to 749 loop
            for x in 0 to 1649 loop
                wait for 1 ns;
                expected_active := x < 1280 and y < 720;
                expected_rectangle := x >= 160 and x < 1120 and y >= 40 and y < 680;
                expected_hs := x >= 1390 and x < 1430;
                expected_vs := y >= 725 and y < 730;
                assert (active = '1') = expected_active report "Active area mismatch" severity failure;
                assert (hs = '1') = expected_hs report "HSYNC mismatch" severity failure;
                assert (vs = '1') = expected_vs report "VSYNC mismatch" severity failure;
                if expected_active then
                    active_count := active_count + 1;
                    assert blue = X"FF" report "Blue component mismatch" severity failure;
                    if expected_rectangle then
                        white_count := white_count + 1;
                        assert red = X"FF" and green = X"FF" report "Rectangle mismatch" severity failure;
                    else
                        blue_count := blue_count + 1;
                        assert red = X"00" and green = X"00" report "Background mismatch" severity failure;
                    end if;
                else
                    assert red = X"00" and green = X"00" and blue = X"00"
                        report "Blanking RGB mismatch" severity failure;
                end if;
                wait until falling_edge(clk);
            end loop;
        end loop;
        wait for 1 ns;
        assert active = '1' and red = X"00" and blue = X"FF" report "Frame did not wrap" severity failure;
        assert active_count = 921600 and white_count = 614400 and blue_count = 307200
            report "Pixel count mismatch" severity failure;
        report "PASS: full 1650x750 raster, sync, margins, colors and frame wrap" severity note;
        finished <= true;
        wait;
    end process;

    process
        variable q, decoded : std_logic_vector(7 downto 0);
        variable ones, running_balance : integer := 0;
    begin
        wait until falling_edge(clk);
        for c in 0 to 3 loop
            test_control <= std_logic_vector(to_unsigned(c, 2));
            wait until rising_edge(clk); wait for 1 ns;
            assert symbol = control_symbols(c) report "Control symbol mismatch" severity failure;
            wait until falling_edge(clk);
        end loop;
        test_active <= '1';
        -- Exercise all byte values repeatedly, including long solid runs.
        for pass in 0 to 7 loop
            for value in 0 to 255 loop
                for repeat in 0 to 15 loop
                    test_data <= std_logic_vector(to_unsigned(value, 8));
                    wait until rising_edge(clk); wait for 1 ns;
                    q := symbol(7 downto 0);
                    if symbol(9) = '1' then q := not q; end if;
                    decoded(0) := q(0);
                    for i in 1 to 7 loop
                        if symbol(8) = '1' then decoded(i) := q(i) xor q(i-1);
                        else decoded(i) := q(i) xnor q(i-1); end if;
                    end loop;
                    assert decoded = test_data report "TMDS decode mismatch" severity failure;
                    ones := 0;
                    for i in 0 to 9 loop
                        if symbol(i) = '1' then ones := ones + 1; end if;
                    end loop;
                    running_balance := running_balance + 2 * ones - 10;
                    assert abs(running_balance) <= 8 report "TMDS DC balance exceeded" severity failure;
                    wait until falling_edge(clk);
                end loop;
            end loop;
        end loop;
        test_active <= '0'; test_control <= "00";
        wait until rising_edge(clk); wait for 1 ns;
        assert symbol = control_symbols(0) report "Blanking did not restore control" severity failure;
        report "PASS: TMDS controls, 32768 data symbols decoded and DC balance checked" severity note;
        wait;
    end process;
end test;
