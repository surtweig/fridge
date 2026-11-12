#include "fridge.h"
#include "stdafx.h"
#include "CPMCompiler.h"
#include "CPMIntermediate.h"
#include "CPMSemantic.h"
#include "CPMAssembly.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <functional>
#include <stdlib.h>
#include <cerrno>
#include <limits>

namespace CPM
{

    CPMDataSymbol::CPMDataSymbol()
    {
        name = "?";
        typeExpr.kind = CPM_TYPE_VOID;
        typeExpr.base = CPM_DATATYPE_UNDEFINED;
        offset = 0;
        owner = nullptr;
    }

    CPMDataSymbol::~CPMDataSymbol()
    {
    }



    // ---- CPMType ----

    CPMType::CPMType()
        : kind(CPM_TYPE_VOID)
        , base(CPM_DATATYPE_VOID)
        , arrayCount(0)
        , child()
    {
    }

    CPMType::~CPMType()
    {
    }

    CPMType::CPMType(const CPMType& other)
        : kind(other.kind)
        , base(other.base)
        , arrayCount(other.arrayCount)
        , child(other.child ? new CPMType(*other.child) : nullptr)
    {
    }

    CPMType& CPMType::operator=(const CPMType& other)
    {
        if (this != &other)
        {
            kind = other.kind;
            base = other.base;
            arrayCount = other.arrayCount;
            child.reset(other.child ? new CPMType(*other.child) : nullptr);
        }
        return *this;
    }

    bool CPMType::operator==(const CPMType& o) const
    {
        if (kind != o.kind)
            return false;
        switch (kind)
        {
        case CPM_TYPE_VOID:
            return true;
        case CPM_TYPE_BASE:
            return base == o.base;
        case CPM_TYPE_PTR:
            return (child && o.child) ? *child == *o.child : !child && !o.child;
        case CPM_TYPE_ARRAY:
            if (arrayCount != o.arrayCount)
                return false;
            return (child && o.child) ? *child == *o.child : !child && !o.child;
        }
        return false;
    }

    bool CPMType::operator<(const CPMType& o) const
    {
        if (kind != o.kind)
            return kind < o.kind;
        switch (kind)
        {
        case CPM_TYPE_VOID:
            return false;
        case CPM_TYPE_BASE:
            return base < o.base;
        case CPM_TYPE_PTR:
            if (!child) return o.child != nullptr;
            if (!o.child) return false;
            return *child < *o.child;
        case CPM_TYPE_ARRAY:
            if (arrayCount != o.arrayCount)
                return arrayCount < o.arrayCount;
            if (!child) return o.child != nullptr;
            if (!o.child) return false;
            return *child < *o.child;
        }
        return false;
    }

    /*
    FRIDGE_RAM_ADDR CPMDataSymbol::serialize(vector<FRIDGE_WORD>& output)
    {
        if (type == CPM_DATATYPE_BOOL || type == CPM_DATATYPE_CHAR || type == CPM_DATATYPE_INT8 || type == CPM_DATATYPE_UINT8)
        {
            for (int i = 0; i < count; ++i)
                output.push_back(FRIDGE_WORD(data[i]));
            return count;
        }
        else if (type == CPM_DATATYPE_INT16 || type == CPM_DATATYPE_UINT16)
        {
            for (int i = 0; i < count; ++i)
            {
                FRIDGE_DWORD dw = FRIDGE_DWORD(data[i]);
                output.push_back(FRIDGE_HIGH_WORD(dw));
                output.push_back(FRIDGE_LOW_WORD(dw));
            }
            return count * 2;
        }
        else if (type == CPM_DATATYPE_STRING)
        {
            FRIDGE_RAM_ADDR size = 0;
            for (int i = 0; i < count; ++i)
            {
                string s((char*)data[i]);
                for (int ci = 0; ci < s.size(); ++ci)
                    output.push_back(s[ci]);
                output.push_back(0);
                size += s.size() + 1;
            }
            return size;
        }
        else if (type >= CPM_DATATYPE_USER)
        {
            FRIDGE_RAM_ADDR size = 0;
            for (int i = 0; i < count; ++i)
            {
                CPMStructSymbol* ss = (CPMStructSymbol*)data[i];
                for (auto fi = ss->fields.begin(); fi != ss->fields.end(); ++fi)
                    size += fi->second.serialize(output);
            }
            return size;
        }
    }
    */

    CPMStaticSymbol::CPMStaticSymbol()
    {
        isconst = false;
        importSource = -1;
        staticData = nullptr;
        immediateData = 0;
        declNode = nullptr;
    }

    CPMStructSymbol::CPMStructSymbol()
    {
        name = "?";
        type = CPM_DATATYPE_UNDEFINED;
        node = NULL;
        isUnion = false;
        size = -1;
    }

    bool CPMFunctionSignature::operator< (const CPMFunctionSignature& other) const // other < this
    {
        if (other.name != name)
            return other.name < name;

        int n = arguments.size();
        int n2 = other.arguments.size();
        if (n2 != n)
            return n2 < n;

        for (int i = 0; i < n; ++i)
        {
            // Compare the full recursive type structure. The new operator<
            // on CPMType gives a total ordering that distinguishes every
            // distinct type-expr shape, so overloads that differ by
            // pointer-ness, array-ness, or nested shape all resolve correctly.
            if (!(arguments[i].typeExpr == other.arguments[i].typeExpr))
                return other.arguments[i].typeExpr < arguments[i].typeExpr;
        }
        return false;
    }

    bool IsIntDataType(CPMDataType dtype)
    {
        return dtype == CPM_DATATYPE_UINT8 || dtype == CPM_DATATYPE_UINT16 || dtype == CPM_DATATYPE_INT8 || dtype == CPM_DATATYPE_INT16;
    }

    // 1-2 byte primitive type
    bool IsImmediateDataType(CPMDataType dtype)
    {
        return IsIntDataType(dtype) || dtype == CPM_DATATYPE_BOOL || dtype == CPM_DATATYPE_CHAR;
    }

