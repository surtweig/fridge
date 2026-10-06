library ieee;
use ieee.std_logic_1164.all;

entity video_timing is
    port (
        clk, reset : in std_logic;
        x : out integer range 0 to 1649;
        y : out integer range 0 to 749;
        hsync, vsync, active : out std_logic
    );
end video_timing;

architecture rtl of video_timing is
    signal x_r : integer range 0 to 1649 := 0;
    signal y_r : integer range 0 to 749 := 0;
begin
    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                x_r <= 0;
                y_r <= 0;
            elsif x_r = 1649 then
                x_r <= 0;
                if y_r = 749 then
                    y_r <= 0;
                else
                    y_r <= y_r + 1;
                end if;
            else
                x_r <= x_r + 1;
            end if;
        end if;
    end process;

    x <= x_r;
    y <= y_r;

    -- CEA 720p60: 1280 + 110 + 40 + 220, 720 + 5 + 5 + 20. Positive sync.
    -- Raster numbers copied from ../hdmi/video_pattern.vhd (standalone demo).
    active <= '1' when x_r < 1280 and y_r < 720 and reset = '0' else '0';
    hsync <= '1' when x_r >= 1390 and x_r < 1430 else '0';
    vsync <= '1' when y_r >= 725 and y_r < 730 else '0';
end rtl;
