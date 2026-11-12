#include "support.h"
#include "CPMIR.h"
#include "FridgeAssemblyLanguageCompiler.h"

extern "C"
{
#include "fridgemulib.h"
}

#include <algorithm>

static const CPMIRFunction& findFunction(const CPMIRProgram& program, const string& name)
{
    auto function = std::find_if(program.functions.begin(), program.functions.end(),
        [&](const auto& body) { return body.symbol->signature.name == name; });
    require(function != program.functions.end(), "Missing IR for " + name);
    return *function;
}

static void inspectConstantInitializers(Fixture& fixture)
{
    fixture.check(R"(
        void main()
        (
            uint8 byte_value (+ 3, 4);
            uint16 word_value 0xABCD;
            <uint8 array 3> array_value (1; 2; 3);
        );
    )", true, [](CPMCompiler& compiler)
    {
        const auto program = CPMBuildIR(compiler);
        require(compiler.NoErrors(), compiler.CompilerLog()->GetText());
        const auto& main = findFunction(program, "main");
        const vector<vector<FRIDGE_WORD>> expected{{7}, {0xAB, 0xCD}, {1, 2, 3}};
        require(main.localBytes == 6 && main.frameBytes == main.localBytes &&
                main.slots.size() == expected.size(),
                "Constant local initializers should not allocate temporary slots");
        require(main.blocks.size() == 1 && main.blocks[0].size() == expected.size() + 1,
                "Constant local initializers should need only one write per variable");
        for (size_t i = 0; i < expected.size(); ++i)
        {
            const auto& instruction = main.blocks[0][i];
            require(instruction.op == CPMIROp::Constant && instruction.output == i &&
                    instruction.bytes == expected[i],
                    "Constant initializer did not write its bytes directly to the local slot");
        }
    });
}

static void inspectIR(Fixture& fixture, const string& source)
{
    fixture.check(source, true, [](CPMCompiler& compiler)
    {
        const auto program = CPMBuildIR(compiler);
        require(compiler.NoErrors(), compiler.CompilerLog()->GetText());
        const auto& choose = findFunction(program, "choose");
        require(choose.localBytes == 1 && choose.frameBytes >= choose.localBytes,
                "Incorrect local frame for choose");
        require(choose.argumentBytes == 3 && choose.slots[choose.parameters[0]].offset == 4 &&
                choose.slots[choose.parameters[1]].offset == 2 &&
                choose.slots[choose.result].offset == 5 && choose.slots[choose.result].size == 1,
                "Incorrect mixed-size incoming argument or result layout");
        size_t returns = 0;
        for (const auto& block : choose.blocks)
            for (const auto& instruction : block)
                if (instruction.op == CPMIROp::Return && instruction.output == choose.result)
                    ++returns;
        require(returns == 2, "Both branches must write the caller's result slot");

        const auto& strings = findFunction(program, "strings");
        vector<size_t> characterOffsets;
        for (const auto& slot : strings.slots)
            if (slot.name == "first characters" || slot.name == "second characters")
            {
                require(slot.storage == CPMIRStorage::Frame && slot.size == 4,
                        "String buffer must reserve space for its terminator");
                characterOffsets.push_back(slot.offset);
            }
        require(characterOffsets.size() == 2 && characterOffsets[0] + 4 <= characterOffsets[1],
                "Local string initializers must reserve distinct buffers");
    });
}

