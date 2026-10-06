library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
use work.FridgeRasterFont.all;

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

        COMMAND_VALID : in std_logic := '0';
        COMMAND_CODE, COMMAND_A, COMMAND_B, COMMAND_C : in XCM2_WORD := X"00";
        COMMAND_HL, COMMAND_BC : in XCM2_DWORD := X"0000";
        COMMAND_ARG0, COMMAND_ARG1 : in XCM2_WORD := X"00";
        COMMAND_READY : out std_logic := '0';
        COMMAND_RESULT : out XCM2_WORD := X"00";

        FRAME_STORE : in std_logic;
        FRAME_ADDR : in XCM2_DWORD;
        FRAME_DATA : in XCM2_WORD;

        PRESENT_TRIGGER : in std_logic;
        PRESENT_MODE : in XCM2_WORD;
        FRAME_OFFSET : in XCM2_DWORD;

        MODE_SWITCH : in std_logic_vector(0 to 1);

        PALETTE_WRITE : in std_logic := '0';
        PALETTE_INDEX : in XCM2_WORD := X"00";
        PALETTE_RGB : in std_logic_vector(23 downto 0) := (others => '0');
        PALETTE_READY : out std_logic;

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
    -- Frame bytes are held in two lanes per buffer: byte addresses that are
    -- even (the high byte of a 16-bit word) and odd (the low byte). One read
    -- therefore fetches both bytes of a word, which is exactly what a text
    -- cell needs -- the glyph code and the attribute occupy one word at
    -- (row*40 + col). EGA still reads one byte per pixel pair and picks a
    -- nibble. Byte order is big-endian like the CPU's 16-bit immediates.
    constant MEM_WORDS : integer := MEM_SPACE / 2;

    type palette_t is array (0 to 15) of std_logic_vector(23 downto 0);
    constant DEFAULT_PALETTE : palette_t := (
        x"000000", x"000080", x"008000", x"008080",
        x"800000", x"800080", x"804000", x"808080",
        x"404040", x"0040FF", x"40FF40", x"40FFFF",
        x"FF4040", x"FF40FF", x"FFFF40", x"FFFFFF");

    signal palette : palette_t := DEFAULT_PALETTE;
    -- Bundled-data mailbox. The command domain holds index/RGB until the
    -- pixel domain acknowledges the request through two synchronizer stages.
    signal pal_index_hold : integer range 0 to 15 := 0;
    signal pal_rgb_hold : std_logic_vector(23 downto 0) := (others => '0');
    signal pal_request, pal_ack : std_logic := '0';
    signal pal_req_meta, pal_req_sync : std_logic := '0';
    signal pal_ack_meta, pal_ack_sync : std_logic := '0';
    signal pal_write_last : std_logic := '0';
    attribute ASYNC_REG : string;
    attribute SHREG_EXTRACT : string;
    attribute ASYNC_REG of pal_req_meta, pal_req_sync, pal_ack_meta, pal_ack_sync : signal is "TRUE";
    attribute SHREG_EXTRACT of pal_req_meta, pal_req_sync, pal_ack_meta, pal_ack_sync : signal is "NO";

    type lane_mem_t is array (0 to MEM_WORDS-1) of XCM2_WORD;
    signal frame0_hi, frame0_lo : lane_mem_t := (others => (others => '0'));
    signal frame1_hi, frame1_lo : lane_mem_t := (others => (others => '0'));
    attribute ram_style : string;
    attribute ram_style of frame0_hi : signal is "block";
    attribute ram_style of frame0_lo : signal is "block";
    attribute ram_style of frame1_hi : signal is "block";
    attribute ram_style of frame1_lo : signal is "block";

    signal wr_word : integer range 0 to MEM_WORDS-1 := 0;
    signal wr_lane : std_logic := '0';
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

    signal rd_word : integer range 0 to MEM_WORDS-1 := 0;
    signal px_c : integer range 0 to 239 := 0;
    signal py_c : integer range 0 to 159 := 0;
    signal nibble_c, win_c : std_logic := '0';
    signal px_s : integer range 0 to 239 := 0;
    signal py_s : integer range 0 to 159 := 0;
    signal nibble_s, win_s : std_logic := '0';
    signal byte_sel_s : std_logic := '0';
    signal charx_s : integer range 0 to 5 := 0;
    signal chary_s : integer range 0 to 7 := 0;
    signal hs_s, vs_s, act_s : std_logic := '0';
    signal pxo_s : integer range 0 to 1649 := 0;
    signal pyo_s : integer range 0 to 749 := 0;
    signal text_s : std_logic := '1';
    signal rd0_hi, rd0_lo, rd1_hi, rd1_lo : XCM2_WORD := (others => '0');
    signal rd_sel : std_logic := '0';
    signal nibble_r, win_r : std_logic := '0';
    signal byte_sel_r : std_logic := '0';
    signal charx_r : integer range 0 to 5 := 0;
    signal chary_r : integer range 0 to 7 := 0;
    signal hs_r, vs_r, act_r : std_logic := '0';
    signal px_r : integer range 0 to 1649 := 0;
    signal py_r : integer range 0 to 749 := 0;
    signal text_r : std_logic := '1';
    signal rgb : std_logic_vector(23 downto 0);
    signal command_frame_addr, sprite_addr : XCM2_DWORD;
    signal command_frame_write, sprite_write, desc_valid, desc_ready, desc_define : std_logic;
    signal command_frame_out, command_frame_in, sprite_out, sprite_in : XCM2_WORD;
    signal command_rd0_hi, command_rd0_lo, command_rd1_hi, command_rd1_lo : XCM2_WORD;
    signal desc_id, desc_b, desc_c : XCM2_WORD;
    signal desc_hl : XCM2_DWORD;
    signal port_addr : XCM2_DWORD;
    signal port_data : XCM2_WORD;
    signal overlay : std_logic_vector(27 downto 0);
    type rgb_pipe_t is array(0 to 4) of std_logic_vector(23 downto 0);
    type overlay_pipe_t is array(0 to 3) of std_logic_vector(27 downto 0);
    type x_pipe_t is array(0 to 4) of integer range 0 to 1649;
    type y_pipe_t is array(0 to 4) of integer range 0 to 749;
    signal blend_rgb : rgb_pipe_t;
    signal blend_overlay : overlay_pipe_t;
    signal blend_x : x_pipe_t; signal blend_y : y_pipe_t;
    signal blend_hs, blend_vs, blend_act : std_logic_vector(0 to 4);
    function composite(base, source : std_logic_vector(23 downto 0);
                       mode : std_logic_vector(2 downto 0);
                       index : std_logic_vector(3 downto 0)) return std_logic_vector is
        variable result : std_logic_vector(23 downto 0);
        variable a, b, v : integer;
    begin
        result := base;
        case mode is
            when "001" => result := source;
            when "010" => if index /= "0000" then result := source; end if;
            when "011" | "100" =>
                for i in 0 to 2 loop
                    a := to_integer(unsigned(base(i*8+7 downto i*8)));
                    b := to_integer(unsigned(source(i*8+7 downto i*8)));
                    if mode = "011" then v := a+b; if v > 255 then v := 255; end if;
                    else v := a-b; if v < 0 then v := 0; end if; end if;
                    result(i*8+7 downto i*8) := std_logic_vector(to_unsigned(v, 8));
                end loop;
            when "101" => result := base and source;
            when "110" => result := base or source;
            when "111" => result := base xor source;
            when others => null;
        end case;
        return result;
    end;
