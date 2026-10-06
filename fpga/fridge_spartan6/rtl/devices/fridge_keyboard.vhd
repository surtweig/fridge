-- Fridge keyboard controller: PS/2 scan-code-set-2 decoder + keymap +
-- 32-entry key-event FIFO with the CPU device interface (FRIDGE_DEV_KEYBOARD_ID).
--
-- Event format matches the emulator's FRIDGE_KEYBOARD_CONTROLLER
-- (fridgemulib.c / fridge.h): each FIFO entry is one byte, bit 7 = pressed
-- (FRIDGE_KEYBOARD_KEY_STATE_MASK), bits 6..0 = 7-bit Fridge key code
-- (FRIDGE_KEYBOARD_KEY_CODE_MASK), the ASCII subset defined by
-- fridge_emulator/src/keymap.h. `IIN 3` pops the oldest event; an empty
-- read returns 0x00 and is a no-op (no event byte is ever 0x00).
--
-- Overflow policy (defined here): when all 32 entries are unread, a new
-- event is DROPPED (the 32 buffered events stay in their original order)
-- and the sticky OVERFLOW flag is raised until reset. The emulator instead
-- wraps its ring and overwrites the oldest unread entry (scrambling the
-- order once partially consumed); the hardware preserves order and reports
-- the loss. OVERFLOW is a module output (LED); a CPU-visible status read
-- is reserved until the I/O map has a defined status contract.
--
-- PS/2 scan-code-set-2 translation (US layout):
--   * 0xF0 = break prefix, 0xE0 = extended prefix, 0xE1 (Pause) swallows
--     the remaining 7 bytes of its fixed 8-byte sequence.
--   * Modifiers produce no events: Shift (0x12/0x59) and CapsLock (0x58)
--     fold case at event time (letters: Shift XOR CapsLock; digits and
--     punctuation: Shift), Ctrl/Alt are ignored. This matches the emulator
--     frontend, which folds at event time and never reports modifiers.
--   * Extended (E0) keys are dropped, except E0 5A (keypad Enter) which
--     emits '\n', matching keymap.h (arrows etc. are unmapped there).
--   * Keypad digits 70/69/72/7A/6B/73/74/6C/75/7D emit '0'..'9'.
--   * Make codes emit press events, F0 <code> emits release events;
--     typematic repeat therefore emits repeated press events, like the
--     emulator's repeated KEYDOWN handling.
--   * Documented divergence from the emulator: Shift+digit folds to
--     ")!@#$%^&*(" like the emulator, but Shift+punctuation folds to the
--     shifted character (e.g. Shift+',' -> '<'); the emulator's SDL frontend
--     returns the unshifted code for punctuation keys.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;

entity fridge_keyboard is
    port (
        CLK         : in  std_logic;
        RESET       : in  std_logic;
        PS2_CLK     : in  std_logic;
        PS2_DAT     : in  std_logic;
        DEVICE_SEL  : in  XCM2_WORD;
        DEVICE_READ : in  std_logic;
        DEVICE_DATA : out XCM2_WORD;
        OVERFLOW    : out std_logic;
        RX_ERROR    : out std_logic;
        RX_ACTIVITY : out std_logic
    );
end fridge_keyboard;