static void runAssembly(Fixture& fixture, const string& source)
{
    fixture.source(source);
    const auto root = fixture.directory.string() + "/";
    CPMCompiler compiler(root, "input.cpm", "", {}, CPMCompileMode::Assembly);
    require(compiler.NoErrors(), compiler.CompilerLog()->GetText());
    require(compiler.AssemblyOutput().find("offset 0x0200\nmain cpm_start\n") == 0,
            "Missing assembly entry stub");
    std::ofstream(fixture.directory / "program.falc") << compiler.AssemblyOutput();

    std::ostringstream assemblyLog;
    const auto binary = fixture.directory / "output.bin";
    FridgeAssemblyLanguageCompiler assembler(root, "program.falc", binary.string(), {}, &assemblyLog);
    require(fs::exists(binary), assemblyLog.str());
    std::ifstream image(binary, std::ios::binary);
    const vector<FRIDGE_WORD> bytes(std::istreambuf_iterator<char>(image), {});
    require(!bytes.empty(), "FALC produced an empty image");

    auto cpu = std::make_unique<FRIDGE_CPU>();
    FRIDGE_cpu_reset(cpu.get());
    const auto initialSP = cpu->SP;
    cpu->PC = FRIDGE_EXECUTABLE_OFFSET;
    std::copy(bytes.begin(), bytes.end(), cpu->ram + cpu->PC);
    FRIDGE_SYSTEM system{};
    system.cpu = cpu.get();
    for (int steps = 0; steps < 30000 && cpu->PC != 0 && cpu->state == FRIDGE_CPU_ACTIVE; ++steps)
        FRIDGE_sys_tick(&system);
    require(cpu->PC == 0 && cpu->state == FRIDGE_CPU_ACTIVE && !FRIDGE_cpu_flag_PANIC(cpu.get()),
            "Generated program did not return to the loader");
    require(cpu->SP == initialSP, "Calls or control flow did not restore SP");

    const vector<FRIDGE_WORD> expected{
        0x3C, 0x78, 5, 'b', 'c', 'H', 3, 10, 0, 1,
        0x01, 0x00, 0x02, 0x12, 0xFF, 0xFD, 42, 21, 10, 1, 2, 6, 3, 42
    };
    for (size_t i = 0; i < expected.size(); ++i)
        require(cpu->ram[0x0100 + i] == expected[i],
                "Incorrect generated result at byte " + to_string(i) + " (got " +
                to_string(cpu->ram[0x0100 + i]) + ", expected " + to_string(expected[i]) + ")");
}

