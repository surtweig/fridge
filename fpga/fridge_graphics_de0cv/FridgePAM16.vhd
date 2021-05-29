library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;

use work.FridgeGlobals.all;
use work.FridgePAM16Commands.all;

entity FridgePAM16 is

port (
    CLK : in std_logic;
    
    COMMAND_ENABLED : in std_logic; 
    COMMAND_READY : out std_logic;
    DATA : inout XCM2_DWORD;
    COMMAND_CODE : in PAM16_COMMAND
);

end FridgePAM16;

architecture main of FridgePAM16 is
    
    type PAMState is (
        PAM_READY,
        PAM_UNPACK_ARG0,
        PAM_UNPACK_ARG1,
        PAM_UNPACK_ARG2,
        PAM_OP_START,
        PAM_OP_NORMALIZE_ARG0,
        PAM_OP_NORMALIZE_ARG1,
        PAM_OP_COMPUTE,
        PAM_OP_NORMALIZE_RESULT,
        PAM_OP_FINISH,
        PAM_PACK_RESULT,
        PAM_PUSH_RESULT
    );
    
    signal stack : PAM16_STACK;
    signal sp : integer range 0 to PAM16_STACK_SIZE-1;
    signal currentCmd : PAM16_COMMAND;
    signal ready : std_logic:= '1';
    signal inputData : XCM2_DWORD;
    signal state, nextState : PAMState:= PAM_READY;
    
    signal result : PAM16_POSIT;
    signal unp_result : PAM16_POSIT_UNPACKED;
    signal p_a : PAM16_POSIT;
    signal p_b : PAM16_POSIT;
    signal unp_a : PAM16_POSIT_UNPACKED;
    signal unp_b : PAM16_POSIT_UNPACKED;
    
    signal es : XCM2_WORD;
    
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
        elsif sel = ('1', '1') then
            return z3;
        end if;
    end function;
    
    function clz(p : PAM16_POSIT) return unsigned is    
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
        return unsigned(res);
    end function;
    
    procedure positUnpack(signal p : in PAM16_POSIT; signal unp : out PAM16_POSIT_UNPACKED; signal es : in PAM16_POSIT) is
    begin
        unp.sign <= p(0);
        
    end procedure positUnpack;
    
    procedure positAdd(signal result : out PAM16_POSIT_UNPACKED;
                       signal state : in PAMState; signal nextState : out PAMState;
                       signal a, b : in PAM16_POSIT_UNPACKED; signal es : in PAM16_POSIT) is
    begin
        
    end procedure positAdd;
    
    
begin

    COMMAND_READY <= ready;

    process (CLK) is
        variable inputLow : XCM2_WORD;
        variable stackPos : integer;
    begin
    
        if falling_edge(CLK) then
            if state = PAM_READY then
                if COMMAND_ENABLED = '1' and ready = '1' then
                
                    if currentCmd = PAM16_NOP then
                                    
                        currentCmd <= COMMAND_CODE;
                        inputData <= DATA;
                    
                    elsif currentCmd = PAM16_RESET then
                    
                        -- input low word to ES
                        inputLow:= inputData(8 to 15);
                        if inputLow < 1 then
                            es <= X"01";
                        elsif inputLow > PAM16_POSIT_SIZE-1 then
                            es <= XCM2_WORD(to_unsigned(PAM16_POSIT_SIZE-1, 8));
                        else
                            es <= inputLow;
                        end if;             
                        
                        for i in 0 to PAM16_STACK_SIZE-1 loop
                            stack(i) <= X"0000";
                        end loop;
                        sp <= 0;
                        
                    elsif currentCmd = PAM16_PUSH then
                    
                        if stackPos < PAM16_STACK_SIZE then
                            stackPos:= sp;
                            stack(stackPos) <= PAM16_POSIT(DATA);
                            sp <= stackPos+1;
                        end if;
                    
                    elsif currentCmd = PAM16_POP then
                    
                        stackPos:= sp-1;
                        if stackPos >= 0 then
                            DATA <= PAM16_POSIT(stack(stackPos));
                            sp <= stackPos;
                        else
                            DATA <= PAM16_POSIT_ZERO;
                        end if;
                    
                    elsif currentCmd = PAM16_ADD then
                    
                        p_a <= PAM16_POSIT(stack(sp-1));
                        p_b <= PAM16_POSIT(stack(sp-2));
                        nextState <= PAM_OP_START;
                        
                    end if;
                
                end if; -- COMMAND_ENABLED
            elsif state >= PAM_UNPACK_ARG0 and state <= PAM_UNPACK_ARG2 then
            elsif state >= PAM_OP_START and state <= PAM_OP_FINISH then
            elsif state = PAM_PACK_RESULT then
            end if;
        end if;
        
        if rising_edge(CLK) then
            state <= nextState;
        end if;
        
    end process;

end main;