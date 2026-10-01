library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library unisim;
use unisim.vcomponents.all;
use work.FridgeGlobals.all;
use work.FridgeRAMBootImage.all;

entity cpu_smoke is
    port (
        clk : in std_logic;
        reset_button : in std_logic;
        led : out std_logic_vector(3 downto 0)
    );
end cpu_smoke;

architecture rtl of cpu_smoke is
    signal reference_clk, cpu_clk_raw, cpu_clk : std_logic;
    signal dcm_locked : std_logic;
    signal startup : unsigned(7 downto 0) := (others => '0');
    signal button_sync : std_logic_vector(1 downto 0) := (others => '0');
    signal dcm_reset : std_logic := '1';
    signal cpu_release : std_logic_vector(3 downto 0) := (others => '1');
    signal cpu_reset, clock_fault : std_logic;

    signal cpu_halted, cpu_inte : std_logic;
    signal cpu_int : std_logic := '0';
    signal cpu_int_irq : XCM2_WORD := (others => '0');
    signal cpu_device_sel, cpu_device_data : XCM2_WORD;
    signal cpu_device_read : std_logic;

    signal ram_write_data, ram_read_data : XCM2_WORD;
    signal ram_write_addr, ram_read_addr : XCM2_DWORD;
    signal ram_write_enabled : std_logic;

    signal gpu_mode_switch : std_logic_vector(0 to 1);
    signal gpu_palette_switch, gpu_present_trigger : std_logic;
    signal gpu_back_store, gpu_back_load, gpu_back_clr : std_logic;
    signal gpu_back_addr, gpu_vmem_addr : XCM2_DWORD;
    signal gpu_back_data, gpu_vmem_data : XCM2_WORD;
    signal gpu_vmem_store, gpu_vmem_load : std_logic;

    signal pam16_cmd_enabled, pam16_cmd_ready : std_logic;
    signal pam16_data_write, pam16_data_read : XCM2_DWORD;
    signal pam16_cmd_code : PAM16_COMMAND;

    signal debug_state, debug_ir : XCM2_WORD;
    signal debug_pc : XCM2_DWORD;

    signal heartbeat_cnt : unsigned(25 downto 0) := (others => '0');
    signal heartbeat : std_logic := '0';

    attribute ASYNC_REG : string;
    attribute ASYNC_REG of button_sync, cpu_release : signal is "TRUE";
    attribute SHREG_EXTRACT : string;
    attribute SHREG_EXTRACT of button_sync : signal is "NO";
begin
    reference_buffer : BUFG port map (I => clk, O => reference_clk);

    process (reference_clk)
    begin
        if rising_edge(reference_clk) then
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

    cpu_dcm : DCM_CLKGEN
        generic map (CLKFX_MULTIPLY => 2, CLKFX_DIVIDE => 20,
                     CLKIN_PERIOD => 10.0, STARTUP_WAIT => false)
        port map (CLKIN => reference_clk, RST => dcm_reset,
                  FREEZEDCM => '0', PROGCLK => reference_clk, PROGDATA => '0', PROGEN => '0',
                  CLKFX => cpu_clk_raw, CLKFX180 => open, CLKFXDV => open,
                  LOCKED => dcm_locked, PROGDONE => open, STATUS => open);
    cpu_buffer : BUFG port map (I => cpu_clk_raw, O => cpu_clk);

    clock_fault <= not dcm_locked or dcm_reset;
    process (cpu_clk, clock_fault)
    begin
        if clock_fault = '1' then cpu_release <= (others => '1');
        elsif rising_edge(cpu_clk) then cpu_release <= cpu_release(2 downto 0) & '0';
        end if;
    end process;
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
            DEVICE_SEL => cpu_device_sel,
            DEVICE_READ => cpu_device_read,
            DEVICE_DATA => cpu_device_data,
            RAM_WRITE_DATA => ram_write_data,
            RAM_WRITE_ADDR => ram_write_addr,
            RAM_WRITE_ENABLED => ram_write_enabled,
            RAM_READ_DATA => ram_read_data,
            RAM_READ_ADDR => ram_read_addr,
            GPU_MODE_SWITCH => gpu_mode_switch,
            GPU_PALETTE_SWITCH => gpu_palette_switch,
            GPU_PRESENT_TRIGGER => gpu_present_trigger,
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
            DEBUG_PC => debug_pc
        );

    pam16_cmd_ready <= '1';
    pam16_data_read <= (others => '0');

    ram : entity work.FridgeRAM
        generic map (INIT_DATA => RAMBootImage)
        port map (
            CLK => cpu_clk,
            WRITE_DATA => ram_write_data,
            WRITE_ADDR => ram_write_addr,
            WRITE_ENABLED => ram_write_enabled,
            READ_DATA => ram_read_data,
            READ_ADDR => ram_read_addr
        );

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
    led(2) <= debug_state(0);
    led(3) <= debug_state(1);
end rtl;
