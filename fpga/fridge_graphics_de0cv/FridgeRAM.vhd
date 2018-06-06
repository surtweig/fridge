library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;

entity FridgeRAM is 

generic ( INIT_DATA : XCM2_RAM );

port (
     CLK : in std_logic;
     WRITE_DATA : in XCM2_WORD;
     WRITE_ADDR : in XCM2_DWORD;
     WRITE_ENABLED : in std_logic;

     READ_DATA : out XCM2_WORD;
     READ_ADDR : in XCM2_DWORD
);

end FridgeRAM;

architecture main of FridgeRAM is
     signal mem : XCM2_RAM:= INIT_DATA;
     signal read_addr_reg : XCM2_DWORD;
begin     
     process (CLK)
     begin
          if rising_edge(CLK) then
               if WRITE_ENABLED = '1' then
                    mem(to_integer(WRITE_ADDR)) <= WRITE_DATA;
               end if;
               read_addr_reg <= READ_ADDR;
          end if;
     end process;
     
     READ_DATA <= mem(to_integer(read_addr_reg));
end main;