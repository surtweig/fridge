-- Unit tests for ps2_receiver: valid frames at realistic and boundary PS/2
-- rates, a full 256-byte sweep at accelerated rate, parity/start/stop
-- rejection, and the watchdog that abandons truncated frames.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity tb_ps2_receiver is
end tb_ps2_receiver;

architecture sim of tb_ps2_receiver is
    signal clk : std_logic := '0';
    signal finished : boolean := false;
    signal reset : std_logic := '1';
    signal ps2_clk : std_logic := '1';
    signal ps2_dat : std_logic := '1';
    signal code : std_logic_vector(7 downto 0);
    signal code_new, err : std_logic;

    signal new_count : integer := 0;
    signal err_count : integer := 0;
    signal last_code : std_logic_vector(7 downto 0) := (others => '0');

    constant CLK_PERIOD : time := 100 ns;

    procedure send_bit(signal ps2_clk : out std_logic;
                       signal ps2_dat : out std_logic;
                       d : std_logic; t : time) is
    begin
        ps2_dat <= d;
        wait for t / 4;
        ps2_clk <= '0';                 -- falling edge: receiver samples
        wait for t / 2;
        ps2_clk <= '1';
        wait for t / 4;
    end procedure;

    procedure send_byte(signal ps2_clk : out std_logic;
                        signal ps2_dat : out std_logic;
                        b : std_logic_vector(7 downto 0); t : time;
                        bad_parity : boolean := false;
                        bad_stop : boolean := false;
                        bad_start : boolean := false) is
        variable par : std_logic;
    begin
        par := '1';
        for i in 0 to 7 loop
            par := par xor b(i);
        end loop;
        if bad_parity then par := not par; end if;
        if bad_start then
            send_bit(ps2_clk, ps2_dat, '1', t);
        else
            send_bit(ps2_clk, ps2_dat, '0', t);
        end if;
        for i in 0 to 7 loop
            send_bit(ps2_clk, ps2_dat, b(i), t);
        end loop;
        send_bit(ps2_clk, ps2_dat, par, t);
        if bad_stop then
            send_bit(ps2_clk, ps2_dat, '0', t);
        else
            send_bit(ps2_clk, ps2_dat, '1', t);
        end if;
        ps2_dat <= '1';
    end procedure;
begin
    clk <= not clk after CLK_PERIOD / 2 when not finished else '0';

    dut : entity work.ps2_receiver
        generic map (CLK_FREQ => 10_000_000, IDLE_US => 150)
        port map (
            CLK => clk, RESET => reset,
            PS2_CLK => ps2_clk, PS2_DAT => ps2_dat,
            CODE => code, CODE_NEW => code_new, ERROR => err);

    monitor : process (clk)
    begin
        if rising_edge(clk) then
            if code_new = '1' then
                new_count <= new_count + 1;
                last_code <= code;
            end if;
            if err = '1' then
                err_count <= err_count + 1;
            end if;
        end if;
    end process;

    stimulus : process
        variable n0, e0 : integer;
    begin
        reset <= '1';
        wait for 10 * CLK_PERIOD;
        wait until falling_edge(clk);
        reset <= '0';
        wait for 10 * CLK_PERIOD;

        -- Valid frames at the typical PS/2 rate (15 kHz).
        n0 := new_count;
        send_byte(ps2_clk, ps2_dat, X"1C", 66 us);
        send_byte(ps2_clk, ps2_dat, X"F0", 66 us);
        send_byte(ps2_clk, ps2_dat, X"AA", 66 us);
        wait for 2 us;
        assert new_count = n0 + 3
            report "valid 15 kHz frames not received" severity failure;
        assert last_code = X"AA"
            report "wrong last code" severity failure;
        report "PASS: valid frames at 15 kHz" severity note;

        -- Slowest spec rate (10 kHz): no spurious watchdog abort.
        e0 := err_count;
        n0 := new_count;
        send_byte(ps2_clk, ps2_dat, X"5A", 100 us);
        wait for 2 us;
        assert new_count = n0 + 1 and err_count = e0
            report "10 kHz frame lost or aborted" severity failure;
        report "PASS: valid frame at 10 kHz (slowest spec rate)" severity note;

        -- Parity error: rejected, no byte delivered.
        n0 := new_count;
        e0 := err_count;
        send_byte(ps2_clk, ps2_dat, X"3B", 66 us, bad_parity => true);
        wait for 2 us;
        assert new_count = n0
            report "parity-bad frame was accepted" severity failure;
        assert err_count = e0 + 1
            report "parity error not flagged" severity failure;
        report "PASS: parity error rejected" severity note;

        -- Bad stop bit and bad start bit: rejected.
        n0 := new_count;
        e0 := err_count;
        send_byte(ps2_clk, ps2_dat, X"3B", 66 us, bad_stop => true);
        wait for 2 us;
        assert new_count = n0 and err_count = e0 + 1
            report "stop-bad frame handling wrong" severity failure;
        send_byte(ps2_clk, ps2_dat, X"3B", 66 us, bad_start => true);
        wait for 2 us;
        assert new_count = n0 and err_count = e0 + 2
            report "start-bad frame handling wrong" severity failure;
        report "PASS: start/stop errors rejected" severity note;

        -- Watchdog: a truncated frame is abandoned, the next frame is clean.
        n0 := new_count;
        e0 := err_count;
        send_bit(ps2_clk, ps2_dat, '0', 66 us);   -- start
        send_bit(ps2_clk, ps2_dat, '1', 66 us);   -- 5 data bits
        send_bit(ps2_clk, ps2_dat, '1', 66 us);
        send_bit(ps2_clk, ps2_dat, '0', 66 us);
        send_bit(ps2_clk, ps2_dat, '1', 66 us);
        send_bit(ps2_clk, ps2_dat, '1', 66 us);
        ps2_dat <= '1';
        wait for 400 us;                          -- > IDLE_US with clock high
        assert err_count = e0 + 1
            report "truncated frame not abandoned" severity failure;
        assert new_count = n0
            report "truncated frame produced a byte" severity failure;
        send_byte(ps2_clk, ps2_dat, X"29", 66 us);
        wait for 2 us;
        assert new_count = n0 + 1 and last_code = X"29"
            report "frame after watchdog abort not received" severity failure;
        report "PASS: watchdog abandons truncated frames" severity note;

        -- Long idle between frames is not an error.
        e0 := err_count;
        wait for 1 ms;
        n0 := new_count;
        send_byte(ps2_clk, ps2_dat, X"76", 66 us);
        wait for 2 us;
        assert new_count = n0 + 1 and err_count = e0
            report "idle gap handling wrong" severity failure;
        report "PASS: idle gap between frames is clean" severity note;

        -- All 256 byte values, back to back at an accelerated rate.
        n0 := new_count;
        for i in 0 to 255 loop
            send_byte(ps2_clk, ps2_dat, std_logic_vector(to_unsigned(i, 8)), 2 us);
        end loop;
        wait for 2 us;
        assert new_count = n0 + 256
            report "byte sweep lost frames: " & integer'image(new_count - n0)
            severity failure;
        assert last_code = X"FF"
            report "wrong byte at end of sweep" severity failure;
        report "PASS: 256-byte sweep at accelerated rate" severity note;

        report "PASS: PS/2 receiver tests" severity note;
        finished <= true;
        wait;
    end process;
end sim;
