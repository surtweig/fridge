library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;

use work.XCMGlobals.all;

entity XCMKeyboard is
     generic (
          constant KeyBreakF0 : unsigned:= X"F0";
          constant KeyBreakE0 : unsigned:= X"E0";
          constant KeyP1Up : unsigned:= X"1D";
          constant KeyP1Down : unsigned:= X"1B";
          constant KeyP2Up : unsigned:= X"44";
          constant KeyP2Down : unsigned:= X"4B"
     );

     port (
          CODE_NEW : in std_logic;
          CODE : in std_logic_vector(7 downto 0);
          P1_ARROW : out XCMWord;
          P2_ARROW : out XCMWord
     );
end XCMKeyboard;

architecture main of XCMKeyboard is
     signal P1Up : std_logic:= '0';
     signal P1Down : std_logic:= '0';
     signal P2Up : std_logic:= '0';
     signal P2Down : std_logic:= '0';
     signal Break : std_logic:= '0';
     signal BreakBuf : std_logic:= '0';
begin
     process (CODE)
          variable ucode : unsigned (7 downto 0);
          --variable breakbuf : std_logic;
     begin
          if CODE_NEW = '1' then
               ucode:= unsigned(CODE);
               --breakbuf:= Break;
               if ucode = KeyBreakF0 then
                    Break <= '1';
               else
                    case ucode is
                        when KeyP1Up => P1Up <= not BreakBuf;
                        when KeyP1Down => P1Down <= not BreakBuf;
                        when KeyP2Up => P2Up <= not BreakBuf;
                        when KeyP2Down => P2Down <= not BreakBuf;
                        when others => null;
                    end case;
                    Break <= '0';
               end if;
               
               if (P1Up = '1') then
                    P1_ARROW <= ARROW_UP;
               elsif (P1Down = '1') then
                    P1_ARROW <= ARROW_DOWN;
               else
                    P1_ARROW <= ARROW_NONE;
               end if;
               
               if (P2Up = '1') then
                    P2_ARROW <= ARROW_UP;
               elsif (P2Down = '1') then
                    P2_ARROW <= ARROW_DOWN;
               else
                    P2_ARROW <= ARROW_NONE;
               end if;
          end if;
          
          if falling_edge(CODE_NEW) then
               BreakBuf <= Break;
          end if;
     end process;
end main;