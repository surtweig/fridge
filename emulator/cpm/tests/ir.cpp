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
        0x01, 0x00, 0x02, 0x12, 0xFF, 0xFD, 42, 21, 10, 1, 2, 6, 3, 42,
        0, 3, 0xFF, '\r', '\n', 0xFF, 3, '\n', 0x80
    };
    for (size_t i = 0; i < expected.size(); ++i)
        require(cpu->ram[0x0100 + i] == expected[i],
                "Incorrect generated result at byte " + to_string(i) + " (got " +
                to_string(cpu->ram[0x0100 + i]) + ", expected " + to_string(expected[i]) + ")");
}

static void runVtextClear(Fixture& fixture)
{
    std::ofstream(fixture.directory / "clear.cpm") << R"(
        include "library files/vtext.cpm";
        using vtext;
        void main()
        (
            vtext_begin;
            vtext_clear 4, 3, 2;
            vtext_clear (- VTEXT_COLUMNS, 1), 9, 2;
            vtext_clear (- VTEXT_COLUMNS, 2), (- VTEXT_ROWS, 1), 255;
            vtext_clear 0, 0, 0;
            vtext_clear VTEXT_COLUMNS, 0, 1;
            vtext_clear 0, VTEXT_ROWS, 1;
            vtext_present;
        );
    )";
    const auto root = fixture.directory.string() + "/";
    CPMCompiler compiler(root, "clear.cpm", "", {}, CPMCompileMode::Assembly, {0x0800, CPMMainReturn::Halt});
    require(compiler.NoErrors(), compiler.CompilerLog()->GetText());
    std::istringstream assembly(compiler.AssemblyOutput());
    std::ostringstream log;
    FridgeAssemblyLanguageCompiler program(assembly, root, "clear.falc", "", &log);
    require(program.succeeded(), log.str());

    auto cpu = std::make_unique<FRIDGE_CPU>();
    auto gpu = std::make_unique<FRIDGE_GPU>();
    FRIDGE_cpu_reset(cpu.get());
    FRIDGE_gpu_reset(gpu.get());
    std::fill_n(FRIDGE_gpu_active_frame(gpu.get()), FRIDGE_GPU_FRAME_BUFFER_SIZE, 0xA5);
    const auto sp = cpu->SP;
    cpu->PC = program.getOffset();
    std::copy_n(program.getObjectCode(), program.getProgramSize(), cpu->ram + cpu->PC);
    FRIDGE_SYSTEM system{};
    system.cpu = cpu.get();
    system.gpu = gpu.get();
    for (int step = 0; step < 10000 && cpu->state == FRIDGE_CPU_ACTIVE; ++step) FRIDGE_sys_tick(&system);
    require(cpu->state == FRIDGE_CPU_HALTED && !FRIDGE_cpu_flag_PANIC(cpu.get()) && cpu->SP == sp,
            "vtext_clear did not complete with a balanced stack");

    constexpr size_t columns = FRIDGE_GPU_TEXT_FRAME_WIDTH;
    constexpr size_t rows = FRIDGE_GPU_TEXT_FRAME_HEIGHT;
    vector<FRIDGE_WORD> expected(FRIDGE_GPU_FRAME_BUFFER_SIZE, 0xA5);
    for (auto first : {3 * columns + 4, 10 * columns - 1, columns * rows - 2})
        std::fill_n(expected.begin() + first * 2, 4, 0);
    require(std::equal(expected.begin(), expected.end(), FRIDGE_gpu_visible_frame(gpu.get())),
            "vtext_clear changed the wrong framebuffer bytes");
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
        = VTEXT_BACK_COLOR, (palette.select_default_color);
        = image_end, palette.IMAGE_END;
        fixed.marker;
    )");
    fixture.source(R"(
        include "library files/vtext.cpm";
        namespace palette
        (
            uint8 select_default_color() (select_color; return COLOR;);
        );
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
    const size_t first = 0;
    const string expected = "Hello, World!";
    for (size_t i = 0; i < expected.size(); ++i)
        require(frame[first + i * 2] == expected[i] && frame[first + i * 2 + 1] == 0x1F,
                "Incorrect text or color in visible cell " + to_string(i));
    require(frame[first + expected.size() * 2] == 0,
            "The string write affected a neighboring text cell");

    runVtextClear(fixture);

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

static void runKeyboardVectorRelocations(Fixture& fixture)
{
    const auto root = fixture.directory.string() + "/";
    std::ofstream(fixture.directory / "vectors.falc") << R"(
        @void()
        subroutine init_vectors
            LXI HL, init_vectors_$ADDR
            SHLD init_address_$ADDR
            LXI HL, on_press_$ADDR
            SHLD 0x0008
            LHLD release_address_$ADDR
            SHLD 0x000B
            MVI A, #JMP
            STA FRIDGE_IRQ_KEYBOARD
            LXI BC, RELEASE_ADDRESS_SIZE
        endsub

        subroutine keyboard_handlers
            entry on_press
                MVI A, 1
                STA 0x0100
                RET
            entry on_release
                MVI A, 2
                STA 0x0100
        endsub

        static init_address 0x0000
        static release_address RELEASE_HANDLER
        alias RELEASE_HANDLER on_release_$ADDR
        alias RELEASE_ADDRESS_SIZE release_address_$SIZE
    )";
    std::ostringstream libraryLog;
    FridgeAssemblyLanguageCompiler library(root, "vectors.falc", root + "vectors.bin", {}, &libraryLog, false, false, true);
    require(library.succeeded(), libraryLog.str());
    std::istringstream program(R"(
        offset 0x0800
        incbin "vectors.bin" as vectors
        main start
            CALL vectors+0x0000
            HLT
    )");
    std::ostringstream log;
    FridgeAssemblyLanguageCompiler linked(program, root, "program.falc", "", &log);
    require(linked.succeeded(), log.str());

    auto cpu = std::make_unique<FRIDGE_CPU>();
    FRIDGE_cpu_reset(cpu.get());
    const auto initialSP = cpu->SP;
    cpu->PC = linked.getOffset();
    std::copy_n(linked.getObjectCode(), linked.getProgramSize(), cpu->ram + cpu->PC);
    FRIDGE_SYSTEM system{};
    system.cpu = cpu.get();
    for (int steps = 0; steps < 100 && cpu->state == FRIDGE_CPU_ACTIVE; ++steps) FRIDGE_sys_tick(&system);
    const auto base = linked.getOffset() + linked.getProgramSize() - library.getProgramSize();
    const auto initAddress = base + library.getResources().at("init_address").address;
    require(cpu->state == FRIDGE_CPU_HALTED && !FRIDGE_cpu_flag_PANIC(cpu.get()) && cpu->SP == initialSP &&
            cpu->rB == 0 && cpu->rC == 2 && (FRIDGE_DWORD_HL(cpu->ram[initAddress], cpu->ram[initAddress + 1])) == base,
            "Driver initialization did not use the relocated static and subroutine addresses");

    const auto resume = cpu->PC;
    const auto release = base + library.getEntries().at("on_release");
    require((FRIDGE_DWORD_HL(cpu->ram[0x000B], cpu->ram[0x000C])) == release,
            "The static entry initializer did not relocate");
    cpu->inte = FRIDGE_CPU_INTERRUPTS_ENABLED;
    FRIDGE_cpu_interrupt(&system, CALL, FRIDGE_DWORD_TO_WORDS(FRIDGE_IRQ_KEYBOARD));
    require(cpu->PC == FRIDGE_IRQ_KEYBOARD && cpu->ram[FRIDGE_IRQ_KEYBOARD] == JMP, "Interrupt did not call its vector slot");
    for (int steps = 0; steps < 10 && cpu->PC != resume && cpu->state == FRIDGE_CPU_ACTIVE; ++steps)
        FRIDGE_sys_tick(&system);
    require(cpu->PC == resume && cpu->SP == initialSP && cpu->ram[0x0100] == 1,
            "The vector did not jump to its relocated handler and return to the interrupted code");

    std::istringstream invalid("LXI HL, missing_$ADDR\n");
    std::ostringstream invalidLog;
    FridgeAssemblyLanguageCompiler rejected(invalid, root, "invalid.falc", "", &invalidLog);
    require(!rejected.succeeded() && invalidLog.str().find("Unknown address alias 'missing_$ADDR'") != string::npos,
            "An undefined address alias should fail assembly");
}