    CPMCompiler::CPMCompiler(string sourceRootFolder, string sourceFileName, string outputFile,
                             vector<string> includeFolders, CPMCompileMode mode, CPMCompileOptions options)
        : compilerLog(), asmDebugOutput(), options(options)
    {
        noErrors = true;
        outputFileName = outputFile;
        this->includeFolders = includeFolders;
        preprocessSourceFile(sourceRootFolder, sourceFileName);
        if (!noErrors)
        {
            compilerLog.Add(LOG_ERROR, "Compilation failed due to syntax errors.");
            return;
        }
        dataTypeCounter = CPM_DATATYPE_USER;

        namespaces[GlobalNamespace] = new CPMNamespace(GlobalNamespace);
        CPMNamespace* global = namespaces[GlobalNamespace];
        global->datatypes["void"] = CPM_DATATYPE_VOID;
        global->datatypes["bool"] = CPM_DATATYPE_BOOL;
        global->datatypes["char"] = CPM_DATATYPE_CHAR;
        global->datatypes["uint8"] = CPM_DATATYPE_UINT8;
        global->datatypes["uint16"] = CPM_DATATYPE_UINT16;
        global->datatypes["int8"] = CPM_DATATYPE_INT8;
        global->datatypes["int16"] = CPM_DATATYPE_INT16;
        global->datatypes["string"] = CPM_DATATYPE_STRING;

        addNumStatic("false", global, true, CPM_DATATYPE_BOOL, 0);
        addNumStatic("true", global, true, CPM_DATATYPE_BOOL, 1);
        addNumStatic("FRIDGE_ROM_SEGMENT_SIZE", global, true, CPM_DATATYPE_UINT16, FRIDGE_ROM_SEGMENT_SIZE);
        addNumStatic("FRIDGE_MAX_IO_DEVICES", global, true, CPM_DATATYPE_UINT8, FRIDGE_MAX_IO_DEVICES);
        addNumStatic("FRIDGE_GPU_BUS_SIZE", global, true, CPM_DATATYPE_UINT8, FRIDGE_GPU_BUS_SIZE);
        addNumStatic("FRIDGE_GPU_FRAME_EGA_WIDTH", global, true, CPM_DATATYPE_UINT8, FRIDGE_GPU_FRAME_EGA_WIDTH);
        addNumStatic("FRIDGE_GPU_FRAME_EGA_HEIGHT", global, true, CPM_DATATYPE_UINT8, FRIDGE_GPU_FRAME_EGA_HEIGHT);
        addNumStatic("FRIDGE_GPU_MAX_SPRITES", global, true, CPM_DATATYPE_UINT8, FRIDGE_GPU_MAX_SPRITES);
        addNumStatic("FRIDGE_GPU_MAX_SPRITES_PER_PIXEL", global, true, CPM_DATATYPE_UINT8, FRIDGE_GPU_MAX_SPRITES_PER_PIXEL);
        addNumStatic("FRIDGE_GPU_FRAME_BUFFER_SIZE", global, true, CPM_DATATYPE_UINT16, FRIDGE_GPU_FRAME_BUFFER_SIZE);
        addNumStatic("FRIDGE_BOOT_SECTION_INDEX_ADDRESS", global, true, CPM_DATATYPE_UINT16, FRIDGE_BOOT_SECTION_INDEX_ADDRESS);
        addNumStatic("FRIDGE_EXECUTABLE_OFFSET", global, true, CPM_DATATYPE_UINT16, FRIDGE_EXECUTABLE_OFFSET);
        addNumStatic("FRIDGE_IRQ_SYS_TIMER", global, true, CPM_DATATYPE_UINT16, FRIDGE_IRQ_SYS_TIMER);
        addNumStatic("FRIDGE_IRQ_KEYBOARD_PRESS", global, true, CPM_DATATYPE_UINT16, FRIDGE_IRQ_KEYBOARD_PRESS);
        addNumStatic("FRIDGE_IRQ_KEYBOARD_RELEASE", global, true, CPM_DATATYPE_UINT16, FRIDGE_IRQ_KEYBOARD_RELEASE);
        addNumStatic("FRIDGE_KEYBOARD_KEY_STATE_MASK", global, true, CPM_DATATYPE_UINT8, FRIDGE_KEYBOARD_KEY_STATE_MASK);
        addNumStatic("FRIDGE_KEYBOARD_KEY_CODE_MASK", global, true, CPM_DATATYPE_UINT8, FRIDGE_KEYBOARD_KEY_CODE_MASK);
        addNumStatic("FRIDGE_DEV_ROM_RESET_ID", global, true, CPM_DATATYPE_UINT8, FRIDGE_DEV_ROM_RESET_ID);
        addNumStatic("FRIDGE_DEV_ROM_ID", global, true, CPM_DATATYPE_UINT8, FRIDGE_DEV_ROM_ID);
        addNumStatic("FRIDGE_DEV_KEYBOARD_ID", global, true, CPM_DATATYPE_UINT8, FRIDGE_DEV_KEYBOARD_ID);

        if (noErrors) readNamespaces();
        if (noErrors) readDeclarations();
        if (noErrors) readFunctions();

        if (noErrors && mode == CPMCompileMode::Emit)
        {
            CPMIntermediate im(this);
        }
        else if (noErrors && mode == CPMCompileMode::Assembly)
        {
            const auto ir = CPMBuildIR(*this);
            if (noErrors) assemblyOutput = CPMEmitAssembly(*this, ir);
        }

        if (!noErrors)
            compilerLog.Add(LOG_ERROR, "Compilation failed due to errors.");
        else
            compilerLog.Add(LOG_MESSAGE, "Compilation successful.");
    }

    void CPMCompiler::preprocessSourceFile(string rootFolder, string filename)
    {
        map<string, CPMSourceFile>::iterator isrc = sources.find(filename);
        if (isrc == sources.end())
        {
            compilerLog.Add(LOG_MESSAGE, "Source file: '")->Add(filename)->Add("'.");
            //sources.emplace(filename, CPMSourceFile());
            sources[filename].name = filename;
            isrc = sources.find(filename);
            isrc->second.name = filename;
            isrc->second.rootFolder = rootFolder;
            isrc->second.parser = new CPMParser(rootFolder, filename, CompilerLog());
            if (isrc->second.parser->NoErrors())
            {
                vector<CPMSyntaxTreeNode*> layer;// = isrc->second.parser->Layer();
                isrc->second.parser->GetLayer(layer);
                for (vector<CPMSyntaxTreeNode*>::iterator li = layer.begin(); li != layer.end(); ++li)
                {
                    CPMSyntaxTreeNode* node = (*li);
                    if (node->type == CPM_LINE)
                    {
                        if (node->children.size() > 0)
                        {
                            if (node->children[0]->type == CPM_ID && node->children[0]->text == R_INCLUDE)
                            {
                                if (node->children.size() != 2)
                                {
                                    compilerLog.Add(LOG_ERROR, "Invalid #include directive syntax.", node->sourceFileName, node->lineNumber);
                                    noErrors = false;
                                    continue;
                                }
                                if (node->children[1]->type != CPM_STR)
                                {
                                    compilerLog.Add(LOG_ERROR, "Included source file name must be a string literal.", node->sourceFileName, node->lineNumber);
                                    noErrors = false;
                                    continue;
                                }

                                string includedSourceFileName = CPMSTRContent(node->children[1]);
                                bool fileFound = false;
                                ifstream f(rootFolder+includedSourceFileName);
                                if (f.is_open())
                                {
                                    f.close();
                                    preprocessSourceFile(rootFolder, includedSourceFileName);
                                    fileFound = true;
                                }
                                else
                                    for (int i = 0; i < includeFolders.size(); i++)
                                    {
                                        f.open(includeFolders[i] + includedSourceFileName);
                                        if (f.is_open())
                                        {
                                            f.close();
                                            preprocessSourceFile(includeFolders[i], includedSourceFileName);
                                            fileFound = true;
                                            break;
                                        }
                                    }

                                if (!fileFound)
                                {
                                    compilerLog.Add(LOG_ERROR, "Cannot open file: '", node->sourceFileName, node->lineNumber)->Add(rootFolder + includedSourceFileName)->Add("'.");
                                    noErrors = false;
                                }
                                continue;
                            }

                            if (node->children[0]->type == CPM_ID && node->children[0]->text == R_USING)
                            {
                                if (node->children.size() != 2)
                                {
                                    compilerLog.Add(LOG_ERROR, "Invalid #using directive syntax.", node->sourceFileName, node->lineNumber);
                                    noErrors = false;
                                    continue;
                                }
                                if (node->children[1]->type != CPM_ID)
                                {
                                    compilerLog.Add(LOG_ERROR, "Invalid #using directive syntax.", node->sourceFileName, node->lineNumber);
                                    noErrors = false;
                                    continue;
                                }

                                isrc->second.usingNamespaces.push_back(node->children[1]->text);
                                continue;
                            }
                        }
                    }
                    sourceNodes.push_back(node);
                }
            }
            else
            {
                compilerLog.Add(LOG_ERROR, "Failed to parse '")->Add(filename)->Add("'.");
                noErrors = false;
            }
        }
        else
            compilerLog.Add(LOG_WARNING, "File '")->Add(filename)->Add("' has already been included.");
    }

