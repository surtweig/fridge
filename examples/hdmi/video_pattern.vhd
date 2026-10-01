library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity video_pattern is
    port (
        clk, reset : in std_logic;
        red, green, blue : out std_logic_vector(7 downto 0);
        hsync, vsync, active : out std_logic
    );
end video_pattern;

architecture rtl of video_pattern is
    signal x : integer range 0 to 1649 := 0;
    signal y : integer range 0 to 749 := 0;
    signal visible, rectangle : std_logic;
begin
    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                x <= 0;
                y <= 0;
            elsif x = 1649 then
                x <= 0;
                if y = 749 then y <= 0; else y <= y + 1; end if;
            else
                x <= x + 1;
            end if;
        end if;
    end process;

    -- CEA 720p60: 1280 + 110 + 40 + 220, 720 + 5 + 5 + 20.
    visible <= '1' when x < 1280 and y < 720 and reset = '0' else '0';
    rectangle <= '1' when x >= 160 and x < 1120 and y >= 40 and y < 680 else '0';
    active <= visible;
    hsync <= '1' when x >= 1390 and x < 1430 else '0';
    vsync <= '1' when y >= 725 and y < 730 else '0';
    red <= X"FF" when visible = '1' and rectangle = '1' else X"00";
    green <= X"FF" when visible = '1' and rectangle = '1' else X"00";
    blue <= X"FF" when visible = '1' else X"00";
end rtl;
