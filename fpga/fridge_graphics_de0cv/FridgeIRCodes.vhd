use work.FridgeGlobals.all;

package FridgePAM16Commands is

constant PAM16_NOP   : PAM16_COMMAND:= X"00";
constant PAM16_RESET : PAM16_COMMAND:= X"01";
constant PAM16_PUSH  : PAM16_COMMAND:= X"02";
constant PAM16_POP   : PAM16_COMMAND:= X"03";
constant PAM16_ADD   : PAM16_COMMAND:= X"04";
constant PAM16_SUB   : PAM16_COMMAND:= X"05";
constant PAM16_MUL   : PAM16_COMMAND:= X"06";
constant PAM16_DIV   : PAM16_COMMAND:= X"07";
constant PAM16_FMADD : PAM16_COMMAND:= X"08";

end FridgePAM16Commands;

use work.FridgeGlobals.all;

package FridgeIRCodes is

constant NOP     : XCM2_WORD:= X"00";
constant MOV_AB  : XCM2_WORD:= X"01";
constant MOV_AC  : XCM2_WORD:= X"02";
constant MOV_AD  : XCM2_WORD:= X"03";
constant MOV_AE  : XCM2_WORD:= X"04";
constant MOV_AH  : XCM2_WORD:= X"05";
constant MOV_AL  : XCM2_WORD:= X"06";
constant MOV_AM  : XCM2_WORD:= X"07";
constant MOV_BA  : XCM2_WORD:= X"08";
constant MOV_BC  : XCM2_WORD:= X"09";
constant MOV_BD  : XCM2_WORD:= X"0A";
constant MOV_BE  : XCM2_WORD:= X"0B";
constant MOV_BH  : XCM2_WORD:= X"0C";
constant MOV_BL  : XCM2_WORD:= X"0D";
constant MOV_BM  : XCM2_WORD:= X"0E";
constant MOV_CA  : XCM2_WORD:= X"0F";
constant MOV_CB  : XCM2_WORD:= X"10";
constant MOV_CD  : XCM2_WORD:= X"11";
constant MOV_CE  : XCM2_WORD:= X"12";
constant MOV_CH  : XCM2_WORD:= X"13";
constant MOV_CL  : XCM2_WORD:= X"14";
constant MOV_CM  : XCM2_WORD:= X"15";
constant MOV_DA  : XCM2_WORD:= X"16";
constant MOV_DB  : XCM2_WORD:= X"17";
constant MOV_DC  : XCM2_WORD:= X"18";
constant MOV_DE  : XCM2_WORD:= X"19";
constant MOV_DH  : XCM2_WORD:= X"1A";
constant MOV_DL  : XCM2_WORD:= X"1B";
constant MOV_DM  : XCM2_WORD:= X"1C";
constant MOV_EA  : XCM2_WORD:= X"1D";
constant MOV_EB  : XCM2_WORD:= X"1E";
constant MOV_EC  : XCM2_WORD:= X"1F";
constant MOV_ED  : XCM2_WORD:= X"20";
constant MOV_EH  : XCM2_WORD:= X"21";
constant MOV_EL  : XCM2_WORD:= X"22";
constant MOV_EM  : XCM2_WORD:= X"23";
constant MOV_HA  : XCM2_WORD:= X"24";
constant MOV_HB  : XCM2_WORD:= X"25";
constant MOV_HC  : XCM2_WORD:= X"26";
constant MOV_HD  : XCM2_WORD:= X"27";
constant MOV_HE  : XCM2_WORD:= X"28";
constant MOV_HL  : XCM2_WORD:= X"29";
constant MOV_HM  : XCM2_WORD:= X"2A";
constant MOV_LA  : XCM2_WORD:= X"2B";
constant MOV_LB  : XCM2_WORD:= X"2C";
constant MOV_LC  : XCM2_WORD:= X"2D";
constant MOV_LD  : XCM2_WORD:= X"2E";
constant MOV_LE  : XCM2_WORD:= X"2F";
constant MOV_LH  : XCM2_WORD:= X"30";
constant MOV_LM  : XCM2_WORD:= X"31";
constant MOV_MA  : XCM2_WORD:= X"32";
constant MOV_MB  : XCM2_WORD:= X"33";
constant MOV_MC  : XCM2_WORD:= X"34";
constant MOV_MD  : XCM2_WORD:= X"35";
constant MOV_ME  : XCM2_WORD:= X"36";
constant MOV_MH  : XCM2_WORD:= X"37";
constant MOV_ML  : XCM2_WORD:= X"38";
constant MVI_A   : XCM2_WORD:= X"39";
constant MVI_B   : XCM2_WORD:= X"3A";
constant MVI_C   : XCM2_WORD:= X"3B";
constant MVI_D   : XCM2_WORD:= X"3C";
constant MVI_E   : XCM2_WORD:= X"3D";
constant MVI_H   : XCM2_WORD:= X"3E";
constant MVI_L   : XCM2_WORD:= X"3F";
constant MVI_M   : XCM2_WORD:= X"40";
constant LXI_BC  : XCM2_WORD:= X"41";
constant LXI_DE  : XCM2_WORD:= X"42";
constant LXI_HL  : XCM2_WORD:= X"43";
constant LXI_SP  : XCM2_WORD:= X"44";
constant LDA     : XCM2_WORD:= X"45";
constant STA     : XCM2_WORD:= X"46";
constant LHLD    : XCM2_WORD:= X"47";
constant SHLD    : XCM2_WORD:= X"48";
constant LDAX_BC : XCM2_WORD:= X"49";
constant LDAX_DE : XCM2_WORD:= X"4A";
constant LDAX_HL : XCM2_WORD:= X"4B";
constant STAX_BC : XCM2_WORD:= X"4C";
constant STAX_DE : XCM2_WORD:= X"4D";
constant STAX_HL : XCM2_WORD:= X"4E";
constant XCNG    : XCM2_WORD:= X"4F";
constant ADD_A   : XCM2_WORD:= X"50";
constant ADD_B   : XCM2_WORD:= X"51";
constant ADD_C   : XCM2_WORD:= X"52";
constant ADD_D   : XCM2_WORD:= X"53";
constant ADD_E   : XCM2_WORD:= X"54";
constant ADD_H   : XCM2_WORD:= X"55";
constant ADD_L   : XCM2_WORD:= X"56";
constant ADD_M   : XCM2_WORD:= X"57";
constant ADI     : XCM2_WORD:= X"58";
constant ADC_A   : XCM2_WORD:= X"59";
constant ADC_B   : XCM2_WORD:= X"5A";
constant ADC_C   : XCM2_WORD:= X"5B";
constant ADC_D   : XCM2_WORD:= X"5C";
constant ADC_E   : XCM2_WORD:= X"5D";
constant ADC_H   : XCM2_WORD:= X"5E";
constant ADC_L   : XCM2_WORD:= X"5F";
constant ADC_M   : XCM2_WORD:= X"60";
constant ACI     : XCM2_WORD:= X"61";
constant SUB_A   : XCM2_WORD:= X"62";
constant SUB_B   : XCM2_WORD:= X"63";
constant SUB_C   : XCM2_WORD:= X"64";
constant SUB_D   : XCM2_WORD:= X"65";
constant SUB_E   : XCM2_WORD:= X"66";
constant SUB_H   : XCM2_WORD:= X"67";
constant SUB_L   : XCM2_WORD:= X"68";
constant SUB_M   : XCM2_WORD:= X"69";
constant SUI     : XCM2_WORD:= X"6A";
constant SBB_A   : XCM2_WORD:= X"6B";
constant SBB_B   : XCM2_WORD:= X"6C";
constant SBB_C   : XCM2_WORD:= X"6D";
constant SBB_D   : XCM2_WORD:= X"6E";
constant SBB_E   : XCM2_WORD:= X"6F";
constant SBB_H   : XCM2_WORD:= X"70";
constant SBB_L   : XCM2_WORD:= X"71";
constant SBB_M   : XCM2_WORD:= X"72";
constant SBI     : XCM2_WORD:= X"73";
constant INR_A   : XCM2_WORD:= X"74";
constant INR_B   : XCM2_WORD:= X"75";
constant INR_C   : XCM2_WORD:= X"76";
constant INR_D   : XCM2_WORD:= X"77";
constant INR_E   : XCM2_WORD:= X"78";
constant INR_H   : XCM2_WORD:= X"79";
constant INR_L   : XCM2_WORD:= X"7A";
constant INR_M   : XCM2_WORD:= X"7B";
constant DCR_A   : XCM2_WORD:= X"7C";
constant DCR_B   : XCM2_WORD:= X"7D";
constant DCR_C   : XCM2_WORD:= X"7E";
constant DCR_D   : XCM2_WORD:= X"7F";
constant DCR_E   : XCM2_WORD:= X"80";
constant DCR_H   : XCM2_WORD:= X"81";
constant DCR_L   : XCM2_WORD:= X"82";
constant DCR_M   : XCM2_WORD:= X"83";
constant INX_BC  : XCM2_WORD:= X"84";
constant INX_DE  : XCM2_WORD:= X"85";
constant INX_HL  : XCM2_WORD:= X"86";
constant INX_SP  : XCM2_WORD:= X"87";
constant DCX_BC  : XCM2_WORD:= X"88";
constant DCX_DE  : XCM2_WORD:= X"89";
constant DCX_HL  : XCM2_WORD:= X"8A";
constant DCX_SP  : XCM2_WORD:= X"8B";
constant DAD_BC  : XCM2_WORD:= X"8C";
constant DAD_DE  : XCM2_WORD:= X"8D";
constant DAD_HL  : XCM2_WORD:= X"8E";
constant DAD_SP  : XCM2_WORD:= X"8F";
constant DAI     : XCM2_WORD:= X"90";
constant ANA_A   : XCM2_WORD:= X"91";
constant ANA_B   : XCM2_WORD:= X"92";
constant ANA_C   : XCM2_WORD:= X"93";
constant ANA_D   : XCM2_WORD:= X"94";
constant ANA_E   : XCM2_WORD:= X"95";
constant ANA_H   : XCM2_WORD:= X"96";
constant ANA_L   : XCM2_WORD:= X"97";
constant ANA_M   : XCM2_WORD:= X"98";
constant ANI     : XCM2_WORD:= X"99";
constant ORA_A   : XCM2_WORD:= X"9A";
constant ORA_B   : XCM2_WORD:= X"9B";
constant ORA_C   : XCM2_WORD:= X"9C";
constant ORA_D   : XCM2_WORD:= X"9D";
constant ORA_E   : XCM2_WORD:= X"9E";
constant ORA_H   : XCM2_WORD:= X"9F";
constant ORA_L   : XCM2_WORD:= X"A0";
constant ORA_M   : XCM2_WORD:= X"A1";
constant ORI     : XCM2_WORD:= X"A2";
constant XRA_A   : XCM2_WORD:= X"A3";
constant XRA_B   : XCM2_WORD:= X"A4";
constant XRA_C   : XCM2_WORD:= X"A5";
constant XRA_D   : XCM2_WORD:= X"A6";
constant XRA_E   : XCM2_WORD:= X"A7";
constant XRA_H   : XCM2_WORD:= X"A8";
constant XRA_L   : XCM2_WORD:= X"A9";
constant XRA_M   : XCM2_WORD:= X"AA";
constant XRI     : XCM2_WORD:= X"AB";
constant CMP_A   : XCM2_WORD:= X"AC";
constant CMP_B   : XCM2_WORD:= X"AD";
constant CMP_C   : XCM2_WORD:= X"AE";
constant CMP_D   : XCM2_WORD:= X"AF";
constant CMP_E   : XCM2_WORD:= X"B0";
constant CMP_H   : XCM2_WORD:= X"B1";
constant CMP_L   : XCM2_WORD:= X"B2";
constant CMP_M   : XCM2_WORD:= X"B3";
constant CPI     : XCM2_WORD:= X"B4";
constant RLC     : XCM2_WORD:= X"B5";
constant RRC     : XCM2_WORD:= X"B6";
constant RAL     : XCM2_WORD:= X"B7";
constant RAR     : XCM2_WORD:= X"B8";
constant CMA     : XCM2_WORD:= X"B9";
constant CMC     : XCM2_WORD:= X"BA";
constant STC     : XCM2_WORD:= X"BB";
constant RTC     : XCM2_WORD:= X"BC";
constant JMP     : XCM2_WORD:= X"BD";
constant JNZ     : XCM2_WORD:= X"BE";
constant JZ      : XCM2_WORD:= X"BF";
constant JNC     : XCM2_WORD:= X"C0";
constant JC      : XCM2_WORD:= X"C1";
constant JPO     : XCM2_WORD:= X"C2";
constant JPE     : XCM2_WORD:= X"C3";
constant JP      : XCM2_WORD:= X"C4";
constant JM      : XCM2_WORD:= X"C5";
constant CALL    : XCM2_WORD:= X"C6";
constant CNZ     : XCM2_WORD:= X"C7";
constant CZ      : XCM2_WORD:= X"C8";
constant CNC     : XCM2_WORD:= X"C9";
constant CC      : XCM2_WORD:= X"CA";
constant CPO     : XCM2_WORD:= X"CB";
constant CPE     : XCM2_WORD:= X"CC";
constant CP      : XCM2_WORD:= X"CD";
constant CM      : XCM2_WORD:= X"CE";
constant RET     : XCM2_WORD:= X"CF";
constant RNZ     : XCM2_WORD:= X"D0";
constant RZ      : XCM2_WORD:= X"D1";
constant RNC     : XCM2_WORD:= X"D2";
constant IRRC    : XCM2_WORD:= X"D3";
constant RPO     : XCM2_WORD:= X"D4";
constant RPE     : XCM2_WORD:= X"D5";
constant RP      : XCM2_WORD:= X"D6";
constant RM      : XCM2_WORD:= X"D7";
constant PCHL    : XCM2_WORD:= X"D8";
constant PUSH_AF : XCM2_WORD:= X"D9";
constant PUSH_BC : XCM2_WORD:= X"DA";
constant PUSH_DE : XCM2_WORD:= X"DB";
constant PUSH_HL : XCM2_WORD:= X"DC";
constant POP_AF  : XCM2_WORD:= X"DD";
constant POP_BC  : XCM2_WORD:= X"DE";
constant POP_DE  : XCM2_WORD:= X"DF";
constant POP_HL  : XCM2_WORD:= X"E0";
constant XTHL    : XCM2_WORD:= X"E1";
constant SPHL    : XCM2_WORD:= X"E2";
constant HLSP    : XCM2_WORD:= X"E3";
constant IIN     : XCM2_WORD:= X"E4";
constant IOUT    : XCM2_WORD:= X"E5";
constant HLT     : XCM2_WORD:= X"E6";
constant EI      : XCM2_WORD:= X"E7";
constant DI      : XCM2_WORD:= X"E8";
constant VPRE    : XCM2_WORD:= X"E9";
constant VMODE   : XCM2_WORD:= X"EA";
constant VPAL    : XCM2_WORD:= X"EB";
constant VFSA    : XCM2_WORD:= X"EC";
constant VFSI    : XCM2_WORD:= X"ED";
constant VFSAC   : XCM2_WORD:= X"EE";
constant VFLA    : XCM2_WORD:= X"EF";
constant VFLAC   : XCM2_WORD:= X"F0";
constant VS2F    : XCM2_WORD:= X"F1";
constant VSSA    : XCM2_WORD:= X"F2";
constant VSSI    : XCM2_WORD:= X"F3";
constant VSLA    : XCM2_WORD:= X"F4";
constant VSS     : XCM2_WORD:= X"F5";
constant VSD     : XCM2_WORD:= X"F6";
constant IR247   : XCM2_WORD:= X"F7";
constant IR248   : XCM2_WORD:= X"F8";
constant IR249   : XCM2_WORD:= X"F9";
constant IR250   : XCM2_WORD:= X"FA";
constant IR251   : XCM2_WORD:= X"FB";
constant IR252   : XCM2_WORD:= X"FC";
constant IR253   : XCM2_WORD:= X"FD";
constant IR254   : XCM2_WORD:= X"FE";
constant IR255   : XCM2_WORD:= X"FF";

end FridgeIRCodes;
























































































































































































































































