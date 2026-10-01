library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeIRCodes.all;

package FridgeRAMBootImage is

constant RAMBootImage : XCM2_RAM :=
(
     -- CPU/RAM smoke test
     -- Exercises MVI, MOV, ADD, STA, LDA, CMP, JZ, JMP, HLT.
     -- Pass: CPU halts (PC = 0x001A, HALTED = 1).
     -- Fail: CPU loops forever at 0x0016 (HALTED = 0).
     --
     -- addr  bytes          instruction
     16#00# => MVI_A,        -- A = 0x12
     16#01# => X"12",
     16#02# => MVI_B,        -- B = 0x34
     16#03# => X"34",
     16#04# => MVI_C,        -- C = 0x56
     16#05# => X"56",
     16#06# => ADD_B,        -- A = 0x12 + 0x34 = 0x46
     16#07# => ADD_C,        -- A = 0x46 + 0x56 = 0x9C
     16#08# => STA,          -- mem[0x0080] = 0x9C
     16#09# => X"00",
     16#0A# => X"80",
     16#0B# => MVI_A,        -- A = 0x00
     16#0C# => X"00",
     16#0D# => LDA,          -- A = mem[0x0080] = 0x9C
     16#0E# => X"00",
     16#0F# => X"80",
     16#10# => MVI_E,        -- E = 0x9C
     16#11# => X"9C",
     16#12# => CMP_E,        -- A == E?
     16#13# => JZ,           -- if zero (equal), jump to halt
     16#14# => X"00",
     16#15# => X"19",
     16#16# => JMP,          -- fail: infinite loop
     16#17# => X"00",
     16#18# => X"16",
     16#19# => HLT,          -- pass: halt
     others => X"00"
);

end FridgeRAMBootImage;
