library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;
use work.FridgeGlobals.all;
use work.FridgeRasterFont.all;
use work.FridgeWelcomeScreen.all;

entity FridgeGraphicsAdapter is

generic (
     constant HRange : integer:= 1688;
     constant VRange : integer:= 1066;
     constant ScreenWidth : integer:= 1280;
     constant ScreenHeight : integer:= 1024;

     constant HStart : integer:= 408;--HRange-ScreenWidth;
     constant VStart : integer:= 42;--VRange-ScreenHeight;
     
     constant BufferWidth : integer:= XCM2_VIDEO_FRAME_WIDTH;
     constant BufferHeight : integer:= XCM2_VIDEO_FRAME_HEIGHT;
     constant ScreenHorOffset : integer:= 40;
     constant ScreenVerOffset : integer:= 112;
     constant PixelSize : integer:= 5;
     
     constant VTextModeCharWidth : integer:= 6;
     constant VTextModeCharHeight : integer:= 8;
     constant VTextModeRows : integer:= 20; -- XCM2_VIDEO_FRAME_HEIGHT/VTextModeCharHeight
     constant VTextModeCols : integer:= 40; -- XCM2_VIDEO_FRAME_WIDTH/VTextModeCharWidth
   
     constant DefaultPalette : XCM2_VIDEO_PALETTE:= (
          ('0','0','0','0',  '0','0','0','0',  '0','0','0','0'), -- black
          ('0','0','0','0',  '0','0','0','0',  '1','0','0','0'), -- blue
          ('0','0','0','0',  '1','0','0','0',  '0','0','0','0'), -- green
          ('0','0','0','0',  '1','0','0','0',  '1','0','0','0'), -- cyan
          
          ('1','0','0','0',  '0','0','0','0',  '0','0','0','0'), -- red
          ('1','0','0','0',  '0','0','0','0',  '1','0','0','0'), -- magenta
          ('1','0','0','0',  '0','1','0','0',  '0','0','0','0'), -- brown
          ('1','0','0','0',  '1','0','0','0',  '1','0','0','0'), -- light gray
          
          ('0','1','0','0',  '0','1','0','0',  '0','1','0','0'), -- dark gray
          ('0','1','0','0',  '0','1','0','0',  '1','1','1','1'), -- bright blue
          ('0','1','0','0',  '1','1','1','1',  '0','1','0','0'), -- bright green
          ('0','1','0','0',  '1','1','1','1',  '1','1','1','1'), -- bright cyan 
          
          ('1','1','1','1',  '0','1','0','0',  '0','1','0','0'), -- bright red
          ('1','1','1','1',  '0','1','0','0',  '1','1','1','1'), -- bright magenta
          ('1','1','1','1',  '1','1','1','1',  '0','1','0','0'), -- bright yellow
          ('1','1','1','1',  '1','1','1','1',  '1','1','1','1')  -- white            
     )
);
port (
     CLK : in std_logic;
     COMMAND_CLK : in std_logic;
     RESET : in std_logic;
     HSYNC, VSYNC : out std_logic;
     R, G, B : out std_logic_vector(3 downto 0);
     
     MODE_SWITCH : in std_logic_vector(0 to 1);
     PALETTE_SWITCH : in std_logic;
     PRESENT_TRIGGER : in std_logic;
     
     BACK_STORE : in std_logic;
     BACK_LOAD : in std_logic;
     BACK_ADDR : in XCM2_DWORD;
     BACK_DATA : inout XCM2_WORD;
     BACK_CLR : in std_logic:= '0';

     VMEM_STORE : in std_logic;
     VMEM_LOAD : in std_logic;
     VMEM_ADDR : in XCM2_DWORD;
     VMEM_DATA : inout XCM2_WORD;
     
     DEBUG_BTN : in std_logic_vector(3 downto 0)
);

end FridgeGraphicsAdapter;

