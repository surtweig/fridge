library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library unisim;
use unisim.vcomponents.all;

entity atlys_clocks is
    port (clk, reset_button : in std_logic;
          reference_clk, cpu_clk, pixel_clk, double_clk, io_clk, strobe : out std_logic;
          cpu_reset, pixel_reset, double_reset, locked : out std_logic);
end atlys_clocks;
architecture rtl of atlys_clocks is
    signal reference_clk_i, dcm_clk, cpu_clk_raw, cpu_clk_i, feedback : std_logic;
    signal pixel_raw, double_raw, serial_raw : std_logic;
    signal pixel_clk_i, double_clk_i, io_clk_i, strobe_i : std_logic;
    signal dcm_locked, cpu_dcm_locked, pll_locked, io_locked : std_logic;
    signal startup : unsigned(7 downto 0) := (others => '0');
    signal button_sync : std_logic_vector(1 downto 0) := (others => '0');
    signal dcm_reset : std_logic := '1';
    signal pixel_release, double_release, cpu_release : std_logic_vector(3 downto 0) := (others => '1');
    signal pixel_reset_i, double_reset_i, cpu_reset_i, clock_fault : std_logic;
    signal pixel_clk_n, pll_reset : std_logic;
    attribute ASYNC_REG : string;
    attribute ASYNC_REG of button_sync, pixel_release, double_release, cpu_release : signal is "TRUE";
    attribute SHREG_EXTRACT : string;
    attribute SHREG_EXTRACT of button_sync : signal is "NO";
begin
    pll_reset <= not dcm_locked;
    pixel_clk_n <= not pixel_clk_i;
    reference_buffer : BUFG port map (I => clk, O => reference_clk_i);

    process (reference_clk_i)
    begin
        if rising_edge(reference_clk_i) then
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
        port map (CLKIN => reference_clk_i, RST => dcm_reset,
                  FREEZEDCM => '0', PROGCLK => reference_clk_i, PROGDATA => '0', PROGEN => '0',
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
    pixel_buffer : BUFG port map (I => pixel_raw, O => pixel_clk_i);
    double_buffer : BUFG port map (I => double_raw, O => double_clk_i);
    io_buffer : BUFPLL
        generic map (DIVIDE => 5)
        port map (PLLIN => serial_raw, GCLK => double_clk_i, LOCKED => pll_locked,
                  IOCLK => io_clk_i, SERDESSTROBE => strobe_i, LOCK => io_locked);

    cpu_clock_generator : DCM_CLKGEN
        generic map (CLKFX_MULTIPLY => 2, CLKFX_DIVIDE => 20,
                     CLKIN_PERIOD => 10.0, STARTUP_WAIT => false)
        port map (CLKIN => reference_clk_i, RST => dcm_reset,
                  FREEZEDCM => '0', PROGCLK => reference_clk_i, PROGDATA => '0', PROGEN => '0',
                  CLKFX => cpu_clk_raw, CLKFX180 => open, CLKFXDV => open,
                  LOCKED => cpu_dcm_locked, PROGDONE => open, STATUS => open);
    cpu_buffer : BUFG port map (I => cpu_clk_raw, O => cpu_clk_i);

    clock_fault <= not (dcm_locked and pll_locked and io_locked and cpu_dcm_locked) or dcm_reset;
    process (pixel_clk_i, clock_fault)
    begin
        if clock_fault = '1' then pixel_release <= (others => '1');
        elsif rising_edge(pixel_clk_i) then pixel_release <= pixel_release(2 downto 0) & '0'; end if;
    end process;
    process (double_clk_i, clock_fault)
    begin
        if clock_fault = '1' then double_release <= (others => '1');
        elsif rising_edge(double_clk_i) then double_release <= double_release(2 downto 0) & '0'; end if;
    end process;
    process (cpu_clk_i, clock_fault)
    begin
        if clock_fault = '1' then cpu_release <= (others => '1');
        elsif rising_edge(cpu_clk_i) then cpu_release <= cpu_release(2 downto 0) & '0'; end if;
    end process;
    pixel_reset_i <= pixel_release(3);
    double_reset_i <= double_release(3);
    cpu_reset_i <= cpu_release(3);


    reference_clk <= reference_clk_i;
    cpu_clk <= cpu_clk_i;
    pixel_clk <= pixel_clk_i;
    double_clk <= double_clk_i;
    io_clk <= io_clk_i;
    strobe <= strobe_i;
    cpu_reset <= cpu_reset_i;
    pixel_reset <= pixel_reset_i;
    double_reset <= double_reset_i;
    locked <= dcm_locked and pll_locked and io_locked and cpu_dcm_locked;
end rtl;
