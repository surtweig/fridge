use work.XCMGlobals.all;

package XCMIRCodes is

     constant IR_NOP   : XCMWord:= 31;
     constant IR_JMP   : XCMWord:= 0;
     constant IR_CMPRC : XCMWord:= 1;
     constant IR_NEQRC : XCMWord:= 2;
     constant IR_SET   : XCMWord:= 3;
     constant IR_CPY   : XCMWord:= 4;
     constant IR_INC   : XCMWord:= 5;
     constant IR_DEC   : XCMWord:= 6;
     constant IR_DRWR  : XCMWord:= 7;
     constant IR_DRWC  : XCMWord:= 8;
     constant IR_ERSR  : XCMWord:= 9;
     constant IR_ERSC  : XCMWord:= 10;
     constant IR_BCPY  : XCMWord:= 11;
     constant IR_CMPBC : XCMWord:= 12;
     constant IR_NEQBC : XCMWord:= 13;
     constant IR_CMPRR : XCMWord:= 14;
     constant IR_NEQRR : XCMWord:= 15;

end XCMIRCodes;