static void runLinkedHello(Fixture& fixture)
{
    const auto librarySource = fs::path(CPM_VTEXT_LIBRARY);
    const auto root = fixture.directory.string() + "/";
    const auto package = fixture.directory / "library files";
    fs::create_directory(package);
    const auto binary = package / "vtext.bin";
    std::ostringstream libraryLog;
    FridgeAssemblyLanguageCompiler library(librarySource.parent_path().string() + "/",
        librarySource.filename().string(), binary.string(), {}, &libraryLog, false, false, true);
    require(library.succeeded(), libraryLog.str());
    require(library.getSubroutines().at("mul_dc") == 0 && fs::exists(binary.string() + ".rel"),
            "A relocatable library should start with its first instruction and emit relocation metadata");
    std::ifstream header(package / "vtext.cpm");
    string declaration;
    require(static_cast<bool>(getline(header, declaration)) && declaration == "namespace vtext imports \"vtext.bin\"",
            "A generated relocatable header should use the default import syntax");

    std::ofstream(package / "palette.falc") << R"(
        @static uint8
        static COLOR 0
        @static uint16
        static IMAGE_END 0x0000
        @void()
        subroutine select_color
            MVI A, 1
            STA COLOR
            LXI HL, HEAP_ORIGIN
            SHLD IMAGE_END
        endsub
    )";
    const auto packageRoot = package.string() + "/";
    std::ostringstream paletteLog;
    FridgeAssemblyLanguageCompiler palette(packageRoot, "palette.falc", (package / "palette.bin").string(),
        {}, &paletteLog, false, false, true);
    require(palette.succeeded(), paletteLog.str());

    std::ofstream(package / "fixed.falc") << R"(
        offset 0x1000
        @void()
        subroutine marker
            MVI A, 0xA5
            STA 0x0103
        endsub
    )";
    std::ostringstream fixedLog;
    FridgeAssemblyLanguageCompiler fixed(packageRoot, "fixed.falc", (package / "fixed.bin").string(),
        {}, &fixedLog, false, false, true);
    require(fixed.succeeded(), fixedLog.str());
    require(fixed.getMainEntry() == 0x1000 && fixed.getSubroutines().at("marker") == 0x1000 &&
            fixed.getProgramSize() == 6 && fixed.getObjectCode()[0] == MVI_A,
            "A nonzero origin without main must not generate a startup jump");

    std::ifstream helloSource(CPM_HELLO_SOURCE);
    string include;
    require(static_cast<bool>(getline(helloSource, include)) && include == "include \"vtext.cpm\";",
            "Expected Hello World to include the generated header");
    string hello(std::istreambuf_iterator<char>(helloSource), {});
    const string colorAssignment = "= VTEXT_BACK_COLOR, 1;";
    const auto colorLine = hello.find(colorAssignment);
    require(colorLine != string::npos, "Expected Hello World to set its text color");
    hello.replace(colorLine, colorAssignment.size(), R"(
        palette.select_color;
        = VTEXT_BACK_COLOR, palette.COLOR;
        = image_end, palette.IMAGE_END;
        fixed.marker;
    )");
    fixture.source(R"(
        include "library files/vtext.cpm";
        include "library files/palette.cpm";
        include "library files/fixed.cpm";
        static uint16 image_end imports 0x0101;
    )" + hello);
    CPMCompiler compiler(root, "input.cpm", "", {}, CPMCompileMode::Assembly);
    require(compiler.NoErrors(), compiler.CompilerLog()->GetText());
    std::ofstream(fixture.directory / "hello.falc") << compiler.AssemblyOutput();
    std::ostringstream assemblyLog;
    FridgeAssemblyLanguageCompiler program(root, "hello.falc", "", {}, &assemblyLog);
    require(program.succeeded(), assemblyLog.str());
    require(program.getOffset() == FRIDGE_EXECUTABLE_OFFSET, "Incorrect executable load address");
    const auto fixedOffset = fixed.getOffset() - program.getOffset();
    require(program.getProgramSize() == fixedOffset + fixed.getProgramSize() &&
            std::equal(fixed.getObjectCode(), fixed.getObjectCode() + fixed.getProgramSize(),
                       program.getObjectCode() + fixedOffset), "Relocatable libraries should fit before the fixed binary");

    auto cpu = std::make_unique<FRIDGE_CPU>();
    auto gpu = std::make_unique<FRIDGE_GPU>();
    FRIDGE_cpu_reset(cpu.get());
    FRIDGE_gpu_reset(gpu.get());
    gpu->vmode = FRIDGE_VIDEO_EGA;
    const auto initialSP = cpu->SP;
    cpu->PC = program.getOffset();
    std::copy_n(program.getObjectCode(), program.getProgramSize(), cpu->ram + cpu->PC);
    FRIDGE_SYSTEM system{};
    system.cpu = cpu.get();
    system.gpu = gpu.get();
    for (int steps = 0; steps < 30000 && cpu->PC != 0 && cpu->state == FRIDGE_CPU_ACTIVE; ++steps)
        FRIDGE_sys_tick(&system);
    require(cpu->PC == 0 && cpu->SP == initialSP && !FRIDGE_cpu_flag_PANIC(cpu.get()),
            "Linked program did not return to the loader with a balanced stack");
    require(cpu->ram[0x0103] == 0xA5 &&
            FRIDGE_DWORD_HL(cpu->ram[0x0101], cpu->ram[0x0102]) == program.getOffset() + program.getProgramSize(),
            "The fixed entry point or relocated heap address is incorrect");
    require(gpu->vmode == FRIDGE_VIDEO_TEXT, "Hello World did not switch to text mode");
    const auto* frame = FRIDGE_gpu_visible_frame(gpu.get());
    const size_t first = (9 * FRIDGE_GPU_TEXT_FRAME_WIDTH + 1) * 2;
    const string expected = "Hello, World!";
    for (size_t i = 0; i < expected.size(); ++i)
        require(frame[first + i * 2] == expected[i] && frame[first + i * 2 + 1] == 0x1F,
                "Incorrect text or color in visible cell " + to_string(i));
    require(frame[first - 2] == 0 && frame[first + expected.size() * 2] == 0,
            "The string write affected a neighboring text cell");

    std::ofstream(fixture.directory / "overlap.falc") << compiler.AssemblyOutput()
        << "incbin \"library files/fixed.bin\" at 0x1001\n";
    std::ostringstream overlapLog;
    FridgeAssemblyLanguageCompiler overlap(root, "overlap.falc", "", {}, &overlapLog);
    require(!overlap.succeeded() && overlapLog.str().find("overlaps another binary section") != string::npos,
            "Overlapping fixed binaries were accepted");

    std::ofstream(binary.string() + ".rel", std::ios::app) << "base 0xFFFF\n";
    std::ostringstream corruptLog;
    FridgeAssemblyLanguageCompiler corrupt(root, "hello.falc", "", {}, &corruptLog);
    require(!corrupt.succeeded() && corruptLog.str().find("Invalid relocation record") != string::npos,
            "An out-of-bounds relocation record was accepted");

    fs::remove(binary);
    CPMCompiler missing(root, "input.cpm", "", {}, CPMCompileMode::Assembly);
    require(!missing.NoErrors() && missing.CompilerLog()->GetText().find("Cannot find imported binary") != string::npos,
            "A missing imported binary was accepted");
}

