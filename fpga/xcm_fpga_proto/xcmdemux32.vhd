library IEEE;
use IEEE.STD_LOGIC_1164.all;
use ieee.numeric_std.all;
 
entity cpu_demux is
port
(
   sel : in std_logic;
   ircode : in std_logic_vector(4 downto 0);
   irsel : out std_logic_vector(31 downto 0)
);
end cpu_demux;

architecture cpu_demux_behaviour of cpu_demux is  
begin
   cpu_demux_process : process(sel, ircode)
   begin
      irsel <= (others => '0');
      if sel = '1' then
         irsel(to_integer(unsigned(ircode))) <= '1';
      end if;
   end process;
end cpu_demux_behaviour;