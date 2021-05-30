library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;

use work.FridgeGlobals.all;
use work.FridgeIRCodes.all;

entity FridgeCPU is
     
port (
     CLK_MAIN : in std_logic;
     CLK_PHI2 : in std_logic;
     RESET : in std_logic;
     HALTED : out std_logic;
     
     INTE : out std_logic;
     INT : in std_logic;
     INT_IRQ : in XCM2_WORD;
     
     DEVICE_SEL : out XCM2_WORD;
     DEVICE_READ : out std_logic;
     DEVICE_DATA : inout XCM2_WORD;
  
     RAM_WRITE_DATA : out XCM2_WORD;
     RAM_WRITE_ADDR : out XCM2_DWORD;
     RAM_WRITE_ENABLED : out std_logic;
     RAM_READ_DATA : in XCM2_WORD;
     RAM_READ_ADDR : out XCM2_DWORD;	  
     
     GPU_MODE_SWITCH : out std_logic_vector(0 to 1);
     GPU_PALETTE_SWITCH : out std_logic;
     GPU_PRESENT_TRIGGER : out std_logic;
     
     GPU_BACK_STORE : out std_logic;
     GPU_BACK_LOAD : out std_logic;
     GPU_BACK_ADDR : out XCM2_DWORD;
     GPU_BACK_DATA : inout XCM2_WORD;
     GPU_BACK_CLR : out std_logic;

     GPU_VMEM_STORE : out std_logic;
     GPU_VMEM_LOAD : out std_logic;
     GPU_VMEM_ADDR : out XCM2_DWORD;
     GPU_VMEM_DATA : inout XCM2_WORD;
     
     DEBUG_STEP : in std_logic;
     DEBUG_STATE : out XCM2_WORD;
     DEBUG_CURRENT_IR : out XCM2_WORD;
     DEBUG_PC_L : out XCM2_WORD
);
     
end FridgeCPU;