static void runMainReturnModes(Fixture& fixture)
{
    fixture.source(R"(
        static uint8 observed imports 0x0100;
        const string message "OK";
        void helper() (= observed, (cast uint8, (at message)););
        void main() (helper; ++ observed;);
    )");
    constexpr FRIDGE_RAM_ADDR origin = 0x0800;
    constexpr FRIDGE_RAM_ADDR caller = 0x0110;
    const auto root = fixture.directory.string() + "/";
    for (auto mode : {CPMMainReturn::None, CPMMainReturn::Bootloader, CPMMainReturn::Halt})
    {
        CPMCompiler compiler(root, "input.cpm", "", {}, CPMCompileMode::Assembly, {origin, mode});
        require(compiler.NoErrors(), compiler.CompilerLog()->GetText());
        std::istringstream source(compiler.AssemblyOutput());
        std::ostringstream log;
        FridgeAssemblyLanguageCompiler assembler(source, root, "program.falc", "", &log);
        require(assembler.succeeded() && assembler.getOffset() == origin, log.str());

        auto cpu = std::make_unique<FRIDGE_CPU>();
        FRIDGE_cpu_reset(cpu.get());
        const auto initialSP = cpu->SP;
        std::copy_n(assembler.getObjectCode(), assembler.getProgramSize(), cpu->ram + origin);
        cpu->PC = origin;
        FRIDGE_RAM_ADDR returnPC = 0;
        if (mode == CPMMainReturn::None)
        {
            cpu->ram[caller] = CALL;
            cpu->ram[caller + 1] = FRIDGE_HIGH_WORD(origin);
            cpu->ram[caller + 2] = FRIDGE_LOW_WORD(origin);
            cpu->PC = caller;
            returnPC = caller + 3;
        }
        FRIDGE_SYSTEM system{};
        system.cpu = cpu.get();
        for (int steps = 0; steps < 30000 && cpu->PC != returnPC && cpu->state == FRIDGE_CPU_ACTIVE; ++steps)
            FRIDGE_sys_tick(&system);
        require(cpu->SP == initialSP && !FRIDGE_cpu_flag_PANIC(cpu.get()) && cpu->ram[0x0100] == 'P',
                "main failed to restore the stack or use its relocated data");
        if (mode == CPMMainReturn::Halt)
            require(cpu->state == FRIDGE_CPU_HALTED && cpu->PC != 0, "main did not halt");
        else
            require(cpu->state == FRIDGE_CPU_ACTIVE && cpu->PC == returnPC, "main returned to the wrong address");
    }
}

