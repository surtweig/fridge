-- OSERDES2 cascade wiring adapted from LiteVideo's Spartan-6 HDMI PHY.
-- See LICENSE.litevideo for the original copyright and BSD-2-Clause terms.
library ieee;
use ieee.std_logic_1164.all;
library unisim;
use unisim.vcomponents.all;

entity tmds_serializer is
    port (
        double_clk, io_clk, strobe, reset : in std_logic;
        symbol : in std_logic_vector(9 downto 0);
        output_p, output_n : out std_logic
    );
end tmds_serializer;

architecture rtl of tmds_serializer is
    signal half : std_logic_vector(4 downto 0) := (others => '0');
    signal upper_half : std_logic_vector(4 downto 0) := (others => '0');
    signal phase : std_logic := '0';
    signal cascade_data_in, cascade_data_out, cascade_tri_in, cascade_tri_out : std_logic;
    signal serial : std_logic;
begin
    -- Capture a complete word every other 2x edge, then emit its low/high
    -- halves. Holding the upper half preserves word boundaries regardless of
    -- which related 2x edge follows reset release. No clock is used as data.
    process (double_clk)
    begin
        if rising_edge(double_clk) then
            if reset = '1' then
                half <= (others => '0');
                upper_half <= (others => '0');
                phase <= '0';
            else
                phase <= not phase;
                if phase = '0' then
                    half <= symbol(4 downto 0);
                    upper_half <= symbol(9 downto 5);
                else
                    half <= upper_half;
                end if;
            end if;
        end if;
    end process;

    master : OSERDES2
        generic map (DATA_WIDTH => 5, DATA_RATE_OQ => "SDR", DATA_RATE_OT => "SDR",
                     SERDES_MODE => "MASTER", OUTPUT_MODE => "SINGLE_ENDED")
        port map (
            OQ => serial, SHIFTOUT1 => cascade_data_in, SHIFTOUT2 => cascade_tri_in,
            SHIFTOUT3 => open, SHIFTOUT4 => open, TQ => open,
            CLK0 => io_clk, CLK1 => '0', CLKDIV => double_clk,
            IOCE => strobe, OCE => '1', RST => reset,
            D1 => half(4), D2 => '0', D3 => '0', D4 => '0',
            SHIFTIN1 => '1', SHIFTIN2 => '1',
            SHIFTIN3 => cascade_data_out, SHIFTIN4 => cascade_tri_out,
            T1 => '0', T2 => '0', T3 => '0', T4 => '0', TCE => '1', TRAIN => '0');
    slave : OSERDES2
        generic map (DATA_WIDTH => 5, DATA_RATE_OQ => "SDR", DATA_RATE_OT => "SDR",
                     SERDES_MODE => "SLAVE", OUTPUT_MODE => "SINGLE_ENDED")
        port map (
            OQ => open, SHIFTOUT1 => open, SHIFTOUT2 => open,
            SHIFTOUT3 => cascade_data_out, SHIFTOUT4 => cascade_tri_out, TQ => open,
            CLK0 => io_clk, CLK1 => '0', CLKDIV => double_clk,
            IOCE => strobe, OCE => '1', RST => reset,
            D1 => half(0), D2 => half(1), D3 => half(2), D4 => half(3),
            SHIFTIN1 => cascade_data_in, SHIFTIN2 => cascade_tri_in,
            SHIFTIN3 => '1', SHIFTIN4 => '1',
            T1 => '0', T2 => '0', T3 => '0', T4 => '0', TCE => '1', TRAIN => '0');
    output_buffer : OBUFDS
        generic map (IOSTANDARD => "TMDS_33")
        port map (I => serial, O => output_p, OB => output_n);
end rtl;