    CPMStaticSymbol* CPMCompiler::addNumStatic(string name, CPMNamespace* ns, bool isConst, CPMDataType type, int data)
    {
        ns->statics[name].field.name = name;
        CPMStaticSymbol* ss = &(ns->statics.find(name)->second);
        ss->isconst = isConst;
        ss->field.typeExpr.kind = CPM_TYPE_BASE;
        ss->field.typeExpr.base = type;
        ss->field.owner = ns;
        if (!isConst)
        {
            if (type == CPM_DATATYPE_INT16 || type == CPM_DATATYPE_UINT16)
                ss->staticData = ns->staticAllocate((FRIDGE_DWORD)data);
            else if (type == CPM_DATATYPE_INT8 || type == CPM_DATATYPE_UINT8 || type == CPM_DATATYPE_BOOL || type == CPM_DATATYPE_CHAR)
                ss->staticData = ns->staticAllocate((FRIDGE_WORD)data);
            else
            {
                //CPM_ASSERT_MESSAGE(false, "Invalid data type '" + GetTypeName(type) + "' for numeric static '" + name + "'.");
                compilerLog.Add(LOG_ERROR, "Invalid data type '")->Add(GetTypeName(type))->Add("' for numeric static '")->Add(name)->Add("'.");
                Error();
            }
            if (ss->staticData == nullptr)
            {
                compilerLog.Add(LOG_ERROR, "Failed to allocate static data for symbol '")->Add(ns->name)->Add(".")->Add(name)->Add("'");
                Error();
            }
            else
                ss->field.offset = (FRIDGE_DWORD)ns->getStaticRelativeAddress(ss->staticData);
        }
        else
        {
            ss->immediateData = data;
            ss->staticData = nullptr;
        }
        return ss;
    }

    CPMStaticSymbol* CPMCompiler::addLiteral(CPMFunctionSymbol* func, CPMSyntaxTreeNode* valueNode)
    {
        auto value = CPMSemanticAnalyzer(*this, func->owner).expression(valueNode);
        if (!value) return nullptr;
        if (value.bytes.empty() || value.bytes.size() > 2 || value.type.isArray() || value.type.isStruct())
        {
            compilerLog.Add(LOG_ERROR, "This expression requires executable lowering. Use --check to analyze it.",
                            valueNode->sourceFileName, valueNode->lineNumber);
            Error();
            return nullptr;
        }
        auto& literal = func->literals.emplace_back();
        literal.isconst = true;
        literal.declNode = valueNode;
        literal.field.typeExpr = value.type;
        literal.field.owner = func->owner;
        for (auto byte : value.bytes) literal.immediateData = (literal.immediateData << 8) | byte;
        return &literal;
    }

    CPMStaticSymbol* CPMCompiler::addStatic(string name, CPMNamespace* ns, bool isConst,
                                             const CPMType& type, int importSource)
    {
        auto& symbol = ns->statics[name];
        symbol.field.name = name;
        symbol.field.typeExpr = type;
        symbol.field.owner = ns;
        symbol.isconst = isConst;
        symbol.importSource = importSource;
        const bool immediate = isConst && (type.isPtr() ||
            (type.isBase() && (type.isString() || IsImmediateDataType(type.base))));
        if (!immediate && importSource < 0)
        {
            const int size = sizeOfTypeExpr(type);
            symbol.staticData = ns->staticAllocate(nullptr, size, 1);
            if (!symbol.staticData)
            {
                compilerLog.Add(LOG_ERROR, "Failed to allocate static data for symbol '" + ns->name + "." + name + "'.");
                Error();
            }
            else
                symbol.field.offset = (FRIDGE_DWORD)ns->getStaticRelativeAddress(symbol.staticData);
        }
        return &symbol;
    }

    const CPMStaticSymbol* CPMCompiler::findStatic(CPMNamespace* ns, const string& name,
                                                   const CPMSyntaxTreeNode* before)
    {
        auto& pending = staticDeclarations[ns];
        auto declaration = pending.find(name);
        if (declaration != pending.end() && before &&
            declarationOrder.at(declaration->second) >= declarationOrder.at(before))
            return nullptr;

        auto found = ns->statics.find(name);
        if (found != ns->statics.end()) return &found->second;
        if (declaration == pending.end() || !noErrors) return nullptr;

        auto node = declaration->second;
        if (!activeStatics.insert(node).second)
        {
            compilerLog.Add(LOG_ERROR, "Circular static initializer for '" + name + "'.",
                            node->sourceFileName, node->lineNumber);
            Error();
            return nullptr;
        }
        detectStatic(node, ns);
        activeStatics.erase(node);
        found = ns->statics.find(name);
        return found != ns->statics.end() ? &found->second : nullptr;
    }

    void CPMCompiler::detectStatic(CPMSyntaxTreeNode* node, CPMNamespace* owner)
    {
        if (node->type != CPM_LINE || node->children.empty()) return;
        const string& storage = node->children[0]->text;
        if (storage != R_CONST && storage != R_STATIC) return;
        const bool isConst = storage == R_CONST;
        auto fail = [&](const string& message)
        {
            compilerLog.Add(LOG_ERROR, message, node->sourceFileName, node->lineNumber);
            Error();
        };
        if (!CPMValidateDeclaration(node, &compilerLog)) { Error(); return; }
        if (node->children.size() < 3 || node->children.size() > 5 || node->children[2]->type != CPM_ID)
        { fail("Invalid static or const declaration syntax."); return; }
        auto name = node->children[2];
        if (!CPMIsUserIdentifier(name)) { fail("Invalid static or const name '" + name->text + "'."); return; }
        auto existing = owner->statics.find(name->text);
        if (existing != owner->statics.end())
        {
            if (existing->second.declNode != node)
                fail("Static symbol '" + name->text + "' is already declared in " + owner->name + " namespace.");
            return;
        }
        CPMSemanticAnalyzer analyzer(*this, owner, node);
        CPMType type;
        if (!parseTypeExpr(node->children[1], type, getSourceFile(node->sourceFileName), owner, &analyzer)) return;
        if (type.isVoid()) { fail("Void type is not allowed for static or const data."); return; }
        if (!validTypeLayout(type)) { fail("Invalid static value layout."); return; }
        int size = sizeOfTypeExpr(type);
        if (size <= 0 || size > DataMaxSize) { fail("Static symbol '" + name->text + "' is too large."); return; }
        CPMSyntaxTreeNode* value = nullptr;
        int address = -1;
        if (node->children.size() == 5)
        {
            auto keyword = node->children[3];
            auto source = node->children[4];
            if (keyword->type != CPM_ID || keyword->text != R_IMPORT || source->type != CPM_NUM)
            { fail("Expected 'imports address'; array modifiers belong inside '<...>'."); return; }
            auto importedAddress = analyzer.expression(source);
            if (!importedAddress) return;
            if (importedAddress.type.base != CPM_DATATYPE_UINT8 && importedAddress.type.base != CPM_DATATYPE_UINT16)
            { fail("An import address must be an unsigned integer literal."); return; }
            address = 0;
            for (auto byte : importedAddress.bytes) address = (address << 8) | byte;
            if (isConst || size > 65536 - address)
            { fail("Invalid imported static address or size."); return; }
        }
        else if (node->children.size() == 4)
        {
            value = node->children[3];
            if (value->text == R_ARRAY || value->text == R_IMPORT)
            { fail("Invalid declaration initializer; compound types use '<...>'."); return; }
        }
        shared_ptr<CPMCheckedExpression> initializer;
        if (value)
        {
            auto checked = analyzer.initializer(value, type);
            if (!checked) return;
            if (!checked.constant) { fail("A namespace initializer must be computable at compile time."); return; }
            initializer = make_shared<CPMCheckedExpression>(std::move(checked));
        }
        auto symbol = addStatic(name->text, owner, isConst, type, address);
        symbol->declNode = node;
        symbol->initializer = std::move(initializer);
        if (noErrors && symbol->initializer)
            storeInitializer(*symbol, *symbol->initializer, 0);
    }

