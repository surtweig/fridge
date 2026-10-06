library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library unisim;
use unisim.vcomponents.all;

entity hdmi is
    port (
        clk : in std_logic;
        reset_button : in std_logic;
        led : out std_logic_vector(3 downto 0);
        tmds_p, tmds_n : out std_logic_vector(2 downto 0);
        tmds_clk_p, tmds_clk_n : out std_logic
    );
end hdmi;

architecture rtl of hdmi is
    signal reference_clk, dcm_clk, feedback, pixel_raw, double_raw, serial_raw : std_logic;
    signal pixel_clk, double_clk, io_clk, strobe : std_logic;
    signal dcm_locked, pll_locked, io_locked : std_logic;
    signal startup : unsigned(7 downto 0) := (others => '0');
    signal button_sync : std_logic_vector(1 downto 0) := (others => '0');
    signal dcm_reset : std_logic := '1';
    signal pixel_release, double_release : std_logic_vector(3 downto 0) := (others => '1');
    signal pixel_reset, double_reset, clock_fault : std_logic;
    signal red, green, blue : std_logic_vector(7 downto 0);
    signal hs, vs, active : std_logic;
    signal blue_symbol, green_symbol, red_symbol : std_logic_vector(9 downto 0);
    signal forwarded_clock : std_logic;
    signal frames : unsigned(5 downto 0) := (others => '0');
    signal last_vs : std_logic := '0';
    signal pll_reset, pixel_clk_n : std_logic;
    signal blue_control : std_logic_vector(1 downto 0);
    attribute ASYNC_REG : string;
    attribute ASYNC_REG of button_sync, pixel_release, double_release : signal is "TRUE";
    attribute SHREG_EXTRACT : string;
    attribute SHREG_EXTRACT of button_sync : signal is "NO";
begin
    pll_reset <= not dcm_locked;
    pixel_clk_n <= not pixel_clk;
    blue_control <= vs & hs;
    reference_buffer : BUFG port map (I => clk, O => reference_clk);
    process (reference_clk)
    begin
        if rising_edge(reference_clk) then
            -- The Atlys reset pushbutton is active low (T15 / RESET#).
            button_sync <= button_sync(0) & not reset_button;
            if button_sync(1) = '1' then
                startup <= (others => '0');
                dcm_reset <= '1';
            elsif startup /= 255 then
                startup <= startup + 1;
                dcm_reset <= '1';
            else
                dcm_reset <= '0';
            end if;
        end if;
    end process;

    -- 100 * 99/100 * 15/2 = 742.5 MHz VCO; legal PFD = 49.5 MHz.
    clock_generator : DCM_CLKGEN
        generic map (CLKFX_MULTIPLY => 99, CLKFX_DIVIDE => 100,
                     CLKIN_PERIOD => 10.0, STARTUP_WAIT => false)
        port map (CLKIN => reference_clk, RST => dcm_reset,
                  FREEZEDCM => '0', PROGCLK => reference_clk, PROGDATA => '0', PROGEN => '0',
                  CLKFX => dcm_clk, CLKFX180 => open, CLKFXDV => open,
                  LOCKED => dcm_locked, PROGDONE => open, STATUS => open);
    pixel_pll : PLL_BASE
        generic map (CLKFBOUT_MULT => 15, DIVCLK_DIVIDE => 2,
                     CLKOUT0_DIVIDE => 1, CLKOUT1_DIVIDE => 10, CLKOUT2_DIVIDE => 5,
                     CLKIN_PERIOD => 10.101010101, COMPENSATION => "DCM2PLL")
        port map (CLKIN => dcm_clk, CLKFBIN => feedback, RST => pll_reset,
                  CLKFBOUT => feedback, CLKOUT0 => serial_raw, CLKOUT1 => pixel_raw,
                  CLKOUT2 => double_raw, CLKOUT3 => open, CLKOUT4 => open,
                  CLKOUT5 => open, LOCKED => pll_locked);
    pixel_buffer : BUFG port map (I => pixel_raw, O => pixel_clk);
    double_buffer : BUFG port map (I => double_raw, O => double_clk);
    io_buffer : BUFPLL
        generic map (DIVIDE => 5)
        port map (PLLIN => serial_raw, GCLK => double_clk, LOCKED => pll_locked,
                  IOCLK => io_clk, SERDESSTROBE => strobe, LOCK => io_locked);

    clock_fault <= not (dcm_locked and pll_locked and io_locked) or dcm_reset;
    process (pixel_clk, clock_fault)
    begin
        if clock_fault = '1' then pixel_release <= (others => '1');
        elsif rising_edge(pixel_clk) then pixel_release <= pixel_release(2 downto 0) & '0'; end if;
    end process;
    process (double_clk, clock_fault)
    begin
        if clock_fault = '1' then double_release <= (others => '1');
        elsif rising_edge(double_clk) then double_release <= double_release(2 downto 0) & '0'; end if;
    end process;
    pixel_reset <= pixel_release(3);
    double_reset <= double_release(3);
    pattern : entity work.video_pattern
        port map (pixel_clk, pixel_reset, red, green, blue, hs, vs, active);
    blue_encoder : entity work.tmds_encoder
        port map (pixel_clk, pixel_reset, active, blue, blue_control, blue_symbol);
    green_encoder : entity work.tmds_encoder
        port map (pixel_clk, pixel_reset, active, green, "00", green_symbol);
    red_encoder : entity work.tmds_encoder
        port map (pixel_clk, pixel_reset, active, red, "00", red_symbol);
    blue_output : entity work.tmds_serializer
        port map (double_clk, io_clk, strobe, double_reset, blue_symbol, tmds_p(0), tmds_n(0));
    green_output : entity work.tmds_serializer
        port map (double_clk, io_clk, strobe, double_reset, green_symbol, tmds_p(1), tmds_n(1));
    red_output : entity work.tmds_serializer
        port map (double_clk, io_clk, strobe, double_reset, red_symbol, tmds_p(2), tmds_n(2));
    clock_forwarder : ODDR2
        generic map (DDR_ALIGNMENT => "NONE")
        port map (Q => forwarded_clock, C0 => pixel_clk, C1 => pixel_clk_n,
                  CE => '1', D0 => '1', D1 => '0', R => '0', S => '0');
    clock_output : OBUFDS
        generic map (IOSTANDARD => "TMDS_33")
        port map (I => forwarded_clock, O => tmds_clk_p, OB => tmds_clk_n);

    process (pixel_clk)
    begin
        if rising_edge(pixel_clk) then
            if pixel_reset = '1' then frames <= (others => '0'); last_vs <= '0';
            else
                last_vs <= vs;
                if vs = '1' and last_vs = '0' then frames <= frames + 1; end if;
            end if;
        end if;
    end process;
    led <= frames(5) & io_locked & pll_locked & dcm_locked;
end rtl;
