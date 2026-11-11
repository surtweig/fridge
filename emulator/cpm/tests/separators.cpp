#include "support.h"

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
        fixture.parse("static <uint8 array 2> x (1; (+ 1, 1));", true);
        fixture.parse("static uint8 grid array 2 ((1; 2); (3; 4));", true, [](const auto& nodes)
        {
            auto outer = nodes[0]->children.back();
            require(outer->type == CPM_BLOCK && outer->children.size() == 4, "Nested initializer list collapsed incorrectly");
            require(outer->children[1]->children[0]->type == CPM_BLOCK, "Inner initializer lost its own grouping");
        });

        fixture.parse("<char array 4> buf ('a'; 'b'; 'c'; 0);", true, [](const auto& nodes)
        {
            auto type = nodes.at(0)->children.at(0);
            require(type->type == CPM_TYPEEXPR && type->children.size() == 3, "Missing type expression node");
            require(type->children[0]->text == "char" && type->children[1]->text == "array" &&
                    type->children[2]->text == "4", "Type components were absorbed by identifiers");
        });
        fixture.parse("<<char array 4> ptr> p;", true, [](const auto& nodes)
        {
            auto type = nodes.at(0)->children.at(0);
            require(type->type == CPM_TYPEEXPR && type->children.at(0)->type == CPM_TYPEEXPR,
                    "Nested type group was flattened");
        });
        fixture.parse("<NS.Item ptr> p; <NS.Item> q;", true, [](const auto& nodes)
        {
            for (auto line : nodes)
                require(line->children[0]->children[0]->type == CPM_REF, "Qualified type was not joined at angle boundary");
        });
        fixture.parse("<char ptr>\r\n\tp; <char ptr > q;", true);
        fixture.parse("<<uint8 ptr> array 4> pointers;", true);
        fixture.parse("<uint8 ptr ptr> p; <uint8 array 2 array 3> grid;", true);
        fixture.parse("<Unknown ptr> p; <_Private ptr> q;", true);
        fixture.parse("<uint8 array (+ 2, 3)> values;", true);
        fixture.parse("<uint8 array (cast uint8, (> n, 3))> flags;", true);
        fixture.parse("<uint8 array (cast uint8, (< n, 3))> flags;", true);
        fixture.parse("(cast <uint8 ptr>, address);", true, [](const auto& nodes)
        {
            auto call = nodes[0]->children[0];
            require(call->children[1]->type == CPM_TYPEEXPR && call->children.size() == 3,
                    "Type operand was not retained inside cast");
        });
        fixture.parse("<char ptr> /* comment */ p;", true);
        fixture.parse("<char /* > < */ ptr> p; // < >\n(< p, q);", true);
        fixture.parse("(func \"<char ptr>\", '<', '>');", true);
        fixture.parse("void f(<uint8 ptr> p; <<uint8 ptr> array 2> q) ( ; );", true);
        fixture.parse("(< (+ 1, 2), 4); (> x, 1); (<= x, 4); (>= x, 1);", true);
        for (const auto& source : {
            "<char ptr>p;", "<char ptr>/* comment */p;", "<char ptr", "<char ptr);",
            "<char ptr>> p;", "<> p;", "< char ptr> p;", "< char ptr > p;",
            "<<char ptr>ptr> p;", "<uint8 array(+ 2, 3)> p;", "<uint8 array> p;",
            "<uint8 array 4, ptr> p;", "<uint8 ptr, ptr> p;", "<uint8 invalid> p;",
            "<uint8 array (1; 2)> p;", "<uint8 array > n, 3> p;", "<uint8 ptr>= p;",
            "(<=count, 4);", "(>=count, 1);", "(<(+ 1, 2), 4);", "(cast<uint8 ptr>, address);"
        }) fixture.parse(source, false);
        fixture.parse("<char ptr; <uint8 ptr> valid;", false, [](const auto& nodes)
        {
            require(nodes.size() == 2 && nodes[1]->children[0]->type == CPM_TYPEEXPR,
                    "Unclosed type swallowed the following declaration");
        });
        fixture.parse("<char ptr); <uint8 ptr> valid;", false, [](const auto& nodes)
        {
            require(nodes.size() == 2, "Mismatched delimiter prevented declaration recovery");
        });
        string deep = "uint8";
        for (int i = 0; i < 32; ++i) deep = "<" + deep + " ptr>";
        fixture.parse(deep + " p;", true);

        fixture.typeExpression("<char array 4>", true, [](const auto& type, auto& compiler)
        {
            require(type.isArray() && type.arrayCount == 4 && type.child->base == CPM_DATATYPE_CHAR &&
                    compiler.sizeOfTypeExpr(type) == 4, "Array type/size is incorrect");
        });
        fixture.typeExpression("<<uint8 array n> ptr>", true, [](const auto& type, auto& compiler)
        {
            require(type.isPtr() && type.child->isArray() && type.child->arrayCount == 5 &&
                    compiler.sizeOfTypeExpr(type) == 2, "Nested pointer type is incorrect");
        });
        fixture.typeExpression("<uint8 array (+ n, 2)>", true, [](const auto& type, auto& compiler)
        {
            require(type.arrayCount == 7 && compiler.sizeOfTypeExpr(type) == 7, "Array count did not constant-fold");
        });
        fixture.typeExpression("<uint8 array (/ 12, 3)>", true, [](const auto& type, auto&)
        {
            require(type.arrayCount == 4, "Array count division failed");
        });
        fixture.typeExpression("<uint8 ptr ptr>", true, [](const auto& type, auto&)
        {
            require(type.isPtr() && type.child->isPtr(), "Chained pointer modifiers were ignored");
        });
        fixture.typeExpression("<uint8 array 3 array 2>", true, [](const auto& type, auto& compiler)
        {
            require(type.arrayCount == 2 && type.child->arrayCount == 3 && compiler.sizeOfTypeExpr(type) == 6,
                    "Array modifiers did not wrap left to right");
        });
        fixture.typeExpression("<NS.Item ptr>", true);
        fixture.typeExpression("<uint8 array 65535d>", true);
        fixture.typeExpression("uint8", true);
        fixture.typeExpression("void", true, [](const auto& type, auto&) { require(type.isVoid(), "Void lost its kind"); });
        for (const auto& type : {"<uint8>", "(uint8 ptr)", "<void ptr>", "<void array 4>",
                                 "<uint8 array 0>", "<uint16 array 40000d>", "<Unknown ptr>",
                                 "<NS.Unknown ptr>", "<NS.uint8 ptr>", "<uint8 array missing>",
                                 "<<uint8 array 65535d> array 65535d>"})
            fixture.typeExpression(type, false);

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
            static <uint8 array size> numbers (1; + 1, 1; (+ 1, 2););
            static <char array 2> chars ('a'; 'b');
            static <string array 2> texts ("ab"; "cd");
            static Pair pair (a 5; b 10);
            static Wrapper wrapper (pair (a 7; b 8); c 9);
            static <Pair array 2> pairs ((a 1; b 2); (a 3; b 4));
            static uint8 target;
            void main() ( = target, (+ 1, 2); );
        )";
        fixture.check(program, true, [](CPMCompiler& compiler)
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
        fixture.check("static uint8 x; void main() ( (= x, 7); );", true);
        fixture.compile("void main() (uint8 x; (= x, 7););", true);
        fixture.compile("static uint8 x; void main() ( = x 7; );", false);
        fixture.compile("const uint8 x (+ 1 2);", false);
        fixture.compile("static <uint8 array 2> x (1, 2);", false);
        fixture.compile("struct P (uint8 a; uint8 b;); static P p (a 1, b 2);", false);
        fixture.compile("struct P (uint8 a; uint8 b;); static P p (a 1; b 2, 3);", false);
        fixture.compile("static <uint8 array 2> x (1;; 2);", false);
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
