-- Shared-RTL regression, adapted from examples/keyboard/tb_keyboard.vhd.
-- End-to-end tests for fridge_keyboard: PS/2 scan-code-set-2 streams in,
-- FRIDGE_KEYBOARD_* event bytes out through the IIN device interface.
-- Covers make/break, Shift/CapsLock folding, keypad/extended keys, the
-- Pause swallow, empty-read semantics, the device-select guard, FIFO
-- order, the defined overflow behavior (drop-newest + sticky OVERFLOW),
-- and the sticky RX_ERROR flag.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;

entity tb_keyboard is
end tb_keyboard;

architecture sim of tb_keyboard is
    signal clk : std_logic := '0';
    signal finished : boolean := false;
    signal reset : std_logic := '1';
    signal ps2_clk : std_logic := '1';
    signal ps2_dat : std_logic := '1';
    signal device_sel : XCM2_WORD := X"00";
    signal device_read : std_logic := '0';
    signal device_data : XCM2_WORD;
    signal kbd_overflow, kbd_rx_error, kbd_rx_activity : std_logic;

    constant CLK_PERIOD : time := 100 ns;

    procedure send_bit(signal ps2_clk : out std_logic;
                       signal ps2_dat : out std_logic;
                       d : std_logic; t : time) is
    begin
        ps2_dat <= d;
        wait for t / 4;
        ps2_clk <= '0';
        wait for t / 2;
        ps2_clk <= '1';
        wait for t / 4;
    end procedure;

    procedure send_byte(signal ps2_clk : out std_logic;
                        signal ps2_dat : out std_logic;
                        b : std_logic_vector(7 downto 0); t : time) is
        variable par : std_logic;
    begin
        par := '1';
        for i in 0 to 7 loop
            par := par xor b(i);
        end loop;
        send_bit(ps2_clk, ps2_dat, '0', t);
        for i in 0 to 7 loop
            send_bit(ps2_clk, ps2_dat, b(i), t);
        end loop;
        send_bit(ps2_clk, ps2_dat, par, t);
        send_bit(ps2_clk, ps2_dat, '1', t);
        ps2_dat <= '1';
    end procedure;

    -- Emulates one CPU `IIN 3`: assert DEVICE_READ for exactly one clock
    -- (one pop), sample DEVICE_DATA in mid-state like the CPU does.
    procedure cpu_read(signal clk : in std_logic;
                       signal device_sel : out XCM2_WORD;
                       signal device_read : out std_logic;
                       signal device_data : in XCM2_WORD;
                       variable b : out XCM2_WORD) is
    begin
        wait until rising_edge(clk);
        device_sel <= X"03";
        device_read <= '1';
        wait until falling_edge(clk);
        b := device_data;
        wait until rising_edge(clk);   -- pop lands here
        device_read <= '0';
        wait until falling_edge(clk);
    end procedure;
