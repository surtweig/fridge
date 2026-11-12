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
    include "vtext_putstr.inc"
endsub
