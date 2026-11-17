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
        static VALUE 0
        @static uint16
        static HEAP 0x0000

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

        @inline void((uint8 index : A), (uint8 red : B), (uint8 green : C), (uint8 blue : D))
        subroutine palette
            VPAL
            LXI HL, 0x0000
            VFSI 0x12, 0x34
            VSSI 'A', 'B'
        endsub

        @void()
        subroutine ordinary
            MVI A, 0x5A
            STA 0x0104
        endsub
    )" << (fixed ? "\nmain library_start\n    HLT\n" : "");
    std::ostringstream log;
    FridgeAssemblyLanguageCompiler library(root, "inline_lib.falc", root + "inline_lib.bin",
        {}, &log, false, false, true);
    require(library.succeeded(), log.str());
    const auto header = read(fixture.directory / "inline_lib.cpm");
    require(header.find("__mov_b_a;") != string::npos && header.find("__add_b;") != string::npos &&
            header.find("__vpal;") != string::npos && header.find("__vfsi 0x1234;") != string::npos &&
            header.find("__vssi 0x4142;") != string::npos && header.find("__jnz_local") != string::npos &&
            header.find("__ret;") == string::npos && header.find("__rz;") == string::npos,
            "Header did not copy instructions or translate local flow correctly:\n" + header);
    const auto call = header.find("__callsub");
    require(call != string::npos && header.find("__callsub", call + 1) == string::npos,
            "Only the ordinary export should generate __callsub");
    if (!fixed)
        require(header.find("__sta_rel") != string::npos && header.find("__call_rel") != string::npos &&
                header.find("__lxi_hl_heap") != string::npos,
                "Inline operands lost their relocation information");

    fixture.source(R"(
        include "inline_lib.cpm";
        using inline_lib;
        static <uint8 array 3> output imports 0x0100;
        void main()
        (
            uint8 value 7;
            store value;
            empty;
            = (at output, 0), (count 3);
            = (at output, 1), (count 4);
            = (at output, 2), (count 2);
            palette 1, value, 8, 9;
            ordinary;
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
    for (const auto& name : {"store", "count", "empty", "palette"})
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
            cpu->ram[base + library.getResources().at("VALUE").address] == 10 &&
            FRIDGE_DWORD_HL(cpu->ram[heap], cpu->ram[heap + 1]) == expectedHeap &&
            gpu->palette[3] == 7 && gpu->palette[4] == 8 && gpu->palette[5] == 9 &&
            FRIDGE_gpu_active_frame(gpu.get())[0] == 0x12 && FRIDGE_gpu_active_frame(gpu.get())[1] == 0x34 &&
            gpu->sprite_mem[2] == 'A' && gpu->sprite_mem[3] == 'B',
            "Inline instructions, parameters, return values, or relocated addresses were incorrect");
}

static void rejectInvalidInline(Fixture& fixture)
{
    const auto root = fixture.directory.string() + "/";
    const vector<string> sources{
        "@inline static uint8\nstatic invalid 0\n",
        "@inline void()\nsubroutine invalid\nPCHL\nendsub\n",
        "@inline void()\nsubroutine invalid\nJMP external\nendsub\nentry external\nHLT\n",
        "@inline void()\nsubroutine invalid\nMVI A, 0\nJMP 0x0001\nendsub\n"
    };
    for (const auto& source : sources)
    {
        std::ofstream(fixture.directory / "invalid.falc") << source;
        std::ostringstream log;
        FridgeAssemblyLanguageCompiler library(root, "invalid.falc", root + "invalid.bin", {}, &log,
            false, false, true);
        require(!library.succeeded() && !fs::exists(fixture.directory / "invalid.cpm"),
                "An invalid inline export was accepted:\n" + source + log.str());
    }
    fixture.check("void main() (uint8 n 1; __adi n;);", false);
    fixture.check("void main() (__vpal 1;);", false);
    fixture.check("void main() (__sta 1;);", false);
    fixture.check("void __vpal(); void main() (__vpal;);", true);
}

int main()
{
    try
    {
        Fixture fixture;
        runInlineLibrary(fixture, false);
        Fixture fixedFixture;
        runInlineLibrary(fixedFixture, true);
        rejectInvalidInline(fixture);
        std::cout << "CPM inline header tests passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
