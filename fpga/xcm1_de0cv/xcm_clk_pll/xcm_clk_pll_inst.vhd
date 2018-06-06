	component xcm_clk_pll is
		port (
			xcm_clk_pll_in_clk      : in  std_logic := 'X'; -- clk
			xcm_clk_pll_reset_reset : in  std_logic := 'X'; -- reset
			xcm_clk_pll_out_clk     : out std_logic         -- clk
		);
	end component xcm_clk_pll;

	u0 : component xcm_clk_pll
		port map (
			xcm_clk_pll_in_clk      => CONNECTED_TO_xcm_clk_pll_in_clk,      --    xcm_clk_pll_in.clk
			xcm_clk_pll_reset_reset => CONNECTED_TO_xcm_clk_pll_reset_reset, -- xcm_clk_pll_reset.reset
			xcm_clk_pll_out_clk     => CONNECTED_TO_xcm_clk_pll_out_clk      --   xcm_clk_pll_out.clk
		);

