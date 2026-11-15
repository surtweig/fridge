file(READ "${INPUT}" bootloader HEX)
string(REGEX REPLACE "(..)" "0x\\1, " bootloader "${bootloader}")
file(WRITE "${OUTPUT}" "static constexpr FRIDGE_WORD dummy_bootloader[] = { ${bootloader}};\n")
