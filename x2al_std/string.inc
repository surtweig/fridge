// ---------------------------------------- //
// string.inc                               //
//                                          //
// String operations                        //
// Copyright (c) Plus&Minus Inc. 2008       //
//                                          //
// ---------------------------------------- //


// Converts hexadecimal digit to the char code
// A - digit value (0..15)
// Sets A to uppercase char code of the digit (or whitespace if A is out of range)
subroutine string_hex_digit
    CPI 10
    JM string_hex_digit.decimal
    CPI 16
    JM string_hex_digit.hexadecimal
    MVI A, 32
    JMP string_hex_digit.end

    entry string_hex_digit.decimal
        ADI '0'
        JMP string_hex_digit.end

    entry string_hex_digit.hexadecimal
        ADI 55 // 55+10 = 65 = 'A'
        JMP string_hex_digit.end

    entry string_hex_digit.end
endsub

// Writes hexadecimal digits of a single word to a string, moves HL pointer +2
// HL - string address
// B - number
subroutine string_hex
    // extracting high digit
    MOV A, B
    RTC 
    RAR
    RTC 
    RAR
    RTC 
    RAR
    RTC
    RAR
    CALL string_hex_digit // retrieving high digit's char
    STAX HL               // writing to string at HL
    INX HL
        
    // extracting low digit
    MOV A, B
    ANI 0x0F // mask 00001111b
    CALL string_hex_digit // retrieving high digit's char
    STAX HL               // writing to string at HL+1
    INX HL
endsub

// Writes hexadecimal digits of a double word to a string, moves HL pointer +4
// HL - string address
// BC - number
subroutine string_dhex
    CALL string_hex
    MOV B, C
    CALL string_hex
endsub