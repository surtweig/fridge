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

static void set_record(FRIDGE_ROM_SEGMENT& toc, unsigned section, FRIDGE_ROM_ADDR first)
{
    toc.data[2 * section] = FRIDGE_HIGH_WORD(first);
    toc.data[2 * section + 1] = FRIDGE_LOW_WORD(first);
}

static void start(FRIDGE_CPU& cpu, FridgeAssemblyLanguageCompiler& boot, FRIDGE_WORD section)
{
    FRIDGE_cpu_reset(&cpu);
    std::memcpy(cpu.ram, boot.getObjectCode(), boot.getProgramSize());
    cpu.ram[FRIDGE_BOOT_SECTION_INDEX_ADDRESS] = section;
}

static bool boot_to_program(FRIDGE_SYSTEM& sys)
{
    const auto& cpu = *sys.cpu;
    for (unsigned step = 0; step < 500000; ++step)
    {
        if (cpu.state == FRIDGE_CPU_HALTED || cpu.PC == FRIDGE_EXECUTABLE_OFFSET)
            return cpu.state == FRIDGE_CPU_ACTIVE;
        FRIDGE_sys_tick(&sys);
    }
    require(false, "ROM bootloader ran out of ticks");
    return false;
}

static void check_loaded(const FRIDGE_SYSTEM& sys, unsigned first, unsigned count)
{
    const auto& cpu = *sys.cpu;
    for (unsigned i = 0; i < count; ++i)
    {
        const auto* bytes = cpu.ram + FRIDGE_EXECUTABLE_OFFSET + i * FRIDGE_ROM_SEGMENT_SIZE;
        require(std::memcmp(bytes, sys.rom->segments[first + i].data, FRIDGE_ROM_SEGMENT_SIZE) == 0,
                "Bootloader copied the wrong ROM segment");
    }
    const auto* trailer = cpu.ram + FRIDGE_EXECUTABLE_OFFSET + count * FRIDGE_ROM_SEGMENT_SIZE;
    require(trailer[0] == JMP && trailer[1] == 0 && trailer[2] == 0, "Missing jump back to reset");
    require(cpu.ram[FRIDGE_BOOT_SECTION_INDEX_ADDRESS] == 0 && cpu.SP == 0xffff,
            "Bootloader did not reset the section index and stack at handoff");
}

static void check_rejected(FRIDGE_SYSTEM& sys, FridgeAssemblyLanguageCompiler& boot, FRIDGE_WORD section)
{
    auto& cpu = *sys.cpu;
    start(cpu, boot, section);
    cpu.ram[FRIDGE_EXECUTABLE_OFFSET + FRIDGE_ROM_SEGMENT_SIZE] = 0xa5;
    require(!boot_to_program(sys) && !FRIDGE_cpu_flag_PANIC(&cpu), "Invalid boot did not halt cleanly");
    require(cpu.ram[FRIDGE_BOOT_SECTION_INDEX_ADDRESS] == section &&
            cpu.ram[FRIDGE_EXECUTABLE_OFFSET + FRIDGE_ROM_SEGMENT_SIZE] == 0xa5,
            "Invalid boot changed the selector or loaded payload data");
}

int main()
{
    FridgeAssemblyLanguageCompiler boot(FRIDGE_BOOTLOADER_DIR, "BootLoader.x2al", "", {}, &std::cerr);
    require(boot.succeeded() && boot.getOffset() == 0, "Could not assemble bootloader at reset address");
    require(boot.getProgramSize() <= 512 && boot.getProgramSize() <= FRIDGE_EXECUTABLE_OFFSET,
            "Bootloader does not fit in the reserved 512 bytes");
    require(boot.getObjectCode()[FRIDGE_BOOT_SECTION_INDEX_ADDRESS] == 0,
            "Bootloader does not default to section 0");

    FRIDGE_CPU cpu{};
    std::vector<FRIDGE_ROM_SEGMENT> segments(0x0104);
    FRIDGE_ROM rom{};
    rom.segments = segments.data();
    rom.segments_count = segments.size();
    FRIDGE_SYSTEM sys{};
    sys.cpu = &cpu;
    sys.rom = &rom;

    set_record(segments[0], 0, 1);
    set_record(segments[0], 1, 0x00ff);
    set_record(segments[0], 2, 0x0101);
    set_record(segments[0], 127, 0x0102);
    segments[0x0100].data[255] = 0x41;
    segments[0x0102].data[0] = 0x52;
    segments[0x0103].data[255] = 0x63;
    const FRIDGE_WORD reboot[] = {MVI_A, 127, STA, 0, FRIDGE_BOOT_SECTION_INDEX_ADDRESS, JMP, 0, 0};
    std::memcpy(segments[0x00ff].data, reboot, sizeof(reboot));

    start(cpu, boot, 1);
    require(boot_to_program(sys), "Bootloader did not load section 1");
    check_loaded(sys, 0x00ff, 2);
    cpu.ram[FRIDGE_IRQ_SYS_TIMER] = HLT;
    cpu.ram[FRIDGE_IRQ_KEYBOARD] = HLT;
    for (int instruction = 0; instruction < 3; ++instruction) FRIDGE_sys_tick(&sys);
    require(cpu.PC == 0 && cpu.ram[FRIDGE_BOOT_SECTION_INDEX_ADDRESS] == 127, "Program did not request section 127");
    require(boot_to_program(sys), "Bootloader did not load the final TOC record");
    check_loaded(sys, 0x0102, 2);
    require(cpu.ram[FRIDGE_IRQ_SYS_TIMER] == EI && cpu.ram[FRIDGE_IRQ_SYS_TIMER + 1] == RET &&
            cpu.ram[FRIDGE_IRQ_KEYBOARD] == JMP, "Warm boot did not restore IRQ vectors");

    check_rejected(sys, boot, 128);
    check_rejected(sys, boot, 3);
    set_record(segments[0], 2, 0x0105);
    check_rejected(sys, boot, 1);

    set_record(segments[0], 1, 255);
    check_rejected(sys, boot, 0); // 254 segments would overwrite the stack.
    set_record(segments[0], 1, 254);
    start(cpu, boot, 0);
    require(boot_to_program(sys), "Bootloader rejected the largest section that fits in RAM");
    check_loaded(sys, 1, 253);

    rom.segments_count = 1;
    check_rejected(sys, boot, 0);
    sys.rom = nullptr;
    check_rejected(sys, boot, 0);
}
