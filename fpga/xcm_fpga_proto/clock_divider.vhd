library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
  
entity clock_divider is
generic (rate : integer:= 1);
port ( 
   refclk, reset: in std_logic;
   outclk: out std_logic);
end clock_divider;
  
architecture clock_divider_behaviour of clock_divider is
  
   signal count: integer:= 1;
   signal tmp : std_logic:= '0';
  
begin
  
   process(refclk, reset)
   begin
      if(reset = '1') then
         count <= 1;
         tmp <= '0';
      elsif (refclk'event and refclk = '1') then
         count <= count + 1;
         if (count = rate) then
            tmp <= NOT tmp;
            count <= 1;
         end if;
      end if;
      outclk <= tmp;
   end process;
  
end clock_divider_behaviour;