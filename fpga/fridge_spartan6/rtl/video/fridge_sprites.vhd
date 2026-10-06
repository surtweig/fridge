-- 32 KiB dual-clock sprite store and an ahead-of-scan double-buffered line cache.
-- At 4x scaling there are 6600 pixel clocks per logical line. Worst case:
-- 64 row preparations + 240*(hit + 4*5 reads + cache write) = 5344 clocks.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.FridgeGlobals.all;
entity fridge_sprites is
    port (CLK, RESET, COMMAND_CLK, COMMAND_RESET : in std_logic;
          MEM_ADDR : in XCM2_DWORD; MEM_WRITE : in std_logic;
          MEM_IN : in XCM2_WORD; MEM_OUT : out XCM2_WORD;
          DESC_VALID, DESC_DEFINE : in std_logic; DESC_READY : out std_logic;
          DESC_ID, DESC_B, DESC_C : in XCM2_WORD; DESC_HL : in XCM2_DWORD;
          RASTER_X : in integer range 0 to 1649;
          RASTER_Y : in integer range 0 to 749;
          HOFF : in integer range 0 to 239; VOFF : in integer range 0 to 159;
          LOOKUP_X : in integer range 0 to 1649;
          LOOKUP_Y : in integer range 0 to 749;
          OVERLAY : out std_logic_vector(27 downto 0));
end;
architecture rtl of fridge_sprites is
    type mem_t is array(0 to 32767) of XCM2_WORD;
    signal mem : mem_t := (others => X"00");
    attribute ram_style : string;
    attribute ram_style of mem : signal is "block";
    signal rd_addr : integer range 0 to 32767 := 0;
    signal rd_data : XCM2_WORD := X"00";
    type bytes_t is array(0 to 63) of unsigned(7 downto 0);
    type words_t is array(0 to 63) of unsigned(15 downto 0);
    type modes_t is array(0 to 63) of std_logic_vector(2 downto 0);
    signal widths, heights, posx, posy : bytes_t := (others => (others => '0'));
    signal bases, rows : words_t := (others => (others => '0'));
    signal modes : modes_t := (others => "000");
    signal line_posx, line_widths : bytes_t := (others => (others => '0'));
    signal line_bases : words_t := (others => (others => '0'));
    signal line_modes : modes_t := (others => "000");
    signal row_hits : std_logic_vector(63 downto 0) := (others => '0');
    signal req, ack, req_meta, req_sync, ack_meta, ack_sync, last : std_logic := '0';
    signal hold_define : std_logic := '0';
    signal hold_id : integer range 0 to 63 := 0;
    signal hold_b, hold_c : unsigned(7 downto 0) := (others => '0');
    signal hold_hl : unsigned(15 downto 0) := (others => '0');
    attribute ASYNC_REG : string;
    attribute SHREG_EXTRACT : string;
    attribute ASYNC_REG of req_meta, req_sync, ack_meta, ack_sync : signal is "TRUE";
    attribute SHREG_EXTRACT of req_meta, req_sync, ack_meta, ack_sync : signal is "NO";
    type cache_t is array(0 to 239) of std_logic_vector(27 downto 0);
    signal cache0, cache1 : cache_t := (others => (others => '0'));
    attribute ram_style of cache0, cache1 : signal is "distributed";
    signal valid0, valid1 : std_logic := '0';
    type state_t is (idle, prepare_rows, hit, choose, choose_group, address_pixel, wait_ram, consume, write_cache);
    signal state : state_t := idle;
    signal row_id, selected : integer range 0 to 63 := 0;
    signal target_y : unsigned(7 downto 0) := (others => '0');
    signal target_x : integer range 0 to 239 := 0;
    signal world_x : unsigned(7 downto 0) := (others => '0');
    signal held_hoff : integer range 0 to 239 := 0;
    signal bank : std_logic := '0';
    signal hits : std_logic_vector(63 downto 0) := (others => '0');
    signal slot : integer range 0 to 3 := 0;
    signal packed : std_logic_vector(27 downto 0) := (others => '0');
    signal pixel_low : std_logic := '0';
    signal pixel_mode : std_logic_vector(2 downto 0) := "000";
    type group_ids_t is array(0 to 7) of integer range 0 to 7;
    signal group_ids : group_ids_t := (others => 0);
    signal group_hits : std_logic_vector(7 downto 0) := (others => '0');
    function first_hit(h : std_logic_vector(7 downto 0)) return integer is
        variable result : integer range 0 to 7 := 0;
    begin
        for i in 7 downto 0 loop
            if h(i) = '1' then result := i; end if;
        end loop;
        return result;
    end;
