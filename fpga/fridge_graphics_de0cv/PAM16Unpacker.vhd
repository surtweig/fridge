library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;

use work.FridgeGlobals.all;

entity PAM16Unpacker is
    port (
        POSIT : in PAM16_POSIT;
        ES : in XCM2_WORD;--unsigned(3 downto 0);
        
        --SIGN, IS_ZERO, IS_NAR : out std_logic;
        --REGIME : out signed(4 downto 0);
        --EXPONENT : out unsigned(12 downto 0);
        --FRACTION : out unsigned(12 downto 0);
        UNP : out PAM16_POSIT_UNPACKED
    );
end PAM16Unpacker;

architecture unpacker_main of PAM16Unpacker is

    -- https://www.researchgate.net/publication/284919835_Modular_Design_Of_Fast_Leading_Zeros_Counting_Circuit
    procedure clz_nlc(x : in std_logic_vector(0 to 3); a : out std_logic; z : out std_logic_vector(0 to 1)) is
    begin
        a:= not (x(0) or x(1) or x(2) or x(3));
        z:= (not (x(0) or (not x(1) and x(2))),
             not (x(0) or x(1)));
    end procedure;
    
    procedure clz_bne(a : in std_logic_vector(0 to 3); q : out std_logic; y : out std_logic_vector(0 to 1)) is
    begin
        q:= a(0) and a(1) and a(2) and a(3);
        y:= (a(0) and (not a(1) or (a(2) and not a(3))),
             a(0) and a(1) and (not a(2) or not a(3)));
    end procedure;
    
    function clz_mux(sel, z0, z1, z2, z3 : in std_logic_vector(0 to 1)) return std_logic_vector is
    begin
        if sel = ('0', '0') then
            return z0;
        elsif sel = ('1', '0') then
            return z1;
        elsif sel = ('0', '1') then
            return z2;
        else 
            return z3;
        end if;
    end function;
    
    function clz(p : PAM16_POSIT) return integer is    
        variable x0, x1, x2, x3, a : std_logic_vector(0 to 3);
        variable y, yz, z0, z1, z2, z3 : std_logic_vector(0 to 1);
        variable res : std_logic_vector(0 to 3);
        variable q : std_logic;
    begin
        x0:= std_logic_vector(p(0 to 3));
        x1:= std_logic_vector(p(4 to 7));
        x2:= std_logic_vector(p(8 to 11));
        x3:= std_logic_vector(p(12 to 15));
        clz_nlc(x0, a(0), z0);
        clz_nlc(x1, a(1), z1);
        clz_nlc(x2, a(2), z2);
        clz_nlc(x3, a(3), z3);
        clz_bne(a, q, y);
        yz:= clz_mux(y, z0, z1, z2, z3);
        res:= y(1) & y(0) & yz(1) & yz(0);
        return to_integer(unsigned(res));
    end function;

begin

    process (POSIT) is
        variable regimeBit : std_logic;
        variable regimeValue, regimeSize, expSize, fracSize : integer;
        variable regtmp, exptmp, fractmp, mask, xposit : PAM16_POSIT;
    begin
        UNP.sign <= POSIT(0);
        
        if (POSIT = PAM16_POSIT_ZERO) then
            UNP.zero <= '1';
        else
            UNP.zero <= '0';
        end if;
        
        if (POSIT = PAM16_POSIT_NAR) then
            UNP.nar <= '1';
        else
            UNP.nar <= '0';
        end if;
        
        regimeBit:= POSIT(1);
        if (regimeBit = '0') then
            xposit:= POSIT;
        else
            xposit:= not POSIT;
        end if;
        regtmp:= X"0000";
        regtmp(0 to PAM16_POSIT_SIZE-2):= xposit(1 to PAM16_POSIT_SIZE-1);
        regimeSize:= clz(regtmp);
        if (regimeBit = '0') then
            regimeValue:= -regimeSize;
        else
            regimeValue:= regimeSize - 1;
        end if;
        UNP.regime <= regimeValue;
        
        expSize:= to_integer(ES);
        exptmp:= shift_left(POSIT, regimeSize+2);
        fractmp:= exptmp;
        exptmp:= shift_right(exptmp, PAM16_POSIT_SIZE-expSize);
        UNP.exponent <= exptmp;
        
        fracSize:= PAM16_POSIT_SIZE-regimeSize-2-expSize;
        fractmp:= shift_left(fractmp, expSize);
        fractmp:= shift_right(fractmp, PAM16_POSIT_SIZE-fracSize);
        UNP.fraction <= fractmp;

        --regimeBit:= POSIT(1);
        --if (regimeBit = '0') then
        --    regtmp:= POSIT;
        --    regtmp(0):= '0';
        --    regimeSize:= clz(regtmp)-1;
        --    regimeValue:= -regimeSize;
        --else
        --    regtmp:= not POSIT;
        --    regtmp(0):= '0';
        --    regimeSize:= clz(regtmp)-1;
        --    regimeValue:= regimeSize-1;
        --end if;
        --UNP.regime <= regimeValue;
        --
        --exptmp:= shift_left(POSIT, regimeSize+2);
        --expSize:= PAM16_POSIT_SIZE-regimeSize-2;
        --if (expSize > to_integer(ES)) then
        --    expSize:= to_integer(ES);
        --end if;
        --
        --fracSize:= PAM16_POSIT_SIZE-regimeSize-2-expSize;
        --fractmp:= exptmp;
        --
        --mask:= X"FFFF";
        --mask:= shift_right(mask, fracSize);
        --UNP.exponent <= exptmp and mask;
        --
        --if (fracSize > 0) then
        --    fractmp:= shift_left(fractmp, expSize);
        --    UNP.fraction <= fractmp;
        --else
        --    UNP.fraction <= (others => '0');
        --end if;
        
    end process;
    
end unpacker_main;