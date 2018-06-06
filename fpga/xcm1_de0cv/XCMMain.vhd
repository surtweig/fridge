library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package XCMGlobals is
     -- General
     constant XCMFreqToHostRatio : integer:= 10000; 
     constant XCMWordLength : integer:= 5;
     subtype XCMWord is integer range 0 to 31;
     --subtype XCMArrowButton is integer range 1 to 3;
     constant ARROW_UP : integer:= 1;
     constant ARROW_NONE : integer:= 2;
     constant ARROW_DOWN : integer:= 3;
     
     -- ROM
     constant XCMROMSize : integer:= 1024*3;
     subtype XCMROMAddr is integer range 0 to XCMROMSize-1;     
     type XCMIRData is array (0 to 2) of XCMWord;
     type XCMROMData is array (0 to XCMROMSize-1) of XCMWord;
     
     -- RAM
     constant XCMRAMSize : integer:= 32;
     type XCMRAMData is array (0 to XCMRAMSize-1) of XCMWord;
     type XCMROBData is array (0 to 3) of XCMWord;
end XCMGlobals;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.XCMGlobals.all;

entity XCMMain is

port (
     CLOCK2_50 : in std_logic;
     --CLOCK_DEBUG : in std_logic;
     VGA_HS, VGA_VS : out std_logic;
     VGA_R, VGA_G, VGA_B : out std_logic_vector(3 downto 0);
     KEY : in std_logic_vector(3 downto 0);
     HEX0, HEX1, HEX4, HEX5 : out std_logic_vector(6 downto 0);
     LEDR : out std_logic_vector(9 downto 0);
     PS2_CLK, PS2_DAT, PS2_CLK2, PS2_DAT2 : in std_logic
     );
     
end XCMMain;

architecture main of XCMMain is

     signal VGACLK, RESET, VGAWritePixel, VGAPixelValue : std_logic:= '0';
     signal VGAPixelX, VGAPixelY : XCMWord;
     signal ROMSEL : std_logic:= '0';
     signal ROMIRData : XCMIRData:= (0, 0, 0);
     signal ROMIRAddr : XCMROMAddr:= 0;
     signal ROBData : XCMROBData:= (others => 0);
     signal CLOCK_5, XCMCLK : std_logic:= '0';
     signal PS2_CODE : std_logic_vector(7 downto 0):= (others => '0');
     signal PS2_NEW : std_logic:= '0';
     signal P1_ARROW, P2_ARROW : XCMWord:= ARROW_NONE;
     
     signal cpuCLKCounter : integer range 0 to XCMFreqToHostRatio-1;

     component vga_pll is
          port (
               vga_pll_clk_in_clk  : in  std_logic := 'X'; --  vga_pll_clk_in.clk
               vga_pll_clk_out_clk : out std_logic;        -- vga_pll_clk_out.clk
               vga_pll_reset_reset : in  std_logic := 'X'  --   vga_pll_reset.reset
          );
     end component vga_pll;
     
     component xcm_clk_pll is
          port (
               xcm_clk_pll_in_clk      : in  std_logic := '0'; --    xcm_clk_pll_in.clk
               xcm_clk_pll_out_clk     : out std_logic;        --   xcm_clk_pll_out.clk
               xcm_clk_pll_reset_reset : in  std_logic := '0'  -- xcm_clk_pll_reset.reset
          );
     end component xcm_clk_pll;     

     component VGAController is
          port (
               CLK : in std_logic;
               COMMAND_CLK : in std_logic;
               RESET : in std_logic;
               HSYNC, VSYNC : out std_logic;
               R, G, B : out std_logic_vector(3 downto 0);
               WritePixel : in std_logic;     
               PixelValue : in std_logic;
               PixelX : in XCMWord;
               PixelY : in XCMWord               
               );
     end component VGAController;
     
     component XCMROM is
          port (
               SEL : in std_logic;
               IRAddr : in XCMROMAddr;
               IRData : out XCMIRData
          );          
     end component XCMROM;
     
     component XCMCPU is
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
     end component XCMCPU;
     
     component ps2_keyboard is
          port (
               clk          : IN  STD_LOGIC;                     --system clock
               ps2_clk      : IN  STD_LOGIC;                     --clock signal from PS/2 keyboard
               ps2_data     : IN  STD_LOGIC;                     --data signal from PS/2 keyboard
               ps2_code_new : OUT STD_LOGIC;                     --flag that new PS/2 code is available on ps2_code bus
               ps2_code     : OUT STD_LOGIC_VECTOR(7 DOWNTO 0)   --code received from PS/2     
          ); 
     end component ps2_keyboard;
     
     component XCMKeyboard is
          port (
               CODE_NEW : in std_logic;
               CODE : in std_logic_vector(7 downto 0);
               P1_ARROW : out XCMWord;
               P2_ARROW : out XCMWord
          );
     end component XCMKeyboard;
     