begin
    commands : entity work.fridge_gpu_commands
        port map(CLK => COMMAND_CLK, RESET => COMMAND_RESET, VALID => COMMAND_VALID,
                 CODE => COMMAND_CODE, A => COMMAND_A, B => COMMAND_B, C => COMMAND_C,
                 HL => COMMAND_HL, BC => COMMAND_BC, ARG0 => COMMAND_ARG0, ARG1 => COMMAND_ARG1,
                 READY => COMMAND_READY, RESULT => COMMAND_RESULT,
                 FRAME_ADDR => command_frame_addr, FRAME_WRITE => command_frame_write,
                 FRAME_OUT => command_frame_out, FRAME_IN => command_frame_in,
                 SPR_ADDR => sprite_addr, SPR_WRITE => sprite_write, SPR_OUT => sprite_out,
                 SPR_IN => sprite_in, DESC_VALID => desc_valid, DESC_READY => desc_ready,
                 DESC_DEFINE => desc_define, DESC_ID => desc_id, DESC_B => desc_b,
                 DESC_C => desc_c, DESC_HL => desc_hl);
    sprites : entity work.fridge_sprites
        port map(CLK => CLK, RESET => RESET, COMMAND_CLK => COMMAND_CLK, COMMAND_RESET => COMMAND_RESET,
                 MEM_ADDR => sprite_addr, MEM_WRITE => sprite_write, MEM_IN => sprite_out, MEM_OUT => sprite_in,
                 DESC_VALID => desc_valid, DESC_DEFINE => desc_define, DESC_READY => desc_ready,
                 DESC_ID => desc_id, DESC_B => desc_b, DESC_C => desc_c, DESC_HL => desc_hl,
                 RASTER_X => x, RASTER_Y => y, HOFF => disp_hoff, VOFF => disp_voff,
                 LOOKUP_X => px_r, LOOKUP_Y => py_r, OVERLAY => overlay);
    port_addr <= command_frame_addr when COMMAND_VALID = '1' else FRAME_ADDR;
    port_data <= command_frame_out when COMMAND_VALID = '1' else FRAME_DATA;
    command_frame_in <= command_rd1_hi when cmd_active = '1' and wr_lane = '0' else
                        command_rd1_lo when cmd_active = '1' else
                        command_rd0_hi when wr_lane = '0' else command_rd0_lo;
    PALETTE_READY <= '1' when COMMAND_RESET = '0' and pal_request = pal_ack_sync else '0';

    palette_command : process (COMMAND_CLK)
    begin
        if rising_edge(COMMAND_CLK) then
            if COMMAND_RESET = '1' then
                pal_request <= '0';
                pal_ack_meta <= '0';
                pal_ack_sync <= '0';
                pal_write_last <= '0';
                pal_index_hold <= 0;
                pal_rgb_hold <= (others => '0');
            else
                pal_ack_meta <= pal_ack;
                pal_ack_sync <= pal_ack_meta;
                pal_write_last <= PALETTE_WRITE;
                if PALETTE_WRITE = '1' and pal_write_last = '0' and
                   pal_request = pal_ack_sync and PALETTE_INDEX < 16 then
                    pal_index_hold <= to_integer(PALETTE_INDEX);
                    pal_rgb_hold <= PALETTE_RGB;
                    pal_request <= not pal_request;
                end if;
            end if;
        end if;
    end process;

    palette_pixel : process (CLK)
    begin
        if rising_edge(CLK) then
            if RESET = '1' then
                palette <= DEFAULT_PALETTE;
                pal_req_meta <= '0';
                pal_req_sync <= '0';
                pal_ack <= '0';
            else
                pal_req_meta <= pal_request;
                pal_req_sync <= pal_req_meta;
                if pal_req_sync /= pal_ack then
                    palette(pal_index_hold) <= pal_rgb_hold;
                    pal_ack <= pal_req_sync;
                end if;
            end if;
        end if;
    end process;

    timing : entity work.video_timing
        port map (CLK, RESET, x, y, hs, vs, act);

    wr_enable <= '1' when (FRAME_STORE = '1' or command_frame_write = '1') and COMMAND_RESET = '0'
                           and to_integer(port_addr) < FRAME_BYTES else '0';

    -- Byte address A lands in word A/2: A even in the high lane, A odd in
    -- the low lane. FRAME_ADDR(0 to 14) is A/2 for A < 32768 and
    -- FRAME_ADDR(15) is A's low bit.
    wr_word <= to_integer(port_addr(1 to 14));
    wr_lane <= port_addr(15);

    -- Each write port explicitly bypasses its new byte to infer WRITE_FIRST.
    -- READ_FIRST dual-clock ports can corrupt memory on Spartan-6 (AR34533).
    wr_port : process (COMMAND_CLK)
    begin
        if rising_edge(COMMAND_CLK) then
            if wr_enable = '1' and cmd_active = '0' and wr_lane = '0' then
                frame0_hi(wr_word) <= port_data; command_rd0_hi <= port_data;
            else command_rd0_hi <= frame0_hi(wr_word); end if;
            if wr_enable = '1' and cmd_active = '0' and wr_lane = '1' then
                frame0_lo(wr_word) <= port_data; command_rd0_lo <= port_data;
            else command_rd0_lo <= frame0_lo(wr_word); end if;
            if wr_enable = '1' and cmd_active = '1' and wr_lane = '0' then
                frame1_hi(wr_word) <= port_data; command_rd1_hi <= port_data;
            else command_rd1_hi <= frame1_hi(wr_word); end if;
            if wr_enable = '1' and cmd_active = '1' and wr_lane = '1' then
                frame1_lo(wr_word) <= port_data; command_rd1_lo <= port_data;
            else command_rd1_lo <= frame1_lo(wr_word); end if;
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
    -- Text scanout ignores the VPRE frame offsets, matching the emulator.
    process (x, y, disp_hoff, disp_voff, disp_mode)
        variable ix, iy, pxa, pya : integer;
    begin
        if x >= 160 and x < 1120 and y >= 40 and y < 680 then
            ix := (x - 160) / 4;
            iy := (y - 40) / 4;
            if disp_mode = '1' then
                pxa := ix;
                pya := iy;
            else
                pxa := ix + disp_hoff;
                if pxa >= 240 then
                    pxa := pxa - 240;
                end if;
                pya := iy + disp_voff;
                if pya >= 160 then
                    pya := pya - 160;
                end if;
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

    -- Stage 2: word address from the registered frame coordinates. The line
    -- stride is 120 = 128 - 8 so the multiply stays in carry chains.
    -- Text mode addresses a cell word directly: (row*40 + col) with row = y/8
    -- and col = x/6; the in-cell pixel offsets feed the font lookup instead.
    process (px_s, py_s, text_s)
        variable a : integer range 0 to MEM_WORDS-1;
    begin
        if text_s = '1' then
            a := (py_s / 8) * 40 + (px_s / 6);
            charx_s <= px_s mod 6;
            chary_s <= py_s mod 8;
            byte_sel_s <= '0';
        else
            a := (py_s * 128 - py_s * 8 + px_s / 2) / 2;
            charx_s <= 0;
            chary_s <= 0;
            -- Byte address py*120 + px/2 is even in py*120, so its lane is
            -- decided by bit 1 of px.
            if (px_s / 2) mod 2 = 0 then
                byte_sel_s <= '0';
            else
                byte_sel_s <= '1';
            end if;
        end if;
        rd_word <= a;
    end process;

    rd_port : process (CLK)
    begin
        if rising_edge(CLK) then
            rd0_hi <= frame0_hi(rd_word);
            rd0_lo <= frame0_lo(rd_word);
            rd1_hi <= frame1_hi(rd_word);
            rd1_lo <= frame1_lo(rd_word);
            rd_sel <= disp_visible;
            nibble_r <= nibble_s;
            win_r <= win_s;
            byte_sel_r <= byte_sel_s;
            charx_r <= charx_s;
            chary_r <= chary_s;
            hs_r <= hs_s;
            vs_r <= vs_s;
            act_r <= act_s;
            px_r <= pxo_s;
            py_r <= pyo_s;
            text_r <= text_s;
        end if;
    end process;

    process (rd0_hi, rd0_lo, rd1_hi, rd1_lo, rd_sel, byte_sel_r, nibble_r,
             charx_r, chary_r, win_r, text_r, act_r, palette)
        variable hi, lo, byte : XCM2_WORD;
        variable glyph : std_logic_vector(0 to 7);
        variable idx, fg, bg : integer;
        variable rgb_v : std_logic_vector(23 downto 0);
    begin
        if rd_sel = '1' then
            hi := rd1_hi;
            lo := rd1_lo;
        else
            hi := rd0_hi;
            lo := rd0_lo;
        end if;
        if act_r = '0' then
            rgb_v := x"000000";
        elsif win_r = '0' then
            rgb_v := MARGIN_RGB;
        elsif text_r = '1' then
            -- Text cell: hi = glyph code (even byte), lo = attribute
            -- (odd byte), background in the high nibble, foreground in the
            -- low nibble. Font columns are 8 vertical pixels with the most
            -- significant bit in the top row.
            glyph := RasterFontData(to_integer(hi))(charx_r);
            fg := to_integer(lo(4 to 7));
            bg := to_integer(lo(0 to 3));
            if glyph(chary_r) = '1' then
                idx := fg;
            else
                idx := bg;
            end if;
            rgb_v := PALETTE(idx);
        else
            if byte_sel_r = '0' then
                byte := hi;
            else
                byte := lo;
            end if;
            if nibble_r = '0' then
                idx := to_integer(byte(0 to 3));
            else
                idx := to_integer(byte(4 to 7));
            end if;
            rgb_v := PALETTE(idx);
        end if;
        rgb <= rgb_v;
    end process;

    -- The colour lookup is registered so that the frame-store to encoder path
    -- ends here; the TMDS encoder's own logic is no longer part of the pixel
    -- domain's critical path. Every output is delayed together, so the raster
    -- alignment between colour, sync and position is unchanged.
    -- One stage per overlapping sprite keeps RGB saturation and palette
    -- selection off the same timing path. Raster metadata follows every stage.
    out_reg : process (CLK)
        variable layer : std_logic_vector(6 downto 0);
    begin
        if rising_edge(CLK) then
            blend_rgb(0) <= rgb;
            if text_r = '0' and win_r = '1' and act_r = '1' then blend_overlay(0) <= overlay;
            else blend_overlay(0) <= (others => '0'); end if;
            blend_x(0) <= px_r; blend_y(0) <= py_r;
            blend_hs(0) <= hs_r; blend_vs(0) <= vs_r; blend_act(0) <= act_r;
            for i in 0 to 3 loop
                layer := blend_overlay(i)(i*7+6 downto i*7);
                blend_rgb(i+1) <= composite(blend_rgb(i), palette(to_integer(unsigned(layer(3 downto 0)))),
                                             layer(6 downto 4), layer(3 downto 0));
                if i < 3 then blend_overlay(i+1) <= blend_overlay(i); end if;
                blend_x(i+1) <= blend_x(i); blend_y(i+1) <= blend_y(i);
                blend_hs(i+1) <= blend_hs(i); blend_vs(i+1) <= blend_vs(i); blend_act(i+1) <= blend_act(i);
            end loop;
        end if;
    end process;
    RED <= blend_rgb(4)(23 downto 16); GREEN <= blend_rgb(4)(15 downto 8); BLUE <= blend_rgb(4)(7 downto 0);
    HSYNC <= blend_hs(4); VSYNC <= blend_vs(4); ACTIVE <= blend_act(4);
    PIXEL_X <= blend_x(4); PIXEL_Y <= blend_y(4);
end rtl;
