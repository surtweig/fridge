    MOV A, D
    ORA A
    RZ
    MOV A, B
    CPI VTEXT_COLUMNS
    RP
    MOV A, C
    CPI VTEXT_ROWS
    RP

    // Each cell is two bytes. HL = 2 * (row * columns + column).
    PUSH DE
    PUSH BC
    MVI D, VTEXT_COLUMNS
    CALL mul_dc
    POP BC
    MOV E, B
    MVI D, 0
    DAD DE
    DAD HL
    POP DE

    entry vtext_clear.loop
        XRA A
        VFSA // Character byte.
        INX HL
        VFSA // Color byte.
        INX HL
        DCR D
        RZ
        INR B
        MOV A, B
        CPI VTEXT_COLUMNS
        JNZ vtext_clear.loop

        MVI B, 0
        INR C
        MOV A, C
        CPI VTEXT_ROWS
        JNZ vtext_clear.loop