begin
     C_VGA : VGAController port map (VGACLK, XCMCLK, KEY(0), VGA_HS, VGA_VS, VGA_R, VGA_G, VGA_B, VGAWritePixel, VGAPixelValue, VGAPixelX, VGAPixelY);
     C_VGA_PLL : vga_pll port map (CLOCK2_50, VGACLK, RESET);
     C_XCM_PLL : xcm_clk_pll port map (CLOCK2_50, XCMCLK, RESET);
     C_ROM : XCMROM port map (XCMCLK, ROMIRAddr, ROMIRData);
     C_CPU : XCMCPU port map (ROMIRAddr, ROMIRData, ROBData, XCMCLK, KEY(0), VGAWritePixel, VGAPixelValue, VGAPixelX, VGAPixelY, HEX0, HEX1, HEX4, HEX5);
     C_KEYBOARD : ps2_keyboard port map (XCMCLK, PS2_CLK, PS2_DAT, PS2_NEW, PS2_CODE);
     C_XCM_KEYBOARD : XCMKeyboard port map (PS2_NEW, PS2_CODE, P1_ARROW, P2_ARROW);
     --process (PS2_CLK)
     --begin
     --     if rising_edge(PS2_CLK) then
     --          LEDR(1) <= not LEDR(1);
     --     end if;
     --end process;

     process (XCMCLK)
     begin
          if rising_edge(XCMCLK) then
               if KEY(0) = '0' then
                    ROBData <= (others => ARROW_NONE);
               else
                    ROBData(0) <= P1_ARROW;
                    ROBData(1) <= P2_ARROW;
               end if;
          end if;
     end process;
     
     process (PS2_NEW)
     begin
          if rising_edge(PS2_NEW) then          
               LEDR(0) <= not LEDR(0);
               
               LEDR(1) <= PS2_CODE(0);
               LEDR(2) <= PS2_CODE(1);
               LEDR(3) <= PS2_CODE(2);
               LEDR(4) <= PS2_CODE(3);
               LEDR(5) <= PS2_CODE(4);
               LEDR(6) <= PS2_CODE(5);
               LEDR(7) <= PS2_CODE(6);
               LEDR(8) <= PS2_CODE(7);
          end if;
     end process;
     
     --process (CLOCK_5)
     --begin
     --     if rising_edge(CLOCK_5) then
     --          if (cpuCLKCounter = XCMFreqToHostRatio-1) then
     --               cpuCLKCounter <= 0;
     --               XCMCLK <= not XCMCLK;
     --          else
     --               cpuCLKCounter <= cpuCLKCounter + 1;
     --          end if;
     --     end if;
     --end process;
     
     --process (CLOCK2_50)
     --begin
     --     if rising_edge(CLOCK2_50) then
     --          if (cpuCLKCounter = XCMFreqToHostRatio-2) then
     --               ROMSEL <= '1';
     --          else
     --               ROMSEL <= '0';
     --          end if;
     --               
     --          if (cpuCLKCounter < XCMFreqToHostRatio-1) then
     --               cpuCLKCounter <= cpuCLKCounter + 1;
     --               XCMCLK <= '0';
     --          else
     --               cpuCLKCounter <= 0;
     --               XCMCLK <= '1';
     --          end if;
     --     end if;
     --end process;
     --
     --process (XCMCLK)
     --begin
--
     --end process;
end main;     