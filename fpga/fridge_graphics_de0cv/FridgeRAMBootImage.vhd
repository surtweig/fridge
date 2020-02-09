library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeIRCodes.all;

package FridgeRAMBootImage is

constant RAMBootImage : XCM2_RAM:= 
( 
     --VPRE,
     --VMODE, X"01",
     --HLT,
     --others => X"00"
     
     --VMODE, X"01",
     --MVI_A, X"03",
     --LXI_HL, X"01", X"00",
     --VFSA,
     --
     --INX_HL,
     --INX_HL,
     --INR_A,
     --VFSA,
     --
     --INX_HL,
     --INX_HL,
     --INR_A,
     --VFSA,
--
     --INX_HL,
     --INX_HL,
     --INR_A,
     --VFSA,
--
     --INX_HL,
     --INX_HL,
     --INR_A,
     --VFSA,
--
     --HLT,

-- Hello, World     
--X"bb", X"00", X"5a", X"3d", X"08", X"42", X"00", X"00", X"3a", X"00", X"ba", X"03", X"b6", X"16", X"be", X"00", 
--X"17", X"05", X"50", X"24", X"06", X"51", X"2b", X"ba", X"01", X"b5", X"08", X"ba", X"02", X"b5", X"0f", X"01", 
--X"60", X"00", X"08", X"7f", X"bd", X"00", X"2a", X"bb", X"00", X"0b", X"cd", X"da", X"d8", X"39", X"28", X"16", 
--X"42", X"00", X"28", X"dc", X"10", X"3a", X"00", X"8b", X"06", X"b5", X"2b", X"05", X"b5", X"24", X"dc", X"44", 
--X"00", X"84", X"b5", X"b5", X"b5", X"b5", X"16", X"44", X"00", X"85", X"52", X"16", X"48", X"b2", X"00", X"cf", 
--X"e7", X"85", X"03", X"e7", X"85", X"83", X"bb", X"00", X"4c", X"cd", X"f1", X"01", X"eb", X"39", X"0f", X"45", 
--X"00", X"84", X"39", X"01", X"45", X"00", X"85", X"3a", X"01", X"3b", X"01", X"42", X"00", X"76", X"c4", X"00", 
--X"2b", X"f0", X"e4", X"bb", X"00", X"00", X"48", X"65", X"6c", X"6c", X"6f", X"2c", X"20", X"57", X"6f", X"72", 
--X"6c", X"64", X"21", X"00", X"00", X"0f", 
     
     --X"bb", X"00", X"5a", X"3d", X"08", X"42", X"00", X"00", X"3a", X"00", X"ba", X"03", X"b6", X"16", X"be", X"00", 
     --X"17", X"05", X"50", X"24", X"06", X"51", X"2b", X"ba", X"01", X"b5", X"08", X"ba", X"02", X"b5", X"0f", X"01", 
     --X"60", X"00", X"08", X"7f", X"bd", X"00", X"2a", X"bb", X"00", X"0b", X"cd", X"da", X"d8", X"39", X"28", X"16", 
     --X"c4", X"00", X"03", X"dc", X"10", X"3a", X"00", X"8b", X"06", X"b5", X"2b", X"05", X"b5", X"24", X"dc", X"44", 
     --X"00", X"84", X"b5", X"b5", X"b5", X"b5", X"16", X"44", X"00", X"85", X"52", X"16", X"48", X"b2", X"00", X"cf", 
     --X"e7", X"85", X"03", X"e7", X"85", X"83", X"bb", X"00", X"4c", X"cd", X"f1", X"01", X"eb", X"39", X"01", X"45", 
     --X"00", X"84", X"39", X"0f", X"45", X"00", X"85", X"3a", X"01", X"3b", X"01", X"42", X"00", X"76", X"c4", X"00", 
     --X"2b", X"f0", X"e4", X"bb", X"00", X"00", X"48", X"65", X"6c", X"6c", X"6f", X"2c", X"20", X"57", X"6f", X"72", 
     --X"6c", X"64", X"21", X"00", X"00", X"0f", 
others => X"00"     
);

end FridgeRAMBootImage;