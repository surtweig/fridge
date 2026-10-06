-- End-to-end tests for fridge_rom (Case B, BRAM-backed read-only ROM):
-- the device contract on the CPU device bus. Covers the mode/segment
-- stream protocol (all four demo segments, byte-exact), re-select without
-- a device reset (stream end returns to the mode state), the read-only
-- STORE rejection, out-of-range segments, IN outside streaming, OUT
-- during streaming, the device-select guard, the device reset command
-- (including value 0 = ignored and mid-stream resets) and the sticky
-- ERROR flag semantics.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeROMImage.all;

entity tb_rom is
end tb_rom;

architecture sim of tb_rom is
    signal clk : std_logic := '0';
    signal finished : boolean := false;
    signal reset : std_logic := '1';
    signal device_sel : XCM2_WORD := X"00";
    signal device_read : std_logic := '0';
    signal device_write : std_logic := '0';
    signal device_data : XCM2_WORD := (others => 'Z');
    signal rom_error : std_logic;

    constant CLK_PERIOD : time := 100 ns;
    -- NUMERIC_STD "=" treats 'Z' as a metavalue and returns FALSE; the
    -- floating-bus check needs element-wise comparison instead.
    function is_z(v : XCM2_WORD) return boolean is
    begin
        for i in v'range loop
            if v(i) /= 'Z' then
                return false;
            end if;
        end loop;
        return true;
    end function;

    -- Independent re-implementation of the FridgeROMImage demo patterns.
    function expect_byte(seg, i : integer) return XCM2_WORD is
    begin
        case seg is
            when 0 => return to_unsigned(i, 8);
            when 1 => return to_unsigned(255 - i, 8);
            when 2 =>
                if i mod 2 = 0 then
                    return X"AA";
                else
                    return X"55";
                end if;
            when others => return X"42";
        end case;
    end function;

    -- Emulates one CPU `IOUT <dev>`: DEVICE_WRITE for exactly one clock,
    -- the device latches at the rising edge that ends the state.
    procedure cpu_write(signal clk : in std_logic;
                        signal device_sel : out XCM2_WORD;
                        signal device_write : out std_logic;
                        signal device_data : out XCM2_WORD;
                        dev, val : in integer) is
    begin
        wait until rising_edge(clk);
        device_sel <= to_unsigned(dev, 8);
        device_write <= '1';
        device_data <= to_unsigned(val, 8);
        wait until rising_edge(clk);
        device_write <= '0';
        device_data <= (others => 'Z');
        wait until falling_edge(clk);
    end procedure;

    -- Emulates one CPU `IIN <dev>`: DEVICE_READ for exactly one clock,
    -- DEVICE_DATA sampled in the mid-state falling edge (one pop).
    procedure cpu_read(signal clk : in std_logic;
                       signal device_sel : out XCM2_WORD;
                       signal device_read : out std_logic;
                       signal device_data : in XCM2_WORD;
                       dev : in integer;
                       variable b : out XCM2_WORD) is
    begin
        wait until rising_edge(clk);
        device_sel <= to_unsigned(dev, 8);
        device_read <= '1';
        wait until falling_edge(clk);
        b := device_data;
        wait until rising_edge(clk);
        device_read <= '0';
        wait until falling_edge(clk);
    end procedure;
