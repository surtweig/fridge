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

int main()
{
    try
    {
        Fixture fixture;
        inspectConstantInitializers(fixture);
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
