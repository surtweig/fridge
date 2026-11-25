#include "support.h"
#include "FridgeAssemblyLanguageCompiler.h"

extern "C"
{
#include "fridgemulib.h"
}

#include <algorithm>

static string read(const fs::path& path)
{
    std::ifstream file(path);
    return string(std::istreambuf_iterator<char>(file), {});
}

static void runInlineLibrary(Fixture& fixture, bool fixed)
{
    const auto root = fixture.directory.string() + "/";
    std::ofstream(fixture.directory / "inline_lib.falc") << (fixed ? "offset 0x4000\n" : "") << R"(
        @const uint8
        alias INITIAL 3
        @static uint8
        static VALUE INITIAL_VALUE
        @static uint16
        static HEAP INITIAL_HEAP
        static TEXT INITIAL_TEXT
        alias INITIAL_VALUE FRIDGE_GPU_VIDEO_SWAP_AUTO
        alias INITIAL_HEAP 0xABCD
        alias INITIAL_TEXT "OK"

        subroutine helper
            MVI A, 0xA5
        endsub

        @inline void((uint8 value : A)) // Bind the argument as usual.
        subroutine store
            PUSH BC
            MOV B, A
            MVI A, INITIAL
            ADD B
            STA VALUE
            POP BC
            LXI HL, HEAP_ORIGIN
            SHLD HEAP
            CALL helper
            STA 0x0103
        endsub

        @inline uint8 : A((uint8 iterations : B))
        subroutine count
            MVI A, 0
            entry loop
                INR A
                DCR B
                JNZ loop
            CPI 3
            RZ
            CPI 4
            JNZ extra
            RET
            entry extra
                ADI 10
        endsub

        @inline void()
        subroutine empty
        endsub

        @inline uint16:HL((uint16 address : HL))
        subroutine next_address
            INX HL
        endsub

        @inline void((uint8 index : A), (uint8 red : B), (uint8 green : C), (uint8 blue : D), (uint8 x : H), (uint8 y : L))
        subroutine palette
            VPAL
            SHLD 0x0107
            MVI H, 0xCD
            MVI L, 0xEF
            SHLD 0x0109
            LXI HL, 0x0000
            VFSI 0x12, 0x34
            VSSI 'A', 'B'
            LXI DE, 0x0106
            MVI A, 0xC7
            STAX DE
        endsub

        @uint16:HL((uint16 source : HL), (uint8 value : A), (uint16 dest : DE))
        subroutine ordinary
            STAX DE
            INX DE
            LDAX HL
            STAX DE
            LXI HL, 0xA7D3
        endsub
    )" << (fixed ? "\nmain library_start\n    HLT\n" : "");
    std::ostringstream log;
    FridgeAssemblyLanguageCompiler library(root, "inline_lib.falc", root + "inline_lib.bin",
        {}, &log, false, false, true);
    require(library.succeeded(), log.str());
    const auto* bytes = library.getObjectCode();
    const auto initialValue = library.getResources().at("VALUE").address - library.getOffset();
    const auto initialHeap = library.getResources().at("HEAP").address - library.getOffset();
    const auto initialText = library.getResources().at("TEXT").address - library.getOffset();
    require(bytes[initialValue] == FRIDGE_GPU_VIDEO_SWAP_AUTO &&
            bytes[initialHeap] == 0xAB && bytes[initialHeap + 1] == 0xCD &&
            bytes[initialText] == 'O' && bytes[initialText + 1] == 'K' && bytes[initialText + 2] == 0,
            "Static aliases did not emit the expected byte, word, and string initializers");
    const auto header = read(fixture.directory / "inline_lib.cpm");
    require(header.find("__mov_b_a;") != string::npos && header.find("__add_b;") != string::npos &&
            header.find("__vpal;") != string::npos && header.find("__vfsi 0x1234;") != string::npos &&
            header.find("__vssi 0x4142;") != string::npos && header.find("__jnz_local") != string::npos &&
            header.find("__ret;") == string::npos && header.find("__rz;") == string::npos,
            "Header did not copy instructions or translate local flow correctly:\n" + header);
    const auto call = header.find("__callsub");
    require(call != string::npos && header.find("__callsub", call + 1) == string::npos,
            "Only the ordinary export should generate __callsub");
    require(header.find("__lxi_de dest;") != string::npos && header.find("__lxi_de 0x0106;") != string::npos,
            "Header did not generate the DE binding and inline load:\n" + header);
    if (!fixed)
        require(header.find("__sta_rel") != string::npos && header.find("__call_rel") != string::npos &&
                header.find("__lxi_hl_heap") != string::npos,
                "Inline operands lost their relocation information");

    fixture.source(R"(
        include "inline_lib.cpm";
        using inline_lib;
        static <uint8 array 3> output imports 0x0100;
        static <uint16 array 2> addresses imports 0x010B;
        void main()
        (
            uint8 value 7;
            uint16 address 0x34FF;
            store value;
            empty;
            = (at output, 0), (count 3);
            = (at output, 1), (count 4);
            = (at output, 2), (count 2);
            palette 1, value, 8, 9, 0x12, 0x34;
            = (at addresses, 0), (ordinary 0x0103, 0x5A, 0x0104);
            = (at addresses, 1), (next_address address);
        );
    )");
    CPMCompiler compiler(root, "input.cpm", "", {}, CPMCompileMode::Assembly, {0x0800, CPMMainReturn::Halt});
    require(compiler.NoErrors(), compiler.CompilerLog()->GetText());
    std::istringstream assembly(compiler.AssemblyOutput());
    std::ostringstream programLog;
    FridgeAssemblyLanguageCompiler program(assembly, root, "program.falc", "", &programLog);
    require(program.succeeded(), programLog.str() + compiler.AssemblyOutput());

    auto cpu = std::make_unique<FRIDGE_CPU>();
    auto gpu = std::make_unique<FRIDGE_GPU>();
    FRIDGE_cpu_reset(cpu.get());
    FRIDGE_gpu_reset(gpu.get());
    const auto initialSP = cpu->SP;
    cpu->PC = program.getOffset();
    std::copy_n(program.getObjectCode(), program.getProgramSize(), cpu->ram + cpu->PC);
    const size_t base = fixed ? 0 : program.getOffset() + program.getProgramSize() - library.getProgramSize();
    // The inline wrappers must work even when the original routines cannot run.
    for (const auto& name : {"store", "count", "empty", "next_address", "palette"})
        cpu->ram[base + library.getSubroutines().at(name)] = HLT;
    FRIDGE_SYSTEM system{};
    system.cpu = cpu.get();
    system.gpu = gpu.get();
    for (int steps = 0; steps < 30000 && cpu->state == FRIDGE_CPU_ACTIVE; ++steps) FRIDGE_sys_tick(&system);
    const size_t heap = base + library.getResources().at("HEAP").address;
    const size_t expectedHeap = fixed ? library.getOffset() + library.getProgramSize() :
                                       program.getOffset() + program.getProgramSize();
