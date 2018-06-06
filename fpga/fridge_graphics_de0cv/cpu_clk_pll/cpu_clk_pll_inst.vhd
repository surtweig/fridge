	component cpu_clk_pll is
		port (
			cpu_clk_pll_in_clk      : in  std_logic := 'X'; -- clk
			cpu_clk_pll_reset_reset : in  std_logic := 'X'; -- reset
			cpu_clk_pll_out_clk     : out std_logic         -- clk
		);
	end component cpu_clk_pll;

	u0 : component cpu_clk_pll
		port map (
			cpu_clk_pll_in_clk      => CONNECTED_TO_cpu_clk_pll_in_clk,      --    cpu_clk_pll_in.clk
			cpu_clk_pll_reset_reset => CONNECTED_TO_cpu_clk_pll_reset_reset, -- cpu_clk_pll_reset.reset
			cpu_clk_pll_out_clk     => CONNECTED_TO_cpu_clk_pll_out_clk      --   cpu_clk_pll_out.clk
		);

