// ---------------------------------------- //
// arithm.inc                               //
//                                          //
// Basic arithmetic functions               //
// Copyright (c) Plus&Minus Inc. 2008       //
//                                          //
// ---------------------------------------- //

// Unsigned one-byte multiplication
// D - multiplicand
// C - multiplier
// BC - 16bit result
//subroutine mul_dc
//    MVI E, 9
//    MVI B, 0
//    entry mul_dc.loop
//        MOV A, C
//        RAR
//        MOV C, A
//        DCR E
//        JZ mul_dc.end
//        MOV A, B
//        JNC mul_dc.shifthigh
//        ADD D
//    entry mul_dc.shifthigh
//        RAR
//        MOV B, A
//        JMP mul_dc.loop
//    entry mul_dc.end    
//endsub


// Unsigned one-byte multiplication
// D - multiplicand
// C - multiplier
// HL - 16bit result
subroutine mul_dc
    MVI E, 8 // bit counter
    LXI HL, 0x0000
    MVI B, 0 // C bits cache
    RTC
    entry mul_dc.loop
        MOV A, D
        RAR
        MOV D, A
        JNC mul_dc.next
               
        MOV A, H          // H += B
        ADD B
        MOV H, A       
        
        MOV A, L          // L += C
        ADD C
        MOV L, A

    entry mul_dc.next       
        RTC
        MOV A, B          // B <<= 1
        RAL
        MOV B, A

        RTC
        MOV A, C          // C <<= 1
        RAL
        MOV C, A
        
        MOV A, B          // B += Carry
        ACI 0
        MOV B, A
        
        DCR E
        JZ mul_dc.end
        JMP mul_dc.loop
    
    entry mul_dc.end
endsub