architecture rtl of fridge_keyboard is
    constant DEV_KEYBOARD : integer := 3;

    signal rx_code : std_logic_vector(7 downto 0);
    signal rx_code_new, rx_error_pulse : std_logic;
    signal rx_activity_r : std_logic := '0';
    signal rx_error_r : std_logic := '0';

    -- Scan-code decode state
    signal ext_prefix, brk_prefix : std_logic := '0';
    signal swallow : integer range 0 to 7 := 0;
    signal shift_l, shift_r, caps : std_logic := '0';

    -- Event FIFO (32 entries, FRIDGE_KEYBOARD_BUFFER_SIZE)
    type fifo_buf_t is array (0 to 31) of XCM2_WORD;
    signal buf : fifo_buf_t := (others => (others => '0'));
    signal wp, rp : unsigned(4 downto 0) := (others => '0');
    signal count : unsigned(5 downto 0) := (others => '0');
    signal overflow_r : std_logic := '0';

    function to_word(b : std_logic_vector(7 downto 0)) return XCM2_WORD is
        variable r : XCM2_WORD;
    begin
        for i in 0 to 7 loop
            r(i) := b(7 - i);
        end loop;
        return r;
    end function;

    -- US-layout keymap: PS/2 set-2 make code -> ASCII character, with the
    -- emulator's Shift/CapsLock folding. Returns -1 for unmapped keys.
    function key_char(code : std_logic_vector(7 downto 0);
                      sh, cp : std_logic) return integer is
        variable upper : boolean;
    begin
        upper := (sh = '1') xor (cp = '1');
        case code is
            -- letters: a..z / A..Z (CapsLock XOR Shift)
            when X"1C" => if upper then return 16#41#; else return 16#61#; end if; -- a
            when X"32" => if upper then return 16#42#; else return 16#62#; end if; -- b
            when X"21" => if upper then return 16#43#; else return 16#63#; end if; -- c
            when X"23" => if upper then return 16#44#; else return 16#64#; end if; -- d
            when X"24" => if upper then return 16#45#; else return 16#65#; end if; -- e
            when X"2B" => if upper then return 16#46#; else return 16#66#; end if; -- f
            when X"34" => if upper then return 16#47#; else return 16#67#; end if; -- g
            when X"33" => if upper then return 16#48#; else return 16#68#; end if; -- h
            when X"43" => if upper then return 16#49#; else return 16#69#; end if; -- i
            when X"3B" => if upper then return 16#4A#; else return 16#6A#; end if; -- j
            when X"42" => if upper then return 16#4B#; else return 16#6B#; end if; -- k
            when X"4B" => if upper then return 16#4C#; else return 16#6C#; end if; -- l
            when X"3A" => if upper then return 16#4D#; else return 16#6D#; end if; -- m
            when X"31" => if upper then return 16#4E#; else return 16#6E#; end if; -- n
            when X"44" => if upper then return 16#4F#; else return 16#6F#; end if; -- o
            when X"4D" => if upper then return 16#50#; else return 16#70#; end if; -- p
            when X"15" => if upper then return 16#51#; else return 16#71#; end if; -- q
            when X"2D" => if upper then return 16#52#; else return 16#72#; end if; -- r
            when X"1B" => if upper then return 16#53#; else return 16#73#; end if; -- s
            when X"2C" => if upper then return 16#54#; else return 16#74#; end if; -- t
            when X"3C" => if upper then return 16#55#; else return 16#75#; end if; -- u
            when X"2A" => if upper then return 16#56#; else return 16#76#; end if; -- v
            when X"1D" => if upper then return 16#57#; else return 16#77#; end if; -- w
            when X"22" => if upper then return 16#58#; else return 16#78#; end if; -- x
            when X"35" => if upper then return 16#59#; else return 16#79#; end if; -- y
            when X"1A" => if upper then return 16#5A#; else return 16#7A#; end if; -- z

            -- digit row, Shift folds to ")!@#$%^&*(" like the emulator
            when X"45" => if sh = '1' then return 16#29#; else return 16#30#; end if; -- 0 )
            when X"16" => if sh = '1' then return 16#21#; else return 16#31#; end if; -- 1 !
            when X"1E" => if sh = '1' then return 16#40#; else return 16#32#; end if; -- 2 @
            when X"26" => if sh = '1' then return 16#23#; else return 16#33#; end if; -- 3 #
            when X"25" => if sh = '1' then return 16#24#; else return 16#34#; end if; -- 4 $
            when X"2E" => if sh = '1' then return 16#25#; else return 16#35#; end if; -- 5 %
            when X"36" => if sh = '1' then return 16#5E#; else return 16#36#; end if; -- 6 ^
            when X"3D" => if sh = '1' then return 16#26#; else return 16#37#; end if; -- 7 &
            when X"3E" => if sh = '1' then return 16#2A#; else return 16#38#; end if; -- 8 *
            when X"46" => if sh = '1' then return 16#28#; else return 16#39#; end if; -- 9 (

            -- punctuation, full US Shift folding (documented divergence)
            when X"0E" => if sh = '1' then return 16#7E#; else return 16#60#; end if; -- ` ~
            when X"4E" => if sh = '1' then return 16#5F#; else return 16#2D#; end if; -- - _
            when X"55" => if sh = '1' then return 16#2B#; else return 16#3D#; end if; -- = +
            when X"54" => if sh = '1' then return 16#7B#; else return 16#5B#; end if; -- [ {
            when X"5B" => if sh = '1' then return 16#7D#; else return 16#5D#; end if; -- ] }
            when X"5D" => if sh = '1' then return 16#7C#; else return 16#5C#; end if; -- \ |
            when X"4C" => if sh = '1' then return 16#3A#; else return 16#3B#; end if; -- ; :
            when X"52" => if sh = '1' then return 16#22#; else return 16#27#; end if; -- ' "
            when X"41" => if sh = '1' then return 16#3C#; else return 16#2C#; end if; -- , <
            when X"49" => if sh = '1' then return 16#3E#; else return 16#2E#; end if; -- . >
            when X"4A" => if sh = '1' then return 16#3F#; else return 16#2F#; end if; -- / ?
            when X"29" => return 16#20#;                                              -- space

            -- control keys (keymap.h)
            when X"76" => return 16#1B#; -- Esc
            when X"5A" => return 16#0A#; -- Enter
            when X"66" => return 16#08#; -- Backspace
            when X"0D" => return 16#09#; -- Tab

            -- keypad digits -> '0'..'9' (Shift/CapsLock insensitive)
            when X"70" => return 16#30#;
            when X"69" => return 16#31#;
            when X"72" => return 16#32#;
            when X"7A" => return 16#33#;
            when X"6B" => return 16#34#;
            when X"73" => return 16#35#;
            when X"74" => return 16#36#;
            when X"6C" => return 16#37#;
            when X"75" => return 16#38#;
            when X"7D" => return 16#39#;

            when others => return -1;
        end case;
    end function;
