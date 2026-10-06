library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeIRCodes.all;

package FridgeRAMBootImage is

constant RAMBootImage : XCM2_RAM :=
(
     -- HDMI/GPU integration demo: 16 horizontal color bars via VFSA.
     -- VMODE 0 (EGA), then fill the active frame (19200 bytes) with
     -- bar i = byte i*0x11 for rows 10i..10i+9, then VPRE AUTO and HLT.
     -- Pass: bars 0..15 fill the 960x640 window (black, blue, ..., white),
     -- margins blue, LED0 halted.
     --
     -- addr  bytes          instruction
     16#00# => MVI_A,        -- A = 0 (EGA)
     16#01# => X"00",
     16#02# => VMODE,
     16#03# => MVI_H,        -- HL = 0x0000
     16#04# => X"00",
     16#05# => MVI_L,
     16#06# => X"00",
     16#07# => MVI_E,        -- E = 0x00 (bar color byte)
     16#08# => X"00",
     16#09# => MVI_B,        -- B = 16 bars
     16#0A# => X"10",
     16#0B# => MVI_C,        -- C = 10 rows per bar
     16#0C# => X"0A",
     16#0D# => MVI_D,        -- D = 120 bytes per row
     16#0E# => X"78",
     16#0F# => VFSA,         -- frame[HL] = A
     16#10# => INX_HL,
     16#11# => DCR_D,
     16#12# => JNZ,          -- bytes
     16#13# => X"00",
     16#14# => X"0F",
     16#15# => DCR_C,
     16#16# => JNZ,          -- rows
     16#17# => X"00",
     16#18# => X"0D",
     16#19# => MOV_AE,       -- next bar color
     16#1A# => ADI,
     16#1B# => X"11",
     16#1C# => MOV_EA,
     16#1D# => DCR_B,
     16#1E# => JNZ,          -- bars
     16#1F# => X"00",
     16#20# => X"0B",
     16#21# => MVI_H,        -- offsets (0,0) for VPRE
     16#22# => X"00",
     16#23# => MVI_L,
     16#24# => X"00",
     16#25# => MVI_A,        -- A = 1 (SWAP_AUTO)
     16#26# => X"01",
     16#27# => VPRE,
     16#28# => HLT,
     others => X"00"
);

end FridgeRAMBootImage;