begin
    -- Port A belongs entirely to the CPU command clock; invalid addresses
    -- never alias the low 32 KiB. RAM is retained over a warm reset.
    process(COMMAND_CLK)
    begin
        if rising_edge(COMMAND_CLK) then
            if MEM_ADDR < 32768 then
                if MEM_WRITE = '1' and COMMAND_RESET = '0' then
                    mem(to_integer(MEM_ADDR(1 to 15))) <= MEM_IN;
                    MEM_OUT <= MEM_IN; -- Infer WRITE_FIRST: avoid Spartan-6 AR34533.
                else MEM_OUT <= mem(to_integer(MEM_ADDR(1 to 15))); end if;
            else MEM_OUT <= X"00"; end if;
        end if;
    end process;
    process(CLK)
    begin
        if rising_edge(CLK) then rd_data <= mem(rd_addr); end if;
    end process;
    DESC_READY <= '1' when last = '1' and req = ack_sync and COMMAND_RESET = '0' else '0';
    process(COMMAND_CLK)
    begin
        if rising_edge(COMMAND_CLK) then
            if COMMAND_RESET = '1' then
                req <= '0'; last <= '0'; ack_meta <= '0'; ack_sync <= '0';
            else
                ack_meta <= ack; ack_sync <= ack_meta; last <= DESC_VALID;
                if DESC_VALID = '1' and last = '0' and req = ack_sync then
                    hold_define <= DESC_DEFINE; hold_id <= to_integer(DESC_ID(2 to 7));
                    hold_b <= unsigned(DESC_B); hold_c <= unsigned(DESC_C);
                    hold_hl <= unsigned(DESC_HL); req <= not req;
                end if;
            end if;
        end if;
    end process;
    process(CLK)
    begin
        if rising_edge(CLK) then
            if RESET = '1' then
                req_meta <= '0'; req_sync <= '0'; ack <= '0';
                modes <= (others => "000"); widths <= (others => (others => '0'));
                heights <= (others => (others => '0')); bases <= (others => (others => '0'));
                posx <= (others => (others => '0')); posy <= (others => (others => '0'));
            else
                req_meta <= req; req_sync <= req_meta;
                if req_sync /= ack then
                    if hold_define = '1' then
                        widths(hold_id) <= hold_b; heights(hold_id) <= hold_c;
                        bases(hold_id) <= hold_hl; modes(hold_id) <= "000";
                    else
                        modes(hold_id) <= std_logic_vector(hold_b(2 downto 0));
                        posx(hold_id) <= hold_hl(15 downto 8); posy(hold_id) <= hold_hl(7 downto 0);
                    end if;
                    ack <= req_sync;
                end if;
            end if;
        end if;
    end process;
    process(CLK)
        variable ty, wx, id, pixel_offset, addr : integer;
        variable h : std_logic_vector(63 downto 0);
        variable color : std_logic_vector(3 downto 0);
    begin
        if rising_edge(CLK) then
            if RESET = '1' then
                state <= idle; valid0 <= '0'; valid1 <= '0'; hits <= (others => '0');
            else
                case state is
                    when idle =>
                        if RASTER_X = 0 and RASTER_Y >= 36 and RASTER_Y <= 672 and RASTER_Y mod 4 = 0 then
                            ty := (RASTER_Y-36)/4;
                            if ty mod 2 = 0 then bank <= '0'; else bank <= '1'; end if;
                            ty := ty + VOFF; if ty >= 160 then ty := ty-160; end if;
                            target_y <= to_unsigned(ty, 8); held_hoff <= HOFF;
                            row_id <= 0; target_x <= 0; state <= prepare_rows;
                        end if;
                    when prepare_rows =>
                        line_posx(row_id) <= posx(row_id); line_widths(row_id) <= widths(row_id);
                        line_bases(row_id) <= bases(row_id); line_modes(row_id) <= modes(row_id);
                        rows(row_id) <= (target_y-posy(row_id))*widths(row_id);
                        if target_y >= posy(row_id) and target_y-posy(row_id) < heights(row_id) and modes(row_id) /= "000" then
                            row_hits(row_id) <= '1';
                        else row_hits(row_id) <= '0'; end if;
                        if row_id = 63 then state <= hit; else row_id <= row_id+1; end if;
                    when hit =>
                        wx := target_x + held_hoff; if wx >= 240 then wx := wx-240; end if;
                        world_x <= to_unsigned(wx, 8);
                        for i in 0 to 63 loop
                            if row_hits(i) = '1' and wx >= to_integer(line_posx(i)) and wx-to_integer(line_posx(i)) < to_integer(line_widths(i)) then h(i) := '1';
                            else h(i) := '0'; end if;
                        end loop;
                        hits <= h; packed <= (others => '0'); slot <= 0; state <= choose;
                    when choose =>
                        if hits = X"0000000000000000" then state <= write_cache;
                        else
                            for g in 0 to 7 loop
                                group_ids(g) <= first_hit(hits(g*8+7 downto g*8));
                                if hits(g*8+7 downto g*8) /= X"00" then group_hits(g) <= '1';
                                else group_hits(g) <= '0'; end if;
                            end loop;
                            state <= choose_group;
                        end if;
                    when choose_group =>
                        id := first_hit(group_hits);
                        selected <= id*8+group_ids(id);
                        state <= address_pixel;
                    when address_pixel =>
                        hits(selected) <= '0';
                        pixel_offset := to_integer(rows(selected))+to_integer(world_x)-to_integer(line_posx(selected));
                        addr := to_integer(line_bases(selected))+pixel_offset/2;
                        rd_addr <= addr;
                        if pixel_offset mod 2 = 0 then pixel_low <= '0'; else pixel_low <= '1'; end if;
                        pixel_mode <= line_modes(selected); state <= wait_ram;
                    when wait_ram => state <= consume;
                    when consume =>
                        if pixel_low = '0' then color := std_logic_vector(rd_data(0 to 3));
                        else color := std_logic_vector(rd_data(4 to 7)); end if;
                        packed(slot*7+6 downto slot*7) <= pixel_mode & color;
                        if slot = 3 then state <= write_cache;
                        else slot <= slot+1; state <= choose; end if;
                    when write_cache =>
                        if bank = '0' then cache0(target_x) <= packed;
                        else cache1(target_x) <= packed; end if;
                        if target_x = 239 then
                            if bank = '0' then valid0 <= '1'; else valid1 <= '1'; end if;
                            state <= idle;
                        else target_x <= target_x+1; state <= hit; end if;
                end case;
            end if;
        end if;
    end process;
    process(LOOKUP_X, LOOKUP_Y, cache0, cache1, valid0, valid1)
        variable lx, ly : integer;
    begin
        OVERLAY <= (others => '0');
        if LOOKUP_X >= 160 and LOOKUP_X < 1120 and LOOKUP_Y >= 40 and LOOKUP_Y < 680 then
            lx := (LOOKUP_X-160)/4; ly := (LOOKUP_Y-40)/4;
            if ly mod 2 = 0 and valid0 = '1' then OVERLAY <= cache0(lx);
            elsif ly mod 2 = 1 and valid1 = '1' then OVERLAY <= cache1(lx); end if;
        end if;
    end process;
end;
