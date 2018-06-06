	component vga_pll is
		port (
			vga_pll_clk_in_clk  : in  std_logic := 'X'; -- clk
			vga_pll_reset_reset : in  std_logic := 'X'; -- reset
			vga_pll_clk_out_clk : out std_logic         -- clk
		);
	end component vga_pll;

	u0 : component vga_pll
		port map (
			vga_pll_clk_in_clk  => CONNECTED_TO_vga_pll_clk_in_clk,  --  vga_pll_clk_in.clk
			vga_pll_reset_reset => CONNECTED_TO_vga_pll_reset_reset, --   vga_pll_reset.reset
			vga_pll_clk_out_clk => CONNECTED_TO_vga_pll_clk_out_clk  -- vga_pll_clk_out.clk
		);

