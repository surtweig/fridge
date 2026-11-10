#include "support.h"
#include "CPMIntermediate.h"

static CPMNamespace& globalNamespace(CPMCompiler& compiler)
{
    vector<CPMNamespace*> spaces;
    compiler.getNamespaces(spaces);
    for (auto ns : spaces) if (ns->name == GlobalNamespace) return *ns;
    throw std::runtime_error("Missing global namespace");
}

int main()
{
    try
    {
        Fixture fixture;
        fixture.compile(R"(
            const uint8 count (+ 1, 1);
            struct Node ( <Node ptr> next; <<uint16 array count> array 2> words; );
            struct Envelope ( <Tail array 2> z; <Node ptr ptr> a; );
            struct Tail ( uint8 z; uint16 a; );
            static <<uint16 array count> array 2> grid ((1; 258); (3; 4));
            const <uint8 array 1> single (7);
            static <Tail array 2> tails ((z 5; a 258); (z 6; a 772));
            static <<string array 1> array 2> texts (("ab"); ("cd"));
            static <uint8 ptr ptr> chain;
            const <uint8 ptr ptr> address (cast <uint8 ptr ptr>, 0x1234);
            static <<uint8 ptr> array 2> pointers ((cast <uint8 ptr>, 0x1234); (cast <uint8 ptr>, 0x5678));
            static <uint8 ptr ptr> copied_address address;
            static <uint8 array (cast uint8, (>> (<< 1, 3), 1))> shifted;
            static <uint8 array (cast uint8, (> count, 1))> compared;
            static <uint8 array 1> imported imports 0x1000;
            static Envelope envelope;
            <uint8 array 2> make(<uint8 ptr ptr> p; <Tail array 1> values) ( ; );
            void overload(<uint8 ptr> p) ( ; );
            void overload(<<uint8 array 1> ptr> p) ( ; );
            void overload(<uint8 array 1> p) ( ; );
            void overload(uint8 p) ( ; );
            void locals(<Tail ptr> arg) (
                <<uint8 array 2> array 3> grid;
                <Tail ptr ptr> chain;
                <Tail array 1> one;
                uint8 scalar;
            );
        )", true, [](CPMCompiler& compiler)
        {
            auto& ns = globalNamespace(compiler);
            require(ns.structs.at("Node").size == 10, "Recursive pointer/array struct size incorrect");
            require(ns.structs.at("Tail").fields.at("z").offset == 0 &&
                    ns.structs.at("Tail").fields.at("a").offset == 1, "Fields must retain declaration order");
            require(ns.structs.at("Envelope").size == 8 && ns.structs.at("Envelope").fields.at("a").offset == 6,
                    "Forward struct arrays or pointer chains lost their types");
            auto grid = ns.statics.at("grid").staticData;
            require(grid[0] == 0 && grid[1] == 1 && grid[2] == 1 && grid[3] == 2 &&
                    grid[4] == 0 && grid[5] == 3 && grid[6] == 0 && grid[7] == 4, "Nested array bytes/stride incorrect");
            auto& single = ns.statics.at("single");
            require(single.field.typeExpr.isArray() && single.staticData && single.staticData[0] == 7,
                    "Single-element array collapsed to scalar");
            require(ns.statics.at("shifted").field.typeExpr.arrayCount == 4, "Casts/shifts in array counts failed");
            require(ns.statics.at("compared").field.typeExpr.arrayCount == 1, "Nested comparison closed its enclosing type");
            require(ns.statics.at("address").immediateData == 0x1234 &&
                    ns.statics.at("copied_address").staticData[0] == 0x12, "Typed pointer constant initialization failed");
            auto pointers = ns.statics.at("pointers").staticData;
            require(pointers[0] == 0x12 && pointers[1] == 0x34 && pointers[2] == 0x56 && pointers[3] == 0x78,
                    "Array of pointers used pointee sizes or flattened its type");
            auto tails = ns.statics.at("tails").staticData;
            require(tails[0] == 5 && tails[1] == 1 && tails[2] == 2 && tails[3] == 6 &&
                    tails[4] == 3 && tails[5] == 4, "Struct array initializer lost packed offsets");
            const auto& texts = ns.statics.at("texts");
            require(texts.staticStrings.size() == 2 && texts.staticStrings[0] == texts.field.offset &&
                    texts.staticStrings[1] == texts.field.offset + 2, "Nested string relocation slots incorrect");
            require(ns.statics.at("imported").importSource == 0x1000 &&
                    ns.statics.at("imported").field.typeExpr.isArray(), "Imported type was flattened");
            int overloads = 0;
            for (auto& entry : ns.functions)
            {
                auto& function = entry.second;
                if (function.signature.name == "overload") ++overloads;
                if (function.signature.name == "make")
                    require(function.returnType.isArray() && function.returnType.arrayCount == 2 &&
                            function.arguments[0].typeExpr.child->isPtr() && function.arguments[1].typeExpr.isArray(),
                            "Function signature lost recursive types");
                if (function.signature.name == "locals")
                {
                    CPMFunctionSemanticBlock block(&function);
                    require(block.StackOffset() == 12, "Local allocation used flattened sizes");
                    require(block.resolveLocalSymbolName("one")->typeExpr.isArray(), "Local array-one identity lost");
                    require(block.resolveLocalSymbolName("chain")->typeExpr.child->isPtr(), "Local pointer chain lost");
                    require(block.resolveLocalSymbolName("arg") == &function.arguments[0], "Parameter missing from scope");
                }
            }
            require(overloads == 4, "Recursive signature identities collapsed");
            compiler.PrintStaticData(); // Printing nested/imported values must also preserve shape.
        });
        fixture.compile("namespace NS ( struct Item (uint8 a;); ); using NS; static <Item array 1> a ((a 7)); void f(<NS.Item ptr> p) (;);", true);
        fixture.compile("struct A (<B ptr> b;); struct B (<A ptr> a;); static A a;", true);
        fixture.compile("struct A (<<A array 2> ptr> p;); static A a;", true);
        fixture.compile("void f(<uint8 ptr> p); void f(<uint8 ptr> p) (;);", true);
        fixture.compile("void f() (;); void main() (cast <uint8 ptr ptr>, 0;);", true);
        fixture.compile("struct Huge (<uint8 array 65535> data;); static <Huge ptr> p;", true);
        fixture.compile("void main() (if (1) (<uint8 array 2> a;););", true);
        std::ifstream example(CPM_EXAMPLE_SOURCE);
        require(example.good(), "Missing type-expression example");
        fixture.compile(string(std::istreambuf_iterator<char>(example), {}), true);
        for (const string& source : {
            "static <void ptr> p;", "static void value;", "struct P (void field;);",
            "static <uint8 ptr> p 0;", "static <uint8 ptr> p (cast <char ptr>, 0);",
            "const <uint8 ptr> p (cast <uint8 ptr>, 0); static <char ptr> q p;",
            "static uint8 value (cast <uint8 ptr>, 0);",
            "void f(void parameter) (;);", "void main() (void value;);",
            "static <Unknown ptr> p;", "struct P (<Unknown ptr> value;);",
            "<Unknown ptr> f() (;);", "void f(<Unknown ptr> value) (;);",
            "void main() (<Unknown ptr> value;);", "void main() (cast <Unknown ptr>, 0);",
            "void main() (uint16 value (cast <Unknown ptr>, 0););",
            "void main() (return (cast <Unknown ptr>, 0););",
            "static uint8 old array 2;", "struct P (uint8 old array 2;);",
            "void f(uint8 old array 2) (;);", "void main() (uint8 old array 2;);",
            "static #uint8 p;", "#uint8 f() (;);", "void f(#uint8 p) (;);", "void main() (#uint8 p;);",
            "static (uint8 ptr) p;", "void main() (cast (uint8 ptr), 0);",
            "static <char array 2> a \"ab\";",
            "static <uint8 array 1> a (1; 2);", "static <uint8 array 1> a 1;",
            "static <<uint8 array 2> array 2> a ((1; 2); (3));",
            "static <uint8 array 0> a;", "static <uint16 array 65535> a;",
            "static <uint8 array 4294967297> a;",
            "static <uint8 array (* 65535, 65535)> a;",
            "static <uint8 array (<< 1, 32)> a;", "static <uint8 array (<< 1, 31)> a;",
            "struct A (<A array 1> a;);", "struct A (<B array 2> b;); struct B (<A array 1> a;);",
            "struct A (<B array 40000> b;); struct B (uint16 value;);",
            "struct A (<<B array 40000> ptr> p;); struct B (uint16 value;);",
            "void f(uint8 a; uint8 a) (;);", "void f(uint8 a); void f(uint8 renamed) (;);",
            "void f(<uint8 array 1> a) (;); void f(<uint8 array 1> a) (;);",
            "namespace A (struct P (uint8 x;);); namespace B (struct P (uint8 x;);); using A; using B; static <P ptr> p;",
            "static <uint8 array 2> a imports 0xFFFF;",
            "void main() (if (1) (<void ptr> p;););",
            "void main() (<uint8 array 40000> a; <uint8 array 40000> b;);"
        }) fixture.compile(source, false);
        std::cout << "CPM type-expression tests passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
