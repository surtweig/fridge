// ---------------------------------------- //
// vtext.inc                                //
//                                          //
// Text video mode library for XCM2         //
// Copyright (c) Plus&Minus Inc. 2008       //
//                                          //
// ---------------------------------------- //

include "arithm.inc"

static VTEXT_FORE_COLOR 15
static VTEXT_BACK_COLOR 0

alias VTEXT_COLUMNS 40
alias VTEXT_ROWS 20

// Writes a null-terminated string at a given position
// HL - string address
// B  - column number
// C  - row number
subroutine vtext_putstr
    // preserving arguments in stack
    PUSH HL
    PUSH BC
    // Text cell address (HL) = 2*(row number (C) * VTEXT_COLUMNS + column number (B)).
    // Address is two times index because a character takes two bytes - first for an ascii code
    // and second for color information.
    MVI A, VTEXT_COLUMNS
    MOV D, A
    CALL mul_dc    // row * VTEXT_COLUMNS
    POP BC
    MOV C, B
    MVI B, 0
    DAD BC   // adding column (initially B) through double-add with BC pair
    
    // multiplying by 2
    MOV A, L
    RAL
    MOV L, A
    MOV A, H
    RAL
    MOV H, A
    
    //? saving text cell address in BC
    //MOV B, H 
    //MOV C, L
    
    POP BC   // restore string address to BC
    
    // setting up color info: (foreground:0..3, background:4..7)
    LDA VTEXT_BACK_COLOR
    RAL
    RAL
    RAL
    RAL
    MOV D, A    
    LDA VTEXT_FORE_COLOR
    //MVI A, 15
    ADD D
    MOV D, A
    
    entry vtext_putstr.loop
        LDAX BC
        CPI 0        // check for the string end
        RZ
    
        VFSA        // writing ascii code into vmemory
        INX HL
        
        MOV A, D    // writing color info into vmemory
        VFSA
        INX HL
        
        INX BC
    JMP vtext_putstr.loop 
endsub