int main()
{
    try
    {
        Fixture fixture;
        inspectConstantInitializers(fixture);
        runLinkedHello(fixture);
        runMainReturnModes(fixture);
        const string source = R"(
            static <uint8 array 24> output imports 0x0100;
            const string greeting "Hello";
            static uint8 touches;

            uint8 choose(uint8 first; uint16 second)
            (
                uint8 chosen first;
                if (== first, 0) return (convert uint8, second);
                else return chosen;
            );
            void strings()
            (
                string first "cat";
                string second "cat";
                string alias first;
                = (at alias), 'b';
                = (at output, 3), (cast uint8, (at first));
                = (at output, 4), (cast uint8, (at second));
                = (at output, 5), (cast uint8, (at greeting));
            );
            uint8 tally()
            (
                uint8 sum 0;
                for (uint8 i 0; < i, 5; ++ i)
                (
                    if (== i, 1) (continue);
                    if (== i, 4) (break);
                    += sum, i;
                );
                return sum;
            );
            uint8 recurse(uint8 n)
            (
                if (== n, 0) return 0;
                return (+ n, (recurse (- n, 1)));
            );
            uint16 add_words(uint16 a; uint16 b) (return (+ a, b););
            int16 extend(int8 a) (return (convert int16, a););
            bool touch() (++ touches; return true;);
            uint8 earlier(uint8 before; uint8 after) (return before;);
            uint8 persistent(uint8 n) (static uint8 cache n; ++ cache; return cache;);
            uint8 early_exit(uint8 n)
            (
                for (; true; *= n, n) (return n;);
            );
            <uint8 array 3> packet(uint8 tag) (<uint8 array 3> bytes (tag; 20; 10); return bytes;);

            void main()
            (
                = (at output, 0), (choose 0, 0xA53C);
                = (at output, 1), (choose 0x78, 0x1234);
                = (at output, 2), (tally);
                (strings);
                <uint8 array 4> bytes (1; 2; 3; 4);
                <<uint8 array 3> ptr> start (cast <<uint8 array 3> ptr>, (addr (at bytes, 0)));
                <<uint8 array 3> ptr> next (cast <<uint8 array 3> ptr>, (addr (at bytes, 1)));
                = (at next), (at start);
                = (at output, 6), (at bytes, 3);
                = (at output, 7), (recurse 4);
                bool flag false;
                if (&& flag, (touch)) (= (at output, 8), 255);
                = (at output, 8), touches;
                if (|| flag, (touch)) (= (at output, 9), touches);
                = (at (cast <uint16 ptr>, (addr (at output, 10)))), (add_words 0x00FF, 0x0001);
                = (at (cast <uint16 ptr>, (addr (at output, 12)))), (add_words 0x0102, 0x0110);
                = (at (cast <int16 ptr>, (addr (at output, 14)))), (extend -3);
                uint8 snapshot 42;
                = (at output, 16), (earlier snapshot, (++ snapshot));
                <<uint8 array 3> array 2> packets ((packet 11); (packet 21));
                uint16 packet_index 1d;
                = (at output, 17), (at (at packets, packet_index), 0);
                = (at output, 18), (at (at packets, packet_index), 2);
                = (at output, 19), (at bytes, 1);
                = (at output, 20), (at bytes, 2);
                = (at output, 21), (persistent 5);
                = (at output, 22), (persistent 2);
                = (at output, 23), (early_exit 42);
            );
        )";
        inspectIR(fixture, source);
        runAssembly(fixture, source);

        fixture.source("uint8 product(uint8 a; uint8 b) (return (* a, b);); void main() ();");
        CPMCompiler unsupported(fixture.directory.string() + "/", "input.cpm", "", {}, CPMCompileMode::Assembly);
        require(!unsupported.NoErrors() && unsupported.AssemblyOutput().empty() &&
                unsupported.CompilerLog()->GetText().find("runtime '*'") != string::npos,
                "Unsupported runtime operation must fail without printing partial assembly");

        std::cout << "CPM IR and assembly tests passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