    void CPMCompiler::storeInitializer(CPMStaticSymbol& symbol, const CPMCheckedExpression& value, size_t offset)
    {
        const auto write = [&](const vector<FRIDGE_WORD>& bytes)
        {
            if (symbol.staticData)
                copy(bytes.begin(), bytes.end(), symbol.staticData + offset);
            else
                for (auto byte : bytes) symbol.immediateData = (symbol.immediateData << 8) | byte;
        };
        if (!value.bytes.empty())
            write(value.bytes);
        else if (value.kind == CPMExprKind::StringInitializer)
        {
            const auto& characters = value.operands[0].bytes;
            auto ns = symbol.field.owner;
            auto buffer = ns->staticAllocate(characters.data(), 1, characters.size());
            if (!buffer)
            {
                compilerLog.Add(LOG_ERROR, "Static string character data exceeds the namespace buffer.",
                                value.syntax->sourceFileName, value.syntax->lineNumber);
                Error();
                return;
            }
            const auto address = static_cast<FRIDGE_DWORD>(ns->getStaticRelativeAddress(buffer));
            write({FRIDGE_HIGH_WORD(address), FRIDGE_LOW_WORD(address)});
            if (symbol.staticData)
                symbol.staticStrings.push_back(symbol.field.offset + offset);
        }
        else if (value.kind == CPMExprKind::Initializer)
        {
            for (size_t i = 0; i < value.operands.size() && noErrors; ++i)
                storeInitializer(symbol, value.operands[i], offset + value.initializerOffsets[i]);
        }
        // Symbolic addresses remain in the checked initializer for relocation.
    }

    void CPMCompiler::readDeclarations()
    {
        for (const auto& [ns, node] : declarations)
        {
            detectStruct(node, ns);
            if (node->children.size() >= 3 &&
                (node->children[0]->text == R_CONST || node->children[0]->text == R_STATIC) &&
                node->children[2]->type == CPM_ID)
                staticDeclarations[ns].emplace(node->children[2]->text, node);
        }
        for (const auto& [ns, node] : declarations)
        {
            if (!noErrors) return;
            detectStatic(node, ns);
        }
        for (const auto& entry : structTypes)
        {
            if (!noErrors) return;
            computeStructDataSize(entry.second);
        }
        for (const auto& entry : structTypes)
            for (const auto& field : entry.second->fields)
                if (field.second.typeExpr.child && !validTypeLayout(field.second.typeExpr))
                {
                    auto node = entry.second->node;
                    compilerLog.Add(LOG_ERROR, "Field type exceeds 65535 bytes.", node->sourceFileName, node->lineNumber);
                    Error();
                    return;
                }
    }

    void CPMCompiler::ensureValueLayout(const CPMType& type)
    {
        if (type.isArray()) ensureValueLayout(*type.child);
        else if (type.isStruct()) computeStructDataSize(structTypes.at(type.base));
    }

    bool CPMCompiler::validTypeLayout(const CPMType& type)
    {
        if (type.child && !validTypeLayout(*type.child)) return false;
        ensureValueLayout(type);
        return sizeOfTypeExpr(type) >= 0;
    }

    void CPMCompiler::detectStruct(CPMSyntaxTreeNode* node, CPMNamespace* owner)
    {
        if (node->type != CPM_LINE || node->children.empty()) return;
        const auto kind = node->children[0]->text;
        if (kind != R_STRUCT && kind != R_UNION) return;
        if (node->children.size() < 3 || (kind == R_STRUCT && node->children.size() != 3) ||
            !CPMIsUserIdentifier(node->children[1]) ||
            !all_of(node->children.begin() + 2, node->children.end(),
                    [](const auto* group) { return group->type == CPM_BLOCK; }))
        {
            compilerLog.Add(LOG_ERROR, "Invalid struct or union declaration syntax.", node->sourceFileName, node->lineNumber);
            Error();
            return;
        }
        const auto& name = node->children[1]->text;
        if (owner->datatypes.count(name))
        {
            compilerLog.Add(LOG_ERROR, "Type '" + name + "' is already declared.", node->sourceFileName, node->lineNumber);
            Error();
            return;
        }
        auto& structure = owner->structs[name];
        structure.name = name;
        structure.node = node;
        structure.owner = owner;
        structure.isUnion = kind == R_UNION;
        owner->datatypes[name] = registerStructDataType(&structure);
    }

    CPMDataType CPMCompiler::registerStructDataType(CPMStructSymbol* structSymbol)
    {
        structSymbol->type = dataTypeCounter;
        structTypes[dataTypeCounter] = structSymbol;
        return dataTypeCounter++;
    }

    void CPMCompiler::readStructFields(CPMStructSymbol* structure)
    {
        CPMSemanticAnalyzer analyzer(*this, structure->owner, structure->node);
        for (size_t i = 2; i < structure->node->children.size(); ++i)
        {
            auto block = structure->node->children[i];
            auto& view = structure->views.emplace_back();
            for (size_t j = 1; j + 1 < block->children.size(); ++j)
            {
                auto node = block->children[j];
                if (node->children.empty()) continue;
                if (node->type != CPM_LINE || node->children.size() != 2 || !CPMIsUserIdentifier(node->children[1]) ||
                    !CPMValidateDeclaration(node, &compilerLog))
                {
                    compilerLog.Add(LOG_ERROR, "Invalid field declaration. Expected a type and name.",
                                    node->sourceFileName, node->lineNumber);
                    Error();
                    return;
                }
                const string& name = node->children[1]->text;
                if (structure->fields.count(name))
                {
                    compilerLog.Add(LOG_ERROR, "Repeated field '" + structure->name + "." + name + "'.",
                                    node->sourceFileName, node->lineNumber);
                    Error();
                    return;
                }
                CPMType type;
                if (!parseTypeExpr(node->children[0], type, getSourceFile(node->sourceFileName), structure->owner, &analyzer)) return;
                if (type.isVoid())
                {
                    compilerLog.Add(LOG_ERROR, "A field cannot have type void.", node->sourceFileName, node->lineNumber);
                    Error();
                    return;
                }
                auto& field = structure->fields[name];
                field.name = name;
                field.owner = structure->owner;
                field.typeExpr = std::move(type);
                structure->fieldOrder.push_back(name);
                view.push_back(name);
            }
            if (view.empty())
            {
                compilerLog.Add(LOG_ERROR, "An aggregate view must contain a field.", block->sourceFileName, block->lineNumber);
                Error();
                return;
            }
        }
    }