static void runKeyboardDriver(Fixture& fixture)
{
    const auto source = fs::path(CPM_KEYBOARD_LIBRARY);
    const auto root = fixture.directory.string() + "/";
    std::ostringstream libraryLog;
    FridgeAssemblyLanguageCompiler library(source.parent_path().string() + "/", source.filename().string(),
        root + "kbrd.bin", {}, &libraryLog, false, false, true);
    require(library.succeeded(), libraryLog.str());

    fixture.source(R"(
        include "kbrd.cpm";
        using kbrd;
        static uint8 presses imports 0x0100;
        static uint8 releases imports 0x0101;
        static uint8 press_code imports 0x0102;
        static uint8 release_code imports 0x0103;
        static uint8 press_state imports 0x0104;
        static uint8 release_state imports 0x0105;
        void press()
        (
            ++ presses;
            = press_code, KBRD0_KEY_CODE;
            = press_state, KBRD0_KEY_STATE;
        );
        void release()
        (
            ++ releases;
            = release_code, KBRD0_KEY_CODE;
            = release_state, KBRD0_KEY_STATE;
        );
        void main()
        (
            = KBRD0_PRESS_HANDLER, (addr press);
            = KBRD0_RELEASE_HANDLER, (addr release);
            kbrd_begin;
        );
    )");
    CPMCompiler compiler(root, "input.cpm", "", {}, CPMCompileMode::Assembly, {0x0800, CPMMainReturn::Halt});
    require(compiler.NoErrors(), compiler.CompilerLog()->GetText());
    std::istringstream assembly(compiler.AssemblyOutput());
    std::ostringstream log;
    FridgeAssemblyLanguageCompiler program(assembly, root, "keyboard.falc", "", &log);
    require(program.succeeded(), log.str());

    auto cpu = std::make_unique<FRIDGE_CPU>();
    FRIDGE_cpu_reset(cpu.get());
    FRIDGE_KEYBOARD_CONTROLLER keyboard{};
    FRIDGE_SYSTEM system{};
    system.cpu = cpu.get();
    system.kbrd = &keyboard;
    const auto sp = cpu->SP;
    cpu->PC = program.getOffset();
    std::copy_n(program.getObjectCode(), program.getProgramSize(), cpu->ram + cpu->PC);
    for (int step = 0; step < 1000 && cpu->state == FRIDGE_CPU_ACTIVE; ++step) FRIDGE_sys_tick(&system);
    require(cpu->state == FRIDGE_CPU_HALTED && cpu->ram[FRIDGE_IRQ_KEYBOARD] == JMP && cpu->SP == sp,
            "Keyboard initialization failed to install its vector");

    const auto resume = cpu->PC;
    cpu->rA = 0x12; cpu->rF = 0xC5;
    cpu->rB = 0x34; cpu->rC = 0x56;
    cpu->rD = 0x78; cpu->rE = 0x9A;
    cpu->rH = 0xBC; cpu->rL = 0xDE;
    for (int pair = 1; pair <= FRIDGE_KEYBOARD_BUFFER_SIZE; ++pair)
    {
        FRIDGE_keyboard_press(&system, static_cast<FRIDGE_WORD>(0x80 | pair));
        require(cpu->PC == FRIDGE_IRQ_KEYBOARD && cpu->inte == FRIDGE_CPU_INTERRUPTS_DISABLED,
                "Press did not interrupt through the shared vector");
        FRIDGE_keyboard_release(&system, static_cast<FRIDGE_WORD>(pair + 1));
        require(cpu->PC == FRIDGE_IRQ_KEYBOARD && keyboard.count == 2 && keyboard.pending_interrupts == 1,
                "Release was not queued while interrupts were masked");
        for (int step = 0; step < 1000 && (cpu->PC != resume || cpu->SP != sp || keyboard.count != 0); ++step)
            FRIDGE_sys_tick(&system);
        require(cpu->PC == resume && cpu->SP == sp && !FRIDGE_cpu_flag_PANIC(cpu.get()) &&
                keyboard.count == 0 && keyboard.pending_interrupts == 0,
                "Keyboard events did not restore PC and SP on pair " + to_string(pair) +
                " (PC " + to_string(cpu->PC) + ", expected " + to_string(resume) +
                ", SP " + to_string(cpu->SP) + ", queued " + to_string(keyboard.count) + ")");
        require(cpu->rA == 0x12 && cpu->rF == 0xC5 && cpu->rB == 0x34 && cpu->rC == 0x56 &&
                cpu->rD == 0x78 && cpu->rE == 0x9A && cpu->rH == 0xBC && cpu->rL == 0xDE,
                "A callback changed the interrupted registers");
        require(cpu->ram[0x0100] == pair && cpu->ram[0x0101] == pair &&
                cpu->ram[0x0102] == pair && cpu->ram[0x0103] == pair + 1 &&
                cpu->ram[0x0104] == 1 && cpu->ram[0x0105] == 0,
                "Callbacks observed the wrong key code or state");
    }

    cpu->inte = FRIDGE_CPU_INTERRUPTS_DISABLED;
    for (int i = 0; i <= FRIDGE_KEYBOARD_BUFFER_SIZE; ++i) FRIDGE_keyboard_release(&system, i);
    for (int i = 0; i < FRIDGE_KEYBOARD_BUFFER_SIZE; ++i)
        require(cpu->input_dev[FRIDGE_DEV_KEYBOARD_ID](&system) == i, "Keyboard polling read events out of order");
    require(keyboard.count == 0 && keyboard.pending_interrupts == 0 && cpu->PC == resume,
            "Polling did not clear the interrupt requests for events it consumed");
}