#ifdef FRIDGE_ASCENDING_STACK
    const auto expectedSP = program.getOffset() + program.getProgramSize();
#else
    const auto expectedSP = initialSP;
#endif
    require(cpu->state == FRIDGE_CPU_HALTED && !FRIDGE_cpu_flag_PANIC(cpu.get()) &&
            cpu->SP == expectedSP,
            "Inline program did not finish with a balanced stack: PC=" + to_string(cpu->PC) +
            " SP=" + to_string(cpu->SP) + " expected=" + to_string(expectedSP));
    require(cpu->ram[0x0100] == 3 && cpu->ram[0x0101] == 4 && cpu->ram[0x0102] == 12 &&
            cpu->ram[0x0103] == 0xA5 && cpu->ram[0x0104] == 0x5A &&
            cpu->ram[0x0105] == 0xA5 && cpu->ram[0x0106] == 0xC7 &&
            cpu->ram[0x0107] == 0x12 && cpu->ram[0x0108] == 0x34 &&
            cpu->ram[0x0109] == 0xCD && cpu->ram[0x010A] == 0xEF &&
            cpu->ram[base + library.getResources().at("VALUE").address] == 10 &&
            FRIDGE_DWORD_HL(cpu->ram[heap], cpu->ram[heap + 1]) == expectedHeap &&
            gpu->palette[3] == 7 && gpu->palette[4] == 8 && gpu->palette[5] == 9 &&
            FRIDGE_gpu_active_frame(gpu.get())[0] == 0x12 && FRIDGE_gpu_active_frame(gpu.get())[1] == 0x34 &&
            gpu->sprite_mem[2] == 'A' && gpu->sprite_mem[3] == 'B',
            "Inline instructions, parameters, return values, or relocated addresses were incorrect");
    require(cpu->ram[0x010B] == 0xA7 && cpu->ram[0x010C] == 0xD3 &&
            cpu->ram[0x010D] == 0x35 && cpu->ram[0x010E] == 0x00,
            "CPM wrappers did not return both bytes of HL in big-endian order");
}

