library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;
use work.XCMGlobals.all;

entity LUTDisplay is

port (
     Word : in XCMWord;
     HEXH, HEXL : out std_logic_vector(6 downto 0)
);

end LUTDisplay;

architecture main of LUTDisplay is
begin
     process (Word)
          variable wL : XCMWord:= 0;
          variable wH : XCMWord:= 0;    
     begin
          wH:= Word / 10;
          wL:= Word - wH * 10;
          
          case wH is
               when 0 => HEXH <= b"1000000";
               when 1 => HEXH <= b"1111001";
               when 2 => HEXH <= b"0100100";
               when 3 => HEXH <= b"0110000";
               when 4 => HEXH <= b"0011001";
               when 5 => HEXH <= b"0010010";
               when 6 => HEXH <= b"0000010";
               when 7 => HEXH <= b"1111000";
               when 8 => HEXH <= b"0000000";
               when 9 => HEXH <= b"0010000";
               when others => null;
          end case;
          
          case wL is
               when 0 => HEXL <= b"1000000";
               when 1 => HEXL <= b"1111001";
               when 2 => HEXL <= b"0100100";
               when 3 => HEXL <= b"0110000";
               when 4 => HEXL <= b"0011001";
               when 5 => HEXL <= b"0010010";
               when 6 => HEXL <= b"0000010";
               when 7 => HEXL <= b"1111000";
               when 8 => HEXL <= b"0000000";
               when 9 => HEXL <= b"0010000";
               when others => null;
          end case;          
     end process;
end main;