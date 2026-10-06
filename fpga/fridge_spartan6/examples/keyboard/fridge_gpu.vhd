library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;

entity fridge_gpu is
    generic (
        -- Color shown outside the 960x640 scaled window, RGB888.
        MARGIN_RGB : std_logic_vector(23 downto 0) := x"0000FF"
    );
    port (
        CLK : in std_logic;
        COMMAND_CLK : in std_logic;
        RESET : in std_logic;
        COMMAND_RESET : in std_logic;

        FRAME_STORE : in std_logic;
        FRAME_ADDR : in XCM2_DWORD;
        FRAME_DATA : in XCM2_WORD;

        PRESENT_TRIGGER : in std_logic;
        PRESENT_MODE : in XCM2_WORD;
        FRAME_OFFSET : in XCM2_DWORD;

        MODE_SWITCH : in std_logic_vector(0 to 1);

        RED, GREEN, BLUE : out std_logic_vector(7 downto 0);
        HSYNC, VSYNC, ACTIVE : out std_logic;
        PIXEL_X : out integer range 0 to 1649;
        PIXEL_Y : out integer range 0 to 749
    );
end fridge_gpu;

architecture rtl of fridge_gpu is
    constant FRAME_BYTES : integer := 19200;
    -- Each frame lives in a power-of-two address space (15-bit index) so that
    -- BRAM inference sees statically matching index and array ranges; the
    -- region above FRAME_BYTES is never written (wr_enable) nor read.
    constant MEM_SPACE : integer := 32768;

    type palette_t is array (0 to 15) of std_logic_vector(23 downto 0);
    constant PALETTE : palette_t := (
        x"000000", x"000080", x"008000", x"008080",
        x"800000", x"800080", x"804000", x"808080",
        x"404040", x"0040FF", x"40FF40", x"40FFFF",
        x"FF4040", x"FF40FF", x"FFFF40", x"FFFFFF");

    type frame_mem_t is array (0 to MEM_SPACE-1) of XCM2_WORD;
    signal frame0_mem, frame1_mem : frame_mem_t := (others => (others => '0'));
    attribute ram_style : string;
    attribute ram_style of frame0_mem : signal is "block";
    attribute ram_style of frame1_mem : signal is "block";

    signal wr_addr : integer range 0 to MEM_SPACE-1 := 0;
    signal wr_enable : std_logic := '0';

    signal cmd_active : std_logic := '1';
    signal cmd_visible : std_logic := '0';
    signal cmd_mode : std_logic := '1';
    signal cmd_hoff : integer range 0 to 239 := 0;
    signal cmd_voff : integer range 0 to 159 := 0;
    signal cmd_toggle : std_logic := '0';

    signal toggle_meta, toggle_sync : std_logic := '0';
    signal pend_toggle : std_logic := '0';
    signal pend_visible : std_logic := '0';
    signal pend_hoff : integer range 0 to 239 := 0;
    signal pend_voff : integer range 0 to 159 := 0;

    signal mode_meta, disp_mode : std_logic := '1';
    signal disp_visible : std_logic := '0';
    signal disp_hoff : integer range 0 to 239 := 0;
    signal disp_voff : integer range 0 to 159 := 0;

    signal x : integer range 0 to 1649;
    signal y : integer range 0 to 749;
    signal hs, vs, act : std_logic;

    signal rd_addr : integer range 0 to MEM_SPACE-1 := 0;
    signal px_c : integer range 0 to 239 := 0;
    signal py_c : integer range 0 to 159 := 0;
    signal nibble_c, win_c : std_logic := '0';
    signal px_s : integer range 0 to 239 := 0;
    signal py_s : integer range 0 to 159 := 0;
    signal nibble_s, win_s : std_logic := '0';
    signal hs_s, vs_s, act_s : std_logic := '0';
    signal pxo_s : integer range 0 to 1649 := 0;
    signal pyo_s : integer range 0 to 749 := 0;
    signal text_s : std_logic := '1';
    signal rd0, rd1, rd_data : XCM2_WORD := (others => '0');
    signal rd_sel : std_logic := '0';
    signal nibble_r, win_r : std_logic := '0';
    signal hs_r, vs_r, act_r : std_logic := '0';
    signal px_r : integer range 0 to 1649 := 0;
    signal py_r : integer range 0 to 749 := 0;
    signal text_r : std_logic := '1';