architecture main of FridgeGraphicsAdapter is
     signal HPOS : integer range 0 to HRange:= 0;
     signal VPOS : integer range 0 to VRange:= 0;
         
     --signal fbuf_a : XCM2_VIDEO_FBUF:= (others => 2);--(others => 2);--
     --signal fbuf_b : XCM2_VIDEO_FBUF:= (others => 2);--WelcomeScreenData;--(others => 2);--
     signal linebuffer, linebufferRead : XCM2_VIDEO_LINEBUF;
     signal visible_buf : std_logic:= '0';
     signal swap_next : std_logic:= '0';
     signal present_trigger_lock : std_logic:= '0';

     --signal vmem : XCM2_GRAM:= (others => 0);
     signal palette : XCM2_VIDEO_PALETTE:= DefaultPalette;
     signal vmode : XCM2_VIDEO_MODE:= XCM2_VIDEO_TEXT;

     signal vbuffer_horcounter : integer range 0 to XCM2_VIDEO_FRAME_WIDTH:= 0;
     signal vbuffer_vercounter : integer range 0 to XCM2_VIDEO_FRAME_HEIGHT:= 0;
     signal vbuffer_line_addr : integer range 0 to XCM2_VIDEO_FRAME_WIDTH*XCM2_VIDEO_FRAME_HEIGHT-1:= 0;
     signal screen_horcounter : integer range 0 to ScreenWidth:= 0;
     signal screen_vercounter : integer range 0 to ScreenHeight:= 0;
     signal subpixel_horcounter : integer range 0 to PixelSize-1:= 0;
     signal subpixel_vercounter : integer range 0 to PixelSize-1:= 0;
     
     signal text_rowcounter : integer range 0 to VTextModeRows:= 0;
     signal text_colcounter : integer range 0 to VTextModeCols:= 0;
     signal text_cellpixel_horcounter : integer range 0 to VTextModeCharWidth:= 0;
     signal text_cellpixel_vercounter : integer range 0 to VTextModeCharHeight:= 0;
     signal text_row_addr : integer range 0 to VTextModeRows*VTextModeCols*4; -- one actual char takes four indices (8bit ascii + 8bit color)
     
     component FridgeRAM is
          generic ( INIT_DATA : XCM2_RAM );
          port (
               CLK : in std_logic;
               WRITE_DATA : in XCM2_WORD;
               WRITE_ADDR : in XCM2_DWORD;
               WRITE_ENABLED : in std_logic;
          
               READ_DATA : out XCM2_WORD;
               READ_ADDR : in XCM2_DWORD
          );     
     end component FridgeRAM;
     
     signal gramReadData : XCM2_WORD;
     signal gramReadAddrOffset : XCM2_DWORD:= X"0000";
     signal gramReadAddrBase : XCM2_DWORD:= X"0000";
     signal gramReadAddr : XCM2_DWORD:= X"0000";
     signal gramReadEnabled : std_logic;
     signal vgaIdle : std_logic:= '0';
     signal vbufferSize : integer range 0 to XCM2_VIDEO_FRAME_WIDTH/2:= XCM2_VIDEO_FRAME_WIDTH/2;--*XCM2_VIDEO_FRAME_HEIGHT/2:= XCM2_VIDEO_FRAME_WIDTH*XCM2_VIDEO_FRAME_HEIGHT/2;
     signal lineSwapVPos : integer range 0 to VRange:= 0;
     signal colorPhase : unsigned(3 downto 0);
     signal colorPhaseDivider : integer range 0 to 2;
     