    int CPMCompiler::parseArraySizeDecl(CPMSyntaxTreeNode* countNode, CPMNamespace* currentNS)
    {
        return CPMSemanticAnalyzer(*this, currentNS).arrayCount(countNode);
    }

    void CPMCompiler::computeStructDataSize(CPMStructSymbol* structure)
    {
        if (structure->size >= 0 || !noErrors) return;
        if (!activeLayouts.insert(structure->type).second)
        {
            compilerLog.Add(LOG_ERROR, "Circular by-value layout for '" + structure->name + "'.",
                            structure->node->sourceFileName, structure->node->lineNumber);
            Error();
            return;
        }
        readStructFields(structure);
        int aggregateSize = 0;
        for (const auto& view : structure->views)
        {
            int viewSize = 0;
            for (const auto& name : view)
            {
                auto& field = structure->fields.at(name);
                ensureValueLayout(field.typeExpr);
                const int size = sizeOfTypeExpr(field.typeExpr);
                if (!noErrors) break;
                if (size <= 0 || size > 65535 - viewSize)
                {
                    compilerLog.Add(LOG_ERROR, "Aggregate type exceeds 65535 bytes.", structure->node->sourceFileName, structure->node->lineNumber);
                    Error();
                    break;
                }
                field.offset = static_cast<FRIDGE_DWORD>(viewSize);
                viewSize += size;
            }
            if (!noErrors) break;
            if (aggregateSize && aggregateSize != viewSize)
            {
                compilerLog.Add(LOG_ERROR, "Union views must have equal total sizes.", structure->node->sourceFileName, structure->node->lineNumber);
                Error();
                break;
            }
            aggregateSize = viewSize;
        }
        activeLayouts.erase(structure->type);
        if (noErrors) structure->size = aggregateSize;
    }

    void CPMCompiler::readFunctions()
    {
        for (map<string, CPMNamespace*>::iterator ins = namespaces.begin(); ins != namespaces.end(); ++ins)
        {
            CPMNamespace* ns = ins->second;
            for (int i = 0; i < ns->nodes.size(); i++)
                detectFunction(ns->nodes[i], ns);
        }
        if (!noErrors) return;
        for (auto& ns : namespaces)
            for (auto& entry : ns.second->functions)
                if (entry.second.bodyNode)
                    CPMSemanticAnalyzer(*this, ns.second).checkFunction(entry.second);
    }

    void CPMCompiler::detectFunction(CPMSyntaxTreeNode* node, CPMNamespace* owner)
    {
        if (node->type != CPM_LINE || node->children.empty()) return;
        auto head = node->children[0];
        if (head->text == R_STATIC || head->text == R_CONST || head->text == R_STRUCT ||
            head->text == R_UNION) return;
        auto fail = [&](const string& message)
        {
            compilerLog.Add(LOG_ERROR, message, node->sourceFileName, node->lineNumber);
            Error();
        };
        if (node->children.size() < 3)
        { fail("Expected a function, aggregate, static, or const declaration."); return; }
        auto name = node->children[1];
        auto args = node->children[2];
        const bool intrinsic = name->type == CPM_ID && name->text.compare(0, 2, "__") == 0;
        if (intrinsic && node->children.size() != 3)
        { fail("Intrinsic names are reserved for prototypes."); return; }
        if ((!intrinsic && !CPMIsUserIdentifier(name)) ||
            (args->type != CPM_EXPR && args->type != CPM_BLOCK))
        { fail("Invalid function declaration syntax."); return; }
        if (!CPMValidateDeclaration(node, &compilerLog)) { Error(); return; }
        if (node->children.size() > 4 ||
            (node->children.size() == 4 && node->children[3]->type != CPM_BLOCK &&
             (node->children[3]->type != CPM_EXPR || !node->children[3]->children.empty())))
        { fail("Invalid function declaration syntax."); return; }
        CPMSemanticAnalyzer analyzer(*this, owner, node);
        CPMType result;
        if (!parseTypeExpr(head, result, getSourceFile(node->sourceFileName), owner, &analyzer)) return;
        CPMFunctionSignature signature;
        signature.name = name->text;
        vector<CPMSyntaxTreeNode*> parameters;
        if (args->type == CPM_BLOCK)
        {
            for (size_t i = 1; i + 1 < args->children.size(); ++i)
                if (!args->children[i]->children.empty()) parameters.push_back(args->children[i]);
        }
        else if (!args->children.empty()) parameters.push_back(args);
        unordered_set<string> names;
        for (auto parameter : parameters)
        {
            CPMArgumentSignature argument;
            if (parameter->children.size() != 2 || !CPMIsUserIdentifier(parameter->children[1]) ||
                !CPMValidateDeclaration(parameter, &compilerLog))
            { fail("Invalid function parameter; use 'type-expr name'."); return; }
            if (!parseTypeExpr(parameter->children[0], argument.typeExpr, getSourceFile(node->sourceFileName), owner, &analyzer)) return;
            if (argument.typeExpr.isVoid()) { fail("Void type is not allowed for function parameters."); return; }
            argument.nameNode = parameter->children[1];
            argument.name = argument.nameNode->text;
            if (!names.insert(argument.name).second) { fail("Duplicate function parameter '" + argument.name + "'."); return; }
            signature.arguments.push_back(std::move(argument));
        }
        if (intrinsic && (!result.isVoid() || signature.arguments.size() != 1 ||
                          CPMFindIntrinsic(signature.name, signature.arguments.front().typeExpr) == CPMIntrinsic::None))
        {
            fail("Unknown intrinsic or invalid intrinsic signature for '" + signature.name + "'.");
            return;
        }
        auto found = owner->functions.find(signature);
        auto body = node->children.size() == 4 ? node->children[3] : nullptr;
        if (found != owner->functions.end())
        {
            auto& function = found->second;
            if (function.returnType != result) { fail("Conflicting function return types."); return; }
            for (size_t i = 0; i < signature.arguments.size(); ++i)
                if (function.signature.arguments[i].name != signature.arguments[i].name)
                { fail("Function parameter names must match the earlier declaration."); return; }
            if (function.bodyNode && body) { fail("Function is already defined."); return; }
            if (body)
            {
                function.bodyNode = body;
                for (size_t i = 0; i < signature.arguments.size(); ++i)
                    function.signature.arguments[i].nameNode = signature.arguments[i].nameNode;
            }
            return;
        }
        auto& function = owner->functions[signature];
        function.signature = signature;
        function.returnType = std::move(result);
        function.owner = owner;
        function.compiler = this;
        function.bodyNode = body;
        for (const auto& argument : signature.arguments)
        {
            CPMDataSymbol symbol;
            symbol.name = argument.name;
            symbol.typeExpr = argument.typeExpr;
            symbol.owner = owner;
            function.arguments.push_back(std::move(symbol));
        }
    }

