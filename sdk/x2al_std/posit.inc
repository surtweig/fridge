// ---------------------------------------- //
// posit.inc                                //
//                                          //
// Posit arithmetic utilities               //
// for PAM16 extension                      //
// Copyright (c) Plus&Minus Inc. 2008       //
//                                          //
// ---------------------------------------- //

// Calls PAM16 RESET command
// A - ES parameter
subroutine POSIT_RESET
    // shifting ES value 4 bits left
    RTC
    RAL
    RTC
    RAL
    RTC
    RAL
    RTC
    RAL
    
    // applying RESET code to 4 lower bits
    ORI FRIDGE_PAM16_RESET

    // calling PAM16 command
    PAM16C
endsub