begin
    receiver : entity work.ps2_receiver
        generic map (CLK_FREQ => 10_000_000, IDLE_US => 150)
        port map (
            CLK => CLK, RESET => RESET,
            PS2_CLK => PS2_CLK, PS2_DAT => PS2_DAT,
            CODE => rx_code, CODE_NEW => rx_code_new, ERROR => rx_error_pulse);

    -- Scan-code set-2 decode + keymap + FIFO. The FIFO read side pops one
    -- event per IIN; DEVICE_DATA is combinational (show-ahead) so the CPU
    -- samples the event in mid-state and the pop lands at the state's end.
    decode : process (CLK)
        variable b : std_logic_vector(7 downto 0);
        variable is_break : boolean;
        variable c : integer;
        variable ev : std_logic_vector(7 downto 0);
        variable push, pop : boolean;
        variable v_count : integer range 0 to 32;
    begin
        if rising_edge(CLK) then
            if RESET = '1' then
                ext_prefix <= '0';
                brk_prefix <= '0';
                swallow <= 0;
                shift_l <= '0';
                shift_r <= '0';
                caps <= '0';
                rx_activity_r <= '0';
                rx_error_r <= '0';
                overflow_r <= '0';
                wp <= (others => '0');
                rp <= (others => '0');
                count <= (others => '0');
            else
                if rx_error_pulse = '1' then
                    rx_error_r <= '1';
                end if;

                push := false;
                ev := (others => '0');
                c := -1;

                if rx_code_new = '1' then
                    b := rx_code;
                    rx_activity_r <= not rx_activity_r;

                    if swallow > 0 then
                        swallow <= swallow - 1;
                    elsif b = X"E1" then
                        -- Pause: fixed 8-byte sequence, swallow the rest
                        swallow <= 7;
                    elsif b = X"E0" then
                        ext_prefix <= '1';
                    elsif b = X"F0" then
                        brk_prefix <= '1';
                    else
                        is_break := (brk_prefix = '1');
                        brk_prefix <= '0';
                        ext_prefix <= '0';

                        if ext_prefix = '1' then
                            if b = X"5A" then
                                c := 16#0A#; -- keypad Enter -> '\n'
                            end if;
                        else
                            case b is
                                when X"12" =>
                                    if is_break then shift_l <= '0'; else shift_l <= '1'; end if;
                                when X"59" =>
                                    if is_break then shift_r <= '0'; else shift_r <= '1'; end if;
                                when X"58" =>
                                    if not is_break then
                                        caps <= not caps;
                                    end if;
                                when X"14" | X"11" =>
                                    null; -- Ctrl/Alt: no events, no folding
                                when others =>
                                    c := key_char(b, shift_l or shift_r, caps);
                            end case;
                        end if;

                        if c >= 0 then
                            push := true;
                            ev := std_logic_vector(to_unsigned(c, 8));
                            if not is_break then
                                ev(7) := '1';
                            end if;
                        end if;
                    end if;
                end if;

                -- Read side: one pop per CPU device read of this device.
                pop := (DEVICE_READ = '1' and to_integer(DEVICE_SEL) = DEV_KEYBOARD
                        and to_integer(count) > 0);

                v_count := to_integer(count);
                if push then
                    if v_count < 32 then
                        buf(to_integer(wp)) <= to_word(ev);
                        wp <= wp + 1;
                        v_count := v_count + 1;
                    else
                        -- Full: drop the new event, keep the buffered order.
                        overflow_r <= '1';
                    end if;
                end if;
                if pop then
                    rp <= rp + 1;
                    v_count := v_count - 1;
                end if;
                count <= to_unsigned(v_count, 6);
            end if;
        end if;
    end process;

    -- Read output is zero unless selected; fridge_system multiplexes it.
    device_bus : process (DEVICE_SEL, DEVICE_READ, count, rp, buf)
    begin
        if DEVICE_READ = '1' and to_integer(DEVICE_SEL) = DEV_KEYBOARD then
            if to_integer(count) > 0 then
                DEVICE_DATA <= buf(to_integer(rp));
            else
                DEVICE_DATA <= X"00";
            end if;
        else
            DEVICE_DATA <= X"00";
        end if;
    end process;

    OVERFLOW <= overflow_r;
    RX_ERROR <= rx_error_r;
    RX_ACTIVITY <= rx_activity_r;
end rtl;