static void rejectInvalidExports(Fixture& fixture)
{
    const auto root = fixture.directory.string() + "/";
    const vector<string> sources{
        "@inline static uint8\nstatic invalid 0\n",
        "@inline void()\nsubroutine invalid\nPCHL\nendsub\n",
        "@inline void()\nsubroutine invalid\nJMP external\nendsub\nentry external\nHLT\n",
        "@inline void()\nsubroutine invalid\nMVI A, 0\nJMP 0x0001\nendsub\n",
        "@inline void((uint8 value : D), (uint16 dest : DE))\nsubroutine invalid\nendsub\n",
        "@inline void((uint8 dest : DE))\nsubroutine invalid\nendsub\n",
        "@inline void((uint8 x : H), (uint16 dest : HL))\nsubroutine invalid\nendsub\n",
        "@inline void((string text : HL), (uint8 y : L))\nsubroutine invalid\nendsub\n",
        "@inline void((uint16 dest : H))\nsubroutine invalid\nendsub\n",
        "@inline void((string text : L))\nsubroutine invalid\nendsub\n",
        "@static uint16\nstatic invalid TEXT\nalias TEXT \"x\"\n"
    };
    auto reject = [&](const string& source, const string& expectedDiagnostic = "") {
        std::ofstream(fixture.directory / "invalid.falc") << source;
        std::ostringstream log;
        FridgeAssemblyLanguageCompiler library(root, "invalid.falc", root + "invalid.bin", {}, &log,
            false, false, true);
        require(!library.succeeded() && !fs::exists(fixture.directory / "invalid.cpm"),
                "An invalid export was accepted:\n" + source + log.str());
        require(log.str().find(expectedDiagnostic) != string::npos,
                "Unexpected CPM export diagnostic:\n" + log.str());
    };
    for (const auto& source : sources) reject(source);
    reject("@uint16:DE ( (uint16 size : HL) )\nsubroutine alloc\nendsub\n",
           "[invalid.falc : 1] Unsupported CPM return specification 'uint16:DE' for 'alloc'. "
           "Supported returns are 'void', 'uint8:A' (8-bit), and 'uint16:HL' (16-bit).");
    reject("@void\nsubroutine invalid\nendsub\n",
           "Malformed CPM subroutine decorator '@void' for 'invalid'.");
    fixture.check("void main() (uint8 n 1; __adi n;);", false);
    fixture.check("void main() (__vpal 1;);", false);
    fixture.check("void main() (__sta 1;);", false);
    fixture.check("void __vpal(); void main() (__vpal;);", true);
    fixture.check("uint16 __read_hl(); void main() (uint16 value (__read_hl););", true);
    fixture.check("uint8 __read_hl();", false);
}

