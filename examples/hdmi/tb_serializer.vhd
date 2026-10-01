library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library unisim;
use unisim.vcomponents.all;

entity tb_serializer is end tb_serializer;

architecture test of tb_serializer is
    signal pixel_clk, double_clk, serial_clk : std_logic := '0';
    signal io_clk, strobe, locked, output_p, output_n : std_logic;
    signal reset : std_logic := '1';
    signal finished : boolean := false;
    signal symbol : std_logic_vector(9 downto 0) := "1010010110";
    function test_word(index : integer) return std_logic_vector is
    begin
        return std_logic_vector(to_unsigned((index * 73 + 337) mod 1024, 10));
    end;
begin
    process
    begin
        wait for 5 ns;
        while not finished loop
            pixel_clk <= '1'; wait for 10 ns;
            pixel_clk <= '0'; wait for 10 ns;
        end loop;
        wait;
    end process;
    double_clk <= not double_clk after 5 ns when not finished else '0';
    serial_clk <= not serial_clk after 1 ns when not finished else '0';
    buffer_io : BUFPLL generic map (DIVIDE => 5)
        port map (PLLIN => serial_clk, GCLK => double_clk, LOCKED => '1',
                  IOCLK => io_clk, SERDESSTROBE => strobe, LOCK => locked);
    dut : entity work.tmds_serializer
        port map (double_clk, io_clk, strobe, reset, symbol, output_p, output_n);

    process
    begin
        wait for 300 ns;
        wait until falling_edge(double_clk);
        reset <= '0';
        wait for 700 ns;
        for i in 0 to 255 loop
            wait until rising_edge(pixel_clk);
            symbol <= test_word(i);
        end loop;
        wait;
    end process;

    process
        variable window : std_logic_vector(19 downto 0) := (others => '0');
        variable word : std_logic_vector(9 downto 0);
        variable aligned : boolean := false;
    begin
        wait for 500 ns;
        -- Find two consecutive known words without assuming OSERDES latency.
        while not aligned loop
            wait until falling_edge(serial_clk);
            wait for 100 ps;
            assert output_n = not output_p report "Differential polarity mismatch" severity failure;
            window := output_p & window(19 downto 1);
            aligned := window(9 downto 0) = test_word(0) and window(19 downto 10) = test_word(1);
            assert now < 3 us report "Serializer word/bit alignment not found" severity failure;
        end loop;
        for i in 2 to 127 loop
            for bit_index in 0 to 9 loop
                wait until falling_edge(serial_clk); wait for 100 ps;
                word(bit_index) := output_p;
                assert output_n = not output_p report "Differential polarity mismatch" severity failure;
            end loop;
            assert word = test_word(i) report "Serializer reordered or corrupted a word" severity failure;
        end loop;
        report "PASS: OSERDES2/BUFPLL serialized 128 consecutive words LSB-first" severity note;
        finished <= true;
        wait;
    end process;
end test;
