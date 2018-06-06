library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;
use work.XCMGlobals.all;

entity VGAController is

generic (
     constant HRange : integer:= 1688;
     constant VRange : integer:= 1066;
     constant ScreenWidth : integer:= 1280;
     constant ScreenHeight : integer:= 1024;

     constant HStart : integer:= 408;--HRange-ScreenWidth;
     constant VStart : integer:= 42;--VRange-ScreenHeight;
     
     constant BufferWidth : integer:= 32;
     constant BufferHeight : integer:= 32;
     constant ScreenHorOffset : integer:= 128;
     constant PixelSize : integer:= 32;
     
     constant ClearBuffer : std_logic_vector(0 to 32*32-1):= (others => '0')
);
port (
     CLK : in std_logic;
     COMMAND_CLK : in std_logic;
     RESET : in std_logic;
     HSYNC, VSYNC : out std_logic;
     R, G, B : out std_logic_vector(3 downto 0);
     
     WritePixel : in std_logic;     
     PixelValue : in std_logic;
     PixelX : XCMWord;
     PixelY : XCMWord
);
     
end VGAController;

architecture main of VGAController is
     signal HPOS : integer range 0 to HRange:= 0;
     signal VPOS : integer range 0 to VRange:= 0;
         
     signal vbuffer : std_logic_vector(0 to BufferWidth*BufferHeight-1):= (
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',
          '0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0'
     );
     signal vbuffer_horcounter : integer range 0 to BufferWidth:= 0;
     signal vbuffer_vercounter : integer range 0 to BufferHeight:= 0;
     signal vbuffer_line_addr : integer range 0 to BufferWidth*BufferHeight-1:= 0;
     signal screen_horcounter : integer range 0 to ScreenWidth:= 0;
     signal screen_vercounter : integer range 0 to ScreenHeight:= 0;
     signal subpixel_horcounter : integer range 0 to PixelSize-1:= 0;
     signal subpixel_vercounter : integer range 0 to PixelSize-1:= 0;
     
begin
     process (COMMAND_CLK)
     begin
          if falling_edge(COMMAND_CLK) then
               if (RESET = '0') then
                    vbuffer <= ClearBuffer;
               elsif (WritePixel = '1') then
                    vbuffer(PixelY*BufferWidth + PixelX) <= PixelValue;
               end if;
          end if;
     end process;

     process (CLK)
     begin     
          if rising_edge(CLK) then
               --if (WritePixel = '1') then
               --     vbuffer(PixelY*BufferWidth + PixelX) <= PixelValue;
               --end if;
         
               if (HPOS >= HStart and VPOS >= VStart) then
                    if (screen_horcounter < ScreenWidth) then
                         screen_horcounter <= screen_horcounter + 1;
                    else
                         screen_horcounter <= 0;
                         if (screen_vercounter < ScreenHeight) then
                              screen_vercounter <= screen_vercounter + 1;
                              --vbuffer_horcounter <= 0;
                         else
                              screen_vercounter <= 0;
                              --subpixel_counter <= 0;
                              --vbuffer_horcounter <= 0;
                              --vbuffer_vercounter <= 0;
                              subpixel_vercounter <= 0;
                              vbuffer_vercounter <= 0;
                              vbuffer_line_addr <= 0;  

                         end if;                         
                    end if;
                    
                    if (screen_horcounter >= ScreenHorOffset and screen_horcounter < ScreenWidth-ScreenHorOffset) then
                         if (subpixel_horcounter < PixelSize-1) then
                              subpixel_horcounter <= subpixel_horcounter + 1;
                         else 
                              subpixel_horcounter <= 0;
                              if (vbuffer_horcounter < BufferWidth-1) then
                                   vbuffer_horcounter <= vbuffer_horcounter + 1;                                   
                              else
                                   vbuffer_horcounter <= 0;
                                   
                                   -- TODO!!! vertical subpixel_counter
                                   if (subpixel_vercounter < PixelSize-1) then
                                        subpixel_vercounter <= subpixel_vercounter + 1;
                                   else
                                        subpixel_vercounter <= 0;
                                        vbuffer_vercounter <= vbuffer_vercounter + 1;
                                        vbuffer_line_addr <= vbuffer_line_addr + BufferWidth;
                                   end if;
                                    
                                   --if (vbuffer_vercounter < BufferHeight-1) then
                                   --     vbuffer_vercounter <= vbuffer_vercounter + 1;
                                   --else
                                   --     vbuffer_vercounter <= 0;
                                   --     vbuffer_addr <= 0;
                                   --end if;
                              end if;
                         end if;
                                                
                         --R <= (others=>'0');--std_logic_vector(to_unsigned(subpixel_horcounter, 6))(4 downto 1);
                         --G <= std_logic_vector(to_unsigned(vbuffer_vercounter, 6))(4 downto 1);
                         --B <= std_logic_vector(to_unsigned(vbuffer_horcounter, 6))(4 downto 1);
                         if (vbuffer(vbuffer_line_addr + vbuffer_horcounter) = '1') then
                              R <= (others => '0');
                              G <= (others => '1');
                              B <= (others => '0');
                         else
                              R <= (others => '0');
                              G <= (others => '0');
                              B <= (others => '0');               
                         end if;
                    else
                         R <= ('0', '0', '1', '1');--(others=>'0');
                         G <= ('0', '0', '1', '1');--(others=>'0');
                         B <= ('0', '0', '1', '1');--(others=>'0');
                    end if;
                    
                    
               end if;
                  
               if (HPOS < HRange) then
                    HPOS <= HPOS + 1;
               else
                    HPOS <= 0;
                    if (VPOS < VRange) then                    
                         VPOS <= VPOS + 1;                         
                    else
                         VPOS <= 0;
                    end if;
               end if;
          
               if (HPOS > 48 and HPOS < 160) then
                    HSYNC <= '0';
               else
                    HSYNC <= '1';
               end if;
               
               if (VPOS > 0 and VPOS < 4) then
                    VSYNC <= '0';
               else
                    VSYNC <= '1';
               end if;
               
               if ((HPOS > 0 and HPOS < 408) or (VPOS > 0 and VPOS < 42)) then
                    R <= (others => '0');
                    G <= (others => '0');
                    B <= (others => '0');
               end if;
          end if;
     end process;
     
end main;     