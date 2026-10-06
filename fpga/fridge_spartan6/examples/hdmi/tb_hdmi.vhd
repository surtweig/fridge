library ieee;
use ieee.std_logic_1164.all;

entity tb_hdmi is end tb_hdmi;

architecture test of tb_hdmi is
    signal clk : std_logic := '0';
    signal finished : boolean := false;
    signal led : std_logic_vector(3 downto 0);
    signal p, n : std_logic_vector(2 downto 0);
    signal clock_p, clock_n : std_logic;
begin
    clk <= not clk after 5 ns when not finished else '0';
    dut : entity work.hdmi port map (clk, '1', led, p, n, clock_p, clock_n);
    process
        variable last_edge : time;
    begin
        wait until led(2 downto 0) = "111";
        wait for 1 us;
        wait until rising_edge(clock_p);
        last_edge := now;
        for i in 0 to 99 loop
            wait until rising_edge(clock_p);
            assert now - last_edge >= 13460 ps and now - last_edge <= 13480 ps
                report "Pixel clock is not 74.25 MHz" severity failure;
            assert clock_n = '0' report "Forwarded clock polarity mismatch" severity failure;
            last_edge := now;
        end loop;
        report "PASS: DCM/PLL/BUFPLL lock and 74.25 MHz forwarded pixel clock" severity note;
        finished <= true;
        wait;
    end process;
    process
    begin
        wait for 100 us;
        assert finished report "HDMI clock chain failed to lock" severity failure;
        wait;
    end process;
end test;
