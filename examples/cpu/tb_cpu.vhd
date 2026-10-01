library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeRAMBootImage.all;

entity tb_cpu is
end tb_cpu;

architecture sim of tb_cpu is
    signal clk : std_logic := '0';
    signal reset : std_logic := '1';
    signal halted : std_logic;
    signal inte : std_logic;
    signal int : std_logic := '0';
    signal int_irq : XCM2_WORD := (others => '0');
    signal device_sel, device_data : XCM2_WORD;
    signal device_read : std_logic;

    signal ram_write_data, ram_read_data : XCM2_WORD;
    signal ram_write_addr, ram_read_addr : XCM2_DWORD;
    signal ram_write_enabled : std_logic;

    signal gpu_mode_switch : std_logic_vector(0 to 1);
    signal gpu_palette_switch, gpu_present_trigger : std_logic;
    signal gpu_back_store, gpu_back_load, gpu_back_clr : std_logic;
    signal gpu_back_addr, gpu_vmem_addr : XCM2_DWORD;
    signal gpu_back_data, gpu_vmem_data : XCM2_WORD;
    signal gpu_vmem_store, gpu_vmem_load : std_logic;

    signal pam16_cmd_enabled, pam16_cmd_ready : std_logic := '1';
    signal pam16_data_write, pam16_data_read : XCM2_DWORD := (others => '0');
    signal pam16_cmd_code : PAM16_COMMAND;

    signal debug_state, debug_ir : XCM2_WORD;
    signal debug_pc : XCM2_DWORD;

    signal store_seen : boolean := false;
    signal store_addr : XCM2_DWORD;
    signal store_data : XCM2_WORD;
    signal load_seen : boolean := false;
    signal load_addr : XCM2_DWORD;
    signal done : boolean := false;

    constant CLK_PERIOD : time := 10 ns;
begin
    clk <= not clk after CLK_PERIOD / 2 when not done else '0';

    cpu : entity work.FridgeCPU
        port map (
            CLK_MAIN => clk,
            CLK_PHI2 => clk,
            RESET => reset,
            DEBUG_SWITCH => '0',
            HALTED => halted,
            INTE => inte,
            INT => int,
            INT_IRQ => int_irq,
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

    ram : entity work.FridgeRAM
        generic map (INIT_DATA => RAMBootImage)
        port map (
            CLK => clk,
            WRITE_DATA => ram_write_data,
            WRITE_ADDR => ram_write_addr,
            WRITE_ENABLED => ram_write_enabled,
            READ_DATA => ram_read_data,
            READ_ADDR => ram_read_addr
        );

    monitor : process
    begin
        wait until falling_edge(clk);
        wait for 1 ns;
        if reset = '0' and not done then
            if ram_write_enabled = '1' then
                store_seen <= true;
                store_addr <= ram_write_addr;
                store_data <= ram_write_data;
            end if;
            if ram_read_addr = X"0080" then
                load_seen <= true;
                load_addr <= ram_read_addr;
            end if;
        end if;
    end process;

    stimulus : process
    begin
        reset <= '1';
        wait for 10 * CLK_PERIOD;
        reset <= '0';

        wait for 100 * CLK_PERIOD;

        assert halted = '1'
            report "FAIL: CPU did not halt (HALTED=" & std_logic'image(halted) &
                   " PC=" & integer'image(to_integer(debug_pc)) & ")"
            severity failure;
        report "PASS: CPU halted";

        assert debug_pc = X"001A"
            report "FAIL: unexpected PC at halt, got " & integer'image(to_integer(debug_pc))
            severity failure;
        report "PASS: PC = 0x001A at halt";

        assert store_seen
            report "FAIL: no memory write observed"
            severity failure;
        report "PASS: memory write observed";

        assert store_addr = X"0080"
            report "FAIL: write to wrong address, got " & integer'image(to_integer(store_addr))
            severity failure;
        assert store_data = X"9C"
            report "FAIL: wrong data written, got " & integer'image(to_integer(store_data))
            severity failure;
        report "PASS: wrote 0x9C to address 0x0080";

        assert load_seen
            report "FAIL: no read from 0x0080 observed"
            severity failure;
        report "PASS: read from address 0x0080";

        report "PASS: CPU/RAM smoke test";
        done <= true;
        wait;
    end process;
end sim;