begin
     --process (COMMAND_CLK)
     --begin
     --     if falling_edge(COMMAND_CLK) then
     --          if (RESET = '0') then
     --               vbuffer <= ClearBuffer;
     --          elsif (WritePixel = '1') then
     --               vbuffer(PixelY*BufferWidth + PixelX) <= PixelValue;
     --          end if;
     --     end if;
     --end process;
     
     C_BACK_BUFFER : FridgeRAM 
          generic map (INIT_DATA => WelcomeScreenData)
          port map(COMMAND_CLK, BACK_DATA, BACK_ADDR, '1', gramReadData, gramReadAddr);     

     process (COMMAND_CLK)
     variable offset : XCM2_DWORD;
     begin
          if falling_edge(COMMAND_CLK) then
               if (gramReadEnabled = '1') then
                    offset:= gramReadAddrOffset;
                    if (to_integer(offset) < vbufferSize) then
                         linebufferRead(to_integer(offset)*2) <= to_integer(gramReadData(0 to 3));
                         linebufferRead(to_integer(offset)*2+1) <= to_integer(gramReadData(4 to 7));
                         gramReadAddr <= gramReadAddrBase + offset - 1;
                         gramReadAddrOffset <= offset + 1;
                         --fbuf_a(2 to XCM2_VIDEO_FRAME_HEIGHT*XCM2_VIDEO_FRAME_WIDTH-1) <= fbuf_a(0 to XCM2_VIDEO_FRAME_HEIGHT*XCM2_VIDEO_FRAME_WIDTH-3);
                         --fbuf_a(0) <= to_integer(gramReadData(0 to 3));
                         --fbuf_a(1) <= to_integer(gramReadData(4 to 7));
                    end if;
               else
                    gramReadAddrOffset <= X"0000";
               end if;
               
          end if;
     end process;
          
     process (CLK)
          variable palColor : XCM2_VIDEO_RGB_COLOR;
          variable charRasterData : XCM2_RASTER_FONT_LETTER;
          variable charForeColor : XCM2_INDEX_COLOR;
          variable charBackColor : XCM2_INDEX_COLOR;
          variable backAddr : integer;
     begin         
          if rising_edge(CLK) then
               --if (WritePixel = '1') then
               --     vbuffer(PixelY*BufferWidth + PixelX) <= PixelValue;
               --end if;

               if RESET = '1' then
                    vmode <= XCM2_VIDEO_TEXT;
               else                                            
                    if PRESENT_TRIGGER = '1' and present_trigger_lock = '0' then
                         swap_next <= '1';
                         present_trigger_lock <= '1';
                    elsif PRESENT_TRIGGER = '0' and present_trigger_lock = '1' then
                         present_trigger_lock <= '0';
                    end if;
                    
                    if MODE_SWITCH(0) = '1' then
                         if MODE_SWITCH(1) = '0' then 
                              vmode <= XCM2_VIDEO_EGA;
                              vbufferSize <= XCM2_VIDEO_FRAME_WIDTH/2;--*XCM2_VIDEO_FRAME_HEIGHT/2;
                         else
                              vmode <= XCM2_VIDEO_TEXT;
                              vbufferSize <= VTextModeCols*2;--*VTextModeRows
                         end if;
                    end if;
                    
                    --if BACK_CLR = '1' then
                    --     --vmode <= XCM2_VIDEO_TEXT;
                    --     if visible_buf = '0' then
                    --          fbuf_a(0) <= 15;
                    --     else
                    --          fbuf_b(0) <= 15;
                    --     end if;
                    --end if;  
                    
                    --if BACK_STORE = '1' then
                    --     backAddr:= to_integer(BACK_ADDR);
                    --     if visible_buf = '0' then
                    --          fbuf_b(backAddr*2) <= to_integer(BACK_DATA(0 to 3));
                    --          fbuf_b(backAddr*2+1) <= to_integer(BACK_DATA(4 to 7));
                    --     else
                    --          fbuf_a(backAddr*2) <= to_integer(BACK_DATA(0 to 3));
                    --          fbuf_a(backAddr*2+1) <= to_integer(BACK_DATA(4 to 7));
                    --     end if;
                    --end if;
                    
                    if (HPOS >= HStart and VPOS >= VStart) then
                         if (screen_horcounter = 1 and screen_vercounter = 0) then
                              gramReadEnabled <= '1';
                         end if;
                         
                         if (screen_horcounter < ScreenWidth) then
                              screen_horcounter <= screen_horcounter + 1;
                         else
                              screen_horcounter <= 0;
                              if (screen_vercounter < ScreenHeight) then
                                   screen_vercounter <= screen_vercounter + 1;
                                   --vbuffer_horcounter <= 0;
                              else
                              
                                   if DEBUG_BTN(0) = '0' then
                                        vmode <= XCM2_VIDEO_EGA;
                                        vbufferSize <= XCM2_VIDEO_FRAME_WIDTH/2;
                                   elsif DEBUG_BTN(1) = '0' then
                                        vmode <= XCM2_VIDEO_TEXT;
                                        vbufferSize <= VTextModeCols*2;
                                   end if;
                    
                                   screen_vercounter <= 0;
                                   --subpixel_counter <= 0;
                                   --vbuffer_horcounter <= 0;
                                   --vbuffer_vercounter <= 0;
                                   subpixel_vercounter <= 0;
                                   vbuffer_vercounter <= 0;
                                   vbuffer_line_addr <= 0;
                                   gramReadAddrBase <= X"0000";
                                   text_cellpixel_vercounter <= 0;
                                   text_rowcounter <= 0;
                                   text_row_addr <= 0;
                                   lineSwapVPos <= ScreenVerOffset;
                                   gramReadEnabled <= '0';
  
                                   --colorPhaseDivider <= colorPhaseDivider + 1;
                                   --if colorPhaseDivider = 0 then
                                   --     colorPhase <= colorPhase - X"1";
                                   --     palette(1) <= X"FFF";
                                   --     palette(2) <= X"0" & std_logic_vector(colorPhase) & std_logic_vector(colorPhase);
                                   --     palette(3) <= X"0" & std_logic_vector(colorPhase+X"4") & std_logic_vector(colorPhase+X"4");
                                   --     palette(4) <= X"0" & std_logic_vector(colorPhase+X"8") & std_logic_vector(colorPhase+X"8");
                                   --     palette(5) <= X"0" & std_logic_vector(colorPhase+X"C") & std_logic_vector(colorPhase+X"C");
                                   --end if;
                                   
                                   --text_cellpixel_vercounter <= VTextModeCharHeight-1;
                                   
                                   if swap_next = '1' then
                                        visible_buf <= not visible_buf;
                                        swap_next <= '0';
                                   end if;
                              end if;                         
                         end if;
                         
                         if (screen_vercounter = lineSwapVPos and screen_vercounter < ScreenHeight-ScreenVerOffset) then
                              if (screen_horcounter = 0) then
                                   linebuffer <= linebufferRead;
                                   vbuffer_line_addr <= vbuffer_line_addr + vbufferSize;
                                   gramReadEnabled <= '0';
                              elsif (screen_horcounter = ScreenHorOffset-1) then
                                   --gramReadAddrOffset <= X"0000";
                                   gramReadAddrBase <= to_unsigned(vbuffer_line_addr, 16);
                                   if vmode = XCM2_VIDEO_EGA then
                                        lineSwapVPos <= lineSwapVPos + PixelSize;
                                   elsif vmode = XCM2_VIDEO_TEXT then
                                        lineSwapVPos <= lineSwapVPos + PixelSize*VTextModeCharHeight;
                                   end if;
                                   gramReadEnabled <= '1';
                              end if;
                         end if;
                         
                         if (screen_horcounter >= ScreenHorOffset and screen_horcounter < ScreenWidth-ScreenHorOffset and
                         screen_vercounter >= ScreenVerOffset and screen_vercounter < ScreenHeight-ScreenVerOffset) then
                              if (subpixel_horcounter < PixelSize-1) then
                                   subpixel_horcounter <= subpixel_horcounter + 1;
                              else 
                                   subpixel_horcounter <= 0;
                                   if (vbuffer_horcounter < XCM2_VIDEO_FRAME_WIDTH-1) then
                                        vbuffer_horcounter <= vbuffer_horcounter + 1;
                                        if (text_cellpixel_horcounter < VTextModeCharWidth-1) then
                                             text_cellpixel_horcounter <= text_cellpixel_horcounter + 1;
                                        else
                                             text_cellpixel_horcounter <= 0;
                                             text_colcounter <= text_colcounter + 1; 
                                             --text_cellcounter <= text_cellcounter + 4; -- one actual char takes four indices (8bit ascii + 8bit color)
                                        end if;
                                   else
                                        vbuffer_horcounter <= 0;
                                        text_cellpixel_horcounter <= 0;
                                        text_colcounter <= 0;
                                        
                                        -- TODO!!! vertical subpixel_counter
                                        if (subpixel_vercounter < PixelSize-1) then
                                             subpixel_vercounter <= subpixel_vercounter + 1;
                                        else
                                             subpixel_vercounter <= 0;
                                             vbuffer_vercounter <= vbuffer_vercounter + 1;
                                             
                                             if (text_cellpixel_vercounter < VTextModeCharHeight-1) then
                                                  text_cellpixel_vercounter <= text_cellpixel_vercounter + 1;
                                                  --if (text_cellpixel_vercounter = VTextModeCharHeight-2 and vmode = XCM2_VIDEO_TEXT) then
                                                  --     vbuffer_line_addr <= vbuffer_line_addr + VTextModeCols*2;
                                                  --end if;
                                             else
                                                  text_cellpixel_vercounter <= 0;
                                                  text_rowcounter <= text_rowcounter + 1;
                                                  text_row_addr <= text_row_addr + VTextModeCols*4;
                                             end if;
                                             
                                             --if vmode = XCM2_VIDEO_EGA then
                                             --     vbuffer_line_addr <= vbuffer_line_addr + XCM2_VIDEO_FRAME_WIDTH/2;
                                             --end if;
                                             
                                             --gramReadAddrBase <= to_unsigned(vbuffer_line_addr, 16);
                                        end if;
                                        
                                        --if (vbuffer_vercounter < BufferHeight-1) then
                                        --     vbuffer_vercounter <= vbuffer_vercounter + 1;
                                        --else
                                        --     vbuffer_vercounter <= 0;
                                        --     vbuffer_addr <= 0;
                                        --end if;
                                   end if;
                              end if;
                                   
                              --if vbuffer_vercounter > 0 then
                                   if vmode = XCM2_VIDEO_EGA then
                                        --if visible_buf = '0' then
                                             palColor:= palette(linebuffer(vbuffer_horcounter));
                                             --palColor:= palette(fbuf_a(vbuffer_line_addr + vbuffer_horcounter));
                                        --else
                                        --     palColor:= palette(fbuf_b(vbuffer_line_addr + vbuffer_horcounter));
                                        --end if;                              
                                        
                                   elsif vmode = XCM2_VIDEO_TEXT then
                                        --if visible_buf = '0' then
                                             charRasterData:= RasterFontData(to_integer(
                                                  to_unsigned(linebuffer(text_colcounter*4), 4) &
                                                  to_unsigned(linebuffer(text_colcounter*4 + 1), 4) --text_row_addr
                                                  )
                                             );
                                             charForeColor:= to_integer(to_unsigned(linebuffer(text_colcounter*4 + 2), 4));
                                             charBackColor:= to_integer(to_unsigned(linebuffer(text_colcounter*4 + 3), 4));
                                        --else
                                        --     charRasterData:= RasterFontData(to_integer(
                                        --          to_unsigned(fbuf_b(text_row_addr + text_colcounter*4), 4) &
                                        --          to_unsigned(fbuf_b(text_row_addr + text_colcounter*4 + 1), 4)
                                        --          )
                                        --     );
                                        --     charForeColor:= to_integer(to_unsigned(fbuf_b(text_row_addr + text_colcounter*4 + 2), 4));
                                        --     charBackColor:= to_integer(to_unsigned(fbuf_b(text_row_addr + text_colcounter*4 + 3), 4));
                                        --end if;
                                        
                                        if (charRasterData(text_cellpixel_horcounter)(text_cellpixel_vercounter) = '1') then
                                             palColor:= palette(charForeColor);
                                        else
                                             palColor:= palette(charBackColor);
                                        end if;
                                   end if;
                              --end if;
                              
                              R <= palColor(11 downto 8);
                              G <= palColor(7 downto 4);
                              B <= palColor(3 downto 0);
                         else
                              R <= ('0', '0', '0', '0');--(others=>'0');
                              G <= ('0', '0', '0', '0');--(others=>'0');
                              B <= ('0', '0', '0', '0');--(others=>'0');
                         end if;
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
                    vgaIdle <= '1';
                    --gramReadEnabled <= '0';
               else
                    vgaIdle <= '0';
                    --gramReadEnabled <= '1';
               end if;
          end if;
     end process;
     
end main;   