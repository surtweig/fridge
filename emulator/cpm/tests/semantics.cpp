#include "support.h"
#include "CPMSemantic.h"

static const CPMNamespace& globalNamespace(CPMCompiler& compiler)
{
    vector<CPMNamespace*> spaces;
    compiler.getNamespaces(spaces);
    for (auto ns : spaces)
        if (ns->name == GlobalNamespace) return *ns;
    throw std::runtime_error("Missing global namespace");
}

int main()
{
    try
    {
        Fixture fixture;
        fixture.check(R"(
            struct Pair (uint8 high; uint8 low;);
            union Word (Pair pair;) (uint16 bits;);
            const Word word (bits 0x1234);
            const uint8 high_byte word.pair.high;
            const <uint8 array 2> bytes (cast <uint8 array 2>, word);
            const uint16 round_trip (cast uint16, bytes);
            const int8 wrapped (+ +127, +1);
            const int8 quotient (/ -7, +3);
            const int8 remainder (% -7, +3);
            const int8 shifted (>> -4, 1);
            const uint8 negative (- 1);
            const bool distinct (!= 1, 2, 1);
            const uint8 raw_bool (cast uint8, (cast bool, 5));
            const uint8 logical_bool (convert uint8, (cast bool, 5));
            const bool skipped (&& false, (== (/ 1, 0), 3));
            const char terminator (at "cat", 3);
            const <Pair ptr> pointer (cast <Pair ptr>, 0x1000);
            const uint16 moved (cast uint16, (+ pointer, 2));
            static <uint8 array high_byte> table;
            const <uint8 ptr> cell (addr (at table, 1));
            static string first_text "abc";
            static string second_text "abc";

            namespace Other (uint16 select(uint16 n) (return n;););
            using Other;
            uint8 select(uint8 n) (return n;);
            <uint8 ptr> default_ptr() (return null;);
            void use_pointer(<uint8 ptr> pointer_arg);
            void use_array(<char array 4> value);
            uint8 body(bool flag; uint8 i)
            (
                Pair value (high 1; low 2);
                <Pair array 1> values (value;);
                = (at values, 0).high, 3;
                <Pair array 1> copy;
                = copy, values;
                <uint8 ptr> address null;
                = address, null;
                use_pointer null;
                use_array "cat";
                string a "same";
                string b "same";
                string alias a;
                = (at alias), 'S';
                const uint8 count 2;
                <uint8 array count> local_array;
                = (at local_array, i), (select 1);
                uint16 wide (select 1d);
                uint16 other (Other.select 1d);
                static uint8 persistent i;
                if (flag) return (at copy, 0d).high;
                else return (at local_array, i);
            );
            uint8 infinite() (for (; true;) (continue););
            uint8 nested(bool outer; bool inner)
            (
                if (outer) if (inner) return 1;
                else return 2;
                else return 3;
            );
            void empty() ();
            void empty_branches() (if (true) (); for (; false;) (););
        )", true, [](CPMCompiler& compiler)
        {
            const auto& ns = globalNamespace(compiler);
            const vector<std::pair<string, int>> expected{
                {"high_byte", 0x12}, {"round_trip", 0x1234}, {"wrapped", 0x80},
                {"quotient", 0xFD}, {"remainder", 2}, {"shifted", 0xFE},
                {"negative", 255}, {"distinct", 0}, {"raw_bool", 5}, {"logical_bool", 1},
                {"skipped", 0}, {"terminator", 0}, {"moved", 0x1004}
            };
            for (const auto& [name, value] : expected)
                require(ns.statics.at(name).immediateData == value, "Incorrect constant bytes for " + name);
            const auto& cell = *ns.statics.at("cell").initializer;
            require(cell.constant && cell.bytes.empty() &&
                    cell.operands.front().staticObject == &ns.statics.at("table"),
                    "Symbolic address lost its storage reference");
            const auto& first = ns.statics.at("first_text");
            const auto& second = ns.statics.at("second_text");
            require((first.staticData[0] != second.staticData[0] || first.staticData[1] != second.staticData[1]),
                    "Writable namespace string initializers shared backing storage");
            for (const auto& [signature, function] : ns.functions)
            {
                if (!function.bodyNode) continue;
                require(function.checkedBody != nullptr, "Missing typed function body");
                if (signature.name == "body")
                {
                    const auto& body = function.checkedBody->body;
                    require(!body.fallsThrough && body.statements.back().statements.size() == 2,
                            "Return paths were lost in the checked tree");
                    const auto& argument = body.statements[7].expressions.front().operands.front();
                    require(argument.type.isPtr() && argument.bytes == vector<FRIDGE_WORD>{0, 0},
                            "The call did not resolve contextual null");
                    const auto& a = body.statements[9].expressions.front();
                    const auto& b = body.statements[10].expressions.front();
                    const auto& alias = body.statements[11].expressions.front();
                    require(a.kind == CPMExprKind::StringInitializer && b.kind == CPMExprKind::StringInitializer &&
                            a.syntax != b.syntax && alias.kind == CPMExprKind::Name &&
                            alias.object == &body.statements[9].declaration->symbol,
                            "The checked tree lost distinct literal initializations or the string alias");
                }
            }
        });

        const vector<std::pair<string, string>> invalid{
            {"void f() (uint16 x 1;);", "Expected uint16, got uint8"},
            {"void f(uint8 x) (= x, (+ x, 1d););", "matching integer"},
            {"void f() (if (1) (););", "Expected bool, got uint8"},
            {"void f() (for (; 1;) (););", "Expected bool, got uint8"},
            {"void f() (! 1;);", "matching bool"},
            {"void f() (&& true, 1;);", "matching bool"},
            {"void f() (uint16 x (cast uint16, 1););", "equal-size"},
            {"void f() (uint8 x (convert uint8, 256););", "out-of-range integer"},
            {"void f() (int16 x -32769d;);", "out-of-range integer"},
            {"void f() (uint8 x 0x1;);", "out-of-range integer"},
            {"void f() (uint8 x y; uint8 y;);", "Unknown name"},
            {"void f(uint8 x) (if (true) (uint8 x;););", "conflicts with a visible declaration"},
            {"void f() (for (uint8 i 0; false; ++ i) (); uint8 x i;);", "Unknown name"},
            {"void f() (if (true) (uint8 x;); = x, 1;);", "Unknown name"},
            {"void f() (break;);", "requires an enclosing loop"},
            {"void f() (continue;);", "requires an enclosing loop"},
            {"uint8 f(bool b) (if (b) return 1;);", "reachable end"},
            {"uint8 f() (for (; true;) (break;););", "reachable end"},
            {"uint8 f() (return;);", "must return one value"},
            {"void f() (return 1;);", "return without a value"},
            {"uint16 f() (return 1;);", "Expected uint16, got uint8"},
            {"void f() (uint8 x (/ 1, 0););", "Division by zero"},
            {"void f(uint8 x) (<< x, 8;);", "shift count is out of range"},
            {"void f() (<uint8 array 2> a; = (at a, 2d), 1;);", "index is out of bounds"},
            {"void f() (<uint8 array 2> a; at a, -1;);", "uint8 or uint16"},
            {"void f() (= (at \"ab\", 0), 'x';);", "writable destination"},
            {"void f() (addr (at \"ab\", 0););", "cannot yield a writable pointer"},
            {"void f() (string s; = s, \"ab\";);", "Expected string, got"},
            {"string f() (return \"ab\";);", "Expected string, got"},
            {"void g(string s); void f() (g \"ab\";);", "No matching overload for 'g(<char array 3>)'."},
            {"void f() (missing 1, 2d, null;);", "No matching overload for 'missing(uint8, uint16, null)'."},
            {"void g(uint8 n); void f() ((g););", "No matching overload for 'g()'."},
            {"namespace A (struct Item (uint8 n;); void g(uint8 n);); "
             "void f(<<A.Item ptr> array 2> items) (A.g items;);",
             "No matching overload for 'A.g(<<A.Item ptr> array 2>)'."},
            {"void g(<uint8 ptr> p); void g(<char ptr> p); void f() (g null;);", "Ambiguous call"},
            {"namespace A (void g(uint8 n);); using A; void g(uint8 n); void f() (g 1;);", "Ambiguous call"},
            {"struct P (uint8 x; uint8 y;); void f() (P p (x 1););", "every struct field"},
            {"struct P (uint8 x; uint8 y;); void f() (P p (x 1; x 2););", "repeated initializer field"},
            {"struct P (uint8 x;); void f() (uint8 y (cast P, 1).x;);", "Computed field access"},
            {"union U (uint8 x;) (uint16 y;);", "equal total sizes"},
            {"union U (uint8 x;) (uint8 y;); const U u (x 1; y 2);", "one complete union view"},
            {"const uint8 a b; const uint8 b 1;", "Unknown name"},
            {"const uint8 a a;", "Unknown name"},
            {"using Z; namespace A (const uint8 early Z.later;); namespace Z (const uint8 later 1;);", "Unknown name"},
            {"static uint8 size 4; struct P (<uint8 array size> values;);", "compile-time integer"},
            {"void f(uint8 i) (static <uint8 array 2> a; const <uint8 ptr> p (addr (at a, i)););", "compile time"},
            {"bool predicate(); void f() (const bool value (&& false, (predicate)););", "compile time"}
        };
        for (const auto& [source, diagnostic] : invalid)
            fixture.check(source, false, [&](CPMCompiler& compiler)
            {
                require(compiler.CompilerLog()->GetText().find(diagnostic) != string::npos,
                        "Missing diagnostic '" + diagnostic + "' for " + source + compiler.CompilerLog()->GetText());
            });

        std::ofstream(fixture.directory / "constants.cpm") << "const uint8 count 3;";
        fixture.check("include \"constants.cpm\"; static <uint8 array count> data;", true);
        fixture.check("static <uint8 array count> data; include \"constants.cpm\";", false);
        fixture.check("namespace Z (const uint8 n 3;); using Z; namespace A (const uint8 m n;);", true);
        fixture.compile("void f() (uint16 x 1;);", false);
        fixture.compile("void g(); void f() ((g););", false, [](CPMCompiler& compiler)
        {
            require(compiler.CompilerLog()->GetText().find("Executable lowering is unavailable") != string::npos,
                    "Incomplete backend reported success for an unlowered call");
        });
        std::cout << "CPM semantic tests passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