static void runRomLibrary(Fixture& fixture)
{
    const auto root = fixture.directory.string() + "/";
    fs::copy_file(CPM_ROM_LIBRARY, fixture.directory / "rom.falc");
    std::ostringstream log;
    FridgeAssemblyLanguageCompiler library(root, "rom.falc", root + "rom.bin",
        {}, &log, false, false, true);
    require(library.succeeded(), log.str());

    fixture.source(R"(
        include "rom.cpm";
        using rom;
        struct RomSpan (uint16 first; uint16 count;);
        static RomSpan span imports 0x0100;
        static <uint8 array 4> status imports 0x0104;
        static <uint8 array ROM_SEGMENT_SIZE> toc imports 0x2000;
        void main()
        (
            uint16 toc_address (cast uint16, (addr toc));
            uint16 span_address (cast uint16, (addr span));
            rom_read_toc toc_address;
            = (at toc, 255), 0x5A;
            rom_write_toc toc_address;
            = (at status, 0), (rom_section_bounds_into 1, toc_address, 0x0103, span_address);
            = (at status, 1), (rom_section_bounds_into ROM_TOC_RECORDS, toc_address, 0x0103, span_address);
            = (at status, 2), (rom_write_section span.first, span.count, 0x2300);
            = (at status, 3), (rom_read_section span.first, span.count, 0x2100);
            rom_write_segment 0x0103, 0x2300;
            rom_read_segment 0x0103, 0x2500;
        );
    )");
    CPMCompiler compiler(root, "input.cpm", "", {}, CPMCompileMode::Assembly, {0x0800, CPMMainReturn::Halt});
    require(compiler.NoErrors(), compiler.CompilerLog()->GetText());
    std::istringstream assembly(compiler.AssemblyOutput());
    FridgeAssemblyLanguageCompiler program(assembly, root, "program.falc", "", &log);
    require(program.succeeded(), log.str() + compiler.AssemblyOutput());

    FRIDGE_CPU cpu{};
    FRIDGE_cpu_reset(&cpu);
    cpu.PC = program.getOffset();
    std::copy_n(program.getObjectCode(), program.getProgramSize(), cpu.ram + cpu.PC);
    std::fill_n(cpu.ram + 0x0104, 4, 0xff);
    for (unsigned i = 0; i < 2 * FRIDGE_ROM_SEGMENT_SIZE; ++i)
        cpu.ram[0x2300 + i] = static_cast<FRIDGE_WORD>(i ^ (i >> FRIDGE_WORD_BITS));

    std::vector<FRIDGE_ROM_SEGMENT> segments(0x0104);
    auto& toc = segments[0].data;
    toc[1] = 1;
    toc[2] = 1;
    toc[3] = 1;
    toc[4] = 1;
    toc[5] = 3;
    FRIDGE_ROM rom{};
    rom.segments = segments.data();
    rom.segments_count = segments.size();
    FRIDGE_SYSTEM system{};
    system.cpu = &cpu;
    system.rom = &rom;
    for (int steps = 0; steps < 30000 && cpu.state == FRIDGE_CPU_ACTIVE; ++steps) FRIDGE_sys_tick(&system);

    require(cpu.state == FRIDGE_CPU_HALTED && !FRIDGE_cpu_flag_PANIC(&cpu), "CPM ROM program failed");
    require(cpu.ram[0x0104] == 0 && cpu.ram[0x0105] == 1 && cpu.ram[0x0106] == 0 && cpu.ram[0x0107] == 0,
            "CPM ROM wrapper returned incorrect status values");
    const FRIDGE_DWORD first = FRIDGE_DWORD_HL(cpu.ram[0x0100], cpu.ram[0x0101]);
    const FRIDGE_DWORD count = FRIDGE_DWORD_HL(cpu.ram[0x0102], cpu.ram[0x0103]);
    require(first == 0x0101 && count == 2, "CPM ROM wrapper wrote incorrect section bounds");
    require(toc[255] == 0x5A && std::equal(toc, toc + FRIDGE_ROM_SEGMENT_SIZE, cpu.ram + 0x2000),
            "CPM TOC read or write failed");
    require(std::equal(cpu.ram + 0x2100, cpu.ram + 0x2300, cpu.ram + 0x2300), "CPM section round trip failed");
    require(std::equal(cpu.ram + 0x2500, cpu.ram + 0x2600, cpu.ram + 0x2300), "CPM segment round trip failed");
}

int main()
{
    try
    {
        Fixture fixture;
        runInlineLibrary(fixture, false);
        Fixture fixedFixture;
        runInlineLibrary(fixedFixture, true);
        rejectInvalidExports(fixture);
        runRomLibrary(fixture);
        std::cout << "CPM header tests passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
