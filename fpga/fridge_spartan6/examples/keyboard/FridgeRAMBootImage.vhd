library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeIRCodes.all;

package FridgeRAMBootImage is

constant RAMBootImage : XCM2_RAM :=
(
     -- Keyboard demo: live key-event tape.
     --
     -- VMODE TEXT then EGA (locks the A-bit-0 mode decode), clear the active
     -- frame (19200 bytes) to black, present it with VPRE MANUAL_11 so the
     -- displayed and active frames are both frame 1 (single-buffered live
     -- tape), then poll IIN 3 forever and paint each key-event byte as one
     -- framebuffer byte (two 4bpp pixels) at the tape cursor HL, wrapping at
     -- 19200. Press events have bit 7 set (bright left pixel), releases are
     -- dark; the byte value is the FRIDGE_KEYBOARD_* event byte.
     --
     -- addr  bytes          instruction
     16#00# => MVI_A,        -- A = 1 (TEXT)
     16#01# => X"01",
     16#02# => VMODE,
     16#03# => MVI_A,        -- A = 0 (EGA)
     16#04# => X"00",
     16#05# => VMODE,
     -- clear active frame: 75 * 256 = 19200 zero bytes
     16#06# => MVI_A,        -- A = 0 (black)
     16#07# => X"00",
     16#08# => MVI_H,        -- HL = 0x0000
     16#09# => X"00",
     16#0A# => MVI_L,
     16#0B# => X"00",
     16#0C# => MVI_B,        -- B = 75 outer
     16#0D# => X"4B",
     16#0E# => MVI_C,        -- C = 0 -> 256 inner (wraps)
     16#0F# => X"00",
     16#10# => VFSA,         -- frame[HL] = 0
     16#11# => INX_HL,
     16#12# => DCR_C,
     16#13# => JNZ,          -- inner
     16#14# => X"00",
     16#15# => X"10",
     16#16# => DCR_B,
     16#17# => JNZ,          -- outer
     16#18# => X"00",
     16#19# => X"10",
     -- present: VPRE MANUAL_11 (A=5): visible=1, active=1
     16#1A# => MVI_H,        -- offsets (0,0)
     16#1B# => X"00",
     16#1C# => MVI_L,
     16#1D# => X"00",
     16#1E# => MVI_A,        -- A = 5 (SWAP_MANUAL_11)
     16#1F# => X"05",
     16#20# => VPRE,
     -- poll loop: tape key-event bytes at HL
     16#21# => IIN,          -- A = keyboard event (0 = none)
     16#22# => X"03",
     16#23# => CPI,
     16#24# => X"00",
     16#25# => JZ,
     16#26# => X"00",
     16#27# => X"21",
     16#28# => VFSA,         -- frame[HL] = A
     16#29# => INX_HL,
     16#2A# => MOV_AL,       -- wrap at 19200 (0x4B00)
     16#2B# => CPI,
     16#2C# => X"00",
     16#2D# => JNZ,
     16#2E# => X"00",
     16#2F# => X"21",
     16#30# => MOV_AH,
     16#31# => CPI,
     16#32# => X"4B",
     16#33# => JNZ,
     16#34# => X"00",
     16#35# => X"21",
     16#36# => MVI_H,        -- HL = 0x0000
     16#37# => X"00",
     16#38# => JMP,
     16#39# => X"00",
     16#3A# => X"21",
     others => X"00"
);
end FridgeRAMBootImage;
