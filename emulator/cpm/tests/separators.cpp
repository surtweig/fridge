#include "CPMCompiler.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace CPM;
namespace fs = std::filesystem;

static void require(bool value, const string& message)
{
    if (!value) throw std::runtime_error(message);
}

struct Fixture
{
    fs::path directory = fs::temp_directory_path() /
        ("cpm-separators-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    Fixture() { fs::create_directories(directory); }
    ~Fixture() { fs::remove_all(directory); }

    void source(const string& text)
    {
        std::ofstream(directory / "input.cpm") << text;
        fs::remove(directory / "output.bin");
    }

    void parse(const string& text, bool valid,
               const std::function<void(const vector<CPMSyntaxTreeNode*>&)>& inspect = {})
    {
        source(text);
        Logger logger;
        CPMParser parser(directory.string() + "/", "input.cpm", &logger);
        require(parser.NoErrors() == valid, "Unexpected parser result for:\n" + text + logger.GetText());
        if (!valid) require(logger.GetText().find("[ERROR]") != string::npos, "Missing syntax diagnostic");
        vector<CPMSyntaxTreeNode*> nodes;
        parser.GetLayer(nodes);
        if (inspect) inspect(nodes);
    }

    void compile(const string& text, bool valid,
                 const std::function<void(CPMCompiler&)>& inspect = {})
    {
        source(text);
        CPMCompiler compiler(directory.string() + "/", "input.cpm",
                             (directory / "output.bin").string(), {});
        require(compiler.NoErrors() == valid, "Unexpected compiler result for:\n" + text + compiler.CompilerLog()->GetText());
        require(fs::exists(directory / "output.bin") == valid, "Incorrect binary emission after compilation");
        if (inspect) inspect(compiler);
    }
};

int main()
{
    try
    {
        Fixture fixture;
        fixture.parse("= x, (func a, b, c);", true, [](const auto& nodes)
        {
            require(nodes.size() == 1 && nodes[0]->children.size() == 3, "Assignment operands include punctuation");
            auto call = nodes[0]->children[2];
            require(call->type == CPM_EXPR && call->children.size() == 4, "Call must have exactly three operands");
            require(call->children[2]->text == "b" && call->commaBefore[2], "Comma boundary lost");
        });
        fixture.parse("(= x, (func a, (+ b, c), (other)));", true);
        fixture.parse("(< (+ 1, 2), 4); (<= x, 4); (>= x, 1);", true);
        fixture.parse("(func a,\tb,\r\nc);", true);
        fixture.parse("(NS.func NS.value, p.x);", true);
        fixture.parse("\n\n= x, 1;", true, [](const auto& nodes)
        {
            require(nodes[0]->lineNumber == 3, "Diagnostic location includes preceding trivia");
        });
        fixture.parse("(func /* , ; ( */ a, /* nested syntax ) */ b);", true);
        fixture.parse("// unmatched quotes ' \" and parentheses (\n(func a, b);", true);
        fixture.parse("(func \"a,b;c\", ',');", true);
        fixture.parse("void f(uint8 a; uint16 b) ( return (+ a, b); );", true);
        fixture.parse("static uint8 x array 2 (1; (+ 1, 1));", true);
        fixture.parse("static uint8 grid array 2 ((1; 2); (3; 4));", true, [](const auto& nodes)
        {
            auto outer = nodes[0]->children.back();
            require(outer->type == CPM_BLOCK && outer->children.size() == 4, "Nested initializer list collapsed incorrectly");
            require(outer->children[1]->children[0]->type == CPM_BLOCK, "Inner initializer lost its own grouping");
        });

        for (const auto& source : {
            "(func a b);", "(func, a);", "(func a,, b);", "(func a,);",
            "(func a,b);", "(<(+ 1, 2), 4);", "(+ 1(+ 2, 3));",
            "(func/*comment*/a);", "(func a,/*comment*/b);",
            "(func a, b;", "(func a, b));", "/* unclosed", "'ab';",
            "static uint8 a array 3 (1, 2, 3);"
        }) fixture.parse(source, false);

        const string program = R"(
            struct Pair ( uint8 a; uint8 b; );
            struct Wrapper ( Pair pair; uint8 c; );
            const uint8 size (+ 1, 2);
            static uint8 numbers array size (1; + 1, 1; (+ 1, 2););
            static char chars array 2 ('a'; 'b');
            static string texts array 2 ("ab"; "cd");
            static Pair pair (a 5; b 10);
            static Wrapper wrapper (pair (a 7; b 8); c 9);
            static Pair pairs array 2 ((a 1; b 2); (a 3; b 4));
            static uint8 target;
            void main() ( = target, (+ 1, 2); );
        )";
        fixture.compile(program, true, [](CPMCompiler& compiler)
        {
            vector<CPMNamespace*> spaces;
            compiler.getNamespaces(spaces);
            CPMNamespace* global = nullptr;
            for (auto ns : spaces) if (ns->name == "global") global = ns;
            require(global != nullptr, "Missing global namespace");
            require(global->statics.at("size").immediateData == 3, "Comma-separated constant fold failed");
            auto numbers = global->statics.at("numbers").staticData;
            require(numbers[0] == 1 && numbers[1] == 2 && numbers[2] == 3, "Numeric initializer values are wrong");
            auto pair = global->statics.at("pair").staticData;
            require(pair[0] == 5 && pair[1] == 10, "Named struct initializer values are wrong");
            auto nested = global->statics.at("wrapper").staticData;
            const auto& layout = global->structs.at("Wrapper").fields;
            const auto pairOffset = layout.at("pair").offset;
            require(nested[layout.at("c").offset] == 9 && nested[pairOffset] == 7 && nested[pairOffset + 1] == 8,
                    "Nested struct initialization failed");
            auto pairs = global->statics.at("pairs").staticData;
            require(pairs[0] == 1 && pairs[1] == 2 && pairs[2] == 3 && pairs[3] == 4, "Array of structs initialization failed");
        });
        fixture.compile("static uint8 x; void main() ( (= x, 7); );", true);
        fixture.compile("static uint8 x; void main() ( = x 7; );", false);
        fixture.compile("const uint8 x (+ 1 2);", false);
        fixture.compile("static uint8 x array 2 (1, 2);", false);
        fixture.compile("struct P (uint8 a; uint8 b;); static P p (a 1, b 2);", false);
        fixture.compile("struct P (uint8 a; uint8 b;); static P p (a 1; b 2, 3);", false);
        fixture.compile("static uint8 x array 2 (1;; 2);", false);
        fixture.compile("void main() ( func a b; );", false);
        fixture.compile("void main() ( if (1) ( func a b; ); );", false);
        fixture.compile("void main() ( for (uint8 i 0; < i 3; ++ i) ( ; ); );", false);
        fixture.compile("static uint8 x, 1;", false);
        fixture.compile("struct P (uint8 x, array 3;);", false);
        fixture.compile("void f(uint8 x, array 3) ( ; );", false);
        fixture.compile("void main() ( uint8 x, 1; );", false);
        fixture.compile("const uint8 x (+ 1, 2); void main() ( ; );", true);
        std::cout << "CPM separator tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
