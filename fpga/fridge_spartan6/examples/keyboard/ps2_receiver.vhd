-- PS/2 device-to-host byte receiver.
--
-- The device (the Atlys USB-HID bridge on P17/N15) drives 11-bit frames:
-- start '0', 8 data bits LSB first, odd parity, stop '1'. Data is valid on
-- the falling edge of PS2_CLK. Both lines idle high (open collector, the
-- board has no external pull-ups; the UCF enables the internal ones).
--
-- All logic runs on CLK (the 10 MHz CPU clock); PS2_CLK/PS2_DAT are
-- synchronized with two flip-flops each. Frames are validated (start,
-- odd parity, stop) and only valid bytes pulse CODE_NEW. Rejected frames
-- and abandoned partial frames pulse ERROR.
--
-- Watchdog: if a frame is in progress and PS2_CLK stays high longer than
-- IDLE_US without a falling edge, the partial frame is abandoned and ERROR
-- is pulsed, so a truncated transaction cannot poison the next frame.
-- 150 us is longer than any legal bit high time (the PS/2 clock is
-- 10-16.7 kHz, roughly 30-55 us high per bit) and shorter than typical
-- inter-frame gaps.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity ps2_receiver is
    generic (
        CLK_FREQ : integer := 10_000_000;
        IDLE_US  : integer := 150
    );
    port (
        CLK      : in  std_logic;
        RESET    : in  std_logic;
        PS2_CLK  : in  std_logic;
        PS2_DAT  : in  std_logic;
        CODE     : out std_logic_vector(7 downto 0);
        CODE_NEW : out std_logic;
        ERROR    : out std_logic
    );
end ps2_receiver;

architecture rtl of ps2_receiver is
    constant IDLE_LIMIT : integer := (CLK_FREQ / 1_000_000) * IDLE_US;

    signal clk_sync, dat_sync : std_logic_vector(1 downto 0) := (others => '1');
    signal clk_prev : std_logic := '1';
    signal shift_reg : std_logic_vector(10 downto 0) := (others => '1');
    signal bit_count : integer range 0 to 11 := 0;
    signal idle_count : integer range 0 to IDLE_LIMIT := 0;

    attribute ASYNC_REG : string;
    attribute ASYNC_REG of clk_sync, dat_sync : signal is "TRUE";
    attribute SHREG_EXTRACT : string;
    attribute SHREG_EXTRACT of clk_sync, dat_sync : signal is "NO";
begin
    process (CLK)
        variable v_shift : std_logic_vector(10 downto 0);
        variable v_count : integer range 0 to 12;
        variable v_valid : boolean;
    begin
        if rising_edge(CLK) then
            if RESET = '1' then
                clk_sync <= (others => '1');
                dat_sync <= (others => '1');
                clk_prev <= '1';
                bit_count <= 0;
                idle_count <= 0;
                shift_reg <= (others => '1');
                CODE <= (others => '0');
                CODE_NEW <= '0';
                ERROR <= '0';
            else
                clk_sync <= clk_sync(0) & PS2_CLK;
                dat_sync <= dat_sync(0) & PS2_DAT;
                clk_prev <= clk_sync(1);
                CODE_NEW <= '0';
                ERROR <= '0';

                if clk_sync(1) = '0' then
                    idle_count <= 0;
                elsif idle_count < IDLE_LIMIT then
                    idle_count <= idle_count + 1;
                end if;

                v_shift := shift_reg;
                v_count := bit_count;

                if clk_prev = '1' and clk_sync(1) = '0' then
                    -- Falling edge of the synchronized PS/2 clock: shift in
                    -- one bit. The start bit enters first and ends up in
                    -- v_shift(0) after 11 bits.
                    v_shift := dat_sync(1) & v_shift(10 downto 1);
                    v_count := v_count + 1;
                end if;

                if v_count = 11 then
                    v_valid := (v_shift(0) = '0')                        -- start
                           and (v_shift(10) = '1')                       -- stop
                           and ((v_shift(1) xor v_shift(2) xor
                                 v_shift(3) xor v_shift(4) xor
                                 v_shift(5) xor v_shift(6) xor
                                 v_shift(7) xor v_shift(8) xor
                                 v_shift(9)) = '1');                    -- odd parity
                    if v_valid then
                        CODE <= v_shift(8 downto 1);
                        CODE_NEW <= '1';
                    else
                        ERROR <= '1';
                    end if;
                    v_count := 0;
                elsif v_count > 11 then
                    -- More than 11 edges without a completed frame (can only
                    -- happen if an edge arrives in the validating cycle):
                    -- re-align optimistically on this bit and flag it.
                    v_count := 1;
                    ERROR <= '1';
                elsif v_count > 0 and clk_sync(1) = '1' and idle_count >= IDLE_LIMIT then
                    -- Abandon the partial frame only while PS2_CLK is idle
                    -- high; at a falling edge the idle counter still holds
                    -- its stale saturated value for one cycle, so the
                    -- clock-high qualification prevents a false abort on
                    -- the first bit of the next frame.
                    v_count := 0;
                    ERROR <= '1';
                end if;

                shift_reg <= v_shift;
                bit_count <= v_count;
            end if;
        end if;
    end process;
end rtl;