architecture main of FridgeCPU is

     type CPUState is (
          CPU_INVALID,             -- 00
          CPU_HALTED,              -- 01 
          CPU_FETCH_IR,            -- 02
          CPU_FETCH_ARG0,          -- 03
          CPU_FETCH_ARG1,          -- 04
          CPU_EXECUTE_IR,          -- 05
          CPU_EXECUTE_IR_STAGE_2,  -- 06
          CPU_EXECUTE_IR_STAGE_3,  -- 07
          CPU_EXECUTE_IR_STAGE_4,  -- 08
          CPU_EXECUTE_IR_STAGE_5,  -- 09
          CPU_LOAD_WORD,           -- 0A
          CPU_LOAD_DWORD_H,        -- 0B
          CPU_LOAD_DWORD_L,        -- 0C
          CPU_STORE_WORD,          -- 0D
          CPU_STORE_DWORD_H,       -- 0E
          CPU_STORE_DWORD_L,       -- 0F
          CPU_DEVICE_READ,         -- 10
          CPU_DEVICE_WRITE         -- 11
     );

     signal state : CPUState:= CPU_INVALID;
     signal nextState : CPUState:= CPU_FETCH_IR;
     signal interruptsEnabled, interruptRequested, interruptInProgress : std_logic:= '0';
     signal interruptAddr : XCM2_WORD;
     signal deviceData, deviceSel : XCM2_WORD;
     signal PC, nextPC : XCM2_DWORD;
     signal SP, nextSP : XCM2_DWORD;
     signal rA, rB, rC, rD, rE, rH, rL, rF : XCM2_WORD;
     --signal fSign, fZero, fAux, fParity, fCarry : std_logic;
     alias fSign : std_logic is rF(0);
     alias fZero : std_logic is rF(1);
     alias fAux : std_logic is rF(2);
     alias fParity : std_logic is rF(3);
     alias fCarry : std_logic is rF(4);
     --signal memory : XCM2_RAM:= (others => X"0");
     
     signal currentIRCode : XCM2_WORD:= NOP;
     signal IRArg0 : XCM2_WORD:= X"00";
     signal IRArg1 : XCM2_WORD:= X"00";
     signal memReadBufferH, memReadBufferL : XCM2_WORD;
     signal memWriteBuffer : XCM2_WORD;
     signal memAddrBuffer : XCM2_DWORD;
     signal memWriteEnabled : std_logic;
     signal IRTemp : XCM2_WORD:= X"00";
     
     signal gpuBackClr, gpuPresentTrigger, gpuBackStore, gpuBackLoad : std_logic:= '0';
     signal gpuBackAddr : XCM2_DWORD;
     signal gpuBackData : XCM2_WORD;
     signal gpuModeSwitch : std_logic_vector(0 to 1):= ('0', '0');
     
     signal debugStepPressed : std_logic:= '0';
     
     --function pcRead() return XCM2_WORD is
     --begin
     --     
     --end function pcRead;
     
     procedure defaultSetFlags(result : in XCM2_WORD; signal fSign, fZero, fParity : out std_logic) is
     begin
          fSign <= result(0);
     
          if (result = 0) then
               fZero <= '1';
          else
               fZero <= '0';
          end if;
          
          fParity <= not result(7); -- doesn't meet i8080 specification
     end procedure defaultSetFlags;
     
     procedure compareSetFlags(a, b : in XCM2_WORD; signal fCarry, fSign, fZero, fParity : out std_logic) is
     begin
          if a < b then
               fCarry <= '0';
               fSign <= '1';
               fZero <= '0';
          elsif a > b then
               fCarry <= '1';
               fSign <= '0';
               fZero <= '0';
          else
               fCarry <= '0';
               fSign <= '0';
               fZero <= '1';
          end if;
          
          fParity <= not (a(7) xor b(7)); -- doesn't meet i8080 specification
     end procedure compareSetFlags;
     
     function packFlags(signal fCarry, fSign, fZero, fParity : in std_logic) return XCM2_WORD is
     variable rF : XCM2_WORD;
     begin
          rF:= X"00";
          rF(0):= fCarry;
          rF(1):= fSign;
          rF(2):= fZero;
          rF(3):= fParity;
          return rF;
     end function packFlags;
     
     procedure unpackFlags(rF : in XCM2_WORD; signal fCarry, fSign, fZero, fParity : out std_logic) is
     begin
          fCarry <= rF(0);
          fSign <= rF(1);
          fZero <= rF(2);
          fParity <= rF(3);
     end procedure unpackFlags;
     
     function XCM2_DWORD_HL(H, L : in XCM2_WORD) return XCM2_DWORD is
          variable vh, vl : std_logic_vector(0 to 7);
          variable vhl : std_logic_vector(0 to 15);
     begin
          vh:= std_logic_vector(H);
          vl:= std_logic_vector(L);
          vhl:= vh & vl;
          return unsigned(vhl);
     end function XCM2_DWORD_HL;
     
     function XCM2_HIGH_WORD(D : in XCM2_DWORD) return XCM2_WORD is          
     begin
          return D(0 to 7);
     end function XCM2_HIGH_WORD;
     
     function XCM2_LOW_WORD(D : in XCM2_DWORD) return XCM2_WORD is          
     begin
          return D(8 to 15);
     end function XCM2_LOW_WORD;     
     
     function boolean_to_std_logic(b : in boolean) return std_logic is
     begin
          if b then
               return '1';
          else
               return '0';
          end if;
     end function boolean_to_std_logic;
         
     procedure ir_MOV(signal rDst : inout XCM2_WORD; signal rSrc : in XCM2_WORD; signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               rDst <= rSrc;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_MOV;
     
     procedure ir_MOV_RM(signal rDst : inout XCM2_WORD; signal rH, rL : in XCM2_WORD; 
                         signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD;
                         signal state : in CPUState; variable nextState : inout CPUState) is
     variable vh, vl : std_logic_vector(0 to 7);
     variable vhl : std_logic_vector(0 to 15);
     
     begin
          if (state = CPU_EXECUTE_IR) then
               vh:= std_logic_vector(rH);
               vl:= std_logic_vector(rL);
               vhl:= vh & vl;
               memAddrBuffer <= unsigned(vhl);--std_logic_vector(rH) & std_logic_vector(rL));
               nextState:= CPU_LOAD_WORD;
          elsif (state = CPU_LOAD_WORD) then
               rDst <= RAM_READ_DATA;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_MOV_RM;
     
     procedure ir_MOV_MR(signal rSrc : in XCM2_WORD; signal rH, rL : in XCM2_WORD;
                         signal memAddrBuffer : inout XCM2_DWORD; signal memWriteBuffer : inout XCM2_WORD;
                         signal state : in CPUState; variable nextState : inout CPUState) is
     variable vh, vl : std_logic_vector(0 to 7);
     variable vhl : std_logic_vector(0 to 15);
     
     begin
          if (state = CPU_EXECUTE_IR) then
               vh:= std_logic_vector(rH);
               vl:= std_logic_vector(rL);
               vhl:= vh & vl;
               memAddrBuffer <= unsigned(vhl);
               memWriteBuffer <= rSrc;
               nextState:= CPU_STORE_WORD;
          elsif (state = CPU_STORE_WORD) then
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_MOV_MR;
     
     procedure ir_MVI(signal rDst : inout XCM2_WORD; signal IRArg0 : in XCM2_WORD; signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               rDst <= IRArg0;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_MVI;

     procedure ir_MVI_M(signal IRArg0 : in XCM2_WORD; signal rH, rL : in XCM2_WORD;
                        signal memAddrBuffer : inout XCM2_DWORD; signal memWriteBuffer : inout XCM2_WORD;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     variable vh, vl : std_logic_vector(0 to 7);
     variable vhl : std_logic_vector(0 to 15);
     
     begin
          if (state = CPU_EXECUTE_IR) then
               vh:= std_logic_vector(rH);
               vl:= std_logic_vector(rL);
               vhl:= vh & vl;
               memAddrBuffer <= unsigned(vhl);
               memWriteBuffer <= IRArg0;
               nextState:= CPU_STORE_WORD;
          elsif (state = CPU_STORE_WORD) then
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_MVI_M;     
     
     procedure ir_LXI(signal rDstH, rDstL : inout XCM2_WORD; signal IRArg0, IRArg1 : in XCM2_WORD;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               rDstH <= IRArg0;
               rDstL <= IRArg1;
               nextState:= CPU_FETCH_IR;
          end if;     
     end procedure ir_LXI;
     
     procedure ir_LXI_D(signal rDst : inout XCM2_DWORD; signal IRArg0, IRArg1 : in XCM2_WORD;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     variable vh, vl : std_logic_vector(0 to 7);
     variable vhl : std_logic_vector(0 to 15);
     
     begin
          if (state = CPU_EXECUTE_IR) then
               vh:= std_logic_vector(IRArg0);
               vl:= std_logic_vector(IRArg1);
               vhl:= vh & vl;
               rDst <= unsigned(vhl);
               nextState:= CPU_FETCH_IR;               
          end if;
     end procedure ir_LXI_D;
     
     procedure ir_LDA(signal rA : inout XCM2_WORD; signal rAddrH, rAddrL : in XCM2_WORD;
                      signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rAddrH, rAddrL);
               nextState:= CPU_LOAD_WORD;
          elsif (state = CPU_LOAD_WORD) then
               rA <= RAM_READ_DATA;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_LDA;
     
     procedure ir_STA(signal rA : in XCM2_WORD; signal rAddrH, rAddrL : in XCM2_WORD;
                      signal memAddrBuffer : inout XCM2_DWORD; signal memWriteBuffer : inout XCM2_WORD;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rAddrH, rAddrL);
               memWriteBuffer <= rA;
               nextState:= CPU_STORE_WORD;
          elsif (state = CPU_STORE_WORD) then
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_STA;
     
     procedure ir_LHLD(signal rH, rL : inout XCM2_WORD; signal rAddrH, rAddrL : in XCM2_WORD;
                       signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD;
                       signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rAddrH, rAddrL);
               nextState:= CPU_LOAD_DWORD_H;
          elsif (state = CPU_LOAD_DWORD_H) then
               memAddrBuffer <= memAddrBuffer + 1;
               rH <= RAM_READ_DATA;
               nextState:= CPU_LOAD_DWORD_L;
          elsif (state = CPU_LOAD_DWORD_L) then
               rL <= RAM_READ_DATA;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_LHLD;
     
     procedure ir_SHLD(signal rH, rL : in XCM2_WORD; signal rAddrH, rAddrL : in XCM2_WORD;
                       signal memAddrBuffer : inout XCM2_DWORD; signal memWriteBuffer : inout XCM2_WORD;
                       signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rAddrH, rAddrL);
               memWriteBuffer <= rH;
               nextState:= CPU_STORE_DWORD_H;
          elsif (state = CPU_STORE_DWORD_H) then
               memAddrBuffer <= memAddrBuffer + 1;
               memWriteBuffer <= rL;
               nextState:= CPU_STORE_DWORD_L;
          elsif (state = CPU_STORE_DWORD_L) then
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_SHLD;
     
     procedure ir_XCNG(signal rD, rE, rH, rL : inout XCM2_WORD; signal state : in CPUState; variable nextState : inout CPUState) is
     variable tD, tE : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               tD:= rD;
               tE:= rE;
               rD <= rH;
               rE <= rL;
               rH <= tD;
               rL <= tE;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_XCNG;
     
     procedure ir_ADD(signal rDst : inout XCM2_WORD; signal rAdd : in XCM2_WORD;
                      signal fSign, fZero, fParity, fCarry : inout std_logic;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               fCarry <= boolean_to_std_logic((not rDst) < rAdd);
               tDst:= rDst + rAdd;
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_ADD;
     
     procedure ir_ADD_M(signal rDst : inout XCM2_WORD; signal rH, rL : in XCM2_WORD;
                        signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD;
                        signal fSign, fZero, fParity, fCarry : inout std_logic;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rH, rL);
               nextState:= CPU_LOAD_WORD;          
          elsif (state = CPU_LOAD_WORD) then
               fCarry <= boolean_to_std_logic((not rDst) < RAM_READ_DATA);
               tDst:= rDst + RAM_READ_DATA;
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst;
               nextState:= CPU_FETCH_IR;          
          end if;
     end procedure ir_ADD_M;
     
     procedure ir_ADC(signal rDst : inout XCM2_WORD; signal rAdd : in XCM2_WORD;
                      signal fSign, fZero, fParity : inout std_logic; signal fCarry : inout std_logic;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst, c : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               tDst:= rDst + rAdd;
               if (fCarry = '1') then
                    c:= X"01";
               else
                    c:= X"00";
               end if;
               fCarry <= boolean_to_std_logic((not rDst) < rAdd + c);
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst + c;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_ADC;
     
     procedure ir_ADC_M(signal rDst : inout XCM2_WORD; signal rH, rL : in XCM2_WORD;
                        signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD;
                        signal fSign, fZero, fParity : inout std_logic; signal fCarry : inout std_logic;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst, c : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rH, rL);
               nextState:= CPU_LOAD_WORD;          
          elsif (state = CPU_LOAD_WORD) then
               tDst:= rDst + RAM_READ_DATA;
               if (fCarry = '1') then
                    c:= X"01";
               else
                    c:= X"00";
               end if;       
               fCarry <= boolean_to_std_logic((not rDst) < RAM_READ_DATA + c);
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst + c;
               nextState:= CPU_FETCH_IR;          
          end if;
     end procedure ir_ADC_M;
     
     procedure ir_SUB(signal rDst : inout XCM2_WORD; signal rSub : in XCM2_WORD;
                      signal fSign, fZero, fParity, fCarry : inout std_logic;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               fCarry <= boolean_to_std_logic(rDst < rSub);
               tDst:= rDst - rSub;
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_SUB;
     
     procedure ir_SUB_M(signal rDst : inout XCM2_WORD; signal rH, rL : in XCM2_WORD;
                        signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD;
                        signal fSign, fZero, fParity, fCarry : inout std_logic;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rH, rL);
               nextState:= CPU_LOAD_WORD;          
          elsif (state = CPU_LOAD_WORD) then
               fCarry <= boolean_to_std_logic(rDst < RAM_READ_DATA);
               tDst:= rDst - RAM_READ_DATA;
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst;
               nextState:= CPU_FETCH_IR;          
          end if;
     end procedure ir_SUB_M;
     
     procedure ir_SBB(signal rDst : inout XCM2_WORD; signal rSub : in XCM2_WORD;
                      signal fSign, fZero, fParity : inout std_logic; signal fCarry : inout std_logic;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst, c : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               tDst:= rDst - rSub;
               if (fCarry = '1') then
                    c:= X"01";
               else
                    c:= X"00";
               end if;
               fCarry <= boolean_to_std_logic(rDst < rSub + c);
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst - c;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_SBB;
     
     procedure ir_SBB_M(signal rDst : inout XCM2_WORD; signal rH, rL : in XCM2_WORD;
                        signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD;
                        signal fSign, fZero, fParity : inout std_logic; signal fCarry : inout std_logic;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst, c : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rH, rL);
               nextState:= CPU_LOAD_WORD;          
          elsif (state = CPU_LOAD_WORD) then
               tDst:= rDst - RAM_READ_DATA;
               if (fCarry = '1') then
                    c:= X"01";
               else
                    c:= X"00";
               end if;       
               fCarry <= boolean_to_std_logic(rDst < RAM_READ_DATA + c);
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst - c;
               nextState:= CPU_FETCH_IR;          
          end if;
     end procedure ir_SBB_M;
     
     procedure ir_INR(signal rDst : inout XCM2_WORD;
                      signal fSign, fZero, fParity, fCarry : inout std_logic;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               fCarry <= boolean_to_std_logic((not rDst) = 0);
               tDst:= rDst + X"01";
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_INR;     
     
     procedure ir_INR_M(signal rH, rL : in XCM2_WORD;
                        signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD; signal memWriteBuffer : inout XCM2_WORD;
                        signal fSign, fZero, fParity, fCarry : inout std_logic;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rH, rL);
               nextState:= CPU_LOAD_WORD;          
          elsif (state = CPU_LOAD_WORD) then
               fCarry <= boolean_to_std_logic((not RAM_READ_DATA) = 0);
               tDst:= RAM_READ_DATA + 1;
               defaultSetFlags(tDst, fSign, fZero, fParity);
               memWriteBuffer <= tDst;
               nextState:= CPU_STORE_WORD;
          elsif (state = CPU_STORE_WORD) then
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_INR_M;  

     procedure ir_DCR(signal rDst : inout XCM2_WORD;
                      signal fSign, fZero, fParity, fCarry : inout std_logic;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               fCarry <= boolean_to_std_logic((not rDst) = 0);
               tDst:= rDst - X"01";
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_DCR;  
 
     procedure ir_DCR_M(signal rH, rL : in XCM2_WORD;
                        signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD; signal memWriteBuffer : inout XCM2_WORD;
                        signal fSign, fZero, fParity, fCarry : inout std_logic;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rH, rL);
               nextState:= CPU_LOAD_WORD;          
          elsif (state = CPU_LOAD_WORD) then
               fCarry <= boolean_to_std_logic((not RAM_READ_DATA) = 0);
               tDst:= RAM_READ_DATA - 1;
               defaultSetFlags(tDst, fSign, fZero, fParity);
               memWriteBuffer <= tDst;
               nextState:= CPU_STORE_WORD;
          elsif (state = CPU_STORE_WORD) then
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_DCR_M;  

     procedure ir_INX(signal rDstH, rDstL : inout XCM2_WORD; signal state : in CPUState; variable nextState : inout CPUState) is     
     variable tDst : XCM2_DWORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               tDst:= XCM2_DWORD_HL(rDstH, rDstL) + X"0001";
               rDstH <= XCM2_HIGH_WORD(tDst);
               rDstL <= XCM2_LOW_WORD(tDst);
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_INX;

     procedure ir_INX_D(signal rSrc : in XCM2_DWORD; rDst : inout XCM2_DWORD; signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               rDst := rSrc + X"0001";
               nextState:= CPU_FETCH_IR;
          end if;     
     end procedure ir_INX_D;
     
     procedure ir_DCX(signal rDstH, rDstL : inout XCM2_WORD; signal state : in CPUState; variable nextState : inout CPUState) is     
     variable tDst : XCM2_DWORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               tDst:= XCM2_DWORD_HL(rDstH, rDstL) - X"0001";
               rDstH <= XCM2_HIGH_WORD(tDst);
               rDstL <= XCM2_LOW_WORD(tDst);
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_DCX;
     
     procedure ir_DCX_D(signal rSrc : in XCM2_DWORD; rDst : inout XCM2_DWORD; signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               rDst := rSrc - X"0001";
               nextState:= CPU_FETCH_IR;
          end if;     
     end procedure ir_DCX_D;
     
     procedure ir_DAD(signal rH, rL : inout XCM2_WORD; signal rAddH, rAddL : in XCM2_WORD; signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_DWORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               tDst:= XCM2_DWORD_HL(rH, rL) + XCM2_DWORD_HL(rAddH, rAddL);
               rH <= XCM2_HIGH_WORD(tDst);
               rL <= XCM2_LOW_WORD(tDst);
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_DAD;
     
     procedure ir_DAD_D(signal rH, rL : inout XCM2_WORD; signal rAdd : in XCM2_DWORD; signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_DWORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               tDst:= XCM2_DWORD_HL(rH, rL) + rAdd;
               rH <= XCM2_HIGH_WORD(tDst);
               rL <= XCM2_LOW_WORD(tDst);
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_DAD_D;
     
     procedure ir_ANA(signal rDst : inout XCM2_WORD; signal rOp : in XCM2_WORD;
                      signal fSign, fZero, fParity, fCarry : inout std_logic;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               tDst := rDst and rOp;
               defaultSetFlags(tDst, fSign, fZero, fParity);
               fCarry <= '0';
               rDst <= tDst;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_ANA;
     
     procedure ir_ANA_M(signal rDst : inout XCM2_WORD; signal rH, rL : in XCM2_WORD;
                        signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD;
                        signal fSign, fZero, fParity, fCarry : inout std_logic;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rH, rL);
               nextState:= CPU_LOAD_WORD;          
          elsif (state = CPU_LOAD_WORD) then
               fCarry <= '0';
               tDst:= rDst and RAM_READ_DATA;
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst;
               nextState:= CPU_FETCH_IR;          
          end if;
     end procedure ir_ANA_M;     

     procedure ir_ORA(signal rDst : inout XCM2_WORD; signal rOp : in XCM2_WORD;
                      signal fSign, fZero, fParity, fCarry : inout std_logic;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               tDst := rDst or rOp;
               defaultSetFlags(tDst, fSign, fZero, fParity);
               fCarry <= '0';
               rDst <= tDst;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_ORA;
     
     procedure ir_ORA_M(signal rDst : inout XCM2_WORD; signal rH, rL : in XCM2_WORD;
                        signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD;
                        signal fSign, fZero, fParity, fCarry : inout std_logic;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rH, rL);
               nextState:= CPU_LOAD_WORD;          
          elsif (state = CPU_LOAD_WORD) then
               fCarry <= '0';
               tDst:= rDst or RAM_READ_DATA;
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst;
               nextState:= CPU_FETCH_IR;          
          end if;
     end procedure ir_ORA_M;      

     procedure ir_XRA(signal rDst : inout XCM2_WORD; signal rOp : in XCM2_WORD;
                      signal fSign, fZero, fParity, fCarry : inout std_logic;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               tDst := rDst xor rOp;
               defaultSetFlags(tDst, fSign, fZero, fParity);
               fCarry <= '0';
               rDst <= tDst;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_XRA;     
     
     procedure ir_XRA_M(signal rDst : inout XCM2_WORD; signal rH, rL : in XCM2_WORD;
                        signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD;
                        signal fSign, fZero, fParity, fCarry : inout std_logic;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     variable tDst : XCM2_WORD;
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rH, rL);
               nextState:= CPU_LOAD_WORD;          
          elsif (state = CPU_LOAD_WORD) then
               fCarry <= '0';
               tDst:= rDst xor RAM_READ_DATA;
               defaultSetFlags(tDst, fSign, fZero, fParity);
               rDst <= tDst;
               nextState:= CPU_FETCH_IR;          
          end if;
     end procedure ir_XRA_M;    
     
     procedure ir_CMP(signal rA, rB : in XCM2_WORD; signal fSign, fZero, fParity, fCarry : inout std_logic;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               compareSetFlags(rA, rB, fCarry, fSign, fZero, fParity);
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_CMP;
     
     procedure ir_CMP_M(signal rA : in XCM2_WORD; signal rH, rL : in XCM2_WORD;
                        signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD;
                        signal fSign, fZero, fParity, fCarry : inout std_logic;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               memAddrBuffer <= XCM2_DWORD_HL(rH, rL);
               nextState:= CPU_LOAD_WORD;          
          elsif (state = CPU_LOAD_WORD) then
               compareSetFlags(rA, RAM_READ_DATA, fCarry, fSign, fZero, fParity);
               nextState:= CPU_FETCH_IR;          
          end if;
     end procedure ir_CMP_M;
     
     procedure ir_RLC(signal rA : inout XCM2_WORD; signal fCarry : inout std_logic;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               rA <= ROTATE_LEFT(rA, 1);
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_RLC;

     procedure ir_RRC(signal rA : inout XCM2_WORD; signal fCarry : inout std_logic;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               rA <= ROTATE_RIGHT(rA, 1);
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_RRC;
     
     procedure ir_RAL(signal rA : inout XCM2_WORD; signal fCarry : inout std_logic; signal IRTemp : inout XCM2_WORD;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     variable c : std_logic;
     begin
          if (state = CPU_EXECUTE_IR) then
               IRTemp(7) <= rA(0); --high bit
               nextState:= CPU_EXECUTE_IR_STAGE_2;
          elsif (state = CPU_EXECUTE_IR_STAGE_2) then
               rA <= rA(1 to 7) & fCarry;
               nextState:= CPU_EXECUTE_IR_STAGE_3;
          elsif (state = CPU_EXECUTE_IR_STAGE_3) then
               fCarry <= IRTemp(7);
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_RAL;

     procedure ir_RAR(signal rA : inout XCM2_WORD; signal fCarry : inout std_logic; signal IRTemp : inout XCM2_WORD;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     variable c : std_logic;
     begin
          if (state = CPU_EXECUTE_IR) then
               IRTemp(7) <= rA(7); -- low bit
               nextState:= CPU_EXECUTE_IR_STAGE_2;
          elsif (state = CPU_EXECUTE_IR_STAGE_2) then
               rA <= fCarry & rA(0 to 6);
               nextState:= CPU_EXECUTE_IR_STAGE_3;
          elsif (state = CPU_EXECUTE_IR_STAGE_3) then
               fCarry <= IRTemp(7);
               nextState:= CPU_FETCH_IR;
          end if;
          
          --if (state = CPU_EXECUTE_IR) then
          --     carry:= fCarry;
          --     c:= rA(0);
          --     rA <= ROTATE_RIGHT(rA, 1);
          --     rA(0) <= fCarry;
          --     fCarry <= c;
          --     nextState:= CPU_FETCH_IR;
          --end if;
     end procedure ir_RAR;  
   
     procedure ir_CMA(signal rA : inout XCM2_WORD; signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               rA <= not rA;
               nextState:= CPU_FETCH_IR;
          end if;     
     end procedure ir_CMA;
     
     procedure ir_SetCarry(newValue : in std_logic; signal fCarry : inout std_logic; signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               fCarry <= newValue;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_SetCarry;
     
     procedure ir_JMP(condition : in std_logic; signal pcH, pcL : in XCM2_WORD; variable nextPC : inout XCM2_DWORD; signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               if (condition = '1') then
                    nextPC:= XCM2_DWORD_HL(pcH, pcL);
               end if;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_JMP;
     
     procedure ir_CALL(condition : in std_logic; signal pcH, pcL : in XCM2_WORD; signal PC : in XCM2_DWORD; variable nextPC : inout XCM2_DWORD;
                       signal SP : in XCM2_DWORD; variable nextSP : inout XCM2_DWORD;
                       signal memAddrBuffer : inout XCM2_DWORD; signal memWriteBuffer : inout XCM2_WORD;
                       signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               if (condition = '1') then
                    nextState:= CPU_STORE_DWORD_L;
                    memAddrBuffer <= SP - X"01";
                    memWriteBuffer <= XCM2_LOW_WORD(PC);
               else
                    nextState:= CPU_FETCH_IR;
               end if;
               
          elsif (state = CPU_STORE_DWORD_L) then
               nextState:= CPU_STORE_DWORD_H;
               memAddrBuffer <= SP - X"02";
               memWriteBuffer <= XCM2_HIGH_WORD(PC);
               
          elsif (state = CPU_STORE_DWORD_H) then
               nextState:= CPU_FETCH_IR;
               nextPC:= XCM2_DWORD_HL(pcH, pcL);
               nextSP:= SP - X"02";
          end if;
     end procedure ir_CALL;
     
     procedure ir_RET(condition : in std_logic; variable nextPC : inout XCM2_DWORD;
                      signal SP : in XCM2_DWORD; variable nextSP : inout XCM2_DWORD;
                      signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA, memReadBufferH : in XCM2_WORD;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               if (condition = '1') then
                    nextState:= CPU_LOAD_DWORD_H;
                    memAddrBuffer <= SP;
               else
                    nextState:= CPU_FETCH_IR;
               end if;          
          elsif (state = CPU_LOAD_DWORD_H) then
               nextState:= CPU_LOAD_DWORD_L;
               memAddrBuffer <= SP + X"01";
          elsif (state = CPU_LOAD_DWORD_L) then
               nextState:= CPU_FETCH_IR;
               nextSP:= SP + X"02";
               nextPC:= XCM2_DWORD_HL(memReadBufferH, RAM_READ_DATA);
               memAddrBuffer <= nextPC;
          end if;
     end procedure ir_RET;
     
     procedure ir_PCHL(signal rH, rL : in XCM2_WORD; variable nextPC : inout XCM2_DWORD; signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               nextPC:= XCM2_DWORD_HL(rH, rL);
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_PCHL;
     
     procedure ir_PUSH(signal rH, rL : in XCM2_WORD; 
                       signal SP : in XCM2_DWORD; variable nextSP : inout XCM2_DWORD;
                       signal memAddrBuffer : inout XCM2_DWORD; signal memWriteBuffer : inout XCM2_WORD;
                       signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               nextState:= CPU_STORE_DWORD_L;
               memAddrBuffer <= SP - X"01";
               memWriteBuffer <= rL;          
          elsif (state = CPU_STORE_DWORD_L) then
               nextState:= CPU_STORE_DWORD_H;
               memAddrBuffer <= SP - X"02";
               memWriteBuffer <= rH;            
          elsif (state = CPU_STORE_DWORD_H) then
               nextState:= CPU_FETCH_IR;
               nextSP:= SP - X"02";
          end if;
     end procedure ir_PUSH;
     
     procedure ir_POP(signal rH, rL : inout XCM2_WORD;
                      signal SP : in XCM2_DWORD; variable nextSP : inout XCM2_DWORD;
                      signal memAddrBuffer : inout XCM2_DWORD; signal RAM_READ_DATA : in XCM2_WORD;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               nextState:= CPU_LOAD_DWORD_H;
               memAddrBuffer <= SP;         
          elsif (state = CPU_LOAD_DWORD_H) then
               nextState:= CPU_LOAD_DWORD_L;
               memAddrBuffer <= SP + X"01";
               rH <= RAM_READ_DATA;
          elsif (state = CPU_LOAD_DWORD_L) then
               nextState:= CPU_FETCH_IR;
               nextSP:= SP + X"02";
               rL <= RAM_READ_DATA;
          end if;     
     end procedure ir_POP;
 
     procedure ir_XTHL(signal rH, rL : inout XCM2_WORD;
                       signal SP : in XCM2_DWORD;
                       signal memAddrBuffer : inout XCM2_DWORD; signal memReadBufferH, RAM_READ_DATA : in XCM2_WORD; signal memWriteBuffer : inout XCM2_WORD;
                       signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               nextState:= CPU_LOAD_DWORD_H;
               memAddrBuffer <= SP;
          elsif (state = CPU_LOAD_DWORD_H) then
               nextState:= CPU_LOAD_DWORD_L;
               memAddrBuffer <= SP + X"01";
          elsif (state = CPU_LOAD_DWORD_L) then
               nextState:= CPU_STORE_DWORD_H;
               memAddrBuffer <= SP;
               memWriteBuffer <= rH;
          elsif (state = CPU_STORE_DWORD_H) then
               nextState:= CPU_STORE_DWORD_L;
               memAddrBuffer <= SP + X"01";
               memWriteBuffer <= rL;
          elsif (state = CPU_STORE_DWORD_L) then
               nextState:= CPU_FETCH_IR;
               rH <= memReadBufferH;
               rL <= RAM_READ_DATA;
          end if;
     end procedure ir_XTHL;
     
     procedure ir_SPHL(signal rH, rL : in XCM2_WORD;
                       variable nextSP : inout XCM2_DWORD;
                       signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               nextSP:= XCM2_DWORD_HL(rH, rL);
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_SPHL;
     
     procedure ir_HLSP(signal rH, rL : inout XCM2_WORD;
                       signal SP : in XCM2_DWORD;
                       signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               rH <= XCM2_HIGH_WORD(SP);
               rL <= XCM2_LOW_WORD(SP);
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_HLSP;
    
     procedure ir_HLT(signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               nextState:= CPU_HALTED;
          end if;
     end procedure ir_HLT;
     
     procedure ir_EnableInterrupt(enable : in std_logic; signal interruptsEnabled : inout std_logic; signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               interruptsEnabled <= enable;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_EnableInterrupt;
     
     procedure ir_IIN(signal rA : inout XCM2_WORD; signal IRArg0 : in XCM2_WORD;
                      signal deviceSel : inout XCM2_WORD; signal deviceData : in XCM2_WORD;
                      signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               deviceSel <= IRArg0;
               nextState:= CPU_DEVICE_READ;
          elsif (state = CPU_DEVICE_READ) then
               rA <= deviceData;
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_IIN;

     procedure ir_IOUT(signal rA : in XCM2_WORD; signal IRArg0 : in XCM2_WORD;
                       signal deviceSel, deviceData : inout XCM2_WORD;
                       signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               deviceSel <= IRArg0;
               nextState:= CPU_DEVICE_WRITE;
          elsif (state = CPU_DEVICE_WRITE) then
               deviceData <= rA;
               nextState:= CPU_FETCH_IR;
          end if;               
     end procedure ir_IOUT;
     
     procedure ProcessInterrupt(signal interruptAddr : in XCM2_WORD; signal interruptsEnabled, interruptInProgress : inout std_logic;
                                signal PC : in XCM2_DWORD; variable nextPC : inout XCM2_DWORD;
                                signal SP : in XCM2_DWORD; variable nextSP : inout XCM2_DWORD;
                                signal memAddrBuffer : inout XCM2_DWORD; signal memWriteBuffer : inout XCM2_WORD;
                                signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_FETCH_IR) then
               interruptInProgress <= '1';
               nextState:= CPU_STORE_DWORD_L;
               memAddrBuffer <= SP - X"01";
               memWriteBuffer <= XCM2_LOW_WORD(PC);
          elsif (state = CPU_STORE_DWORD_L) then
               nextState:= CPU_STORE_DWORD_H;
               memAddrBuffer <= SP - X"02";
               memWriteBuffer <= XCM2_HIGH_WORD(PC);
          elsif (state = CPU_STORE_DWORD_H) then
               nextState:= CPU_FETCH_IR;
               nextPC:= XCM2_DWORD_HL(X"00", interruptAddr);
               nextSP:= SP - X"02";
               interruptInProgress <= '0';
               interruptsEnabled <= '0';
          end if;          
     end procedure ProcessInterrupt;
     
     procedure ir_Trigger(signal trigger : inout std_logic;
                          signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               trigger <= '1';
               nextState:= CPU_EXECUTE_IR_STAGE_2;
          elsif (state = CPU_EXECUTE_IR_STAGE_2) then
               trigger <= '0';
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_Trigger;
     
     procedure ir_Dummy(signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_Dummy;
     
     procedure ir_VMODE(signal gpuModeSwitch : inout std_logic_vector(0 to 1); signal mode : in XCM2_WORD;
                        signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               gpuModeSwitch(0) <= '1';
               gpuModeSwitch(1) <= mode(7);
               nextState:= CPU_EXECUTE_IR_STAGE_2;
          elsif (state = CPU_EXECUTE_IR_STAGE_2) then
               gpuModeSwitch(0) <= '0';
               nextState:= CPU_FETCH_IR;
          end if;          
     end procedure ir_VMODE;
     
     procedure ir_VFSA(signal rA, rH, rL : in XCM2_WORD;
                       signal gpuBackStore, gpuBackLoad : inout std_logic; signal gpuBackAddr : inout XCM2_DWORD; signal gpuBackData : inout XCM2_WORD;
                       signal state : in CPUState; variable nextState : inout CPUState) is
     begin
          if (state = CPU_EXECUTE_IR) then
               gpuBackStore <= '1';
               gpuBackLoad <= '0';
               gpuBackAddr <= XCM2_DWORD_HL(rH, rL);
               gpuBackData <= rA;
               nextState:= CPU_EXECUTE_IR_STAGE_2;
          elsif (state = CPU_EXECUTE_IR_STAGE_2) then
               gpuBackStore <= '0';
               gpuBackLoad <= '0';
               nextState:= CPU_FETCH_IR;
          end if;
     end procedure ir_VFSA;
                        
begin

     process (CLK_MAIN) is
          variable buf_nextPC : XCM2_DWORD;
          variable buf_nextSP : XCM2_DWORD;
          variable buf_nextState : CPUState;
          variable buf_currentIRCode : XCM2_WORD;
     begin
              
          if (state = CPU_HALTED) then
               HALTED <= '1';
          else
               HALTED <= '0';
          end if;
               
          INTE <= interruptsEnabled;
          RAM_READ_ADDR <= memAddrBuffer;
          RAM_WRITE_ADDR <= memAddrBuffer; -- concurrent read and write RAM ability is not utilized by the CPU
          RAM_WRITE_DATA <= memWriteBuffer;
          RAM_WRITE_ENABLED <= memWriteEnabled;
          DEVICE_SEL <= deviceSel;
          DEVICE_DATA <= deviceData;
          GPU_BACK_CLR <= gpuBackClr;
          GPU_BACK_STORE <= gpuBackStore;
          GPU_BACK_LOAD <= gpuBackLoad;
          GPU_BACK_ADDR <= gpuBackAddr;
          GPU_MODE_SWITCH <= gpuModeSwitch;
          GPU_PRESENT_TRIGGER <= gpuPresentTrigger;
          GPU_BACK_DATA <= gpuBackData;
          
          DEBUG_STATE <= rB;--to_unsigned(CPUState'POS(state), 8);
          DEBUG_PC_L <= rC;--XCM2_LOW_WORD(PC);
          DEBUG_CURRENT_IR <= currentIRCode;
          
          if (state = CPU_DEVICE_READ) then
               DEVICE_READ <= '1';
          elsif (state = CPU_DEVICE_WRITE) then
               DEVICE_READ <= '0';
          else
               DEVICE_SEL <= X"00";
          end if;
               
          if rising_edge(CLK_MAIN) then
               if RESET = '0' then
                    --if nextState /= CPU_FETCH_IR then
                    --     state <= nextState;
                    --else
                    --    if DEBUG_STEP = '1' and debugStepPressed = '0' then
                    --          debugStepPressed <= '1';
                    --     elsif DEBUG_STEP = '0' and debugStepPressed = '1' then
                    --          state <= nextState;
                    --          debugStepPressed <= '0';
                    --     end if;
                    --end if;
                    
                    state <= nextState;                    
					
                    PC <= nextPC;
                    SP <= nextSP;
                    if (INT = '1') then
                         interruptRequested <= '1';
                         interruptAddr <= INT_IRQ;
                    elsif (interruptInProgress = '0') then
                        interruptRequested <= '0';
                    end if;
               else
                    state <= CPU_INVALID;
                    PC <= X"0000";
                    SP <= X"FFFF";
                    interruptRequested <= '0';
               end if;
          end if;
          
          if falling_edge(CLK_MAIN) then
               
               if RESET = '1' then
                    buf_nextState:= CPU_INVALID;
                    nextPC <= X"0000";
                    nextSP <= X"FFFF";
               end if;
               
               if (interruptRequested = '1') then
                    if (state = CPU_FETCH_IR or interruptInProgress = '1') then
                         ProcessInterrupt(interruptAddr, interruptsEnabled, interruptInProgress,
                                          PC, buf_nextPC, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    end if;
               end if;
          
               if (interruptRequested /= '1' and state = CPU_INVALID) then
                    buf_nextState:= CPU_FETCH_IR;
               end if;
          
               if (interruptRequested /= '1' and (state = CPU_FETCH_IR or state = CPU_FETCH_ARG0 or state = CPU_FETCH_ARG1)) then
                    if (state = CPU_FETCH_IR) then
                         buf_currentIRCode := RAM_READ_DATA;
                         currentIRCode <= RAM_READ_DATA;                 
                    elsif (state = CPU_FETCH_ARG0) then                         
                         IRArg0 <= RAM_READ_DATA;
                    elsif (state = CPU_FETCH_ARG1) then
                         IRArg1 <= RAM_READ_DATA;
                    end if;
                                  
                    buf_nextPC := PC + 1;   
                                       
                    if (buf_currentIRCode >= LXI_BC and buf_currentIRCode <= SHLD) or (buf_currentIRCode >= JMP and buf_currentIRCode <= CM) or (buf_currentIRCode = DAI)
                    then
                         -- 3byte instructions
                         if (state = CPU_FETCH_IR) then
                              buf_nextState := CPU_FETCH_ARG0;
                         elsif (state = CPU_FETCH_ARG0) then
                              buf_nextState := CPU_FETCH_ARG1;
                         else
                              buf_nextState := CPU_EXECUTE_IR;
                         end if;
                    elsif (buf_currentIRCode >= MVI_A and buf_currentIRCode <= MVI_M) or
                       buf_currentIRCode = ADI or
                       buf_currentIRCode = ACI or
                       buf_currentIRCode = SUI or
                       buf_currentIRCode = SBI or
                       buf_currentIRCode = ANI or
                       buf_currentIRCode = ORI or
                       buf_currentIRCode = XRI or
                       buf_currentIRCode = CPI or
                       buf_currentIRCode = IIN or
                       buf_currentIRCode = IOUT
                       --or buf_currentIRCode = VMODE
                    then
                         -- 2byte instructions
                         if (state = CPU_FETCH_IR) then      
                              buf_nextState := CPU_FETCH_ARG0;
                         else
                              buf_nextState := CPU_EXECUTE_IR;
                         end if;                       
                    else
                         -- 1byte instructions
                         buf_nextState := CPU_EXECUTE_IR;                          
                    end if;

               end if;
               
               if (state = CPU_LOAD_DWORD_H) then
                    memReadBufferH <= RAM_READ_DATA;
               elsif (state = CPU_LOAD_DWORD_L) then
                    memReadBufferL <= RAM_READ_DATA;
               end if;
               
               if (interruptRequested /= '1' and
                   state /= CPU_INVALID and
                   state /= CPU_HALTED and
                   state /= CPU_FETCH_IR and
                   state /= CPU_FETCH_ARG0 and
                   state /= CPU_FETCH_ARG1) then
               -- demux
               case currentIRCode is
                    when MOV_AB => ir_MOV(rA, rB, state, buf_nextState);
                    when MOV_BA => ir_MOV(rB, rA, state, buf_nextState);
                    when MOV_CA => ir_MOV(rC, rA, state, buf_nextState);
                    when MOV_DA => ir_MOV(rD, rA, state, buf_nextState);
                    when MOV_EA => ir_MOV(rE, rA, state, buf_nextState);
                    when MOV_HA => ir_MOV(rH, rA, state, buf_nextState);
                    when MOV_LA => ir_MOV(rL, rA, state, buf_nextState);
                    when MOV_AC => ir_MOV(rA, rC, state, buf_nextState);
                    when MOV_BC => ir_MOV(rB, rC, state, buf_nextState);
                    when MOV_CB => ir_MOV(rC, rB, state, buf_nextState);
                    when MOV_DB => ir_MOV(rD, rB, state, buf_nextState);
                    when MOV_EB => ir_MOV(rE, rB, state, buf_nextState);
                    when MOV_HB => ir_MOV(rH, rB, state, buf_nextState);
                    when MOV_LB => ir_MOV(rL, rB, state, buf_nextState);
                    when MOV_AD => ir_MOV(rA, rD, state, buf_nextState);
                    when MOV_BD => ir_MOV(rB, rD, state, buf_nextState);
                    when MOV_CD => ir_MOV(rC, rD, state, buf_nextState);
                    when MOV_DC => ir_MOV(rD, rC, state, buf_nextState);
                    when MOV_EC => ir_MOV(rE, rC, state, buf_nextState);
                    when MOV_HC => ir_MOV(rH, rC, state, buf_nextState);
                    when MOV_LC => ir_MOV(rL, rC, state, buf_nextState);
                    when MOV_AE => ir_MOV(rA, rE, state, buf_nextState);
                    when MOV_BE => ir_MOV(rB, rE, state, buf_nextState);
                    when MOV_CE => ir_MOV(rC, rE, state, buf_nextState);
                    when MOV_DE => ir_MOV(rD, rE, state, buf_nextState);
                    when MOV_ED => ir_MOV(rE, rD, state, buf_nextState);
                    when MOV_HD => ir_MOV(rH, rD, state, buf_nextState);
                    when MOV_LD => ir_MOV(rL, rD, state, buf_nextState);
                    when MOV_AH => ir_MOV(rA, rH, state, buf_nextState);
                    when MOV_BH => ir_MOV(rB, rH, state, buf_nextState);
                    when MOV_CH => ir_MOV(rC, rH, state, buf_nextState);
                    when MOV_DH => ir_MOV(rD, rH, state, buf_nextState);
                    when MOV_EH => ir_MOV(rE, rH, state, buf_nextState);
                    when MOV_HE => ir_MOV(rH, rE, state, buf_nextState);
                    when MOV_LE => ir_MOV(rL, rE, state, buf_nextState);
                    when MOV_AL => ir_MOV(rA, rL, state, buf_nextState);
                    when MOV_BL => ir_MOV(rB, rL, state, buf_nextState);
                    when MOV_CL => ir_MOV(rC, rL, state, buf_nextState);
                    when MOV_DL => ir_MOV(rD, rL, state, buf_nextState);
                    when MOV_EL => ir_MOV(rE, rL, state, buf_nextState);
                    when MOV_HL => ir_MOV(rH, rL, state, buf_nextState);
                    when MOV_LH => ir_MOV(rL, rH, state, buf_nextState);
                    
                    when MOV_AM => ir_MOV_RM(rA, rH, rL, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when MOV_BM => ir_MOV_RM(rB, rH, rL, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when MOV_CM => ir_MOV_RM(rC, rH, rL, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when MOV_DM => ir_MOV_RM(rD, rH, rL, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when MOV_EM => ir_MOV_RM(rE, rH, rL, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when MOV_HM => ir_MOV_RM(rH, rH, rL, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when MOV_LM => ir_MOV_RM(rL, rH, rL, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    
                    when MOV_MA => ir_MOV_MR(rA, rH, rL, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when MOV_MB => ir_MOV_MR(rB, rH, rL, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when MOV_MC => ir_MOV_MR(rC, rH, rL, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when MOV_MD => ir_MOV_MR(rD, rH, rL, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when MOV_ME => ir_MOV_MR(rE, rH, rL, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when MOV_MH => ir_MOV_MR(rH, rH, rL, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when MOV_ML => ir_MOV_MR(rL, rH, rL, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    
                    when MVI_A => ir_MVI(rA, IRArg0, state, buf_nextState);
                    when MVI_B => ir_MVI(rB, IRArg0, state, buf_nextState);
                    when MVI_C => ir_MVI(rC, IRArg0, state, buf_nextState);
                    when MVI_D => ir_MVI(rD, IRArg0, state, buf_nextState);
                    when MVI_E => ir_MVI(rE, IRArg0, state, buf_nextState);
                    when MVI_H => ir_MVI(rH, IRArg0, state, buf_nextState);
                    when MVI_L => ir_MVI(rL, IRArg0, state, buf_nextState);
                    when MVI_M => ir_MVI_M(IRArg0, rH, rL, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    
                    when LXI_BC => ir_LXI(rB, rC, IRArg0, IRArg1, state, buf_nextState);
                    when LXI_DE => ir_LXI(rD, rE, IRArg0, IRArg1, state, buf_nextState);
                    when LXI_HL => ir_LXI(rH, rL, IRArg0, IRArg1, state, buf_nextState);
                    when LXI_SP => ir_LXI_D(nextSP, IRArg0, IRArg1, state, buf_nextState);
                    
                    when LDA => ir_LDA(rA, IRArg0, IRArg1, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when STA => ir_STA(rA, IRArg0, IRArg1, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    
                    when LHLD => ir_LHLD(rH, rL, IRArg0, IRArg1, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when SHLD => ir_SHLD(rH, rL, IRArg0, IRArg1, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    
                    when LDAX_BC => ir_LDA(rA, rB, rC, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when LDAX_DE => ir_LDA(rA, rD, rE, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when LDAX_HL => ir_LDA(rA, rH, rL, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    
                    when STAX_BC => ir_STA(rA, rB, rC, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when STAX_DE => ir_STA(rA, rD, rE, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when STAX_HL => ir_STA(rA, rH, rL, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    
                    when XCNG => ir_XCNG(rD, rE, rH, rL, state, buf_nextState);
                    
                    when ADD_A => ir_ADD(rA, rA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADD_B => ir_ADD(rA, rB, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADD_C => ir_ADD(rA, rC, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADD_D => ir_ADD(rA, rD, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADD_E => ir_ADD(rA, rE, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADD_H => ir_ADD(rA, rH, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADD_L => ir_ADD(rA, rL, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADD_M => ir_ADD_M(rA, rH, rL, memAddrBuffer, RAM_READ_DATA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADI => ir_ADD(rA, IRArg0, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    
                    when ADC_A => ir_ADC(rA, rA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADC_B => ir_ADC(rA, rB, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADC_C => ir_ADC(rA, rC, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADC_D => ir_ADC(rA, rD, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADC_E => ir_ADC(rA, rE, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADC_H => ir_ADC(rA, rH, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADC_L => ir_ADC(rA, rL, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ADC_M => ir_ADC_M(rA, rH, rL, memAddrBuffer, RAM_READ_DATA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ACI => ir_ADC(rA, IRArg0, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    
                    when SUB_A => ir_SUB(rA, rA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SUB_B => ir_SUB(rA, rB, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SUB_C => ir_SUB(rA, rC, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SUB_D => ir_SUB(rA, rD, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SUB_E => ir_SUB(rA, rE, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SUB_H => ir_SUB(rA, rH, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SUB_L => ir_SUB(rA, rL, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SUB_M => ir_SUB_M(rA, rH, rL, memAddrBuffer, RAM_READ_DATA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SUI => ir_SUB(rA, IRArg0, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    
                    when SBB_A => ir_SBB(rA, rA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SBB_B => ir_SBB(rA, rB, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SBB_C => ir_SBB(rA, rC, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SBB_D => ir_SBB(rA, rD, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SBB_E => ir_SBB(rA, rE, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SBB_H => ir_SBB(rA, rH, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SBB_L => ir_SBB(rA, rL, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SBB_M => ir_SBB_M(rA, rH, rL, memAddrBuffer, RAM_READ_DATA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when SBI => ir_SBB(rA, IRArg0, fSign, fZero, fParity, fCarry, state, buf_nextState);                    
                    
                    when INR_A => ir_INR(rA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when INR_B => ir_INR(rB, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when INR_C => ir_INR(rC, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when INR_D => ir_INR(rD, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when INR_E => ir_INR(rE, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when INR_H => ir_INR(rH, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when INR_L => ir_INR(rL, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when INR_M => ir_INR_M(rH, rL, memAddrBuffer, RAM_READ_DATA, memWriteBuffer, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    
                    when DCR_A => ir_DCR(rA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when DCR_B => ir_DCR(rB, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when DCR_C => ir_DCR(rC, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when DCR_D => ir_DCR(rD, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when DCR_E => ir_DCR(rE, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when DCR_H => ir_DCR(rH, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when DCR_L => ir_DCR(rL, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when DCR_M => ir_DCR_M(rH, rL, memAddrBuffer, RAM_READ_DATA, memWriteBuffer, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    
                    when INX_BC => ir_INX(rB, rC, state, buf_nextState);
                    when INX_DE => ir_INX(rD, rH, state, buf_nextState);
                    when INX_HL => ir_INX(rE, rL, state, buf_nextState);
                    when INX_SP => ir_INX_D(SP, buf_nextSP, state, buf_nextState);
                    
                    when DCX_BC => ir_DCX(rB, rC, state, buf_nextState);
                    when DCX_DE => ir_DCX(rD, rH, state, buf_nextState);
                    when DCX_HL => ir_DCX(rE, rL, state, buf_nextState);
                    when DCX_SP => ir_DCX_D(SP, buf_nextSP, state, buf_nextState);
                    
                    when DAD_BC => ir_DAD(rH, rL, rB, rC, state, buf_nextState);
                    when DAD_DE => ir_DAD(rH, rL, rD, rE, state, buf_nextState);
                    when DAD_HL => ir_DAD(rH, rL, rH, rL, state, buf_nextState);
                    when DAD_SP => ir_DAD_D(rH, rL, SP, state, buf_nextState);
                    when DAI    => ir_DAD(rH, rL, IRArg0, IRArg1, state, buf_nextState);
                    
                    when ANA_A => ir_ANA(rA, rA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ANA_B => ir_ANA(rA, rB, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ANA_C => ir_ANA(rA, rC, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ANA_D => ir_ANA(rA, rD, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ANA_E => ir_ANA(rA, rE, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ANA_H => ir_ANA(rA, rH, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ANA_L => ir_ANA(rA, rL, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ANA_M => ir_ANA_M(rA, rH, rL, memAddrBuffer, RAM_READ_DATA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ANI => ir_ANA(rA, IRArg0, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    
                    when ORA_A => ir_ORA(rA, rA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ORA_B => ir_ORA(rA, rB, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ORA_C => ir_ORA(rA, rC, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ORA_D => ir_ORA(rA, rD, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ORA_E => ir_ORA(rA, rE, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ORA_H => ir_ORA(rA, rH, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ORA_L => ir_ORA(rA, rL, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ORA_M => ir_ORA_M(rA, rH, rL, memAddrBuffer, RAM_READ_DATA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when ORI => ir_ORA(rA, IRArg0, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    
                    when XRA_A => ir_XRA(rA, rA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when XRA_B => ir_XRA(rA, rB, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when XRA_C => ir_XRA(rA, rC, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when XRA_D => ir_XRA(rA, rD, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when XRA_E => ir_XRA(rA, rE, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when XRA_H => ir_XRA(rA, rH, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when XRA_L => ir_XRA(rA, rL, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when XRA_M => ir_XRA_M(rA, rH, rL, memAddrBuffer, RAM_READ_DATA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when XRI => ir_XRA(rA, IRArg0, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    
                    when CMP_A => ir_CMP(rA, rA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when CMP_B => ir_CMP(rA, rB, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when CMP_C => ir_CMP(rA, rC, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when CMP_D => ir_CMP(rA, rD, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when CMP_E => ir_CMP(rA, rE, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when CMP_H => ir_CMP(rA, rH, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when CMP_L => ir_CMP(rA, rL, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when CMP_M => ir_CMP_M(rA, rH, rL, memAddrBuffer, RAM_READ_DATA, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    when CPI => ir_CMP(rA, IRArg0, fSign, fZero, fParity, fCarry, state, buf_nextState);
                    
                    when RLC => ir_RLC(rA, fCarry, state, buf_nextState);
                    when RRC => ir_RRC(rA, fCarry, state, buf_nextState);
                    when RAL => ir_RAL(rA, fCarry, IRTemp, state, buf_nextState);
                    when RAR => ir_RAR(rA, fCarry, IRTemp, state, buf_nextState);
                    
                    when CMA => ir_CMA(rA, state, buf_nextState);
                    when CMC => ir_SetCarry(not fCarry, fCarry, state, buf_nextState);
                    when STC => ir_SetCarry('1', fCarry, state, buf_nextState);
                    when RTC => ir_SetCarry('0', fCarry, state, buf_nextState);
                    
                    when JMP => ir_JMP('1',         IRArg0, IRArg1, buf_nextPC, state, buf_nextState);
                    when JNZ => ir_JMP(not fZero,   IRArg0, IRArg1, buf_nextPC, state, buf_nextState);
                    when JZ  => ir_JMP(fZero,       IRArg0, IRArg1, buf_nextPC, state, buf_nextState);
                    when JNC => ir_JMP(not fCarry,  IRArg0, IRArg1, buf_nextPC, state, buf_nextState);
                    when JC  => ir_JMP(fCarry,      IRArg0, IRArg1, buf_nextPC, state, buf_nextState);
                    when JPO => ir_JMP(not fParity, IRArg0, IRArg1, buf_nextPC, state, buf_nextState);
                    when JPE => ir_JMP(fParity,     IRArg0, IRArg1, buf_nextPC, state, buf_nextState);
                    when JP  => ir_JMP(not fSign,   IRArg0, IRArg1, buf_nextPC, state, buf_nextState);
                    when JM  => ir_JMP(fSign,       IRArg0, IRArg1, buf_nextPC, state, buf_nextState);
                    
                    when CALL => ir_CALL('1',         IRArg0, IRArg1, PC, buf_nextPC, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when CNZ  => ir_CALL(not fZero,   IRArg0, IRArg1, PC, buf_nextPC, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when CZ   => ir_CALL(fZero,       IRArg0, IRArg1, PC, buf_nextPC, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when CNC  => ir_CALL(not fCarry,  IRArg0, IRArg1, PC, buf_nextPC, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when CC   => ir_CALL(fCarry,      IRArg0, IRArg1, PC, buf_nextPC, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when CPO  => ir_CALL(not fParity, IRArg0, IRArg1, PC, buf_nextPC, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when CPE  => ir_CALL(fParity,     IRArg0, IRArg1, PC, buf_nextPC, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when CP   => ir_CALL(not fSign,   IRArg0, IRArg1, PC, buf_nextPC, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when CM   => ir_CALL(fSign,       IRArg0, IRArg1, PC, buf_nextPC, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    
                    when RET  => ir_RET('1',         buf_nextPC, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, memReadBufferH, state, buf_nextState);
                    when RNZ  => ir_RET(not fZero,   buf_nextPC, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, memReadBufferH, state, buf_nextState);
                    when RZ   => ir_RET(fZero,       buf_nextPC, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, memReadBufferH, state, buf_nextState);
                    when RNC  => ir_RET(not fCarry,  buf_nextPC, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, memReadBufferH, state, buf_nextState);
                    when IRRC => ir_RET(fCarry,      buf_nextPC, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, memReadBufferH, state, buf_nextState);
                    when RPO  => ir_RET(not fParity, buf_nextPC, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, memReadBufferH, state, buf_nextState);
                    when RPE  => ir_RET(fParity,     buf_nextPC, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, memReadBufferH, state, buf_nextState);
                    when RP   => ir_RET(not fSign,   buf_nextPC, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, memReadBufferH, state, buf_nextState);
                    when RM   => ir_RET(fSign,       buf_nextPC, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, memReadBufferH, state, buf_nextState);                    
                    
                    when PCHL => ir_PCHL(rH, rL, buf_nextPC, state, buf_nextState);
                    
                    when PUSH_AF => ir_PUSH(rA, rF, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when PUSH_BC => ir_PUSH(rB, rC, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when PUSH_DE => ir_PUSH(rD, rE, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    when PUSH_HL => ir_PUSH(rH, rL, SP, buf_nextSP, memAddrBuffer, memWriteBuffer, state, buf_nextState);
                    
                    when POP_AF => ir_POP(rA, rF, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when POP_BC => ir_POP(rB, rC, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when POP_DE => ir_POP(rD, rE, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    when POP_HL => ir_POP(rH, rL, SP, buf_nextSP, memAddrBuffer, RAM_READ_DATA, state, buf_nextState);
                    
                    when XTHL => ir_XTHL(rH, rL, SP, memAddrBuffer, memReadBufferH, RAM_READ_DATA, memWriteBuffer, state, buf_nextState);
                    
                    when SPHL => ir_SPHL(rH, rL, buf_nextSP, state, buf_nextState);
                    when HLSP => ir_HLSP(rH, rL, SP, state, buf_nextState);
                    
                    when HLT => ir_HLT(state, buf_nextState);
                    when EI => ir_EnableInterrupt('1', interruptsEnabled, state, buf_nextState);
                    when DI => ir_EnableInterrupt('0', interruptsEnabled, state, buf_nextState);
                    
                    when IIN => ir_IIN(rA, IRArg0, deviceSel, deviceData, state, buf_nextState);
                    when IOUT => ir_IOUT(rA, IRArg0, deviceSel, deviceData, state, buf_nextState);
                    
                    --when VFCLR => ir_Trigger(gpuBackClr, state, buf_nextState);
                    when VPRE => ir_Trigger(gpuPresentTrigger, state, buf_nextState);
                    when VMODE => ir_VMODE(gpuModeSwitch, rA, state, buf_nextState);
                    when VFSA => ir_VFSA(rA, rH, rL, gpuBackStore, gpuBackLoad, gpuBackAddr, gpuBackData, state, buf_nextState);
                    
                    when others => ir_Dummy(state, buf_nextState);
               end case;
               end if;
                              
               -- pcRead
               if (buf_nextState = CPU_FETCH_IR or buf_nextState = CPU_FETCH_ARG0 or buf_nextState = CPU_FETCH_ARG1) then
                    --memReadBuffer <= RAM_READ_DATA;
                    memWriteEnabled <= '0';
                    memAddrBuffer <= buf_nextPC;   
                    
               -- memory->load    
               elsif (buf_nextState = CPU_LOAD_WORD or buf_nextState = CPU_LOAD_DWORD_H or buf_nextState = CPU_LOAD_DWORD_L) then

                    memWriteEnabled <= '0';
                    --RAM_READ_ADDR <= memAddrBuffer;--to_unsigned(std_logic_vector(rH) & std_logic_vector(rL));
                    
               -- memory->store
               elsif (buf_nextState = CPU_STORE_WORD or buf_nextState = CPU_STORE_DWORD_H or buf_nextState = CPU_STORE_DWORD_L) then
                    --RAM_WRITE_DATA <= memWriteBuffer;
                    memWriteEnabled <= '1';
                    --RAM_WRITE_ADDR <= memAddrBuffer;--to_unsigned(std_logic_vector(rH) & std_logic_vector(rL));
               end if;
               
               nextState <= buf_nextState;
               nextPC <= buf_nextPC;
               nextSP <= buf_nextSP;
          end if;
     end process;

end main;     
