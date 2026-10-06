library ieee;
use ieee.std_logic_1164.all;
use work.FridgeGlobals.all;

package FridgeSystemDebug is
    type system_debug_t is record
        pc : XCM2_DWORD;
        device_sel, device_in, device_out : XCM2_WORD;
        device_read, device_write : std_logic;
        frame_store : std_logic;
        frame_addr : XCM2_DWORD;
        frame_data : XCM2_WORD;
        mode_switch : std_logic_vector(0 to 1);
        present : std_logic;
        present_mode : XCM2_WORD;
        frame_offset : XCM2_DWORD;
        palette_write, palette_ready : std_logic;
        palette_index : XCM2_WORD;
        palette_rgb : std_logic_vector(23 downto 0);
    end record;
end FridgeSystemDebug;
