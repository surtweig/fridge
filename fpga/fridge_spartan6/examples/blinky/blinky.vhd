library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity blinky is
    port (
        clk : in  std_logic;
        led : out std_logic_vector(7 downto 0)
    );
end entity blinky;

architecture rtl of blinky is
    signal counter : unsigned(26 downto 0) := (others => '0');
begin
    process (clk)
    begin
        if rising_edge(clk) then
            counter <= counter + 1;
        end if;
    end process;

    -- 100 MHz / 2^27 -> LED pattern changes at ~0.75 Hz
    led <= std_logic_vector(counter(26 downto 19));
end architecture rtl;