    void CPMCompiler::readNamespaces()
    {
        auto add = [&](CPMNamespace* ns, CPMSyntaxTreeNode* node)
        {
            if (node->type != CPM_LINE || node->children.empty()) return;
            declarationOrder.emplace(node, declarations.size());
            declarations.emplace_back(ns, node);
            ns->nodes.push_back(node);
        };
        for (auto node : sourceNodes)
        {
            if (node->type != CPM_LINE || node->children.empty()) continue;
            if (node->children[0]->text != R_NAMESPACE)
            {
                add(namespaces.at(GlobalNamespace), node);
                continue;
            }
            const auto& parts = node->children;
            const bool fixedImport = parts.size() == 7;
            if ((parts.size() != 3 && parts.size() != 5 && !fixedImport) || !CPMIsUserIdentifier(parts[1]) ||
                parts.back()->type != CPM_BLOCK ||
                (parts.size() > 3 && (parts[2]->text != R_IMPORT || parts[3]->type != CPM_STR)) ||
                (fixedImport && (parts[4]->text != R_AT || parts[5]->type != CPM_NUM)))
            {
                compilerLog.Add(LOG_ERROR, "Invalid namespace declaration syntax.", node->sourceFileName, node->lineNumber);
                Error();
                continue;
            }
            const auto& name = parts[1]->text;
            if (namespaces.count(name))
            {
                compilerLog.Add(LOG_ERROR, "Namespace '" + name + "' is already declared.", node->sourceFileName, node->lineNumber);
                Error();
                continue;
            }
            int address = -1;
            if (fixedImport)
            {
                const auto origin = CPMSemanticAnalyzer(*this, namespaces.at(GlobalNamespace)).expression(parts[5]);
                if (!origin) continue;
                if (origin.type.base != CPM_DATATYPE_UINT8 && origin.type.base != CPM_DATATYPE_UINT16)
                {
                    compilerLog.Add(LOG_ERROR, "A binary origin must be an unsigned integer literal.",
                                    node->sourceFileName, node->lineNumber);
                    Error();
                    continue;
                }
                address = 0;
                for (auto byte : origin.bytes) address = (address << 8) | byte;
            }
            auto ns = new CPMNamespace(name);
            namespaces.emplace(name, ns);
            if (parts.size() > 3)
            {
                ns->importSource = CPMSTRContent(parts[3]);
                ns->importAddress = address;
                ns->importNode = parts[3];
            }
            for (auto child : parts.back()->children) add(ns, child);
            compilerLog.Add(LOG_MESSAGE, "Namespace " + name, node->sourceFileName, node->lineNumber);
        }

        for (map<string, CPMSourceFile>::iterator isrc = sources.begin(); isrc != sources.end(); ++isrc)
        {
            for (int i = 0; i < isrc->second.usingNamespaces.size(); i++)
            {
                map<string, CPMNamespace*>::iterator ins = namespaces.find(isrc->second.usingNamespaces[i]);
                if (ins == namespaces.end())
                {
                    compilerLog.Add(LOG_ERROR, "Cannot find namespace '" + isrc->second.usingNamespaces[i] + "'.", isrc->second.name, -1);
                    noErrors = false;
                }
            }
        }
    }

    int CPMCompiler::sizeOfType(CPMDataType type)
    {
        CPM_ASSERT(type >= CPM_DATATYPE_VOID);

        if (type == CPM_DATATYPE_VOID)
            return 0;
        if (type == CPM_DATATYPE_BOOL || type == CPM_DATATYPE_CHAR || type == CPM_DATATYPE_INT8 || type == CPM_DATATYPE_UINT8)
            return 1;
        else if (type < CPM_DATATYPE_USER)
            return 2;
        else
            return structTypes.count(type) ? structTypes.at(type)->size : -1;
    }

    int CPMCompiler::sizeOfData(CPMDataSymbol* symbol)
    {
        return sizeOfTypeExpr(symbol->typeExpr);
    }

    string CPMCompiler::GetTypeName(CPMDataType typeId, CPMNamespace* ns)
    {
        if (ns)
        {
            for (auto i = ns->datatypes.begin(); i != ns->datatypes.end(); ++i)
            {
                if (i->second == typeId)
                    return i->first;
            }
        }
        else
        {
            for (auto nsi = namespaces.begin(); nsi != namespaces.end(); ++nsi)
            {
                for (auto i = nsi->second->datatypes.begin(); i != nsi->second->datatypes.end(); ++i)
                {
                    if (i->second == typeId)
                        return i->first;
                }
            }
        }
        return "<undefined>";
    }

    string CPMCompiler::GetTypeName(const CPMType& type, bool qualifyAggregates)
    {
        if (type.isVoid()) return "void";
        if (type.isBase())
        {
            if (qualifyAggregates && type.isStruct())
            {
                auto found = structTypes.find(type.base);
                if (found != structTypes.end() && found->second->owner->name != GlobalNamespace)
                    return found->second->owner->name + "." + found->second->name;
            }
            return GetTypeName(type.base);
        }
        return "<" + GetTypeName(*type.child, qualifyAggregates) +
               (type.isPtr() ? " ptr>" : " array " + to_string(type.arrayCount) + ">");
    }

    // Internal helper: try to match `typeName` (which may include a
    // namespace-qualified `NS.Name` form) against a primitive or a
    // user-defined struct type, returning the matching CPMDataType and
    // setting `foundNS` to the owning namespace on success.
    bool CPMCompiler::resolveBaseTypeName(const string& typeName, CPMSourceFile* sourceFile,
                                    CPMNamespace* currentNS,
                                    CPMDataType& outBase, CPMNamespace*& outOwnerNS)
    {
        outOwnerNS = nullptr;

        // Primitives. `string` stays a special primitive per spec §3 footnote.
        if      (typeName == "void")    { outBase = CPM_DATATYPE_VOID;    return true; }
        else if (typeName == "bool")    { outBase = CPM_DATATYPE_BOOL;    return true; }
        else if (typeName == "char")    { outBase = CPM_DATATYPE_CHAR;    return true; }
        else if (typeName == "uint8")   { outBase = CPM_DATATYPE_UINT8;   return true; }
        else if (typeName == "uint16")  { outBase = CPM_DATATYPE_UINT16;  return true; }
        else if (typeName == "int8")    { outBase = CPM_DATATYPE_INT8;    return true; }
        else if (typeName == "int16")   { outBase = CPM_DATATYPE_INT16;   return true; }
        else if (typeName == "string")  { outBase = CPM_DATATYPE_STRING;  return true; }

        // Struct lookup: try the current namespace, then using-list, then global.
        // Caller pre-split any NS.Name into namespace + local name; for the
        // plain unqualified form here, we walk the standard scope order.

        // Single-segment name: walk scope chain.
        if (currentNS)
        {
            map<string, CPMDataType>::iterator idt = currentNS->datatypes.find(typeName);
            if (idt != currentNS->datatypes.end())
            {
                outBase = idt->second;
                outOwnerNS = currentNS;
                return true;
            }
        }
        vector<CPMNamespace*> matches;
        auto consider = [&](CPMNamespace* ns)
        {
            if (!ns->datatypes.count(typeName) || find(matches.begin(), matches.end(), ns) != matches.end()) return;
            matches.push_back(ns);
        };
        consider(namespaces.at(GlobalNamespace));
        if (sourceFile)
            for (const auto& name : sourceFile->usingNamespaces)
                if (namespaces.count(name)) consider(namespaces.at(name));
        if (matches.size() != 1)
        {
            if (matches.size() > 1)
            {
                compilerLog.Add(LOG_ERROR, "Ambiguous type '" + typeName + "'.");
                Error();
            }
            return false;
        }
        outOwnerNS = matches[0];
        outBase = matches[0]->datatypes.at(typeName);
        return true;
    }

