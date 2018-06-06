// testvtext.inc
// This is file is automatically generated on each build.

// Defines the main entry point of testvtext
main testvtext
	JMP testvtext.start
    NOP // 0x0003
    
    // System timer IRQ
    JMP testvtext.SysTimerIRQ // 0x0004
    //NOP
    //NOP
    // Keyboard IRQ
    RET // 0x0007
    NOP
    NOP