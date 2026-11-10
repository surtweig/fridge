#include "fridge.h"
#include "stdafx.h"
#include "CPMCompiler.h"
#include "CPMIntermediate.h"
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

    CPMCompiler::CPMCompiler(string sourceRootFolder, string sourceFileName, string outputFile, vector<string> includeFolders) : compilerLog(), asmDebugOutput()
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
        globalOffset = FRIDGE_EXECUTABLE_OFFSET;

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

        addNumStatic("null", global, true, CPM_DATATYPE_UINT16, 0);
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

        readNamespaces();
        readStatics(1);
        if (noErrors) readStructs();
        if (noErrors) readStatics(2);
        if (noErrors) readFunctions();

        if (noErrors)
        {
            CPMIntermediate im(this);
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

                                string includedSourceFileName = CPMSTRContent(node);
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
                            }
                        }
                    }
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

    CPMStaticSymbol* CPMCompiler::addStrStatic(string name, CPMNamespace* ns, bool isConst, string data)
    {
        ns->statics[name].field.name = name;
        CPMStaticSymbol* ss = &(ns->statics.find(name)->second);
        ss->isconst = isConst;
        ss->field.typeExpr.kind = CPM_TYPE_BASE;
        ss->field.typeExpr.base = CPM_DATATYPE_STRING;
        ss->field.owner = ns;

        FRIDGE_WORD* sdata = ns->staticAllocate(data);
        if (sdata == nullptr)
        {
            compilerLog.Add(LOG_ERROR, "Failed to allocate static data for symbol '")->Add(ns->name)->Add(".")->Add(name)->Add("'");
            Error();
            return ss;
        }
        size_t sdataOffset = ns->getStaticRelativeAddress(sdata);

        if (isConst)
        {
            // Const string: the value (relative address of the chars) is
            // known at compile time, so the symbol is immediate.
            ss->staticData = nullptr;
            ss->immediateData = (int)sdataOffset;
        }
        else
        {
            // Static string variable: a 2-byte slot in the buffer holding the
            // relative address of the chars (patched to absolute at code
            // generation via staticStrings).
            ss->staticData = ns->staticAllocate(nullptr, sizeOfType(CPM_DATATYPE_STRING), 1);
            if (ss->staticData == nullptr)
            {
                compilerLog.Add(LOG_ERROR, "Failed to allocate static data for symbol '")->Add(ns->name)->Add(".")->Add(name)->Add("'");
                Error();
                return ss;
            }
            ss->field.offset = (FRIDGE_DWORD)ns->getStaticRelativeAddress(ss->staticData);
            ns->staticWrite(ss->field.offset, (FRIDGE_DWORD)sdataOffset);
            ss->staticStrings.push_back(ss->field.offset);
        }
        return ss;
    }

    CPMStaticSymbol* CPMCompiler::addLiteral(CPMFunctionSymbol* func, CPMSyntaxTreeNode* valueNode)
    {
        if (!func || !valueNode)
            return nullptr;

        func->literals.push_back(CPMStaticSymbol());
        CPMStaticSymbol* lit = &func->literals.back();
        lit->isconst = true;
        lit->importSource = -1;
        lit->staticData = nullptr;
        lit->immediateData = 0;
        lit->declNode = valueNode;
        lit->field.name = string(1, ServiceSymbol) + "lit" + to_string(func->literals.size() - 1);
        lit->field.owner = func->owner;
        lit->field.offset = 0;

        if (valueNode->type == CPM_NUM || valueNode->type == CPM_EXPR || valueNode->type == CPM_LINE)
        {
            lit->field.typeExpr.kind = CPM_TYPE_BASE;
            lit->field.typeExpr.base = CPM_DATATYPE_UINT8;
            if (!parseLiteralNumber(lit, lit->field, valueNode, 0, true))
            {
                // parseLiteralNumber already logged the failure.
                func->literals.pop_back();
                return nullptr;
            }
            return lit;
        }

        if (valueNode->type == CPM_CHARLIT)
        {
            lit->field.typeExpr.kind = CPM_TYPE_BASE;
            lit->field.typeExpr.base = CPM_DATATYPE_CHAR;
            if (valueNode->text.empty())
            {
                compilerLog.Add(LOG_ERROR, "Empty char literal.", valueNode->sourceFileName, valueNode->lineNumber);
                Error();
                func->literals.pop_back();
                return nullptr;
            }
            lit->immediateData = (unsigned char)valueNode->text[0];
            return lit;
        }

        if (valueNode->type == CPM_STR)
        {
            // Spec §5.6: double-quoted `"..."` is always a `string` literal;
            // the legacy length-1 `"x"`-as-char overload is removed. Char
            // values use the single-quoted `CPM_CHARLIT` form handled above.
            lit->field.typeExpr.kind = CPM_TYPE_BASE;
            lit->field.typeExpr.base = CPM_DATATYPE_STRING;
            if (!parseAndAllocateLiteralString(lit, lit->field, valueNode, 0))
            {
                func->literals.pop_back();
                return nullptr;
            }
            return lit;
        }

        compilerLog.Add(LOG_ERROR, "Unsupported literal form '" + valueNode->text + "'.", valueNode->sourceFileName, valueNode->lineNumber);
        Error();
        func->literals.pop_back();
        return nullptr;
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

    void CPMCompiler::readStatics(int phase)
    {
        for (map<string, CPMNamespace*>::iterator ins = namespaces.begin(); ins != namespaces.end(); ++ins)
        {
            CPMNamespace* ns = ins->second;
            for (int i = 0; i < ns->nodes.size(); i++)
                detectStatic(ns->nodes[i], ns, phase);
        }
    }

    void CPMCompiler::detectStatic(CPMSyntaxTreeNode* node, CPMNamespace* owner, int phase)
    {
        if (node->type != CPM_LINE || node->children.empty()) return;
        const string& storage = node->children[0]->text;
        if (storage != R_CONST && storage != R_STATIC) return;
        const bool isConst = storage == R_CONST;
        // Only bare primitive constants are needed before struct layout. Defer
        // compound types until every struct name and layout is available.
        if (phase == 1)
        {
            if (!isConst || node->children.size() < 3 || node->children[1]->type != CPM_ID) return;
            auto type = namespaces[GlobalNamespace]->datatypes.find(node->children[1]->text);
            if (type == namespaces[GlobalNamespace]->datatypes.end() || type->second <= 0 ||
                type->second >= CPM_DATATYPE_USER) return;
        }
        auto fail = [&](const string& message)
        {
            compilerLog.Add(LOG_ERROR, message, node->sourceFileName, node->lineNumber);
            Error();
        };
        if (!CPMValidateDeclaration(node, &compilerLog)) { Error(); return; }
        if (node->children.size() < 3 || node->children.size() > 5 || node->children[2]->type != CPM_ID)
        { fail("Invalid static or const declaration syntax."); return; }
        auto name = node->children[2];
        auto existing = owner->statics.find(name->text);
        if (existing != owner->statics.end())
        {
            if (existing->second.declNode != node)
                fail("Static symbol '" + name->text + "' is already declared in " + owner->name + " namespace.");
            return;
        }
        CPMType type;
        if (!parseTypeExpr(node->children[1], type, getSourceFile(node->sourceFileName), owner)) return;
        if (type.isVoid()) { fail("Void type is not allowed for static or const data."); return; }
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
            address = parseNum(source->text);
            if (isConst || address < 0 || address > 65535 || size > 65536 - address)
            { fail("Invalid imported static address or size."); return; }
        }
        else if (node->children.size() == 4)
        {
            value = node->children[3];
            if (value->text == R_ARRAY || value->text == R_IMPORT)
            { fail("Invalid declaration initializer; compound types use '<...>'."); return; }
        }
        auto symbol = addStatic(name->text, owner, isConst, type, address);
        symbol->declNode = node;
        if (noErrors && value) parseLiteralValue(symbol, symbol->field, value);
    }

    void CPMCompiler::readStructs()
    {
        for (map<string, CPMNamespace*>::iterator ins = namespaces.begin(); ins != namespaces.end(); ++ins)
        {
            CPMNamespace* ns = ins->second;
            for (int i = 0; i < ns->nodes.size(); i++)
                detectStruct(ns->nodes[i], ns);
        }
        for (map<CPMDataType, CPMStructSymbol*>::iterator ist = structTypes.begin(); ist != structTypes.end(); ++ist)
        {
            readStructFields(ist->second);
        }
        for (map<CPMDataType, CPMStructSymbol*>::iterator ist = structTypes.begin(); ist != structTypes.end(); ++ist)
        {
            computeStructDataSize(ist->second);
        }
        if (noErrors)
            for (const auto& entry : structTypes)
            {
                auto structure = entry.second;
                function<bool(const CPMType&)> valid = [&](const CPMType& type)
                {
                    if (type.child && !valid(*type.child)) return false;
                    return sizeOfTypeExpr(type) >= 0;
                };
                for (const auto& field : structure->fields)
                    if (!valid(field.second.typeExpr))
                    {
                        compilerLog.Add(LOG_ERROR, "Field type exceeds 65535 bytes.", structure->node->sourceFileName, structure->node->lineNumber);
                        Error();
                    }
            }
    }

    void CPMCompiler::detectStruct(CPMSyntaxTreeNode* node, CPMNamespace* owner)
    {
        if (node->type == CPM_LINE && node->children.size() > 0)
        {
            if (node->children[0]->type == CPM_ID && (node->children[0]->text == R_STRUCT || node->children[0]->text == R_UNION))
            {
                if (node->children.size() == 3)
                {
                    if (node->children[1]->type == CPM_ID && node->children[2]->type == CPM_BLOCK)
                    {
                        string stname = node->children[1]->text;
                        map<string, CPMDataType>::iterator idt = owner->datatypes.find(stname);
                        if (idt != owner->datatypes.end())
                        {
                            compilerLog.Add(LOG_ERROR, "Type '" + stname + "' is already declared in " + owner->name + " namespace.", node->sourceFileName, node->children[0]->lineNumber);
                            noErrors = false;
                            return;
                        }

                        owner->structs[stname].name = stname;
                        owner->structs[stname].node = node->children[2];
                        owner->structs[stname].isUnion = node->children[0]->text == R_UNION;
                        owner->structs[stname].size = -1;
                        owner->structs[stname].owner = owner;
                        owner->datatypes[stname] = registerStructDataType(&owner->structs[stname]);
                        compilerLog.Add(LOG_MESSAGE, "Struct " + stname + " in " + owner->name, node->sourceFileName, node->children[0]->lineNumber);
                        return;
                    }
                }
                compilerLog.Add(LOG_ERROR, "Invalid struct or union declaration syntax.", node->sourceFileName, node->children[0]->lineNumber);
                noErrors = false;
            }
        }
    }

    CPMDataType CPMCompiler::registerStructDataType(CPMStructSymbol* structSymbol)
    {
        structSymbol->type = dataTypeCounter;
        structTypes[dataTypeCounter] = structSymbol;
        return dataTypeCounter++;
    }

    void CPMCompiler::readStructFields(CPMStructSymbol* structure)
    {
        for (size_t i = 1; i + 1 < structure->node->children.size(); ++i)
        {
            auto node = structure->node->children[i];
            if (node->type == CPM_LINE && node->children.empty()) continue;
            if (node->type != CPM_LINE || node->children.size() != 2 || node->children[1]->type != CPM_ID ||
                !CPMValidateDeclaration(node, &compilerLog))
            {
                compilerLog.Add(LOG_ERROR, "Invalid struct field declaration; use 'type-expr name'.", node->sourceFileName, node->lineNumber);
                Error(); return;
            }
            const string& name = node->children[1]->text;
            if (structure->fields.count(name))
            {
                compilerLog.Add(LOG_ERROR, "Field '" + structure->name + "." + name + "' is already declared.", node->sourceFileName, node->lineNumber);
                Error(); return;
            }
            CPMType type;
            if (!parseTypeExpr(node->children[0], type, getSourceFile(node->sourceFileName), structure->owner)) return;
            if (type.isVoid())
            {
                compilerLog.Add(LOG_ERROR, "Void type is not allowed for struct fields.", node->sourceFileName, node->lineNumber);
                Error(); return;
            }
            auto& field = structure->fields[name];
            field.name = name;
            field.owner = structure->owner;
            field.typeExpr = std::move(type);
            structure->fieldOrder.push_back(name);
        }
        if (structure->fields.empty())
        {
            compilerLog.Add(LOG_ERROR, "Empty structs are not allowed.", structure->node->sourceFileName, structure->node->lineNumber);
            Error();
        }
    }

    int CPMCompiler::parseArraySizeDecl(CPMSyntaxTreeNode* countNode, CPMNamespace* currentNS)
    {
        if (countNode->type == CPM_NUM)
        {
            int size = parseNum(countNode->text);
            if (size > 0)
                return size;
            else
            {
                compilerLog.Add(LOG_ERROR, "Array size must be positive non-zero integer.", countNode->sourceFileName, countNode->lineNumber);
                noErrors = false;
                return 0;
            }
        }
        else if (countNode->type == CPM_ID || countNode->type == CPM_REF)
        {
            CPMStaticSymbol* sizeConst = resolveStaticSymbolName(countNode, &sources[countNode->sourceFileName], countNode, currentNS);
            if (sizeConst)
            {
                if (sizeConst->isconst)
                {
                    if (sizeConst->field.typeExpr.isBase() && IsIntDataType(sizeConst->field.typeExpr.base))
                    {
                        int size = (int)sizeConst->immediateData;
                        if (size > 0)
                            return size;
                        else
                        {
                            compilerLog.Add(LOG_ERROR, "Array size must be positive non-zero integer.", countNode->sourceFileName, countNode->lineNumber);
                            noErrors = false;
                            return 0;
                        }
                    }
                    else
                    {
                        compilerLog.Add(LOG_ERROR, "Array size value must be an integer.", countNode->sourceFileName, countNode->lineNumber);
                        noErrors = false;
                        return 0;
                    }
                }
                else
                {
                    compilerLog.Add(LOG_ERROR, "Array size value must be constant.", countNode->sourceFileName, countNode->lineNumber);
                    noErrors = false;
                    return 0;
                }
            }
            else
            {
                compilerLog.Add(LOG_ERROR, "Array size value '" + countNode->text + "' is not declared or ambiguous in this context.", countNode->sourceFileName, countNode->lineNumber);
                noErrors = false;
                return 0;
            }
        }
        else
        {
            compilerLog.Add(LOG_ERROR, "Invalid array declaration: '" + countNode->text + "' cannot be evaluated as an array size.", countNode->sourceFileName, countNode->lineNumber);
            noErrors = false;
            return 0;
        }
    }

    void CPMCompiler::computeStructDataSize(CPMStructSymbol* structSymbol)
    {
        unordered_set<CPMDataType> visitedStructs;
        computeStructDataSize(structSymbol, visitedStructs);
    }

    void CPMCompiler::computeStructDataSize(CPMStructSymbol* structure, unordered_set<CPMDataType>& active)
    {
        if (structure->size >= 0) return;
        if (!active.insert(structure->type).second)
        {
            compilerLog.Add(LOG_ERROR, "Circular by-value struct declaration in '" + structure->name + "'.",
                            structure->node->sourceFileName, structure->node->lineNumber);
            Error(); return;
        }
        function<int(const CPMType&)> sizeOf = [&](const CPMType& type) -> int
        {
            if (type.isPtr()) return 2; // A pointer breaks by-value recursion.
            if (type.isArray())
            {
                int element = sizeOf(*type.child);
                return element <= 0 || type.arrayCount > 65535 / element ? -1 : element * type.arrayCount;
            }
            if (type.isBase() && type.base >= CPM_DATATYPE_USER)
            {
                auto target = structTypes.at(type.base);
                computeStructDataSize(target, active);
                return target->size;
            }
            return sizeOfTypeExpr(type);
        };
        int total = 0;
        for (const auto& name : structure->fieldOrder)
        {
            int size = sizeOf(structure->fields.at(name).typeExpr);
            if (!noErrors) break;
            if (size <= 0 || (!structure->isUnion && size > 65535 - total))
            {
                compilerLog.Add(LOG_ERROR, "Struct type exceeds 65535 bytes.", structure->node->sourceFileName, structure->node->lineNumber);
                Error(); break;
            }
            if (structure->isUnion && total && total != size)
            {
                compilerLog.Add(LOG_ERROR, "Union fields must have equal sizes.", structure->node->sourceFileName, structure->node->lineNumber);
                Error(); break;
            }
            total = structure->isUnion ? size : total + size;
        }
        active.erase(structure->type);
        if (noErrors)
        {
            structure->size = total;
            buildStructLayout(structure);
        }
    }

    void CPMCompiler::readFunctions()
    {
        for (map<string, CPMNamespace*>::iterator ins = namespaces.begin(); ins != namespaces.end(); ++ins)
        {
            CPMNamespace* ns = ins->second;
            for (int i = 0; i < ns->nodes.size(); i++)
                detectFunction(ns->nodes[i], ns);
        }
        for (auto& ns : namespaces)
            for (auto& entry : ns.second->functions)
            {
                auto& function = entry.second;
                // Type operands may occur inside initializers, return values,
                // and nested calls even where executable lowering is pending.
                std::function<void(CPMSyntaxTreeNode*)> validateCasts = [&](CPMSyntaxTreeNode* node)
                {
                    if (node->type == CPM_TYPEEXPR) return;
                    if ((node->type == CPM_EXPR || node->type == CPM_LINE) && !node->children.empty() &&
                        node->children[0]->text == R_CAST)
                    {
                        if (node->children.size() != 3 || !CPMValidateCall(node, &compilerLog))
                        {
                            compilerLog.Add(LOG_ERROR, "Cast takes a type and one value.", node->sourceFileName, node->lineNumber);
                            Error(); return;
                        }
                        CPMType type;
                        if (!parseTypeExpr(node->children[1], type, getSourceFile(node->sourceFileName), function.owner)) return;
                        if (type.isVoid() || type.isArray() || type.isStruct())
                        {
                            compilerLog.Add(LOG_ERROR, "Cast requires a scalar target type.", node->sourceFileName, node->lineNumber);
                            Error(); return;
                        }
                        validateCasts(node->children[2]);
                        return;
                    }
                    for (auto child : node->children) validateCasts(child);
                };
                std::function<void(CPMSyntaxTreeNode*)> validate = [&](CPMSyntaxTreeNode* node)
                {
                    if (node->type == CPM_BLOCK)
                    {
                        for (auto child : node->children) validate(child);
                        return;
                    }
                    if (node->type != CPM_LINE || node->children.empty()) return;
                    auto head = node->children[0];
                    const string& op = head->text;
                    if (op == "if" || op == "else" || op == "for")
                    {
                        for (size_t i = 1; i < node->children.size(); ++i) validate(node->children[i]);
                        return;
                    }
                    if (isTypeDeclaration(node, function.owner))
                    {
                        CPMType type;
                        parseDataDeclaration(node, type, function.owner);
                    }
                    else if (op == R_STATIC || op == R_CONST || op == "return")
                    {
                        if (!CPMValidateDeclaration(node, &compilerLog)) Error();
                        if (op == R_STATIC || op == R_CONST)
                        {
                            // Strip storage only for declaration/type validation.
                            CPMSyntaxTreeNode declaration;
                            declaration.type = CPM_LINE;
                            declaration.sourceFileName = node->sourceFileName;
                            declaration.lineNumber = node->lineNumber;
                            declaration.children.assign(node->children.begin() + 1, node->children.end());
                            CPMType type;
                            parseDataDeclaration(&declaration, type, function.owner);
                        }
                    }
                    else if (!CPMValidateCall(node, &compilerLog)) Error();
                };
                if (function.bodyNode)
                {
                    validateCasts(function.bodyNode);
                    validate(function.bodyNode);
                }
            }
    }

    void CPMCompiler::detectFunction(CPMSyntaxTreeNode* node, CPMNamespace* owner)
    {
        if (node->type != CPM_LINE || node->children.size() < 3) return;
        auto head = node->children[0];
        if (head->text == R_STATIC || head->text == R_CONST || head->text == R_STRUCT ||
            head->text == R_UNION) return;
        auto name = node->children[1];
        auto args = node->children[2];
        if (name->type != CPM_ID || (args->type != CPM_EXPR && args->type != CPM_BLOCK)) return;
        auto fail = [&](const string& message)
        {
            compilerLog.Add(LOG_ERROR, message, node->sourceFileName, node->lineNumber);
            Error();
        };
        if (!CPMValidateDeclaration(node, &compilerLog)) { Error(); return; }
        if (node->children.size() > 4 || (node->children.size() == 4 && node->children[3]->type != CPM_BLOCK))
        { fail("Invalid function declaration syntax."); return; }
        CPMType result;
        if (!parseTypeExpr(head, result, getSourceFile(node->sourceFileName), owner)) return;
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
            if (parameter->children.size() != 2 || parameter->children[1]->type != CPM_ID ||
                !CPMValidateDeclaration(parameter, &compilerLog))
            { fail("Invalid function parameter; use 'type-expr name'."); return; }
            if (!parseTypeExpr(parameter->children[0], argument.typeExpr, getSourceFile(node->sourceFileName), owner)) return;
            if (argument.typeExpr.isVoid()) { fail("Void type is not allowed for function parameters."); return; }
            argument.name = parameter->children[1]->text;
            if (!names.insert(argument.name).second) { fail("Duplicate function parameter '" + argument.name + "'."); return; }
            signature.arguments.push_back(std::move(argument));
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
            if (body) function.bodyNode = body;
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
        for (map<string, CPMSourceFile>::iterator isrc = sources.begin(); isrc != sources.end(); ++isrc)
        {
            vector<CPMSyntaxTreeNode*> layer;
            isrc->second.parser->GetLayer(layer);
            for (vector<CPMSyntaxTreeNode*>::iterator li = layer.begin(); li != layer.end(); ++li)
            {
                CPMSyntaxTreeNode* node = (*li);
                if (node->type == CPM_LINE && node->children.size() > 0)
                {
                    if (node->children[0]->type == CPM_ID && node->children[0]->text == R_NAMESPACE)
                    {
                        if (node->children.size() == 3 || node->children.size() == 5)
                        {
                            int blockChildIndex = node->children.size()-1;
                            if (node->children[1]->type == CPM_ID && node->children[blockChildIndex]->type == CPM_BLOCK)
                            {
                                bool valid = true;
                                string nsname = node->children[1]->text;
                                CPMNamespace *ns = new CPMNamespace(nsname);
                                namespaces[nsname] = ns;
                                for (int i = 0; i < node->children[blockChildIndex]->children.size(); i++)
                                    if (node->children[blockChildIndex]->children[i]->type != CPM_CHAR)
                                        ns->nodes.push_back(node->children[blockChildIndex]->children[i]);

                                if (node->children.size() == 5)
                                {
                                    if (node->children[2]->type == CPM_ID && node->children[2]->text == R_IMPORT && node->children[3]->type == CPM_STR)
                                    {
                                        ns->importSource = CPMSTRContent(node->children[3]);
                                    }
                                    else
                                        valid = false;
                                }

                                if (valid)
                                {
                                    compilerLog.Add(LOG_MESSAGE, "Namespace " + nsname, node->sourceFileName, node->children[0]->lineNumber);
                                    continue;
                                }
                            }
                        }

                        compilerLog.Add(LOG_ERROR, "Invalid namespace declaration syntax.", node->sourceFileName, node->children[0]->lineNumber);
                        noErrors = false;
                    }
                    else
                    {
                        namespaces[GlobalNamespace]->nodes.push_back(node);
                    }
                }
                else if (node->type != CPM_CHAR)
                {
                    compilerLog.Add(LOG_ERROR, "Unexpected syntax node.", node->sourceFileName, node->lineNumber);
                    noErrors = false;
                }
            }
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

    void CPMCompiler::buildStructLayout(CPMStructSymbol* structSymbol)
    {
        unsigned int position = 0;
        for (const auto& name : structSymbol->fieldOrder)
        {
            auto fi = structSymbol->fields.find(name);
            if (structSymbol->isUnion)
                fi->second.offset = 0;
            else
            {
                fi->second.offset = position;
                //compilerLog.Add(LOG_MESSAGE, "Field '" + structSymbol->name + "." + fi->second.name + "' offset = ", structSymbol->node->sourceFileName, structSymbol->node->lineNumber)->Add(fi->second.offset);
                position += sizeOfData(&fi->second);
                if (position > 65535)
                {
                    compilerLog.Add(LOG_ERROR, "Struct '" + structSymbol->name + "' is too large.", structSymbol->node->sourceFileName, structSymbol->node->lineNumber);
                    noErrors = false;
                }
            }
        }
    }

    /*
    vector<string> CPMCompiler::ParseSymbolName(const string& name)
    {
        vector<string> parsedName;
        stringstream sname;
        sname.str(name);
        string subname;
        while (getline(sname, subname, CPM_SUBSCRIPT_DELIM))
            parsedName.push_back(subname);
        return parsedName;
    }
    */

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

    string CPMCompiler::GetTypeName(const CPMType& type)
    {
        if (type.isVoid()) return "void";
        if (type.isBase()) return GetTypeName(type.base);
        return "<" + GetTypeName(*type.child) + (type.isPtr() ? " ptr>" : " array " + to_string(type.arrayCount) + ">");
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

    bool CPMCompiler::parseTypeExpr(CPMSyntaxTreeNode* node, CPMType& out, CPMSourceFile* sourceFile, CPMNamespace* currentNS)
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
            if (!parseTypeExpr(node->children[0], result, sourceFile, currentNS))
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
                    int count = 0;
                    if (countNode->type == CPM_EXPR)
                    {
                        vector<CPMUnfoldedExpressionNode> unfolded;
                        if (!parseExpression(countNode, unfolded)) return false;
                        bool ok = false;
                        count = staticEvalNum(unfolded, ok, currentNS);
                        if (!ok) return false;
                    }
                    else
                        count = parseArraySizeDecl(countNode, currentNS);
                    if (count <= 0)
                        return fail("Array size must be a positive constant integer.");
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

    bool CPMCompiler::parseDataDeclaration(CPMSyntaxTreeNode* node, CPMType& type, CPMNamespace* owner)
    {
        if (node->children.size() < 2 || node->children.size() > 3 || node->children[1]->type != CPM_ID ||
            (node->children.size() == 3 && (node->children[2]->text == R_ARRAY || node->children[2]->text == R_IMPORT)) ||
            !CPMValidateDeclaration(node, &compilerLog))
        {
            compilerLog.Add(LOG_ERROR, "Invalid data declaration; use 'type-expr name [initializer]'.", node->sourceFileName, node->lineNumber);
            Error(); return false;
        }
        if (!parseTypeExpr(node->children[0], type, getSourceFile(node->sourceFileName), owner)) return false;
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

    CPMFunctionSymbol* CPMCompiler::resolveFunctionSymbolName(const string &name, CPMSourceFile* sourceFile, CPMNamespace* currentNS)
    {
        return NULL;
    }

    int CPMCompiler::parseNum(const string &num)
    {
        int base = 10;
        if (num.size() >= 3)
            if (num[0] == '0' && num[1] == 'x')
                base = 16;
        errno = 0;
        char* end = nullptr;
        long long value = strtoll(num.c_str(), &end, base);
        if (errno == ERANGE || end == num.c_str() || *end ||
            value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        {
            compilerLog.Add(LOG_ERROR, "Invalid or oversized integer literal '" + num + "'.");
            Error(); return 0;
        }
        return static_cast<int>(value);
    }

    bool CPMCompiler::parseExpression(CPMSyntaxTreeNode* root, vector<CPMUnfoldedExpressionNode> &unfolded)
    {
        if (root->type != CPM_EXPR && root->type != CPM_LINE && root->type != CPM_ID &&
            root->type != CPM_NUM && root->type != CPM_REF)
        {
            compilerLog.Add(LOG_ERROR, "Expected a value expression, not a type or initializer list.", root->sourceFileName, root->lineNumber);
            Error(); return false;
        }

        if (root->type == CPM_EXPR || root->type == CPM_LINE)
        {
            if (root->children.empty())
            {
                compilerLog.Add(LOG_ERROR, "Expected a value or operator expression.", root->sourceFileName, root->lineNumber);
                noErrors = false;
                return false;
            }
            if (root->children.size() == 1)
                return parseExpression(root->children[0], unfolded);
            if (!CPMValidateCall(root, &compilerLog))
            {
                noErrors = false;
                return false;
            }
            if (root->children[0]->text == R_CAST)
            {
                if (root->children.size() != 3)
                {
                    compilerLog.Add(LOG_ERROR, "Cast takes a type and one value.", root->sourceFileName, root->lineNumber);
                    Error(); return false;
                }
                unfolded.push_back({1, root->children[0], root->children[1]});
                return parseExpression(root->children[2], unfolded);
            }
            unfolded.push_back({ (int)root->children.size() - 1, root->children[0] });
            for (int i = 1; i < root->children.size(); ++i)
            {
                if (!parseExpression(root->children[i], unfolded))
                    return false;
            }
        }
        else
            unfolded.push_back({ 0, root });

        return true;
    }

    int CPMCompiler::staticEvalNum(vector<CPMUnfoldedExpressionNode> &unfolded, bool& ok, CPMNamespace* currentNS, bool silent)
    {
        ok = true;
        stack<int> evalStack;
        for (int i = unfolded.size() - 1; i >= 0; --i)
        {
            if (unfolded[i].operandsNumber == 0)
            {
                if (unfolded[i].syntaxNode->type == CPM_NUM)
                    evalStack.push(parseNum(unfolded[i].syntaxNode->text));
                else if (unfolded[i].syntaxNode->type == CPM_ID || unfolded[i].syntaxNode->type == CPM_REF)
                {
                    CPMStaticSymbol* sym = resolveStaticSymbolName(unfolded[i].syntaxNode, &sources[unfolded[i].syntaxNode->sourceFileName], unfolded[i].syntaxNode, currentNS);
                    if (sym)
                    {
                        if (sym->isconst)
                        {
                            if (sym->field.typeExpr.isBase() && IsIntDataType(sym->field.typeExpr.base))
                            {
                                evalStack.push((int)sym->immediateData);
                            }
                            else
                            {
                                ok = false;
                                if (!silent)
                                {
                                    compilerLog.Add(LOG_ERROR, "Symbol '" + unfolded[i].syntaxNode->text + "' is not an integer.", unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                                    noErrors = false;
                                }
                                return 0;
                            }
                        }
                        else
                        {
                            ok = false;
                            if (!silent)
                            {
                                compilerLog.Add(LOG_ERROR, "Symbol '" + unfolded[i].syntaxNode->text + "' is not a constant.", unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                                noErrors = false;
                            }
                            return 0;
                        }
                    }
                    else
                    {
                        ok = false;
                        if (!silent)
                        {
                            compilerLog.Add(LOG_ERROR, "Symbol '" + unfolded[i].syntaxNode->text + "' is not declared or ambiguous in this context.", unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                            noErrors = false;
                        }
                        return 0;
                    }
                }
                else
                {
                    ok = false;
                    if (!silent)
                    {
                        compilerLog.Add(LOG_ERROR, "Cannot evaluate '" + unfolded[i].syntaxNode->text + "' as an integer.", unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                        noErrors = false;
                    }
                    return 0;
                }
            }
            else
            {
                vector<int> args;
                for (int argi = 0; argi < unfolded[i].operandsNumber; ++argi)
                {
                    CPM_ASSERT(!evalStack.empty());
                    args.push_back(evalStack.top());
                    evalStack.pop();
                }

                string op = unfolded[i].syntaxNode->text;
                int result = 0;
                auto checked = [&](long long value)
                {
                    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
                    {
                        compilerLog.Add(LOG_ERROR, "Constant expression exceeds the integer evaluator's range.",
                            unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                        Error(); ok = false; return false;
                    }
                    result = static_cast<int>(value);
                    return true;
                };
                if (unfolded[i].castTarget)
                {
                    CPMType target;
                    if (!parseTypeExpr(unfolded[i].castTarget, target,
                        getSourceFile(unfolded[i].syntaxNode->sourceFileName), currentNS))
                    { ok = false; return 0; }
                    if (target.isVoid() || target.isArray() || target.isStruct())
                    {
                        compilerLog.Add(LOG_ERROR, "Cast requires a scalar target type.", unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                        Error(); ok = false; return 0;
                    }
                    int bits = sizeOfTypeExpr(target) * 8;
                    result = args[0] & ((1 << bits) - 1);
                    if (target.isBase() && (target.base == CPM_DATATYPE_INT8 || target.base == CPM_DATATYPE_INT16) &&
                        (result & (1 << (bits - 1)))) result -= 1 << bits;
                    if (target.isBase() && target.base == CPM_DATATYPE_BOOL) result = args[0] != 0;
                }
                else if (op == "+")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                        if (!checked(static_cast<long long>(result) + args[argi])) return 0;
                }
                else if (op == "-")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                        if (!checked(static_cast<long long>(result) - args[argi])) return 0;
                }
                else if (op == "*")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                        if (!checked(static_cast<long long>(result) * args[argi])) return 0;
                }
                else if (op == "/")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                    {
                        if (args[argi] == 0)
                        {
                            compilerLog.Add(LOG_ERROR, "Division by zero.", unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                            noErrors = false;
                            return 0;
                        }
                        if (!checked(static_cast<long long>(result) / args[argi])) return 0;
                    }
                }
                else if (op == "&")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                        result &= args[argi];
                }
                else if (op == "|")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                        result |= args[argi];
                }
                else if (op == "~")
                {
                    if (args.size() != 1)
                    {
                        compilerLog.Add(LOG_ERROR, "Bitwise NOT operator only accepts one argument.", unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                        noErrors = false;
                        return 0;
                    }
                    result = ~args[0];
                }
                else if (op == "^")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                        result ^= args[argi];
                }
                else if (op == "<" || op == "<=" || op == ">" || op == ">=" || op == "==" || op == "!=")
                {
                    if (args.size() < 2)
                    {
                        compilerLog.Add(LOG_ERROR, "Comparison requires at least two operands.", unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                        Error(); ok = false; return 0;
                    }
                    result = 1;
                    for (size_t n = 1; n < args.size(); ++n)
                    {
                        if (op == "<") result &= args[n - 1] < args[n];
                        else if (op == "<=") result &= args[n - 1] <= args[n];
                        else if (op == ">") result &= args[n - 1] > args[n];
                        else if (op == ">=") result &= args[n - 1] >= args[n];
                        else if (op == "==") result &= args[n - 1] == args[n];
                        else
                            for (size_t previous = 0; previous < n; ++previous)
                                result &= args[previous] != args[n];
                    }
                }
                else if (op == ">>")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                    {
                        if (args[argi] < 0 || args[argi] >= std::numeric_limits<unsigned int>::digits)
                        {
                            compilerLog.Add(LOG_ERROR, "Constant shift count is out of range.", unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                            Error(); ok = false; return 0;
                        }
                        result >>= args[argi];
                    }
                }
                else if (op == "<<")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                    {
                        if (args[argi] < 0 || args[argi] >= std::numeric_limits<unsigned int>::digits)
                        {
                            compilerLog.Add(LOG_ERROR, "Constant shift count is out of range.", unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                            Error(); ok = false; return 0;
                        }
                        if (!checked(static_cast<long long>(result) * (1LL << args[argi]))) return 0;
                    }
                }
                else
                {
                    ok = false;
                    if (!silent)
                    {
                        compilerLog.Add(LOG_ERROR, "Operator '" + op + "' cannot be evaluated at compile time.", unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                        noErrors = false;
                    }
                    return 0;
                }
                evalStack.push(result);
            }
        }
        return evalStack.top();
    }

    bool CPMCompiler::parseLiteralStruct(CPMStaticSymbol* symbol, CPMDataSymbol& field, CPMSyntaxTreeNode* valueNode, int index)
    {
        if (valueNode->type != CPM_BLOCK && valueNode->type != CPM_EXPR)
        {
            compilerLog.Add(LOG_ERROR, "Invalid literal struct syntax.", valueNode->sourceFileName, valueNode->lineNumber);
            noErrors = false;
            return false;
        }

        map<CPMDataType, CPMStructSymbol*>::iterator ist = structTypes.find(field.typeExpr.base);
        if (ist == structTypes.end())
        {
            compilerLog.Add(LOG_ERROR, "Unknown structure type '" + GetTypeName(field.typeExpr.base) + "'.", valueNode->sourceFileName, valueNode->lineNumber);
            noErrors = false;
            return false;
        }
        CPMStructSymbol* structInstance = ist->second;

        // Named fields are semicolon-separated bindings. A single binding
        // without a trailing semicolon has the same grouping as an expression;
        // the declaration's struct type supplies the initializer context.
        vector<pair<CPMSyntaxTreeNode*, CPMSyntaxTreeNode*>> fieldInits;
        vector<CPMSyntaxTreeNode*> entries;
        if (valueNode->type == CPM_BLOCK)
            entries.assign(valueNode->children.begin() + 1, valueNode->children.end() - 1);
        else
            entries.push_back(valueNode);
        for (auto entry : entries)
        {
            const bool hasComma = find(entry->commaBefore.begin(), entry->commaBefore.end(), true) != entry->commaBefore.end();
            if (entry->children.size() != 2 || entry->children[0]->type != CPM_ID || hasComma)
            {
                compilerLog.Add(LOG_ERROR, "Struct initializers use 'field value; field value' bindings, not operand commas.",
                    entry->sourceFileName, entry->lineNumber);
                noErrors = false;
                return false;
            }
            fieldInits.push_back(make_pair(entry->children[0], entry->children[1]));
        }

        for (size_t i = 0; i < fieldInits.size(); ++i)
        {
            CPMSyntaxTreeNode* fieldName  = fieldInits[i].first;
            CPMSyntaxTreeNode* fieldValue = fieldInits[i].second;

            map<string, CPMDataSymbol>::iterator fi = structInstance->fields.find(fieldName->text);
            if (fi != structInstance->fields.end())
            {
                CPMDataSymbol sfield = fi->second;
                sfield.offset += field.offset + index * sizeOfType(field.typeExpr.base);
                // The field's layout entry belongs to the struct's
                // namespace, which may differ from the namespace the
                // static was declared in; redirect writes and name
                // resolution to the symbol's own buffer.
                sfield.owner = symbol->field.owner;
                if (!parseLiteralValue(symbol, sfield, fieldValue))
                    return false;
            }
            else
            {
                compilerLog.Add(LOG_ERROR, "Structure " + structInstance->name + " does not contain '" + fieldName->text + "' field.", fieldName->sourceFileName, fieldName->lineNumber);
                noErrors = false;
                return false;
            }
        }
        return true;
    }

    bool CPMCompiler::parseLiteralValue(CPMStaticSymbol* symbol, CPMDataSymbol& field, CPMSyntaxTreeNode* value)
    {
        if (value->type == CPM_LINE && value->children.size() == 1) value = value->children[0];
        if (field.typeExpr.isArray())
        {
            vector<CPMSyntaxTreeNode*> elements;
            if (value->type == CPM_BLOCK)
                elements.assign(value->children.begin() + 1, value->children.end() - 1);
            else if (value->type == CPM_EXPR)
                elements.push_back(value); // Single element, with no trailing semicolon.
            if (elements.size() != (size_t)field.typeExpr.arrayCount)
            {
                compilerLog.Add(LOG_ERROR, "Array initializer must supply exactly its declared number of semicolon-separated elements.", value->sourceFileName, value->lineNumber);
                Error(); return false;
            }
            CPMDataSymbol element = field;
            element.typeExpr = *field.typeExpr.child;
            const int stride = sizeOfTypeExpr(element.typeExpr);
            for (size_t i = 0; i < elements.size(); ++i)
            {
                element.offset = field.offset + i * stride;
                auto item = elements[i];
                if (item->type == CPM_EXPR && item->children.size() == 1) item = item->children[0];
                if (!parseLiteralValue(symbol, element, item)) return false;
            }
            return true;
        }
        if (field.typeExpr.isPtr() || IsIntDataType(field.typeExpr.base) || field.typeExpr.base == CPM_DATATYPE_BOOL)
            return parseLiteralNumber(symbol, field, value);
        if (field.typeExpr.isString()) return parseAndAllocateLiteralString(symbol, field, value);
        if (field.typeExpr.base == CPM_DATATYPE_CHAR) return parseLiteralChar(symbol, field, value);
        if (field.typeExpr.isStruct()) return parseLiteralStruct(symbol, field, value);
        compilerLog.Add(LOG_ERROR, "Invalid initializer type.", value->sourceFileName, value->lineNumber);
        Error(); return false;
    }

    bool CPMCompiler::parseLiteralNumber(CPMStaticSymbol* symbol, CPMDataSymbol& field, CPMSyntaxTreeNode* valueNode, int index, bool autoType)
    {
        if (!autoType)
            CPM_ASSERT(IsIntDataType(field.typeExpr.base) || field.typeExpr.isPtr() || field.typeExpr.base == CPM_DATATYPE_BOOL)

        while ((valueNode->type == CPM_EXPR || valueNode->type == CPM_LINE) && valueNode->children.size() == 1)
            valueNode = valueNode->children[0];
        CPMType valueType;
        bool typedValue = false;
        CPMStaticSymbol* reference = nullptr;
        if ((valueNode->type == CPM_EXPR || valueNode->type == CPM_LINE) && valueNode->children.size() == 3 &&
            valueNode->children[0]->text == R_CAST)
        {
            if (!parseTypeExpr(valueNode->children[1], valueType, getSourceFile(valueNode->sourceFileName), field.owner)) return false;
            typedValue = true;
        }
        else if (valueNode->type == CPM_ID || valueNode->type == CPM_REF)
        {
            reference = resolveStaticSymbolName(valueNode, getSourceFile(valueNode->sourceFileName), valueNode, field.owner);
            if (reference) { valueType = reference->field.typeExpr; typedValue = true; }
        }
        if (!autoType && (field.typeExpr.isPtr() || (typedValue && valueType.isPtr())) &&
            (!typedValue || valueType != field.typeExpr))
        {
            compilerLog.Add(LOG_ERROR, "Pointer initialization requires its exact type; use an explicit cast.", valueNode->sourceFileName, valueNode->lineNumber);
            Error(); return false;
        }
        if (autoType && typedValue)
        {
            field.typeExpr = valueType;
            autoType = false;
        }

        if (valueNode->type == CPM_NUM || valueNode->type == CPM_ID || valueNode->type == CPM_REF || valueNode->type == CPM_EXPR || valueNode->type == CPM_LINE)
        {
            int val;
            if (valueNode->type == CPM_NUM)
                val = parseNum(valueNode->text);
            else if (reference && reference->isconst && reference->field.typeExpr.isPtr())
                val = reference->immediateData;
            else
            {
                vector<CPMUnfoldedExpressionNode> unfolded;
                if (!parseExpression(valueNode, unfolded))
                    return false;
                bool ok;
                val = staticEvalNum(unfolded, ok, field.owner);
                if (!ok)
                {
                    compilerLog.Add(LOG_ERROR, "Cannot evaluate number value.", valueNode->sourceFileName, valueNode->lineNumber);
                    noErrors = false;
                    return false;
                }
            }

            if (autoType)
            {
                if (val >= UINT8_Min)
                {
                    if (val < UINT8_Max)
                        field.typeExpr.base = CPM_DATATYPE_UINT8;
                    else
                        field.typeExpr.base = CPM_DATATYPE_UINT16;
                }
                else
                {
                    if (val >= INT8_Min)
                        field.typeExpr.base = CPM_DATATYPE_INT8;
                    else
                        field.typeExpr.base = CPM_DATATYPE_INT16;
                }
            }

            if (field.typeExpr.base == CPM_DATATYPE_BOOL)
                val = val > 0 ? 1 : 0;

            bool outofrange = false;
            if (!field.typeExpr.isPtr())
            {
                switch (field.typeExpr.base)
                {
                case CPM_DATATYPE_INT8:
                    if (val < INT8_Min || val > INT8_Max)
                        outofrange = true;
                    break;
                case CPM_DATATYPE_INT16:
                    if (val < INT16_Min || val > INT16_Max)
                        outofrange = true;
                    break;
                case CPM_DATATYPE_UINT8:
                    if (val < UINT8_Min || val > UINT8_Max)
                        outofrange = true;
                    break;
                case CPM_DATATYPE_UINT16:
                    if (val < UINT16_Min || val > UINT16_Max)
                        outofrange = true;
                    break;
                }
            }
            else
            {
                if (val < UINT16_Min || val > UINT16_Max)
                    outofrange = true;
            }

            if (outofrange)
            {
                compilerLog.Add(LOG_ERROR, "Value is out of range.", valueNode->sourceFileName, valueNode->lineNumber);
                noErrors = false;
                return false;
            }
            else
            {
                // Immediate symbols (const scalars, const pointers, literals)
                // store the value itself; buffer-backed symbols store it at
                // the field's offset in the namespace static buffer. The
                // decision depends only on the symbol's storage class, not on
                // the (possibly nested struct) field being written.
                if (symbol->staticData == nullptr)
                    symbol->immediateData = val;
                else
                {
                    int typeSize = field.typeExpr.isPtr() ? (int)sizeof(FRIDGE_DWORD) : sizeOfType(field.typeExpr.base);

                    if (typeSize == 1)
                        field.owner->staticWrite(field.offset + index*typeSize, (FRIDGE_WORD)val);
                    else
                        field.owner->staticWrite(field.offset + index*typeSize, (FRIDGE_DWORD)val);
                }
                //field.data[index] = (FRIDGE_WORD*)val;
                return true;
            }
        }
        else
        {
            compilerLog.Add(LOG_ERROR, "Cannot evaluate '" + valueNode->text + "' as a number.", valueNode->sourceFileName, valueNode->lineNumber);
            noErrors = false;
            return false;
        }
    }

    bool CPMCompiler::parseAndAllocateLiteralString(CPMStaticSymbol* symbol, CPMDataSymbol& field, CPMSyntaxTreeNode* valueNode, int index)
    {
        CPM_ASSERT(field.typeExpr.base == CPM_DATATYPE_STRING);

        if (valueNode->type == CPM_STR)
        {
            CPMNamespace* ns = symbol->field.owner;
            string s = CPMSTRContent(valueNode);
            FRIDGE_WORD* sdata = ns->staticAllocate(s);
            if (sdata)
            {
                size_t sdataOffset = ns->getStaticRelativeAddress(sdata);//sdata - symbol->staticData;
                CPM_ASSERT(sdataOffset < FRIDGE_MAX_DWORD);

                if (symbol->staticData == nullptr)
                {
                    // Const string: the symbol's value is the relative address
                    // of the chars, resolved to absolute at code generation.
                    symbol->immediateData = (int)sdataOffset;
                }
                else
                {
                    // A 2-byte pointer slot (a static string variable or a
                    // string field inside a buffer-backed struct) holds the
                    // relative address of the chars.
                    FRIDGE_DWORD slotOffset = field.offset + index * sizeOfType(CPM_DATATYPE_STRING);
                    ns->staticWrite(slotOffset, (FRIDGE_DWORD)sdataOffset);
                    symbol->staticStrings.push_back(slotOffset); // Actual string addresses will be resolved at code generation stage
                }
            }
            else
            {
                compilerLog.Add(LOG_ERROR, "Failed to allocate static data for symbol '", valueNode->sourceFileName, valueNode->lineNumber)->Add(ns->name)->Add(".")->Add(field.name)->Add("'");
                Error();
                return false;
            }
            /*
            FRIDGE_WORD* s_ptr = (FRIDGE_WORD*)malloc(s.size() + 1);
            memcpy(s_ptr, s.c_str(), s.size());
            s_ptr[s.size()] = 0;

            field.data[index] = s_ptr;*/


            return true;
        }
        else
        {
            compilerLog.Add(LOG_ERROR, "Cannot parse '" + valueNode->text + "' as a string literal.", valueNode->sourceFileName, valueNode->lineNumber);
            noErrors = false;
            return false;
        }
    }
bool CPMCompiler::parseLiteralChar(CPMStaticSymbol* symbol, CPMDataSymbol& field, CPMSyntaxTreeNode* valueNode, int index)
    {
        CPM_ASSERT(field.typeExpr.base == CPM_DATATYPE_CHAR);

        FRIDGE_WORD val;
        if (valueNode->type == CPM_CHARLIT)
        {
            // `'x'`: text carries the single quotes (mirrors CPM_STR shape),
            // so the enclosed char is text[1]. Spec §5.6 makes single quotes
            // the only char-literal form.
            if (valueNode->text.size() != 3)
            {
                compilerLog.Add(LOG_ERROR, "Invalid char literal.", valueNode->sourceFileName, valueNode->lineNumber);
                noErrors = false;
                return false;
            }
            val = (FRIDGE_WORD)valueNode->text[1];
        }
        else if (valueNode->type == CPM_NUM)
        {
            int ccode = parseNum(valueNode->text);
            if (ccode < 0 || ccode > 255)
            {
                compilerLog.Add(LOG_ERROR, "Value is out of range.", valueNode->sourceFileName, valueNode->lineNumber);
                noErrors = false;
                return false;
            }
            val = (FRIDGE_WORD)ccode;
        }
        else
        {
            compilerLog.Add(LOG_ERROR, "Cannot parse '" + valueNode->text + "' as a char literal.", valueNode->sourceFileName, valueNode->lineNumber);
            noErrors = false;
            return false;
        }

        if (symbol->staticData == nullptr)
            symbol->immediateData = val;
        else
            field.owner->staticWrite(field.offset + index, val);

        return true;
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
    }

}