    bool CPMCompiler::parseTypeExpr(CPMSyntaxTreeNode* node, CPMType& out, CPMSourceFile* sourceFile,
                                    CPMNamespace* currentNS, CPMSemanticAnalyzer* analyzer)
    {
        auto fail = [&](const string& message) -> bool
        {
            compilerLog.Add(LOG_ERROR, message, node ? node->sourceFileName : "", node ? node->lineNumber : -1);
            noErrors = false;
            return false;
        };
        if (!node) return fail("Missing type expression.");

        CPMType result;
        if (node->type == CPM_TYPEEXPR)
        {
            if (node->children.size() < 2)
                return fail("An angle-delimited type requires a 'ptr' or 'array' modifier.");
            if (!parseTypeExpr(node->children[0], result, sourceFile, currentNS, analyzer))
                return false;
            // Modifiers wrap the preceding type from left to right. This makes
            // <T ptr ptr> equivalent to <<T ptr> ptr>, without flattening it.
            for (size_t i = 1; i < node->children.size(); ++i)
            {
                auto modifier = node->children[i];
                if (modifier->type != CPM_ID || (modifier->text != R_PTR && modifier->text != R_ARRAY))
                    return fail("Expected 'ptr' or 'array' in a type expression.");
                if (result.isVoid())
                    return fail("Pointers to void and arrays of void are not allowed.");
                CPMType wrapped;
                if (modifier->text == R_PTR)
                    wrapped.kind = CPM_TYPE_PTR;
                else
                {
                    if (++i == node->children.size())
                        return fail("An array modifier requires a constant count.");
                    auto countNode = node->children[i];
                    const int count = analyzer ? analyzer->arrayCount(countNode) : parseArraySizeDecl(countNode, currentNS);
                    if (count <= 0) return false;
                    const int childSize = sizeOfTypeExpr(result, currentNS);
                    if (count > 65535 || (childSize > 0 && count > 65535 / childSize))
                        return fail("The total size of a type expression must not exceed 65535 bytes.");
                    wrapped.kind = CPM_TYPE_ARRAY;
                    wrapped.arrayCount = count;
                }
                wrapped.child.reset(new CPMType(std::move(result)));
                result = std::move(wrapped);
            }
        }
        else if (node->type == CPM_ID)
        {
            CPMDataType base;
            CPMNamespace* owner = nullptr;
            if (!resolveBaseTypeName(node->text, sourceFile, currentNS, base, owner))
                return fail("Unknown type '" + node->text + "'.");
            result.kind = base == CPM_DATATYPE_VOID ? CPM_TYPE_VOID : CPM_TYPE_BASE;
            result.base = base;
        }
        else if (node->type == CPM_REF && node->children.size() == 2)
        {
            const string& nsName = node->children[0]->text;
            const string& localName = node->children[1]->text;
            if (localName == "void" || localName == "bool" || localName == "char" ||
                localName == "uint8" || localName == "uint16" || localName == "int8" ||
                localName == "int16" || localName == "string")
                return fail("Primitive type '" + localName + "' cannot be namespace-qualified.");
            auto ns = namespaces.find(nsName);
            if (ns == namespaces.end()) return fail("Unknown namespace '" + nsName + "' in type expression.");
            auto type = ns->second->datatypes.find(localName);
            if (type == ns->second->datatypes.end())
                return fail("Type '" + localName + "' not found in namespace '" + nsName + "'.");
            result.kind = CPM_TYPE_BASE;
            result.base = type->second;
        }
        else if (node->type == CPM_EXPR)
            return fail("Compound types use '<...>'; parentheses group value expressions.");
        else
            return fail("Expected a type name or angle-delimited type expression.");

        out = std::move(result);
        return true;
    }

    int CPMCompiler::sizeOfTypeExpr(const CPMType& type, CPMNamespace* currentNS)
    {
        switch (type.kind)
        {
        case CPM_TYPE_VOID:
            return 0;
        case CPM_TYPE_BASE:
            return sizeOfType(type.base);
        case CPM_TYPE_PTR:
            return 2; // Fridge pointer width is 16-bit.
        case CPM_TYPE_ARRAY:
            if (!type.child) return 0;
            {
                int element = sizeOfTypeExpr(*type.child, currentNS);
                if (element <= 0 || type.arrayCount <= 0 || type.arrayCount > 65535 / element) return -1;
                return type.arrayCount * element;
            }
        }
        return 0;
    }

    bool CPMCompiler::isTypeDeclaration(CPMSyntaxTreeNode* node, CPMNamespace* owner)
    {
        if (node->children.empty()) return false;
        auto head = node->children[0];
        if (head->type == CPM_TYPEEXPR) return true;
        if (head->type == CPM_REF)
            return head->children.size() == 2 && namespaces.count(head->children[0]->text) &&
                namespaces.at(head->children[0]->text)->datatypes.count(head->children[1]->text);
        if (head->type == CPM_ID && !head->text.empty() && head->text[0] == '#') return true;
        if (head->type != CPM_ID) return false;
        CPMDataType base;
        CPMNamespace* ns = nullptr;
        if (resolveBaseTypeName(head->text, getSourceFile(node->sourceFileName), owner, base, ns)) return true;
        return false;
    }

    bool CPMCompiler::parseDataDeclaration(CPMSyntaxTreeNode* node, CPMType& type, CPMNamespace* owner,
                                           CPMSemanticAnalyzer* analyzer)
    {
        if (node->children.size() < 2 || node->children.size() > 3 || node->children[1]->type != CPM_ID ||
            (node->children.size() == 3 && (node->children[2]->text == R_ARRAY || node->children[2]->text == R_IMPORT)) ||
            !CPMValidateDeclaration(node, &compilerLog))
        {
            compilerLog.Add(LOG_ERROR, "Invalid data declaration; use 'type-expr name [initializer]'.", node->sourceFileName, node->lineNumber);
            Error(); return false;
        }
        if (!parseTypeExpr(node->children[0], type, getSourceFile(node->sourceFileName), owner, analyzer)) return false;
        if (type.isVoid() || sizeOfTypeExpr(type) > DataMaxSize)
        {
            compilerLog.Add(LOG_ERROR, "Local data must have a non-void type fitting available memory.", node->sourceFileName, node->lineNumber);
            Error(); return false;
        }
        return true;
    }

