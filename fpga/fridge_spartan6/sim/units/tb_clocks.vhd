library ieee;
use ieee.std_logic_1164.all;

entity tb_clocks is end tb_clocks;
architecture sim of tb_clocks is
    signal clk : std_logic := '0';
    signal reset_button : std_logic := '0';
    signal reference_clk, cpu_clk, pixel_clk, double_clk, io_clk, strobe : std_logic;
    signal cpu_reset, pixel_reset, double_reset, locked : std_logic;
    signal finished : boolean := false;
begin
    clk <= not clk after 5 ns when not finished else '0';
    dut : entity work.atlys_clocks
        port map (clk, reset_button, reference_clk, cpu_clk, pixel_clk,
                  double_clk, io_clk, strobe, cpu_reset, pixel_reset, double_reset, locked);
    watchdog : process
    begin
        wait for 1 ms;
        assert finished report "clock test timeout" severity failure;
        wait;
    end process;
    stimulus : process
        variable previous : time;
    begin
        wait for 200 ns;
        assert cpu_reset = '1' and pixel_reset = '1' and double_reset = '1'
            report "active-low button did not hold reset" severity failure;
        reset_button <= '1';
        wait until locked = '1' for 100 us;
        assert locked = '1' report "clock chain did not lock" severity failure;
        wait for 2 us;
        assert cpu_reset = '0' and pixel_reset = '0' and double_reset = '0'
            report "synchronised reset release failed" severity failure;
        wait until rising_edge(cpu_clk);
        previous := now;
        for i in 1 to 16 loop
            wait until rising_edge(cpu_clk);
            assert now - previous > 99 ns and now - previous < 101 ns
                report "CPU clock is not 10 MHz" severity failure;
            previous := now;
        end loop;
        wait until rising_edge(pixel_clk);
        previous := now;
        for i in 1 to 128 loop
            wait until rising_edge(pixel_clk);
            assert now - previous > 13400 ps and now - previous < 13500 ps
                report "pixel clock is not 74.25 MHz" severity failure;
            previous := now;
        end loop;
        reset_button <= '0';
        wait for 200 ns;
        assert cpu_reset = '1' and pixel_reset = '1' and double_reset = '1'
            report "warm reset did not assert in all domains" severity failure;
        reset_button <= '1';
        wait until locked = '1' for 100 us;
        wait for 2 us;
        assert locked = '1' and cpu_reset = '0' and pixel_reset = '0'
            report "clock chain failed to recover from reset" severity failure;
        report "PASS: Atlys clocks and reset" severity note;
        finished <= true;
        wait;
    end process;
end sim;
