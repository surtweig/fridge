#include "FridgeAssemblyLanguageCompiler.h"
extern "C" {
#include "fridgemulib.h"
}
#include <cstdlib>
#include <cstring>

static void require(bool passed, const char* message)
{
    if (!passed)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

static void call(FRIDGE_SYSTEM& sys, const char* routine, unsigned transfers,
                 FRIDGE_DWORD bc, FRIDGE_DWORD de, FRIDGE_DWORD hl, FRIDGE_WORD a = 0)
{
    std::istringstream source(std::string("offset FRIDGE_EXECUTABLE_OFFSET\nmain start\nCALL ") +
                              routine + "\nHLT\ninclude \"rom.falc\"\n");
    FridgeAssemblyLanguageCompiler program(source, "./", "rom_library.falc", "", &std::cerr);
    require(program.succeeded(), "Could not assemble ROM library call");

    FRIDGE_CPU& cpu = *sys.cpu;
    const auto start = program.getMainEntry();
    const auto returnAddress = start + 3; // The instruction immediately after CALL.
    std::memcpy(cpu.ram + program.getOffset(), program.getObjectCode(), program.getProgramSize());
    cpu.PC = start;
    cpu.rA = a;
    cpu.rB = FRIDGE_HIGH_WORD(bc);
    cpu.rC = FRIDGE_LOW_WORD(bc);
    cpu.rD = FRIDGE_HIGH_WORD(de);
    cpu.rE = FRIDGE_LOW_WORD(de);
    cpu.rH = FRIDGE_HIGH_WORD(hl);
    cpu.rL = FRIDGE_LOW_WORD(hl);

    unsigned waits = 0;
    for (unsigned step = 0; step < 10000 && cpu.PC != returnAddress; ++step)
    {
        const bool halt = cpu.ram[cpu.PC] == HLT;
        if (halt)
            require(sys.rom->state == FRIDGE_ROM_OPERATE && cpu.inte == FRIDGE_CPU_INTERRUPTS_ENABLED,
                    "ROM-ready interrupt was lost or delivered before HLT");
        FRIDGE_sys_tick(&sys);
        require(cpu.state == FRIDGE_CPU_ACTIVE && !FRIDGE_cpu_flag_PANIC(&cpu), routine);
        if (halt)
        {
            ++waits;
            require(sys.rom->state == FRIDGE_ROM_STREAMING && cpu.inte == FRIDGE_CPU_INTERRUPTS_DISABLED,
                    "ROM-ready NOP did not resume the CPU in the HLT tick");
        }
    }
    require(cpu.PC == returnAddress && waits == transfers, routine);
    require(sys.rom->state == FRIDGE_ROM_SELECT_MODE, "ROM stream was left open");
    if (transfers != 0)
        require(cpu.inte == FRIDGE_CPU_INTERRUPTS_ENABLED, "ROM call left interrupts disabled");
}

int main()
{
    FRIDGE_CPU cpu{};
    FRIDGE_cpu_reset(&cpu);
    cpu.SP = 0x9000;
    std::vector<FRIDGE_ROM_SEGMENT> segments(FRIDGE_ROM_MAX_SEGMENTS);
    FRIDGE_ROM rom{};
    rom.segments = segments.data();
    rom.segments_count = FRIDGE_ROM_MAX_SEGMENTS;
    FRIDGE_SYSTEM sys{};
    sys.cpu = &cpu;
    sys.rom = &rom;

    const FRIDGE_DWORD tocAddress = 0x4075;
    FRIDGE_WORD* toc = cpu.ram + tocAddress;
    toc[1] = 0xff; // Slot 0 starts at 0x00ff.
    toc[2] = 1;    // Slot 1 starts at 0x0101.
    toc[3] = 1;
    toc[254] = 0xff; // Slot 127 starts at the final segment.
    toc[255] = 0xff;
    call(sys, "rom_write_toc", 1, 0xabcd, 0x1357, tocAddress);
    require(std::memcmp(segments[0].data, toc, FRIDGE_ROM_SEGMENT_SIZE) == 0, "TOC write");
    require(FRIDGE_cpu_pair_BC(&cpu) == 0xabcd && FRIDGE_cpu_pair_DE(&cpu) == 0x1357 &&
            FRIDGE_cpu_pair_HL(&cpu) == tocAddress + FRIDGE_ROM_SEGMENT_SIZE, "TOC registers");
    std::memset(toc, 0xa5, FRIDGE_ROM_SEGMENT_SIZE);
    call(sys, "rom_read_toc", 1, 0, 0, tocAddress);
    require(std::memcmp(segments[0].data, toc, FRIDGE_ROM_SEGMENT_SIZE) == 0, "TOC read");

    call(sys, "rom_section_bounds", 0, 0, 0xffff, tocAddress);
    const auto first = FRIDGE_cpu_pair_BC(&cpu);
    const auto count = FRIDGE_cpu_pair_DE(&cpu);
    require(cpu.rA == 0 && first == 0x00ff && count == 2 && FRIDGE_cpu_pair_HL(&cpu) == tocAddress,
            "TOC section bounds across a segment index byte boundary");
    call(sys, "rom_section_bounds", 0, 0, 0xffff, tocAddress, 1);
    require(cpu.rA == 0 && FRIDGE_cpu_pair_DE(&cpu) == 0xfeff, "Section bounds with a zero successor");
    call(sys, "rom_section_bounds", 0, 0, 0xffff, tocAddress, 127);
    require(cpu.rA == 0 && FRIDGE_cpu_pair_BC(&cpu) == 0xffff && FRIDGE_cpu_pair_DE(&cpu) == 1,
            "Section bounds for slot 127 at the final segment");
    call(sys, "rom_section_bounds", 0, 0, 0xffff, tocAddress, 128);
    require(cpu.rA == 1, "Out-of-range TOC index");
    call(sys, "rom_section_bounds", 0, 0, 0xffff, tocAddress, 2);
    require(cpu.rA == 1, "Unused TOC record");
    call(sys, "rom_section_bounds", 0, 0, 0x00ff, tocAddress);
    require(cpu.rA == 1, "TOC section exceeds the supplied media limit");

    const FRIDGE_DWORD source = 0x5023;
    const FRIDGE_DWORD dest = 0x6543;
    const unsigned size = 2 * FRIDGE_ROM_SEGMENT_SIZE;
    for (unsigned i = 0; i < size; ++i)
        cpu.ram[source + i] = static_cast<FRIDGE_WORD>(i ^ (i >> FRIDGE_WORD_BITS));
    call(sys, "rom_write_section", 2, first, count, source);
    require(cpu.rA == 0 && FRIDGE_cpu_pair_BC(&cpu) == first + count && FRIDGE_cpu_pair_DE(&cpu) == 0 &&
            FRIDGE_cpu_pair_HL(&cpu) == source + size, "Section write registers");
    call(sys, "rom_read_section", 2, first, count, dest);
    require(cpu.rA == 0 && std::memcmp(cpu.ram + source, cpu.ram + dest, size) == 0, "Section round trip");

    call(sys, "rom_write_segment", 1, 0xffff, 0, source);
    call(sys, "rom_read_segment", 1, 0xffff, 0, 0xff00);
    require(std::memcmp(cpu.ram + source, cpu.ram + 0xff00, FRIDGE_ROM_SEGMENT_SIZE) == 0 &&
            FRIDGE_cpu_pair_HL(&cpu) == 0, "Segment 0xffff and buffer ending at 0xffff");

    call(sys, "rom_read_section", 0, 0xffff, 2, dest);
    require(cpu.rA == 1, "Section read wraps the ROM address");
    call(sys, "rom_write_section", 0, 1, 1, 0xff01);
    require(cpu.rA == 1, "Section write wraps the RAM address");
    call(sys, "rom_write_section", 0, 0, 1, source);
    require(cpu.rA == 1, "Section write includes the TOC");
    call(sys, "rom_read_section", 0, 1, 0, dest);
    require(cpu.rA == 0, "Zero-length section read");
}
