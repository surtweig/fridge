library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library unisim;
use unisim.vcomponents.all;
use work.FridgeRAMBootImage.all;
use work.FridgeROMImage.all;

entity atlys_top is
    port (clk, reset_button, ps2_clk, ps2_dat : in std_logic;
          led : out std_logic_vector(5 downto 0);
          tmds_p, tmds_n : out std_logic_vector(2 downto 0);
          tmds_clk_p, tmds_clk_n : out std_logic);
end atlys_top;
architecture rtl of atlys_top is
    signal reference_clk, cpu_clk, pixel_clk, double_clk, io_clk, strobe : std_logic;
    signal cpu_reset, pixel_reset, double_reset, locked : std_logic;
    signal cpu_halted, rom_error, kbd_overflow, kbd_error, kbd_activity : std_logic;
    signal red, green, blue : std_logic_vector(7 downto 0);
    signal hs, vs, active : std_logic;
    signal blue_symbol, green_symbol, red_symbol : std_logic_vector(9 downto 0);
    signal blue_control : std_logic_vector(1 downto 0);
    signal forwarded_clock, pixel_clk_n : std_logic;
    signal heartbeat_cnt : unsigned(25 downto 0) := (others => '0');
    signal heartbeat : std_logic := '0';
begin
    pixel_clk_n <= not pixel_clk;
    blue_control <= vs & hs;
    clocking : entity work.atlys_clocks
        port map (clk => clk, reset_button => reset_button,
                  reference_clk => reference_clk, cpu_clk => cpu_clk,
                  pixel_clk => pixel_clk, double_clk => double_clk,
                  io_clk => io_clk, strobe => strobe,
                  cpu_reset => cpu_reset, pixel_reset => pixel_reset,
                  double_reset => double_reset, locked => locked);
    system : entity work.fridge_system
        generic map (BOOT_IMAGE => RAMBootImage, ROM_INIT => ROM_IMAGE)
        port map (cpu_clk => cpu_clk, pixel_clk => pixel_clk,
                  cpu_reset => cpu_reset, pixel_reset => pixel_reset,
                  ps2_clk => ps2_clk, ps2_dat => ps2_dat,
                  RED => red, GREEN => green, BLUE => blue,
                  HSYNC => hs, VSYNC => vs, ACTIVE => active,
                  PIXEL_X => open, PIXEL_Y => open, HALTED => cpu_halted,
                  ROM_ERROR => rom_error, KBD_OVERFLOW => kbd_overflow,
                  KBD_ERROR => kbd_error, KBD_ACTIVITY => kbd_activity, DEBUG => open);
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

    process (reference_clk)
    begin
        if rising_edge(reference_clk) then
            heartbeat_cnt <= heartbeat_cnt + 1;
            if heartbeat_cnt = 0 then
                heartbeat <= not heartbeat;
            end if;
        end if;
    end process;

    led(0) <= cpu_halted;
    led(1) <= heartbeat;
    led(2) <= locked;
    led(3) <= rom_error;
    led(4) <= kbd_overflow or kbd_error;
    led(5) <= kbd_activity;
end rtl;
