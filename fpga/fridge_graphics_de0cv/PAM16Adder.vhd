library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;

use work.FridgeGlobals.all;

entity PAM16Adder is

port (
    CLK : in std_logic;
    
    ENABLED : in std_logic; 
    READY : out std_logic;
    A, B : in PAM16_POSIT_UNPACKED;
    RESULT : out PAM16_POSIT_UNPACKED
);

end PAM16Adder;

architecture adder_main of PAM16Unpacker is

    process (CLK) is begin
    end process;

end adder_main;