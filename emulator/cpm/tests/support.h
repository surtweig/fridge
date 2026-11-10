#pragma once
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

    void typeExpression(const string& text, bool valid,
                        const std::function<void(const CPMType&, CPMCompiler&)>& inspect = {})
    {
        compile("const uint8 n 5; namespace NS ( struct Item ( uint8 x; ); );", true,
            [&](CPMCompiler& compiler)
            {
                std::ofstream(directory / "type.cpm") << text << ';';
                Logger logger;
                CPMParser parser(directory.string() + "/", "type.cpm", &logger);
                require(parser.NoErrors(), "Type failed lexical grouping: " + text + logger.GetText());
                vector<CPMSyntaxTreeNode*> nodes;
                parser.GetLayer(nodes);
                CPMType result;
                result.kind = CPM_TYPE_BASE;
                result.base = CPM_DATATYPE_UINT16;
                CPMType original = result;
                const bool parsed = compiler.parseTypeExpr(nodes.at(0)->children.at(0), result,
                                                          compiler.getSourceFile("type.cpm"));
                require(parsed == valid, "Unexpected semantic type result: " + text + compiler.CompilerLog()->GetText());
                require(compiler.NoErrors() == valid, "Type parser did not propagate its error state");
                if (!valid) require(result == original, "Failed type parsing modified the destination");
                if (inspect) inspect(result, compiler);
            });
    }
};

