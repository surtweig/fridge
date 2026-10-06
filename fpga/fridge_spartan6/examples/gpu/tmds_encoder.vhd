library ieee;
use ieee.std_logic_1164.all;

entity tmds_encoder is
    port (
        clk, reset, active : in std_logic;
        data : in std_logic_vector(7 downto 0);
        control : in std_logic_vector(1 downto 0);
        symbol : out std_logic_vector(9 downto 0)
    );
end tmds_encoder;

architecture rtl of tmds_encoder is
    signal disparity : integer range -31 to 31 := 0;
begin
    process (clk)
        variable ones, balance : integer;
        variable q : std_logic_vector(8 downto 0);
        variable use_xnor : boolean;
    begin
        if rising_edge(clk) then
            if reset = '1' then
                disparity <= 0;
                symbol <= "1101010100";
            elsif active = '0' then
                disparity <= 0;
                case control is
                    when "00" => symbol <= "1101010100";
                    when "01" => symbol <= "0010101011";
                    when "10" => symbol <= "0101010100";
                    when others => symbol <= "1010101011";
                end case;
            else
                ones := 0;
                for i in 0 to 7 loop
                    if data(i) = '1' then ones := ones + 1; end if;
                end loop;
                use_xnor := ones > 4 or (ones = 4 and data(0) = '0');
                q(0) := data(0);
                for i in 1 to 7 loop
                    if use_xnor then q(i) := q(i-1) xnor data(i);
                    else q(i) := q(i-1) xor data(i); end if;
                end loop;
                if use_xnor then q(8) := '0'; else q(8) := '1'; end if;
                ones := 0;
                for i in 0 to 7 loop
                    if q(i) = '1' then ones := ones + 1; end if;
                end loop;
                balance := 2 * ones - 8;
                if disparity = 0 or balance = 0 then
                    symbol(9) <= not q(8);
                    symbol(8) <= q(8);
                    if q(8) = '1' then
                        symbol(7 downto 0) <= q(7 downto 0);
                        disparity <= disparity + balance;
                    else
                        symbol(7 downto 0) <= not q(7 downto 0);
                        disparity <= disparity - balance;
                    end if;
                elsif (disparity > 0 and balance > 0) or (disparity < 0 and balance < 0) then
                    symbol <= '1' & q(8) & not q(7 downto 0);
                    if q(8) = '1' then disparity <= disparity + 2 - balance;
                    else disparity <= disparity - balance; end if;
                else
                    symbol <= '0' & q;
                    if q(8) = '0' then disparity <= disparity - 2 + balance;
                    else disparity <= disparity + balance; end if;
                end if;
            end if;
        end if;
    end process;
end rtl;