    CPMStaticSymbol* CPMCompiler::resolveStaticSymbolName(CPMSyntaxTreeNode* nameNode, CPMSourceFile* sourceFile, CPMSyntaxTreeNode* syntaxNode, CPMNamespace* currentNS)
    {
        CPMStaticSymbol* result = NULL;

        if (nameNode->type == CPM_REF && nameNode->children.size() > 1)
        {
            CPMNamespace* ns = namespaces[GlobalNamespace];
            int nameCounter = 0;
            map<string, CPMNamespace*>::iterator ins = namespaces.find(nameNode->children[nameCounter]->text);
            if (ins != namespaces.end())
            {
                ns = ins->second;
                nameCounter++;
            }
            map<string, CPMStaticSymbol>::iterator iss = ns->statics.find(nameNode->children[nameCounter]->text);
            if (iss != ns->statics.end())
            {
                result = &iss->second;
                nameCounter++;
            }
            else
            {
                return nullptr;
            }
            /*
            if (parsedName.size() >= nameCounter)
            {
                for (; nameCounter < parsedName.size(); ++nameCounter)
                {
                    if (result->field.typeExpr.base >= CPM_DATATYPE_USER)
                    {
                        result->field
                    }
                    else
                    {
                        compilerLog.Add(LOG_ERROR, "Cannot subscript non-struct symbol '" + result->field.name + "'.", sourceFile->name, syntaxNode->lineNumber);
                        Error();
                        return nullptr;
                    }
                }
            }
            */
            /*map<string, CPMNamespace>::iterator ins = namespaces.find(parsedName[0]);
            if (ins != namespaces.end())
            {
                map<string, CPMStaticSymbol>::iterator iss = ins->second.statics.find(parsedName[1]);
                if (iss != ins->second.statics.end())
                {
                    return &iss->second;
                }
            }*/
        }
        else if (nameNode->type == CPM_ID)
        {
            vector<string> candidatesNamespaces;
            map<string, CPMStaticSymbol>::iterator iss;
            if (currentNS)
            {
                iss = currentNS->statics.find(nameNode->text);
                if (iss != currentNS->statics.end())
                    return &iss->second;
            }
            iss = namespaces[GlobalNamespace]->statics.find(nameNode->text);
            if (iss != namespaces[GlobalNamespace]->statics.end())
            {
                result = &iss->second;
                candidatesNamespaces.push_back(GlobalNamespace);
            }

            for (int i = 0; i < sourceFile->usingNamespaces.size(); i++)
            {
                string ns = sourceFile->usingNamespaces[i];
                iss = namespaces[ns]->statics.find(nameNode->text);
                if (iss != namespaces[ns]->statics.end())
                {
                    result = &iss->second;
                    candidatesNamespaces.push_back(ns);
                }
            }

            if (candidatesNamespaces.size() > 1)
            {
                compilerLog.Add(LOG_ERROR, "Ambiguous static symbol reference '" + nameNode->text + "'. Candidates are: ", sourceFile->name);
                for (int i = 0; i < candidatesNamespaces.size(); i++)
                {
                    compilerLog.Add(candidatesNamespaces[i] + CPM_SUBSCRIPT_DELIM + nameNode->text);
                    if (i < candidatesNamespaces.size() - 1)
                        compilerLog.Add(", ");
                }
                return NULL;
            }
        }

        return result;
    }

    void CPMCompiler::getNamespaces(vector<CPMNamespace*>& nslist)
    {
        nslist.clear();
        for (auto i = namespaces.begin(); i != namespaces.end(); ++i)
            nslist.push_back(i->second);
    }

    string CPMCompiler::printStaticValue(CPMStaticSymbol* symbol, CPMDataSymbol& field)
    {
        string result = GetTypeName(field.typeExpr) + " " + field.name + " = ";
        if (symbol->importSource >= 0) return result + "imports " + to_string(symbol->importSource);
        if (!symbol->staticData)
            return result + (field.typeExpr.isString() ? field.owner->staticReadString(symbol->immediateData) : to_string(symbol->immediateData));
        if (field.typeExpr.isArray())
        {
            result += "(";
            CPMDataSymbol element = field;
            element.typeExpr = *field.typeExpr.child;
            for (int i = 0; i < field.typeExpr.arrayCount; ++i)
            {
                element.offset = field.offset + i * sizeOfTypeExpr(element.typeExpr);
                if (i) result += "; ";
                result += printStaticValue(symbol, element);
            }
            return result + ")";
        }
        if (field.typeExpr.isPtr() || IsIntDataType(field.typeExpr.base) || field.typeExpr.base == CPM_DATATYPE_BOOL)
            return result + printStaticNumber(symbol, field);
        if (field.typeExpr.isString()) return result + printStaticString(symbol, field);
        if (field.typeExpr.base == CPM_DATATYPE_CHAR) return result + printStaticChar(symbol, field);
        return result + printStaticStruct(symbol, field);
    }

    string CPMCompiler::printStaticNumber(CPMStaticSymbol* symbol, CPMDataSymbol& field, int index)
    {
        int val = 0;
        size_t typeSize = field.typeExpr.isPtr() ? sizeof(FRIDGE_DWORD) : sizeOfType(field.typeExpr.base);
        if (typeSize == 1)
            val = field.owner->staticBuffer[field.offset + index];
        else if (typeSize == 2)
            val = FRIDGE_DWORD_HL(field.owner->staticBuffer[field.offset + 2*index],
                                  field.owner->staticBuffer[field.offset + 2*index + 1]);
        return to_string(val);
    }

    string CPMCompiler::printStaticString(CPMStaticSymbol* symbol, CPMDataSymbol& field, int index)
    {
        FRIDGE_DWORD soffset = FRIDGE_DWORD_HL(field.owner->staticBuffer[field.offset + 2 * index],
                                               field.owner->staticBuffer[field.offset + 2 * index + 1]);
        return string((char*)&(field.owner->staticBuffer[soffset]));
    }

    string CPMCompiler::printStaticChar(CPMStaticSymbol* symbol, CPMDataSymbol& field, int index)
    {
        return string(1, (char)field.owner->staticBuffer[field.offset + index]);
    }

    string CPMCompiler::printStaticStruct(CPMStaticSymbol* symbol, CPMDataSymbol& field, int index)
    {
        string s = "(";
        CPMStructSymbol* ss = structTypes[field.typeExpr.base];

        FRIDGE_DWORD typeSize = sizeOfType(field.typeExpr.base);
        for (map<string, CPMDataSymbol>::iterator fi = ss->fields.begin(); fi != ss->fields.end(); ++fi)
        {
            CPMDataSymbol sfield = fi->second;
            sfield.offset += field.offset + typeSize * index;
            sfield.owner = symbol->field.owner;
            s += printStaticValue(symbol, sfield) + "; ";
        }

        s += ")";
        return s;
    }

    void CPMCompiler::PrintStaticData()
    {
        compilerLog.Add(LOG_MESSAGE, "Static data:\n");
        for (map<string, CPMNamespace*>::iterator nsi = namespaces.begin(); nsi != namespaces.end(); ++nsi)
        {
            compilerLog.Add("\n" + nsi->first + ":\n");
            for (map<string, CPMStaticSymbol>::iterator ssi = nsi->second->statics.begin(); ssi != nsi->second->statics.end(); ++ssi)
            {
                compilerLog.Add("   " + printStaticValue(&ssi->second, ssi->second.field) + "\n");
            }
        }
    }

    CPMCompiler::~CPMCompiler()
    {
        for (map<string, CPMNamespace*>::iterator nsi = namespaces.begin(); nsi != namespaces.end(); ++nsi)
        {
            delete nsi->second;
        }
        for (auto& source : sources) delete source.second.parser;
    }

}
