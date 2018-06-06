library std;
use std.textio.all;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.XCMGlobals.all;
use work.ROMImage.all;
use work.PongROMImage.all;
--use work.pong_debugROMImage.all;
use work.XCMIRCodes.all;

entity XCMROM is

port (
     SEL : in std_logic;
     IRAddr : in XCMROMAddr;
     IRData : out XCMIRData
);

type BinFile is file of character;
function ReadROMFile(FileName : string) return XCMROMData is
     file FileHandle : BinFile open READ_MODE is FileName;
     variable c0, c1 : character;  
     variable Result : XCMROMData;

begin
     for i in 0 to XCMROMSize - 1 loop
          exit when endfile(FileHandle);
          read(FileHandle, c0);
          read(FileHandle, c1);
          --readline(FileHandle, CurrentLine);
          --hread(CurrentLine, TempWord);
          --Result(i)    := resize(TempWord, XCMIRData'length);
     end loop;     
     return Result;
end function;
     
end XCMROM;

architecture main of XCMROM is
     
     signal data : XCMROMData:= pongData;--ROMImageData;--(others => (0, 2, 0));
     
begin
     process (IRAddr) begin
          IRData(0) <= data(IRAddr);--(0, 1, 1);
          IRData(1) <= data(IRAddr+1);
          IRData(2) <= data(IRAddr+2);
     end process;
     --IRData <= data(IRAddr);
     --process (SEL)
     --begin
     --     if rising_edge(SEL) then
     --          IRData <= data(IRAddr);
     --     end if;
     --end process;
end main;