static void runStringFormatting(Fixture& fixture)
{
    const auto source = fs::path(CPM_STRING_LIBRARY);
    const auto root = fixture.directory.string() + "/";
    std::ostringstream libraryLog;
    FridgeAssemblyLanguageCompiler library(source.parent_path().string() + "/", source.filename().string(),
        root + "string.bin", {}, &libraryLog, false, false, true);
    require(library.succeeded(), libraryLog.str());

    fixture.source(R"(
        include "string.cpm";
        using string_lib;
        static <uint8 array 53> output imports 0x0100;
        void main()
        (
            string_ddec (cast string, (addr (at output, 0))), 0x0000;
            string_ddec (cast string, (addr (at output, 7))), 0x000A;
            string_ddec (cast string, (addr (at output, 14))), 0x03E9;
            string_ddec (cast string, (addr (at output, 21))), 0x8000;
            string_ddec (cast string, (addr (at output, 28))), 0xFFFF;
            string_dec (cast string, (addr (at output, 35))), 255;
            string_dhex (cast string, (addr (at output, 40))), 0x09AF;
            string_hex (cast string, (addr (at output, 45))), 0x0B;
            = (at output, 48), (string_hex_digit 0);
            = (at output, 49), (string_hex_digit 15);
            = (at output, 50), (string_hex_digit 16);
            = (at output, 51), (string_hex_digit 255);
        );
    )");
    CPMCompiler compiler(root, "input.cpm", "", {}, CPMCompileMode::Assembly, {0x0800, CPMMainReturn::Halt});
    require(compiler.NoErrors(), compiler.CompilerLog()->GetText());
    std::istringstream assembly(compiler.AssemblyOutput());
    std::ostringstream log;
    FridgeAssemblyLanguageCompiler program(assembly, root, "format.falc", "", &log);
    require(program.succeeded(), log.str());

    auto cpu = std::make_unique<FRIDGE_CPU>();
    FRIDGE_cpu_reset(cpu.get());
    FRIDGE_SYSTEM system{};
    system.cpu = cpu.get();
    const auto sp = cpu->SP;
    cpu->PC = program.getOffset();
    std::copy_n(program.getObjectCode(), program.getProgramSize(), cpu->ram + cpu->PC);
    std::fill_n(cpu->ram + 0x0100, 53, 0xCC);
    const char placeholder[] = "RPM=____!";
    std::copy_n(placeholder, sizeof(placeholder), cpu->ram + 0x010A);
    for (int step = 0; step < 20000 && cpu->state == FRIDGE_CPU_ACTIVE; ++step) FRIDGE_sys_tick(&system);
    require(cpu->state == FRIDGE_CPU_HALTED && !FRIDGE_cpu_flag_PANIC(cpu.get()) && cpu->SP == sp,
            "String formatters did not finish with a balanced stack");

    const vector<pair<size_t, string>> expected{
        {0, "0"}, {7, "10"}, {21, "32768"}, {28, "65535"}, {35, "255"},
        {40, "09AF"}, {45, "0B"}, {48, "0F  "}
    };
    for (const auto& item : expected)
    {
        const auto* buffer = cpu->ram + 0x0100 + item.first;
        require(std::equal(item.second.begin(), item.second.end(), buffer) && buffer[item.second.size()] == 0xCC,
                "Unexpected string or write past the buffer at offset " + to_string(item.first));
    }
    const char filled[] = "RPM=1001!";
    require(std::equal(filled, filled + sizeof(filled), cpu->ram + 0x010A),
            "Decimal formatting changed the text or the terminator outside the placeholder");
}