begin
    timing : entity work.video_timing
        port map (CLK, RESET, x, y, hs, vs, act);

    wr_enable <= '1' when FRAME_STORE = '1' and COMMAND_RESET = '0'
                           and to_integer(FRAME_ADDR) < FRAME_BYTES else '0';

    wr_addr <= to_integer(FRAME_ADDR(1 to 15));

    wr_port : process (COMMAND_CLK)
    begin
        if rising_edge(COMMAND_CLK) then
            if wr_enable = '1' then
                if cmd_active = '1' then
                    frame1_mem(wr_addr) <= FRAME_DATA;
                else
                    frame0_mem(wr_addr) <= FRAME_DATA;
                end if;
            end if;
        end if;
    end process;

    process (COMMAND_CLK)
        variable h, v : integer;
    begin
        if rising_edge(COMMAND_CLK) then
            if COMMAND_RESET = '1' then
                cmd_active <= '1';
                cmd_visible <= '0';
                cmd_mode <= '1';
                cmd_hoff <= 0;
                cmd_voff <= 0;
                cmd_toggle <= '0';
            else
                if MODE_SWITCH(0) = '1' then
                    cmd_mode <= MODE_SWITCH(1);
                end if;

                if PRESENT_TRIGGER = '1' then
                    h := to_integer(FRAME_OFFSET(0 to 7));
                    v := to_integer(FRAME_OFFSET(8 to 15));
                    if h >= 240 then
                        h := h - 240;
                    end if;
                    if v >= 160 then
                        v := v - 160;
                    end if;
                    cmd_hoff <= h;
                    cmd_voff <= v;
                    if to_integer(PRESENT_MODE) = 1 then
                        if cmd_visible = '0' then
                            cmd_visible <= '1';
                            cmd_active <= '0';
                        else
                            cmd_visible <= '0';
                            cmd_active <= '1';
                        end if;
                    elsif to_integer(PRESENT_MODE) = 2 then
                        cmd_visible <= '0';
                        cmd_active <= '0';
                    elsif to_integer(PRESENT_MODE) = 3 then
                        cmd_visible <= '0';
                        cmd_active <= '1';
                    elsif to_integer(PRESENT_MODE) = 4 then
                        cmd_visible <= '1';
                        cmd_active <= '0';
                    elsif to_integer(PRESENT_MODE) = 5 then
                        cmd_visible <= '1';
                        cmd_active <= '1';
                    end if;
                    cmd_toggle <= not cmd_toggle;
                end if;
            end if;
        end if;
    end process;

    process (CLK)
    begin
        if rising_edge(CLK) then
            if RESET = '1' then
                toggle_meta <= '0';
                toggle_sync <= '0';
                pend_toggle <= '0';
                pend_visible <= '0';
                pend_hoff <= 0;
                pend_voff <= 0;
                mode_meta <= '1';
                disp_mode <= '1';
                disp_visible <= '0';
                disp_hoff <= 0;
                disp_voff <= 0;
            else
                toggle_meta <= cmd_toggle;
                toggle_sync <= toggle_meta;
                if toggle_sync /= pend_toggle then
                    pend_toggle <= toggle_sync;
                    pend_visible <= cmd_visible;
                    pend_hoff <= cmd_hoff;
                    pend_voff <= cmd_voff;
                end if;
                mode_meta <= cmd_mode;
                disp_mode <= mode_meta;
                if y = 749 and x = 1649 then
                    disp_visible <= pend_visible;
                    disp_hoff <= pend_hoff;
                    disp_voff <= pend_voff;
                end if;
            end if;
        end if;
    end process;

    -- Stage 1: raster position to frame coordinates (4x scale, offsets, wrap).
    process (x, y, disp_hoff, disp_voff)
        variable ix, iy, pxa, pya : integer;
    begin
        if x >= 160 and x < 1120 and y >= 40 and y < 680 then
            ix := (x - 160) / 4;
            iy := (y - 40) / 4;
            pxa := ix + disp_hoff;
            if pxa >= 240 then
                pxa := pxa - 240;
            end if;
            pya := iy + disp_voff;
            if pya >= 160 then
                pya := pya - 160;
            end if;
            win_c <= '1';
        else
            pxa := 0;
            pya := 0;
            win_c <= '0';
        end if;
        px_c <= pxa;
        py_c <= pya;
        if pxa mod 2 = 0 then
            nibble_c <= '0';
        else
            nibble_c <= '1';
        end if;
    end process;

    process (CLK)
    begin
        if rising_edge(CLK) then
            px_s <= px_c;
            py_s <= py_c;
            nibble_s <= nibble_c;
            win_s <= win_c;
            hs_s <= hs;
            vs_s <= vs;
            act_s <= act;
            pxo_s <= x;
            pyo_s <= y;
            text_s <= disp_mode;
        end if;
    end process;

    -- Stage 2: byte address from the registered frame coordinates. The line
    -- stride is 120 = 128 - 8 so the multiply stays in carry chains.
    process (px_s, py_s)
        variable a : integer range 0 to MEM_SPACE-1;
    begin
        a := py_s * 128 - py_s * 8 + px_s / 2;
        rd_addr <= a;
    end process;

    rd_port : process (CLK)
    begin
        if rising_edge(CLK) then
            rd0 <= frame0_mem(rd_addr);
            rd1 <= frame1_mem(rd_addr);
            rd_sel <= disp_visible;
            nibble_r <= nibble_s;
            win_r <= win_s;
            hs_r <= hs_s;
            vs_r <= vs_s;
            act_r <= act_s;
            px_r <= pxo_s;
            py_r <= pyo_s;
            text_r <= text_s;
        end if;
    end process;

    rd_data <= rd1 when rd_sel = '1' else rd0;

    process (rd_data, nibble_r, win_r, text_r, act_r)
        variable idx : integer;
        variable rgb : std_logic_vector(23 downto 0);
    begin
        if nibble_r = '0' then
            idx := to_integer(rd_data(0 to 3));
        else
            idx := to_integer(rd_data(4 to 7));
        end if;
        if act_r = '0' then
            rgb := x"000000";
        elsif win_r = '0' then
            rgb := MARGIN_RGB;
        elsif text_r = '1' then
            rgb := x"000000";
        else
            rgb := PALETTE(idx);
        end if;
        RED <= rgb(23 downto 16);
        GREEN <= rgb(15 downto 8);
        BLUE <= rgb(7 downto 0);
    end process;

    HSYNC <= hs_r;
    VSYNC <= vs_r;
    ACTIVE <= act_r;
    PIXEL_X <= px_r;
    PIXEL_Y <= py_r;
end rtl;
