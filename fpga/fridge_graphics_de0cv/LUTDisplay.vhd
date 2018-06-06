library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;
use work.FridgeGlobals.all;

entity LUTDisplay is

port (
     Word : in XCM2_WORD;
     HEXH, HEXL : out std_logic_vector(6 downto 0)
);

end LUTDisplay;

architecture main of LUTDisplay is
begin
     process (Word)
          variable wL : XCM2_WORD:= X"00";
          variable wH : XCM2_WORD:= X"00";    
     begin
          -- 00000011
          wH(4 to 7):= Word(0 to 3);-- / X"10";
          wL(4 to 7):= Word(4 to 7);-- - wH * X"10";
          
--    --0--
--    5   1
--    |-6-|
--    4   2
--    --3--
          
          case wH is
               when X"00" => HEXH <= b"1000000";
               when X"01" => HEXH <= b"1111001";
               when X"02" => HEXH <= b"0100100";
               when X"03" => HEXH <= b"0110000";
               when X"04" => HEXH <= b"0011001";
               when X"05" => HEXH <= b"0010010";
               when X"06" => HEXH <= b"0000010";
               when X"07" => HEXH <= b"1111000";
               when X"08" => HEXH <= b"0000000";
               when X"09" => HEXH <= b"0010000";
               when X"0A" => HEXH <= b"0001000"; -- A
               when X"0B" => HEXH <= b"0000011"; -- b
               when X"0C" => HEXH <= b"1000110"; -- C
               when X"0D" => HEXH <= b"0100001"; -- d
               when X"0E" => HEXH <= b"0000110"; -- E
               when X"0F" => HEXH <= b"0001110"; -- F
               when others => HEXH <= b"1111111";
          end case;
          
          case wL is
               when X"00" => HEXL <= b"1000000";
               when X"01" => HEXL <= b"1111001";
               when X"02" => HEXL <= b"0100100";
               when X"03" => HEXL <= b"0110000";
               when X"04" => HEXL <= b"0011001";
               when X"05" => HEXL <= b"0010010";
               when X"06" => HEXL <= b"0000010";
               when X"07" => HEXL <= b"1111000";
               when X"08" => HEXL <= b"0000000";
               when X"09" => HEXL <= b"0010000";
               when X"0A" => HEXL <= b"0001000"; -- A
               when X"0B" => HEXL <= b"0000011"; -- b
               when X"0C" => HEXL <= b"1000110"; -- C
               when X"0D" => HEXL <= b"0100001"; -- d
               when X"0E" => HEXL <= b"0000110"; -- E
               when X"0F" => HEXL <= b"0001110"; -- F
               when others => HEXL <= b"1111111";
          end case;          
     end process;
end main;