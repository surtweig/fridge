library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;

use work.XCMGlobals.all;
use work.XCMIRCodes.all;

entity XCMCPU is

port (
     IRAddr : out XCMROMAddr;
     IRData : in XCMIRData;
     ROBData : in XCMROBData;
     XCMCLK : in std_logic;
     RESET : in std_logic;
     VGAWritePixel : out std_logic;
     VGAPixelValue : out std_logic;
     VGAPixelX, VGAPixelY : out XCMWord;
     HEX0, HEX1, HEX4, HEX5 : out std_logic_vector(6 downto 0)
);

end XCMCPU;

architecture main of XCMCPU is     
     signal ram : XCMRAMData:= (others => 0);
     signal IRAddrSrc : XCMROMAddr:= 0;
     signal tickSkipCounter : integer range 0 to XCMFreqToHostRatio;
     
     component LUTDisplay is
          port (
               Word : in XCMWord;
               HEXH, HEXL : out std_logic_vector(6 downto 0)
          );
     end component LUTDisplay;
begin

     C_LUT_LEFT : LUTDisplay port map (ram(30), HEX5, HEX4);
     C_LUT_RIGHT : LUTDisplay port map (ram(31), HEX1, HEX0);

     process (XCMCLK)
          variable writePixel : std_logic:= '0';
          variable irbuffer : XCMIRData;
          variable addrStep : XCMWord:= 0;
     begin     
          if falling_edge(XCMCLK) then
               if RESET = '0' then
                    IRAddr <= 0;
               else
                    IRAddr <= IRAddrSrc;
               end if;
          end if;
          if rising_edge(XCMCLK) then
               if (tickSkipCounter < XCMFreqToHostRatio) then
                    tickSkipCounter <= tickSkipCounter + 1;
               else
                    irbuffer:= IRData;
                    addrStep:= 3;
                    if RESET = '0' then
                         --IRAddr <= 0;
                         IRAddrSrc <= 0;
                         tickSkipCounter <= 0;
                         writePixel:= '0';
                    else               
                         tickSkipCounter <= 0;
                         writePixel:= '0';
                         VGAPixelValue <= '1';               
                         
                         case irbuffer(0) is
                              when IR_JMP =>
                                   IRAddrSrc <= (irbuffer(1) + irbuffer(2)*32)*3;

                              when IR_CMPRC =>
                                   if ram(irbuffer(1)) /= irbuffer(2) then
                                        addrStep:= 6;
                                   end if;
                                   
                              when IR_NEQRC =>
                                   if ram(irbuffer(1)) = irbuffer(2) then
                                        addrStep:= 6;
                                   end if;
                                   
                              when IR_CMPBC =>
                                   if ROBData(irbuffer(1)) /= irbuffer(2) then
                                        addrStep:= 6;
                                   end if;
                                   
                              when IR_NEQBC =>
                                   if ROBData(irbuffer(1)) = irbuffer(2) then
                                        addrStep:= 6;
                                   end if;
                                   
                              when IR_CMPRR =>
                                   if ram(irbuffer(1)) /= ram(irbuffer(2)) then
                                        addrStep:= 6;
                                   end if;
                                   
                              when IR_NEQRR =>
                                   if ram(irbuffer(1)) = ram(irbuffer(2)) then
                                        addrStep:= 6;
                                   end if; 
                                   
                              when IR_SET =>
                                   ram(irbuffer(1)) <= irbuffer(2);
                                   
                              when IR_CPY =>
                                   ram(irbuffer(2)) <= ram(irbuffer(1));
                                   
                              when IR_INC =>
                                   ram(irbuffer(1)) <= ram(irbuffer(1)) + 1;
                                   
                              when IR_DEC =>
                                   ram(irbuffer(1)) <= ram(irbuffer(1)) - 1;
                                   
                              when IR_DRWR =>
                                   writePixel:= '1';
                                   VGAPixelValue <= '1';
                                   VGAPixelX <= ram(irbuffer(1));
                                   VGAPixelY <= ram(irbuffer(2));
                                   
                              when IR_DRWC =>
                                   writePixel:= '1';
                                   VGAPixelValue <= '1';
                                   VGAPixelX <= irbuffer(1);
                                   VGAPixelY <= irbuffer(2);
                                   
                              when IR_ERSR =>
                                   writePixel:= '1';
                                   VGAPixelValue <= '0';
                                   VGAPixelX <= ram(irbuffer(1));
                                   VGAPixelY <= ram(irbuffer(2));
                                   
                              when IR_ERSC =>
                                   writePixel:= '1';
                                   VGAPixelValue <= '0';
                                   VGAPixelX <= irbuffer(1);
                                   VGAPixelY <= irbuffer(2);
                              
                              when IR_BCPY =>
                                   ram(irbuffer(2)) <= ROBData(irbuffer(1));
                                   
                         
                              --when 1 =>
                              --     writePixel:= '1';
                              --     VGAPixelValue <= '1';
                              --     VGAPixelX <= irbuffer(1);
                              --     VGAPixelY <= irbuffer(2);                         
                              when others => null;
                         end case;
                    
                         if irbuffer(0) /= IR_JMP then
                              if (IRAddrSrc < XCMROMSize-addrStep) then
                                   IRAddrSrc <= IRAddrSrc + addrStep;
                              else
                                   IRAddrSrc <= 0;
                              end if;
                         end if;
                         
                         --IRAddr <= IRAddrSrc;               
                         VGAWritePixel <= writePixel;
                         
                         
                    end if;
               end if;
          end if;
     end process;
     
end main;