static void runFunctionAddresses(Fixture& fixture)
{
    const auto root = fixture.directory.string() + "/";
    std::ofstream(fixture.directory / "callbacks.falc") << R"(
        @void( (uint16 callback : HL) )
        subroutine invoke
            CALL invoke_hl
            LDA 0x0101
            INR A
            STA 0x0101
        endsub

        subroutine invoke_hl
            PCHL
        endsub
    )";
    std::ostringstream libraryLog;
    FridgeAssemblyLanguageCompiler library(root, "callbacks.falc", root + "callbacks.bin",
        {}, &libraryLog, false, false, true);
    require(library.succeeded(), libraryLog.str());

    fixture.source(R"(
        include "callbacks.cpm";
        using Handlers;
        static uint8 hits imports 0x0100;
        const uint16 saved (addr Handlers.add_two);
        const uint16 copied saved;
        namespace Handlers
        (
            void add_two();
            void add_one(uint8 count) (+= hits, count;);
            void add_one() (uint8 count 1; += hits, count;);
            void add_two() (+= hits, 2;);
        );
        void main()
        (
            callbacks.invoke (addr add_one);
            callbacks.invoke copied;
        );
    )");
    CPMCompileOptions options;
    options.offset = 0x0800;
    CPMCompiler compiler(root, "input.cpm", "", {}, CPMCompileMode::Assembly, options);
    require(compiler.NoErrors(), compiler.CompilerLog()->GetText());
    std::istringstream assembly(compiler.AssemblyOutput());
    std::ostringstream assemblyLog;
    FridgeAssemblyLanguageCompiler program(assembly, root, "callbacks-program.falc", "", &assemblyLog);
    require(program.succeeded(), assemblyLog.str());

    auto cpu = std::make_unique<FRIDGE_CPU>();
    FRIDGE_cpu_reset(cpu.get());
    const auto initialSP = cpu->SP;
    cpu->PC = program.getOffset();
    std::copy_n(program.getObjectCode(), program.getProgramSize(), cpu->ram + cpu->PC);
    FRIDGE_SYSTEM system{};
    system.cpu = cpu.get();
    for (int steps = 0; steps < 30000 && cpu->PC != 0 && cpu->state == FRIDGE_CPU_ACTIVE; ++steps)
        FRIDGE_sys_tick(&system);
    require(cpu->PC == 0 && cpu->state == FRIDGE_CPU_ACTIVE && !FRIDGE_cpu_flag_PANIC(cpu.get()) &&
            cpu->SP == initialSP, "FALC callback calls did not return with the stack restored");
    require(cpu->ram[0x0100] == 3 && cpu->ram[0x0101] == 2,
            "FALC did not invoke both CPM entry addresses and resume after each callback");
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
        runKeyboardVectorRelocations(fixture);
        runKeyboardDriver(fixture);
        runStringFormatting(fixture);
        runFunctionAddresses(fixture);
        runMainReturnModes(fixture);
        const auto assets = fixture.directory / "assets";
        fs::create_directory(assets);
        std::ofstream(assets / "test data.bin", std::ios::binary).write("\x00\x03\xFF\r\n", 5);
        std::ofstream(assets / "data.cpm") << R"(
            const uint8 asset_size 5;
            const <uint8 array asset_size> asset imports "test data.bin";
            static <uint8 array asset_size> writable_asset imports "test data.bin";
            uint8 local_asset_byte()
            (
                static <uint8 array asset_size> local_asset imports "test data.bin";
                uint8 value (at local_asset, 4);
                = (at local_asset, 4), 1;
                return value;
            );
        )";
        const string source = R"(
            include "assets/data.cpm";
            static <uint8 array 33> output imports 0x0100;
            const string greeting "Hello";
            static uint8 touches;
            static uint8 asset_index 2;
            const uint8 asset_count (at asset, 1);

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
                = (at (cast <<uint8 array asset_size> ptr>, (addr (at output, 24)))), writable_asset;
                = (at output, 29), (at asset, asset_index);
                = (at output, 30), asset_count;
                local_asset_byte;
                = (at output, 31), (local_asset_byte);
                const <uint8 array asset_size> local_asset imports "assets/test data.bin";
                = (at writable_asset, 0), 0x80;
                = (at output, 32), (| (at local_asset, 0), (at writable_asset, 0));
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
