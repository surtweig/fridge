library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;
use work.FridgeGlobals.all;
use work.FridgeRAMBootImage.all;

entity FridgeMain is

port (
     CLOCK2_50 : in std_logic;
     --CLOCK_DEBUG : in std_logic;
     VGA_HS, VGA_VS : out std_logic;
     VGA_R, VGA_G, VGA_B : out std_logic_vector(3 downto 0);
     KEY : in std_logic_vector(3 downto 0);
     RESET_N : in std_logic;
     HEX0, HEX1, HEX2, HEX3, HEX4, HEX5 : out std_logic_vector(6 downto 0);
     LEDR : out std_logic_vector(9 downto 0);
     PS2_CLK, PS2_DAT, PS2_CLK2, PS2_DAT2 : in std_logic
     );
     
end FridgeMain;

architecture main of FridgeMain is

     signal VGACLK, RESET, CPU_CLK, VGAWritePixel, VGAPixelValue : std_logic:= '0';
     
     signal GPU_VIDEO_MODE_SWITCH : std_logic_vector(0 to 1):= (others => '0');
     signal GPU_PALETTE_SWITCH, GPU_PRESENT_TRIGGER : std_logic:= '0';
     signal GPU_BACK_STORE, GPU_BACK_LOAD, GPU_VMEM_STORE, GPU_VMEM_LOAD, GPU_BACK_CLR : std_logic:= '0';
     signal GPU_BACK_ADDR, GPU_VMEM_ADDR : XCM2_DWORD;
     signal GPU_BACK_DATA, GPU_VMEM_DATA : XCM2_WORD;
     
     signal CPU_DEBUG_STEP, CPU_HLT, CPU_INTE, CPU_INT, RAM_WRITE_ENABLED, CPU_DEVICE_READ : std_logic:= '0';
     signal CPU_INT_IRQ, CPU_DEVICE_SEL, CPU_DEVICE_DATA : XCM2_WORD;
     signal RAM_WRITE_DATA, RAM_READ_DATA : XCM2_WORD;
     signal RAM_WRITE_ADDR, RAM_READ_ADDR : XCM2_DWORD;
     
     signal CPU_DEBUG_STATE, CPU_DEBUG_CURRENT_IR, CPU_DEBUG_PC_L : XCM2_WORD;
     
     component vga_pll is
          port (
               vga_pll_clk_in_clk  : in  std_logic := '0'; --  vga_pll_clk_in.clk
               vga_pll_clk_out_clk : out std_logic;        -- vga_pll_clk_out.clk
               vga_pll_reset_reset : in  std_logic := '0'  --   vga_pll_reset.reset
          );
     end component vga_pll;
     
     component cpu_clk_pll is
          port (
               cpu_clk_pll_in_clk      : in  std_logic := '0'; --    cpu_clk_pll_in.clk
               cpu_clk_pll_out_clk     : out std_logic;        --   cpu_clk_pll_out.clk
               cpu_clk_pll_reset_reset : in  std_logic := '0'  -- cpu_clk_pll_reset.reset
          );
     end component cpu_clk_pll;
     
     component FridgeCPU is
          port (
               CLK_MAIN : in std_logic;
               CLK_PHI2 : in std_logic;
               RESET : in std_logic;
               HALTED : out std_logic;
               
               INTE : out std_logic;
               INT : in std_logic;
               INT_IRQ : in XCM2_WORD;
               
               DEVICE_SEL : out XCM2_WORD;
               DEVICE_READ : out std_logic;
               DEVICE_DATA : inout XCM2_WORD;
               
               RAM_WRITE_DATA : out XCM2_WORD;
               RAM_WRITE_ADDR : out XCM2_DWORD;
               RAM_WRITE_ENABLED : out std_logic;
               RAM_READ_DATA : in XCM2_WORD;
               RAM_READ_ADDR : out XCM2_DWORD;	  
               
               GPU_MODE_SWITCH : out std_logic_vector(0 to 1);
               GPU_PALETTE_SWITCH : out std_logic;
               GPU_PRESENT_TRIGGER : out std_logic;
               
               GPU_BACK_STORE : out std_logic;
               GPU_BACK_LOAD : out std_logic;
               GPU_BACK_ADDR : out XCM2_DWORD;
               GPU_BACK_DATA : inout XCM2_WORD;
               GPU_BACK_CLR : out std_logic;
          
               GPU_VMEM_STORE : out std_logic;
               GPU_VMEM_LOAD : out std_logic;
               GPU_VMEM_ADDR : out XCM2_DWORD;
               GPU_VMEM_DATA : inout XCM2_WORD;
               
               DEBUG_STEP : in std_logic;
               DEBUG_STATE : out XCM2_WORD;
               DEBUG_CURRENT_IR : out XCM2_WORD;
               DEBUG_PC_L : out XCM2_WORD
          );
     end component FridgeCPU;
     
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
     
     component FridgeGraphicsAdapter is
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
               BACK_CLR : in std_logic;
          
               VMEM_STORE : in std_logic;
               VMEM_LOAD : in std_logic;
               VMEM_ADDR : in XCM2_DWORD;
               VMEM_DATA : inout XCM2_WORD;
               
               DEBUG_BTN : in std_logic_vector(3 downto 0)
          );     
     end component FridgeGraphicsAdapter;
     
     component LUTDisplay is
          port (
               Word : in XCM2_WORD;
               HEXH, HEXL : out std_logic_vector(6 downto 0)
          );
     end component LUTDisplay;
     
