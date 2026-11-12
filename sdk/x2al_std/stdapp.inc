// Actual executable starts at 256th byte in RAM
// Lower byte must be zero!
alias EXECUTABLE_OFFSET          0x0100 

// Address of a byte holding the index of ROM section that will be loaded.
// This byte can be modified later by the executable (e.g. boot menu) to load other executable.
// This byte is reset to zero every time after loading.
alias BOOT_SECTION_INDEX_ADDRESS 0x0003

// System devices interrupts will execute instructions at these absolute addresses
alias IRQ_SYS_TIMER              0x0004
alias IRQ_SYS_KEYBOARD           0x0007

alias XCM2_ROM_DEVICE_ID         0x01
alias XCM2_ROM_DEVICE_RESET_ID   0x02
alias XCM2_ROM_DEVICE_MODE_LOAD  0x02

// Telling the compiler that the program starts at this address
offset EXECUTABLE_OFFSET
