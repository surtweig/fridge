-- Fridge ROM device (Case B: BRAM-backed, read-only). Implements the
-- emulator's ROM segment protocol (segment select + 256-byte streams) as
-- used by the appliance firmware (fridge-boot/BootLoader/BootLoader.x2al
-- and x2al_std/stdapp.inc), with the state semantics of the old emulator's
-- ROM device (emulator/emulator/XCM2ROM.cpp): after a 256-byte LOAD
-- stream the device returns to the mode state, so the BootLoader's
-- multi-segment loop (mode, seg hi, seg lo, 256 x IN) works without a
-- device reset in between.
--
-- Device map (firmware-facing):
--   device 1 (XCM2_ROM_DEVICE_ID)         OUT: mode / segment / (streaming
--                                             writes rejected, read-only)
--                                          IN:  stream bytes while streaming
--   device 2 (XCM2_ROM_DEVICE_RESET_ID)   OUT: value > 0 resets the device
--                                             (mode state, segment 0, and
--                                             clears the sticky ERROR flag)
--
-- Contract (defined here; see README for the full divergence table):
--   * LOAD mode only (write 2 in the mode state). The source ROM devices
--     also accept STORE mode (write 1); the appliance ROM is read-only
--     (Case A flash is written only by the host-side programmer), so this
--     device raises ERROR and stays in the mode state.
--   * READY IMMEDIATELY: the first IN after the segment-low write returns
--     stream byte 0. The source raises a ROM interrupt on the
--     operate->streaming transition and the BootLoader HLTs waiting for
--     it; our CPU's interrupt contracts are not defined yet (plan: source
--     interrupt support is incomplete), so this device streams with no
--     latency and no interrupt. BRAM has no latency; the Case A flash
--     backend will need a defined latency or status read here.
--   * Out-of-range segment select (index >= ROM_SEGMENTS) raises ERROR
--     and returns to the mode state without streaming. (fridgemulib.c
--     corePanics and halts the CPU; XCM2ROM.cpp does not bound-check.)
--   * Protocol violations raise the sticky ERROR flag and are otherwise
--     no-ops (IN outside streaming returns 0x00 without advancing; OUT
--     during streaming is ignored). The source either corePanics
--     (fridgemulib.c) or silently ignores / returns stale data
--     (XCM2ROM.cpp). ERROR clears only on reset (RESET input or the
--     device reset command), like the keyboard's sticky flags.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeROMImage.all;

entity fridge_rom is

generic ( IMAGE : ROM_IMAGE_T );

port (
     CLK         : in  std_logic;
     RESET       : in  std_logic;
     DEVICE_SEL  : in  XCM2_WORD;
     DEVICE_READ : in  std_logic;
     DEVICE_WRITE : in std_logic;
     DEVICE_DATA : inout XCM2_WORD;
     ERROR       : out std_logic
);

end fridge_rom;

architecture rtl of fridge_rom is
    constant DEV_ROM_DATA  : integer := 1;
    constant DEV_ROM_RESET : integer := 2;
    constant MODE_LOAD     : integer := 2;

    type rom_state_t is (ST_MODE, ST_SEGHIGH, ST_SEGLOW, ST_STREAMING);

    signal state : rom_state_t := ST_MODE;
    signal seg_hi : integer range 0 to 255 := 0;
    signal seg_lo : integer range 0 to 255 := 0;
    signal pos : integer range 0 to 255 := 0;
    signal error_r : std_logic := '0';

    -- Storage note: XST maps this read-only array to LUT logic
    -- ("distributed Read Only RAM") and ignores ram_style=block for ROMs
    -- (verified with unstructured content). That is fine for the small
    -- development images of Case B; large ROM images are Case A (flash).
    signal mem : ROM_IMAGE_T := IMAGE;
    signal read_addr_reg : integer range 0 to ROM_BYTES - 1 := 0;
begin
    -- Registered read address (BRAM inference, FridgeRAM.vhd pattern):
    -- the byte for the current segment position is stable from the clock
    -- after it becomes current, well before the CPU's next IIN.
    process (CLK)
        variable sel : integer;
        variable dv : integer;
        variable seg_idx : integer;
    begin
        if rising_edge(CLK) then
            if RESET = '1' then
                state <= ST_MODE;
                seg_hi <= 0;
                seg_lo <= 0;
                pos <= 0;
                error_r <= '0';
                read_addr_reg <= 0;
            else
                if DEVICE_WRITE = '1' then
                    sel := to_integer(DEVICE_SEL);
                    dv := to_integer(DEVICE_DATA);
                    if sel = DEV_ROM_RESET then
                        if dv > 0 then
                            state <= ST_MODE;
                            seg_hi <= 0;
                            seg_lo <= 0;
                            pos <= 0;
                            error_r <= '0';
                        end if;
                    elsif sel = DEV_ROM_DATA then
                        case state is
                            when ST_MODE =>
                                if dv = MODE_LOAD then
                                    state <= ST_SEGHIGH;
                                else
                                    -- STORE (1) and invalid modes are
                                    -- rejected: this ROM is read-only.
                                    error_r <= '1';
                                end if;
                            when ST_SEGHIGH =>
                                seg_hi <= dv;
                                state <= ST_SEGLOW;
                            when ST_SEGLOW =>
                                seg_lo <= dv;
                                if seg_hi * 256 + dv >= ROM_SEGMENTS then
                                    error_r <= '1';
                                    state <= ST_MODE;
                                else
                                    pos <= 0;
                                    state <= ST_STREAMING;
                                end if;
                            when ST_STREAMING =>
                                -- Read-only: no streaming writes (STORE).
                                error_r <= '1';
                        end case;
                    end if;
                end if;

                if DEVICE_READ = '1' and to_integer(DEVICE_SEL) = DEV_ROM_DATA then
                    if state = ST_STREAMING then
                        if pos = 255 then
                            pos <= 0;
                            state <= ST_MODE;
                        else
                            pos <= pos + 1;
                        end if;
                    else
                        error_r <= '1';
                    end if;
                end if;

                seg_idx := seg_hi * 256 + seg_lo;
                if seg_idx < ROM_SEGMENTS then
                    read_addr_reg <= seg_idx * 256 + pos;
                else
                    read_addr_reg <= 0;
                end if;
            end if;
        end if;
    end process;

    -- DEVICE_DATA is driven only while the CPU reads this device. The
    -- stream byte is show-ahead (valid from the start of the read state);
    -- the read side advances at the rising edge that ends the state.
    device_bus : process (DEVICE_SEL, DEVICE_READ, state, mem, read_addr_reg)
    begin
        if DEVICE_READ = '1' and to_integer(DEVICE_SEL) = DEV_ROM_DATA then
            if state = ST_STREAMING then
                DEVICE_DATA <= mem(read_addr_reg);
            else
                DEVICE_DATA <= X"00";
            end if;
        else
            DEVICE_DATA <= (others => 'Z');
        end if;
    end process;

    ERROR <= error_r;
end rtl;