begin
    clk <= not clk after CLK_PERIOD / 2 when not finished else '0';

    dut : entity work.fridge_rom
        generic map (IMAGE => ROM_IMAGE)
        port map (
            CLK => clk, RESET => reset,
            DEVICE_SEL => device_sel,
            DEVICE_READ => device_read,
            DEVICE_WRITE => device_write,
            DEVICE_DATA => device_data,
            ERROR => rom_error);

    stimulus : process
        variable b : XCM2_WORD;

        procedure select_segment(seg : integer) is
        begin
            cpu_write(clk, device_sel, device_write, device_data, 1, 2); -- LOAD
            cpu_write(clk, device_sel, device_write, device_data, 1, seg / 256);
            cpu_write(clk, device_sel, device_write, device_data, 1, seg mod 256);
        end procedure;

        procedure check_segment(seg : integer) is
        begin
            select_segment(seg);
            for i in 0 to 255 loop
                cpu_read(clk, device_sel, device_read, device_data, 1, b);
                assert b = expect_byte(seg, i)
                    report "segment " & integer'image(seg) & " byte " &
                           integer'image(i) & " mismatch" severity failure;
            end loop;
        end procedure;

        procedure check_error(expected : std_logic; msg : string) is
        begin
            assert rom_error = expected
                report "ERROR flag mismatch: " & msg severity failure;
        end procedure;
    begin
        reset <= '1';
        wait for 10 * CLK_PERIOD;
        wait until falling_edge(clk);
        reset <= '0';
        wait until falling_edge(clk);

        -- A. IN outside streaming: 0x00, no advance, sticky ERROR.
        cpu_read(clk, device_sel, device_read, device_data, 1, b);
        assert b = X"00" report "A: IN outside streaming must return 0x00" severity failure;
        check_error('1', "A: IN outside streaming must raise ERROR");

        -- B. Device reset command (dev 2, value 1) returns to power-on
        -- state and clears ERROR.
        cpu_write(clk, device_sel, device_write, device_data, 2, 1);
        check_error('0', "B: reset command must clear ERROR");

        -- C. Segment 0: full 256-byte ramp (select + stream).
        check_segment(0);

        -- D. Device-select guard mid-stream: reads/writes of other device
        -- IDs neither drive the bus nor pop the stream.
        select_segment(0);
        for i in 0 to 99 loop
            cpu_read(clk, device_sel, device_read, device_data, 1, b);
        end loop;
        cpu_read(clk, device_sel, device_read, device_data, 3, b);
        assert is_z(device_data)
            report "D: ROM must not drive the bus for other device IDs" severity failure;
        cpu_write(clk, device_sel, device_write, device_data, 3, 16#FF#);
        cpu_read(clk, device_sel, device_read, device_data, 1, b);
        assert b = expect_byte(0, 100)
            report "D: stream must continue at byte 100" severity failure;
        check_error('0', "D: foreign device traffic must not raise ERROR");
        for i in 101 to 255 loop
            cpu_read(clk, device_sel, device_read, device_data, 1, b);
            assert b = expect_byte(0, i)
                report "D: segment 0 byte " & integer'image(i) & " mismatch"
                severity failure;
        end loop;

        -- E. Post-stream read: the stream has ended (mode state).
        cpu_read(clk, device_sel, device_read, device_data, 1, b);
        assert b = X"00" report "E: read after stream end must return 0x00" severity failure;
        check_error('1', "E: read after stream end must raise ERROR");

        -- F. Re-select WITHOUT a device reset (stream end -> mode state),
        -- segments 1..3 byte-exact.
        cpu_write(clk, device_sel, device_write, device_data, 2, 1);
        check_error('0', "F: reset command must clear ERROR");
        check_segment(1);
        check_segment(2);
        check_segment(3);

        -- G. Out-of-range segment: ERROR, no stream.
        select_segment(4);
        check_error('1', "G: out-of-range segment must raise ERROR");
        cpu_read(clk, device_sel, device_read, device_data, 1, b);
        assert b = X"00" report "G: out-of-range must not stream" severity failure;

        -- H. Recovery: reset clears ERROR, segment 0 streams again.
        cpu_write(clk, device_sel, device_write, device_data, 2, 1);
        check_error('0', "H: reset command must clear ERROR");
        check_segment(0);

        -- I. STORE mode is rejected (read-only device): ERROR, and the
        -- error stays sticky even after later successful streams.
        cpu_write(clk, device_sel, device_write, device_data, 1, 1); -- STORE
        check_error('1', "I: STORE mode must be rejected");
        cpu_read(clk, device_sel, device_read, device_data, 1, b);
        assert b = X"00" report "I: rejected mode must not stream" severity failure;
        check_segment(0);
        check_error('1', "I: ERROR must stay sticky after a good stream");
        cpu_write(clk, device_sel, device_write, device_data, 2, 1);
        check_error('0', "I: reset command must clear ERROR");

        -- J. OUT during streaming: ERROR, stream position unaffected.
        select_segment(0);
        cpu_read(clk, device_sel, device_read, device_data, 1, b);
        assert b = expect_byte(0, 0) severity failure;
        cpu_read(clk, device_sel, device_read, device_data, 1, b);
        assert b = expect_byte(0, 1) severity failure;
        cpu_write(clk, device_sel, device_write, device_data, 1, 2);
        check_error('1', "J: OUT during streaming must raise ERROR");
        for i in 2 to 255 loop
            cpu_read(clk, device_sel, device_read, device_data, 1, b);
            assert b = expect_byte(0, i)
                report "J: segment 0 byte " & integer'image(i) & " mismatch"
                severity failure;
        end loop;

        -- K. Device reset command with value 0 is ignored (no state change).
        cpu_write(clk, device_sel, device_write, device_data, 2, 1);
        check_error('0', "K: reset command must clear ERROR");
        select_segment(0);
        cpu_read(clk, device_sel, device_read, device_data, 1, b);
        assert b = expect_byte(0, 0) severity failure;
        cpu_write(clk, device_sel, device_write, device_data, 2, 0);
        check_error('0', "K: reset value 0 must be ignored");
        for i in 1 to 255 loop
            cpu_read(clk, device_sel, device_read, device_data, 1, b);
            assert b = expect_byte(0, i)
                report "K: segment 0 byte " & integer'image(i) & " mismatch"
                severity failure;
        end loop;

        -- L. Mid-stream device reset (value 1) returns to the mode state.
        select_segment(0);
        cpu_read(clk, device_sel, device_read, device_data, 1, b);
        assert b = expect_byte(0, 0) severity failure;
        cpu_write(clk, device_sel, device_write, device_data, 2, 1);
        check_error('0', "L: reset command must clear ERROR");
        cpu_read(clk, device_sel, device_read, device_data, 1, b);
        assert b = X"00" report "L: read after mid-stream reset must return 0x00"
            severity failure;

        -- M. Invalid mode write (not LOAD/STORE) is rejected.
        cpu_write(clk, device_sel, device_write, device_data, 2, 1);
        cpu_write(clk, device_sel, device_write, device_data, 1, 7);
        check_error('1', "M: invalid mode must raise ERROR");

        -- N. The module RESET input also clears ERROR.
        check_error('1', "N: precondition");
        reset <= '1';
        wait until falling_edge(clk);
        wait until falling_edge(clk);
        reset <= '0';
        wait until falling_edge(clk);
        check_error('0', "N: module RESET must clear ERROR");

        report "PASS: ROM device tests" severity note;
        finished <= true;
        wait;
    end process;
end sim;