begin
     RESET <= not RESET_N;
     CPU_DEBUG_STEP <= not KEY(0);

     C_LUT_LEFT : LUTDisplay port map(CPU_DEBUG_STATE, HEX5, HEX4);
     C_LUT_RIGHT : LUTDisplay port map(CPU_DEBUG_CURRENT_IR, HEX1, HEX0);
     C_LUT_CENTER : LUTDisplay port map(CPU_DEBUG_PC_L, HEX3, HEX2);
     
     C_CPU_PLL : cpu_clk_pll port map(CLOCK2_50, CPU_CLK, RESET);
     --CPU_CLK <= not KEY(0);
     
     C_CPU : FridgeCPU port map(
               CPU_CLK, CPU_CLK, RESET, CPU_HLT,              
               CPU_INTE, CPU_INT, CPU_INT_IRQ,
               CPU_DEVICE_SEL, CPU_DEVICE_READ, CPU_DEVICE_DATA,               
               RAM_WRITE_DATA, RAM_WRITE_ADDR, RAM_WRITE_ENABLED, RAM_READ_DATA, RAM_READ_ADDR,                 
               GPU_VIDEO_MODE_SWITCH, GPU_PALETTE_SWITCH, GPU_PRESENT_TRIGGER,               
               GPU_BACK_STORE, GPU_BACK_LOAD, GPU_BACK_ADDR, GPU_BACK_DATA, GPU_BACK_CLR,          
               GPU_VMEM_STORE, GPU_VMEM_LOAD, GPU_VMEM_ADDR, GPU_VMEM_DATA,
               CPU_DEBUG_STEP, CPU_DEBUG_STATE, CPU_DEBUG_CURRENT_IR, CPU_DEBUG_PC_L
     );
     
     C_RAM : FridgeRAM 
          generic map (INIT_DATA => RAMBootImage)
          port map(CPU_CLK, RAM_WRITE_DATA, RAM_WRITE_ADDR, RAM_WRITE_ENABLED, RAM_READ_DATA, RAM_READ_ADDR);
     
     C_VGA_PLL : vga_pll port map (CLOCK2_50, VGACLK, RESET);
     
     C_GPU : FridgeGraphicsAdapter port map(
          VGACLK, CPU_CLK, RESET,
          VGA_HS, VGA_VS, VGA_R, VGA_G, VGA_B,
          GPU_VIDEO_MODE_SWITCH, GPU_PALETTE_SWITCH, GPU_PRESENT_TRIGGER,
          GPU_BACK_STORE, GPU_BACK_LOAD, GPU_BACK_ADDR, GPU_BACK_DATA, GPU_BACK_CLR,
          GPU_VMEM_STORE, GPU_VMEM_LOAD, GPU_VMEM_ADDR, GPU_VMEM_DATA,
          KEY
          );
          
end main;