library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library unisim;
use unisim.vcomponents.all;
use work.FridgeGlobals.all;
use work.FridgeRAMBootImage.all;

entity palette is
    port (
        clk : in std_logic;
        reset_button : in std_logic;
        led : out std_logic_vector(3 downto 0);
        tmds_p, tmds_n : out std_logic_vector(2 downto 0);
        tmds_clk_p, tmds_clk_n : out std_logic
    );
end palette;

architecture rtl of palette is
    signal reference_clk, dcm_clk, cpu_clk_raw, cpu_clk, feedback : std_logic;
    signal pixel_raw, double_raw, serial_raw : std_logic;
    signal pixel_clk, double_clk, io_clk, strobe : std_logic;
    signal dcm_locked, cpu_dcm_locked, pll_locked, io_locked : std_logic;
    signal startup : unsigned(7 downto 0) := (others => '0');
    signal button_sync : std_logic_vector(1 downto 0) := (others => '0');
    signal dcm_reset : std_logic := '1';
    signal pixel_release, double_release, cpu_release : std_logic_vector(3 downto 0) := (others => '1');
    signal pixel_reset, double_reset, cpu_reset, clock_fault : std_logic;
    signal red, green, blue : std_logic_vector(7 downto 0);
    signal hs, vs, active : std_logic;
    signal blue_symbol, green_symbol, red_symbol : std_logic_vector(9 downto 0);
    signal forwarded_clock, pixel_clk_n, pll_reset : std_logic;
    signal frames : unsigned(5 downto 0) := (others => '0');
    signal last_vs : std_logic := '0';
    signal blue_control : std_logic_vector(1 downto 0);

    signal cpu_halted, cpu_inte : std_logic;
    signal cpu_int : std_logic := '0';
    signal cpu_int_irq : XCM2_WORD := (others => '0');
    signal device_sel, device_data : XCM2_WORD;
    signal device_read : std_logic;

    signal ram_write_data, ram_read_data : XCM2_WORD;
    signal ram_write_addr, ram_read_addr : XCM2_DWORD;
    signal ram_write_enabled : std_logic;

    signal gpu_mode_switch : std_logic_vector(0 to 1);
    signal gpu_palette_switch, gpu_present_trigger : std_logic;
    signal gpu_palette_index : XCM2_WORD;
    signal gpu_palette_rgb : std_logic_vector(23 downto 0);
    signal gpu_palette_ready : std_logic;
    signal gpu_present_mode : XCM2_WORD;
    signal gpu_frame_offset : XCM2_DWORD;
    signal gpu_back_store, gpu_back_load, gpu_back_clr : std_logic;
    signal gpu_back_addr, gpu_vmem_addr : XCM2_DWORD;
    signal gpu_back_data, gpu_vmem_data : XCM2_WORD;
    signal gpu_vmem_store, gpu_vmem_load : std_logic;

    signal pam16_cmd_enabled, pam16_cmd_ready : std_logic;
    signal pam16_data_write, pam16_data_read : XCM2_DWORD := (others => '0');
    signal pam16_cmd_code : PAM16_COMMAND;

    signal debug_state, debug_ir : XCM2_WORD;
    signal debug_pc : XCM2_DWORD;

    signal heartbeat_cnt : unsigned(25 downto 0) := (others => '0');
    signal heartbeat : std_logic := '0';

    attribute ASYNC_REG : string;
    attribute ASYNC_REG of button_sync, pixel_release, double_release, cpu_release : signal is "TRUE";
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

    cpu_clock_generator : DCM_CLKGEN
        generic map (CLKFX_MULTIPLY => 2, CLKFX_DIVIDE => 20,
                     CLKIN_PERIOD => 10.0, STARTUP_WAIT => false)
        port map (CLKIN => reference_clk, RST => dcm_reset,
                  FREEZEDCM => '0', PROGCLK => reference_clk, PROGDATA => '0', PROGEN => '0',
                  CLKFX => cpu_clk_raw, CLKFX180 => open, CLKFXDV => open,
                  LOCKED => cpu_dcm_locked, PROGDONE => open, STATUS => open);
    cpu_buffer : BUFG port map (I => cpu_clk_raw, O => cpu_clk);

    clock_fault <= not (dcm_locked and pll_locked and io_locked and cpu_dcm_locked) or dcm_reset;
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
    process (cpu_clk, clock_fault)
    begin
        if clock_fault = '1' then cpu_release <= (others => '1');
        elsif rising_edge(cpu_clk) then cpu_release <= cpu_release(2 downto 0) & '0'; end if;
    end process;
    pixel_reset <= pixel_release(3);
    double_reset <= double_release(3);
    cpu_reset <= cpu_release(3);

    cpu : entity work.FridgeCPU
        port map (
            CLK_MAIN => cpu_clk,
            CLK_PHI2 => cpu_clk,
            RESET => cpu_reset,
            DEBUG_SWITCH => '0',
            HALTED => cpu_halted,
            INTE => cpu_inte,
            INT => cpu_int,
            INT_IRQ => cpu_int_irq,
            DEVICE_SEL => device_sel,
            DEVICE_READ => device_read,
            DEVICE_DATA => device_data,
            RAM_WRITE_DATA => ram_write_data,
            RAM_WRITE_ADDR => ram_write_addr,
            RAM_WRITE_ENABLED => ram_write_enabled,
            RAM_READ_DATA => ram_read_data,
            RAM_READ_ADDR => ram_read_addr,
            GPU_MODE_SWITCH => gpu_mode_switch,
            GPU_PALETTE_SWITCH => gpu_palette_switch,
            GPU_PALETTE_INDEX => gpu_palette_index,
            GPU_PALETTE_RGB => gpu_palette_rgb,
            GPU_PALETTE_READY => gpu_palette_ready,
            GPU_PRESENT_TRIGGER => gpu_present_trigger,
            GPU_PRESENT_MODE => gpu_present_mode,
            GPU_FRAME_OFFSET => gpu_frame_offset,
            GPU_BACK_STORE => gpu_back_store,
            GPU_BACK_LOAD => gpu_back_load,
            GPU_BACK_ADDR => gpu_back_addr,
            GPU_BACK_DATA => gpu_back_data,
            GPU_BACK_CLR => gpu_back_clr,
            GPU_VMEM_STORE => gpu_vmem_store,
            GPU_VMEM_LOAD => gpu_vmem_load,
            GPU_VMEM_ADDR => gpu_vmem_addr,
            GPU_VMEM_DATA => gpu_vmem_data,
            PAM16_COMMAND_ENABLED => pam16_cmd_enabled,
            PAM16_COMMAND_READY => pam16_cmd_ready,
            PAM16_DATA_WRITE => pam16_data_write,
            PAM16_DATA_READ => pam16_data_read,
            PAM16_COMMAND_CODE => pam16_cmd_code,
            DEBUG_STEP => '0',
            DEBUG_STATE => debug_state,
            DEBUG_CURRENT_IR => debug_ir,
            DEBUG_PC => debug_pc);

    pam16_cmd_ready <= '1';

    ram : entity work.FridgeRAM
        generic map (INIT_DATA => RAMBootImage)
        port map (
            CLK => cpu_clk,
            WRITE_DATA => ram_write_data,
            WRITE_ADDR => ram_write_addr,
            WRITE_ENABLED => ram_write_enabled,
            READ_DATA => ram_read_data,
            READ_ADDR => ram_read_addr);

    video : entity work.fridge_gpu
        port map (
            CLK => pixel_clk,
            COMMAND_CLK => cpu_clk,
            RESET => pixel_reset,
            COMMAND_RESET => cpu_reset,
            FRAME_STORE => gpu_back_store,
            FRAME_ADDR => gpu_back_addr,
            FRAME_DATA => gpu_back_data,
            PRESENT_TRIGGER => gpu_present_trigger,
            PRESENT_MODE => gpu_present_mode,
            FRAME_OFFSET => gpu_frame_offset,
            MODE_SWITCH => gpu_mode_switch,
            PALETTE_WRITE => gpu_palette_switch,
            PALETTE_INDEX => gpu_palette_index,
            PALETTE_RGB => gpu_palette_rgb,
            PALETTE_READY => gpu_palette_ready,
            RED => red,
            GREEN => green,
            BLUE => blue,
            HSYNC => hs,
            VSYNC => vs,
            ACTIVE => active,
            PIXEL_X => open,
            PIXEL_Y => open);

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
    led(2) <= io_locked and pll_locked and dcm_locked and cpu_dcm_locked;
    led(3) <= frames(5);
end rtl;