begin
    clk <= not clk after CLK_PERIOD / 2 when not finished else '0';

    dut : entity work.fridge_keyboard
        port map (
            CLK => clk, RESET => reset,
            PS2_CLK => ps2_clk, PS2_DAT => ps2_dat,
            DEVICE_SEL => device_sel,
            DEVICE_READ => device_read,
            DEVICE_DATA => device_data,
            OVERFLOW => kbd_overflow,
            RX_ERROR => kbd_rx_error,
            RX_ACTIVITY => kbd_rx_activity);

    stimulus : process
        constant T : time := 4 us;
        variable b : XCM2_WORD;

        procedure expect(exp : XCM2_WORD; msg : string) is
        begin
            cpu_read(clk, device_sel, device_read, device_data, b);
            assert b = exp
                report msg & ": expected 0x" & integer'image(to_integer(exp)) &
                       " got 0x" & integer'image(to_integer(b))
                severity failure;
        end procedure;

        procedure send(x : std_logic_vector(7 downto 0)) is
        begin
            send_byte(ps2_clk, ps2_dat, x, T);
            wait for 2 us;
        end procedure;
    begin
        reset <= '1';
        wait for 10 * CLK_PERIOD;
        wait until falling_edge(clk);
        reset <= '0';
        wait for 10 * CLK_PERIOD;

        -- Empty reads return 0x00.
        expect(X"00", "empty read 1");
        expect(X"00", "empty read 2");

        -- Make/break of 'a'.
        send(X"1C");
        expect(X"E1", "press a");
        send(X"F0");
        send(X"1C");
        expect(X"61", "release a");
        expect(X"00", "no extra events");

        -- Shift folding: press/release 'A', shift itself is invisible.
        send(X"12");
        send(X"1C");
        expect(X"C1", "press A");
        send(X"F0");
        send(X"1C");
        expect(X"41", "release A");
        send(X"F0");
        send(X"12");
        expect(X"00", "shift emits no events");

        -- CapsLock toggles case until switched back.
        send(X"58");
        send(X"1C");
        expect(X"C1", "press A with caps");
        send(X"F0");
        send(X"1C");
        expect(X"41", "release A with caps");
        send(X"F0");
        send(X"58");
        send(X"58");
        send(X"1C");
        expect(X"E1", "press a after caps off");
        send(X"F0");
        send(X"1C");
        expect(X"61", "release a after caps off");

        -- Extended keys: arrows dropped, keypad Enter -> '\n'.
        send(X"E0");
        send(X"75");
        send(X"E0");
        send(X"F0");
        send(X"75");
        expect(X"00", "extended arrow dropped");
        send(X"E0");
        send(X"5A");
        expect(X"8A", "keypad enter press");
        send(X"E0");
        send(X"F0");
        send(X"5A");
        expect(X"0A", "keypad enter release");

        -- Pause (E1 + 7 bytes) is swallowed entirely.
        send(X"E1");
        send(X"14");
        send(X"77");
        send(X"E1");
        send(X"F0");
        send(X"14");
        send(X"F0");
        send(X"77");
        expect(X"00", "pause sequence dropped");

        -- Digit folding: '1' / '!' like the emulator.
        send(X"16");
        expect(X"B1", "press 1");
        send(X"12");
        send(X"16");
        expect(X"A1", "press !");
        send(X"F0");
        send(X"16");
        expect(X"21", "release !");
        send(X"F0");
        send(X"12");

        -- Punctuation folding (documented divergence: full US shift map).
        send(X"41");
        expect(X"AC", "press ,");
        send(X"12");
        send(X"41");
        expect(X"BC", "press <");
        send(X"F0");
        send(X"41");
        expect(X"3C", "release <");
        send(X"F0");
        send(X"12");
        expect(X"00", "drained");

        -- An empty read is a no-op: it does not skip a later event.
        expect(X"00", "noop read 1");
        expect(X"00", "noop read 2");
        send(X"24");
        expect(X"E5", "press e after noop reads");
        send(X"F0");
        send(X"24");
        expect(X"65", "release e");
        expect(X"00", "drained 2");

        -- Reads of other devices neither drive the bus nor pop.
        send(X"23");
        wait until rising_edge(clk);
        device_sel <= X"02";
        device_read <= '1';
        wait until falling_edge(clk);
        -- Compare as std_logic_vector: NUMERIC_STD."=" rejects metavalues.
        assert device_data = X"00"
            report "keyboard read output was nonzero for another device" severity failure;
        wait until rising_edge(clk);
        device_read <= '0';
        wait for 2 us;
        expect(X"E4", "press d survives other-device read");
        send(X"F0");
        send(X"23");
        expect(X"64", "release d");
        expect(X"00", "drained 3");

        -- FIFO order and the defined overflow behavior: 32 events fill the
        -- FIFO exactly (4 x press a..h); the 33rd is dropped, the buffered
        -- order is preserved, and OVERFLOW sticks until reset.
        for round in 1 to 4 loop
            send(X"1C"); send(X"32"); send(X"21"); send(X"23");
            send(X"24"); send(X"2B"); send(X"34"); send(X"33");
        end loop;
        send(X"43");                        -- 33rd event: press 'i' -> dropped
        assert kbd_overflow = '1'
            report "overflow not flagged" severity failure;
        for round in 1 to 4 loop
            expect(X"E1", "fifo order a");
            expect(X"E2", "fifo order b");
            expect(X"E3", "fifo order c");
            expect(X"E4", "fifo order d");
            expect(X"E5", "fifo order e");
            expect(X"E6", "fifo order f");
            expect(X"E7", "fifo order g");
            expect(X"E8", "fifo order h");
        end loop;
        expect(X"00", "33rd event was dropped");
        assert kbd_overflow = '1'
            report "overflow flag must stick until reset" severity failure;
        report "PASS: FIFO order and drop-newest overflow" severity note;

        -- RX_ERROR is sticky and set by a rejected frame.
        assert kbd_rx_error = '0'
            report "rx error set without any bad frame" severity failure;
        -- Hand-build a frame with wrong parity.
        send_bit(ps2_clk, ps2_dat, '0', T);
        for i in 0 to 7 loop
            send_bit(ps2_clk, ps2_dat, '1', T);
        end loop;
        send_bit(ps2_clk, ps2_dat, '0', T); -- parity should be '0' for 8 ones
        send_bit(ps2_clk, ps2_dat, '1', T);
        wait for 2 us;
        assert kbd_rx_error = '1'
            report "rejected frame not flagged" severity failure;
        expect(X"00", "bad frame produced no event");
        report "PASS: sticky RX_ERROR on rejected frames" severity note;

        report "PASS: keyboard decoder/FIFO tests" severity note;
        finished <= true;
        wait;
    end process;
end sim;
