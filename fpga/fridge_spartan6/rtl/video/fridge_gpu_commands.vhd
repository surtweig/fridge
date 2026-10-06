-- Advanced GPU commands execute in the CPU domain; all RAM ports are synchronous.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeIRCodes.all;
entity fridge_gpu_commands is
    port (CLK, RESET, VALID : in std_logic;
          CODE, A, B, C, ARG0, ARG1 : in XCM2_WORD;
          HL, BC : in XCM2_DWORD;
          READY : out std_logic; RESULT : out XCM2_WORD;
          FRAME_ADDR : out XCM2_DWORD; FRAME_WRITE : out std_logic;
          FRAME_OUT : out XCM2_WORD; FRAME_IN : in XCM2_WORD;
          SPR_ADDR : out XCM2_DWORD; SPR_WRITE : out std_logic;
          SPR_OUT : out XCM2_WORD; SPR_IN : in XCM2_WORD;
          DESC_VALID : out std_logic; DESC_READY : in std_logic;
          DESC_DEFINE : out std_logic; DESC_ID, DESC_B, DESC_C : out XCM2_WORD;
          DESC_HL : out XCM2_DWORD);
end;
architecture rtl of fridge_gpu_commands is
    type state_t is (idle, read_wait, read_done, write_first, write_second,
                     write_drain, desc_wait, done);
    signal state : state_t := idle;
    signal op, value, imm0, imm1 : XCM2_WORD := X"00";
    signal fa, sa : XCM2_DWORD := X"0000";
    signal fw, sw, dv : std_logic := '0';
    signal fo, so : XCM2_WORD := X"00";
    signal right_pixel : boolean := false;
begin
    READY <= '1' when state = done and RESET = '0' else '0';
    FRAME_ADDR <= fa; SPR_ADDR <= sa;
    FRAME_WRITE <= fw when RESET = '0' else '0'; FRAME_OUT <= fo;
    SPR_WRITE <= sw when RESET = '0' else '0'; SPR_OUT <= so;
    DESC_VALID <= dv when RESET = '0' else '0';
    process(CLK)
        variable addr, size, end_addr, x, y : integer;
    begin
        if rising_edge(CLK) then
            if RESET = '1' then
                state <= idle; fw <= '0'; sw <= '0'; dv <= '0';
                fa <= X"0000"; sa <= X"0000"; RESULT <= X"00";
            else
                fw <= '0'; sw <= '0';
                case state is
                    when idle =>
                        if VALID = '1' then
                            op <= CODE; value <= A; imm0 <= ARG0; imm1 <= ARG1;
                            RESULT <= A; fa <= HL; sa <= HL;
                            state <= done; -- Invalid operations have no side effects.
                            if CODE = VFSI and HL < 19199 then
                                state <= write_first;
                            elsif CODE = VSSI and HL < 32767 then
                                state <= write_first;
                            elsif CODE = VSSA and HL < 32768 then
                                so <= A; sw <= '1'; state <= write_drain;
                            elsif CODE = VFLA and HL < 19200 then
                                state <= read_wait;
                            elsif CODE = VSLA and HL < 32768 then
                                state <= read_wait;
                            elsif CODE = VS2F and HL < 32768 and BC < 19200 then
                                fa <= BC; state <= read_wait;
                            elsif CODE = VFSAC or CODE = VFLAC then
                                x := to_integer(HL(0 to 7)); y := to_integer(HL(8 to 15));
                                if x < 240 and y < 160 then
                                    addr := y*120 + x/2;
                                    fa <= to_unsigned(addr, 16); right_pixel <= x mod 2 = 1;
                                    state <= read_wait;
                                end if;
                            elsif CODE = VSS then
                                size := to_integer(B)*to_integer(C);
                                end_addr := to_integer(HL)+(size+1)/2;
                                if A < 64 and B /= 0 and C /= 0 and end_addr <= 32768 then
                                    DESC_DEFINE <= '1'; DESC_ID <= A;
                                    DESC_B <= B; DESC_C <= C; DESC_HL <= HL;
                                    dv <= '1'; state <= desc_wait;
                                end if;
                            elsif CODE = VSD and A < 64 and B < 8 then
                                DESC_DEFINE <= '0'; DESC_ID <= A;
                                DESC_B <= B; DESC_C <= C; DESC_HL <= HL;
                                dv <= '1'; state <= desc_wait;
                            end if;
                        end if;
                    when read_wait => state <= read_done;
                    when read_done =>
                        state <= done;
                        if op = VFLA then RESULT <= FRAME_IN;
                        elsif op = VSLA then RESULT <= SPR_IN;
                        elsif op = VS2F then
                            fo <= SPR_IN; fw <= '1'; state <= write_drain;
                        elsif op = VFLAC then
                            if right_pixel then RESULT <= X"0" & FRAME_IN(4 to 7);
                            else RESULT <= X"0" & FRAME_IN(0 to 3); end if;
                        elsif op = VFSAC then
                            if right_pixel then fo <= FRAME_IN(0 to 3) & value(4 to 7);
                            else fo <= value(0 to 3) & FRAME_IN(4 to 7); end if;
                            fw <= '1'; state <= write_drain;
                        end if;
                    when write_first =>
                        if op = VFSI then fo <= imm0; fw <= '1';
                        else so <= imm0; sw <= '1'; end if;
                        state <= write_second;
                    when write_second =>
                        if op = VFSI then fa <= fa+1; fo <= imm1; fw <= '1';
                        else sa <= sa+1; so <= imm1; sw <= '1'; end if;
                        state <= write_drain;
                    when write_drain => state <= done;
                    when desc_wait =>
                        if DESC_READY = '1' then dv <= '0'; state <= done; end if;
                    when done =>
                        if VALID = '0' then state <= idle; end if;
                end case;
            end if;
        end if;
    end process;
end;
