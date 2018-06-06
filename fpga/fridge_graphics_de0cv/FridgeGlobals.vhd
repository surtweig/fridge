library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package FridgeGlobals is
     constant XCM2_VIDEO_FRAME_WIDTH : integer:= 240;
     constant XCM2_VIDEO_FRAME_HEIGHT : integer:= 160;
     constant XCM2_RAM_SIZE : integer:= 65536;

     subtype XCM2_WORD is unsigned(0 to 7);
     subtype XCM2_DWORD is unsigned(0 to 15);
     subtype XCM2_INDEX_COLOR is integer range 0 to 15;
     type XCM2_VIDEO_MODE is (XCM2_VIDEO_DEFAULT, XCM2_VIDEO_EGA, XCM2_VIDEO_TEXT);
     
     type XCM2_VIDEO_FBUF is array (0 to XCM2_VIDEO_FRAME_HEIGHT*XCM2_VIDEO_FRAME_WIDTH - 1) of XCM2_INDEX_COLOR;
     type XCM2_VIDEO_LINEBUF is array (0 to XCM2_VIDEO_FRAME_WIDTH - 1) of XCM2_INDEX_COLOR;
     type XCM2_VIDEO_FRAME_MEM is array (0 to 1) of XCM2_VIDEO_FBUF;
     type XCM2_RAM is array (0 to XCM2_RAM_SIZE-1) of XCM2_WORD;
     type XCM2_GRAM is array (0 to XCM2_RAM_SIZE-1) of XCM2_WORD;
     
     subtype XCM2_VIDEO_RGB_COLOR is std_logic_vector(11 downto 0);
     type XCM2_VIDEO_PALETTE is array (0 to 15) of XCM2_VIDEO_RGB_COLOR;
     type XCM2_RASTER_FONT_LETTER is array (0 to 5) of std_logic_vector(0 to 7);
     type XCM2_RASTER_FONT_DATA is array(0 to 255) of XCM2_RASTER_FONT_LETTER;
end FridgeGlobals;