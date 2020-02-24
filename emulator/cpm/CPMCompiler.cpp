#include "stdafx.h"
#include "CPMCompiler.h"
#include "CPMIntermediate.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>

namespace CPM
{

    CPMDataSymbol::CPMDataSymbol()
    {
        name = "?";
        type = CPM_DATATYPE_UNDEFINED;
        count = 1;
        isPtr = false;
        offset = 0;
        data = {};
        owner = nullptr;
    }

    CPMDataSymbol::~CPMDataSymbol()
    {
        if (type < CPM_DATATYPE_USER)
            return;
        if (type == CPM_DATATYPE_STRING)
        {

        }
        else
        {
            for (int i = 0; i < data.size(); ++i)
                delete (CPMStructSymbol*)data[i];
        }
    }

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

    CPMStaticSymbol::CPMStaticSymbol()
    {        
        isconst = false;
        importSource = -1;
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
            if (arguments[i].type != other.arguments[i].type)
                return other.arguments[i].type < arguments[i].type;
            else if (arguments[i].isPtr != other.arguments[i].isPtr)
                return arguments[i].isPtr;
        }
        return false;
    }

    CPMCompiler::CPMCompiler(string sourceRootFolder, string sourceFileName, string outputFile, vector<string> includeFolders) : compilerLog(), asmDebugOutput()
    {
        noErrors = true;
        outputFileName = outputFile;
        this->includeFolders = includeFolders;
        preprocessSourceFile(sourceRootFolder, sourceFileName);
        dataTypeCounter = CPM_DATATYPE_USER;
        globalOffset = FRIDGE_EXECUTABLE_OFFSET;

        namespaces[GlobalNamespace].name = GlobalNamespace;
        CPMNamespace* global = &namespaces[GlobalNamespace];
        global->datatypes["void"] = CPM_DATATYPE_VOID;
        global->datatypes["bool"] = CPM_DATATYPE_BOOL;
        global->datatypes["char"] = CPM_DATATYPE_CHAR;
        global->datatypes["uint8"] = CPM_DATATYPE_UINT8;
        global->datatypes["uint16"] = CPM_DATATYPE_UINT16;
        global->datatypes["int8"] = CPM_DATATYPE_INT8;
        global->datatypes["int16"] = CPM_DATATYPE_INT16;
        global->datatypes["string"] = CPM_DATATYPE_STRING;

        addStatic("null", global, true, CPM_DATATYPE_UINT16, 0);
        addStatic("false", global, true, CPM_DATATYPE_BOOL, 0);
        addStatic("true", global, true, CPM_DATATYPE_BOOL, 1);
        addStatic("FRIDGE_ROM_SEGMENT_SIZE", global, true, CPM_DATATYPE_UINT16, FRIDGE_ROM_SEGMENT_SIZE);
        addStatic("FRIDGE_MAX_IO_DEVICES", global, true, CPM_DATATYPE_UINT8, FRIDGE_MAX_IO_DEVICES);
        addStatic("FRIDGE_GPU_BUS_SIZE", global, true, CPM_DATATYPE_UINT8, FRIDGE_GPU_BUS_SIZE);
        addStatic("FRIDGE_GPU_FRAME_EGA_WIDTH", global, true, CPM_DATATYPE_UINT8, FRIDGE_GPU_FRAME_EGA_WIDTH);
        addStatic("FRIDGE_GPU_FRAME_EGA_HEIGHT", global, true, CPM_DATATYPE_UINT8, FRIDGE_GPU_FRAME_EGA_HEIGHT);
        addStatic("FRIDGE_GPU_MAX_SPRITES", global, true, CPM_DATATYPE_UINT8, FRIDGE_GPU_MAX_SPRITES);
        addStatic("FRIDGE_GPU_MAX_SPRITES_PER_PIXEL", global, true, CPM_DATATYPE_UINT8, FRIDGE_GPU_MAX_SPRITES_PER_PIXEL);
        addStatic("FRIDGE_GPU_FRAME_BUFFER_SIZE", global, true, CPM_DATATYPE_UINT16, FRIDGE_GPU_FRAME_BUFFER_SIZE);
        addStatic("FRIDGE_BOOT_SECTION_INDEX_ADDRESS", global, true, CPM_DATATYPE_UINT16, FRIDGE_BOOT_SECTION_INDEX_ADDRESS);
        addStatic("FRIDGE_EXECUTABLE_OFFSET", global, true, CPM_DATATYPE_UINT16, FRIDGE_EXECUTABLE_OFFSET);
        addStatic("FRIDGE_IRQ_SYS_TIMER", global, true, CPM_DATATYPE_UINT16, FRIDGE_IRQ_SYS_TIMER);
        addStatic("FRIDGE_IRQ_KEYBOARD_PRESS", global, true, CPM_DATATYPE_UINT16, FRIDGE_IRQ_KEYBOARD_PRESS);
        addStatic("FRIDGE_IRQ_KEYBOARD_RELEASE", global, true, CPM_DATATYPE_UINT16, FRIDGE_IRQ_KEYBOARD_RELEASE);
        addStatic("FRIDGE_KEYBOARD_KEY_STATE_MASK", global, true, CPM_DATATYPE_UINT8, FRIDGE_KEYBOARD_KEY_STATE_MASK);
        addStatic("FRIDGE_KEYBOARD_KEY_CODE_MASK", global, true, CPM_DATATYPE_UINT8, FRIDGE_KEYBOARD_KEY_CODE_MASK);
        addStatic("FRIDGE_DEV_ROM_RESET_ID", global, true, CPM_DATATYPE_UINT8, FRIDGE_DEV_ROM_RESET_ID);
        addStatic("FRIDGE_DEV_ROM_ID", global, true, CPM_DATATYPE_UINT8, FRIDGE_DEV_ROM_ID);
        addStatic("FRIDGE_DEV_KEYBOARD_ID", global, true, CPM_DATATYPE_UINT8, FRIDGE_DEV_KEYBOARD_ID);

        readNamespaces();
        readStatics(true);
        readStructs();
        readStatics(false);
        readFunctions();

        CPMIntermediate im(this);

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

    CPMStaticSymbol* CPMCompiler::addStatic(string name, CPMNamespace* ns, bool isConst, CPMDataType type, int data)
    {
        ns->statics[name].field.name = name;
        CPMStaticSymbol* ss = &(ns->statics.find(name)->second);
        ss->isconst = isConst;
        ss->field.type = type;
        ss->field.data.push_back((FRIDGE_WORD*)data);
        return ss;
    }

    void CPMCompiler::readStatics(bool isConst)
    {
        for (map<string, CPMNamespace>::iterator ins = namespaces.begin(); ins != namespaces.end(); ++ins)
        {
            CPMNamespace* ns = &(*ins).second;
            for (int i = 0; i < ns->nodes.size(); i++)
                detectStatic(ns->nodes[i], ns, isConst);
        }
    }

    void CPMCompiler::detectStatic(CPMSyntaxTreeNode* node, CPMNamespace* owner, bool isConst)
    {
        if (node->type != CPM_LINE || node->children.size() == 0)
            return;
        if (node->children[0]->type != CPM_ID)
            return;
        if ((isConst && node->children[0]->text != R_CONST) || (!isConst && node->children[0]->text != R_STATIC))
            return;

        bool isInvalid = false;

        if (node->children.size() < 3 || node->children.size() > 7)
        {
            isInvalid = true;
        }
        else
        {
            CPMSyntaxTreeNode* declNode = node->children[0];
            CPMSyntaxTreeNode* typeNode = node->children[1];
            CPMSyntaxTreeNode* nameNode = node->children[2];
            CPMSyntaxTreeNode* countNode = NULL;
            CPMSyntaxTreeNode* valueNode = NULL;
            CPMSyntaxTreeNode* importSourceNode = NULL;
            if (nameNode->type == CPM_ID && typeNode->type == CPM_ID)
            {
                map<string, CPMStaticSymbol>::iterator iss = owner->statics.find(nameNode->text);
                if (iss == owner->statics.end())
                {
                    CPMStaticSymbol* ss = addStatic(nameNode->text, owner, isConst);

                    /*
                    string typeName = typeNode->text;
                    if (typeName[0] == PtrPrefix)
                    {
                        ss->field.isPtr = true;
                        typeName = typeName.substr(1, typeName.size() - 1);
                    }
                    */
                    ss->field.type = resolveDataTypeName(typeNode->text, ss->field.isPtr, &sources[node->sourceFileName], owner);
                    ss->field.owner = owner;

                    if (ss->field.type == CPM_DATATYPE_UNDEFINED)
                    {
                        compilerLog.Add(LOG_ERROR, "Undefined type '" + typeNode->text + "'.", node->sourceFileName, node->children[0]->lineNumber);
                        noErrors = false;
                        return;
                    }
                    if (ss->field.type == CPM_DATATYPE_AMBIGUOUS)
                    {
                        compilerLog.Add(LOG_ERROR, "Type reference '" + typeNode->text + "' is ambiguous in this context. See the message above.", node->sourceFileName, node->children[0]->lineNumber);
                        noErrors = false;
                        return;
                    }
                    if (ss->field.type == CPM_DATATYPE_VOID)
                    {
                        compilerLog.Add(LOG_ERROR, "Void type is not allowed for static or const data.", node->sourceFileName, node->children[0]->lineNumber);
                        noErrors = false;
                        return;
                    }

                    bool isArray = false;
                    bool isImported = false;

                    if (node->children.size() >= 4)
                    {
                        if (node->children[3]->text == R_ARRAY)
                        {
                            isArray = true;
                            countNode = node->children[4];
                        }
                        else
                        if (node->children[3]->text == R_IMPORT)
                        {
                            isImported = true;
                            importSourceNode = node->children[4];
                        }
                        else
                            valueNode = node->children[3];
                    }
                    if (node->children.size() == 7)
                    {
                        if (node->children[6]->text == R_IMPORT)
                        {
                            isImported = true;
                            importSourceNode = node->children[7];
                        }
                        else
                            isInvalid = true;
                    }
                    if (node->children.size() == 6)
                        valueNode = node->children[5];
                        
                    if (isArray)
                    {
                        ss->field.count = parseArraySizeDecl(countNode, owner);
                    }

                    if (isImported)
                    {
                        if (importSourceNode->type == CPM_NUM)
                            ss->importSource = parseNum(importSourceNode->text);
                        else
                        {
                            compilerLog.Add(LOG_ERROR, "Static symbol '" + nameNode->text + "' import address must be an integer.", node->sourceFileName, importSourceNode->lineNumber);
                            noErrors = false;
                            return;
                        }
                    }
                    else
                        ss->importSource = -1;

                    if (sizeOfData(&ss->field) > DataMaxSize)
                    {
                        compilerLog.Add(LOG_ERROR, "Static symbol '" + nameNode->text + "' is too large.", node->sourceFileName, node->lineNumber);
                        noErrors = false;
                        return;
                    }

                    if (valueNode)
                    {
                        parseLiteralValue(&ss->field, valueNode);
                    }

                    compilerLog.Add(LOG_MESSAGE, "Static symbol " + nameNode->text + " in " + owner->name + "; data = ", node->sourceFileName, nameNode->lineNumber);
                    if (ss->field.data.size() > 0)
                        compilerLog.Add((int)ss->field.data[0]);
                }
                else
                {
                    compilerLog.Add(LOG_ERROR, "Static symbol '" + nameNode->text + "' is already declared in " + owner->name + " namespace.", node->sourceFileName, nameNode->lineNumber);
                    noErrors = false;
                    return;
                }
            }
            else
                isInvalid = true;
        }

        if (isInvalid)
        {
            compilerLog.Add(LOG_ERROR, "Invalid static or const declaration syntax.", node->sourceFileName, node->children[0]->lineNumber);
            noErrors = false;
        }
    }

    void CPMCompiler::readStructs()
    {
        for (map<string, CPMNamespace>::iterator ins = namespaces.begin(); ins != namespaces.end(); ++ins)
        {
            CPMNamespace* ns = &(*ins).second;
            for (int i = 0; i < ns->nodes.size(); i++)
                detectStruct(ns->nodes[i], ns);
        }
        for (map<CPMDataType, CPMStructSymbol*>::iterator ist = structTypes.begin(); ist != structTypes.end(); ++ist)
        {
            readStructFields(ist->second);
        }
        for (map<CPMDataType, CPMStructSymbol*>::iterator ist = structTypes.begin(); ist != structTypes.end(); ++ist)
        {
            computeStructDataSize(ist->second, unordered_set<CPMDataType>());
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

    void CPMCompiler::readStructFields(CPMStructSymbol* structSymbol)
    {
        for (int i = 1; i < structSymbol->node->children.size()-1; i++)
        {
            CPMSyntaxTreeNode* node = structSymbol->node->children[i];

            if (node->type == CPM_LINE)
            {
                if (node->children.size() == 0)
                    continue;

                CPMSyntaxTreeNode* typeNode;
                CPMSyntaxTreeNode* nameNode;
                CPMSyntaxTreeNode* countNode = NULL;

                if (node->children.size() != 2 && node->children.size() != 4)
                {
                    compilerLog.Add(LOG_ERROR, "Invalid struct field declaration.", node->sourceFileName, node->lineNumber);
                    noErrors = false;
                    return;
                }

                typeNode = node->children[0];
                nameNode = node->children[1];

                map<string, CPMDataSymbol>::iterator fi = structSymbol->fields.find(nameNode->text);
                if (fi == structSymbol->fields.end())
                {

                    if (typeNode->type != CPM_ID || nameNode->type != CPM_ID)
                    {
                        compilerLog.Add(LOG_ERROR, "Invalid struct field declaration.", node->sourceFileName, node->lineNumber);
                        noErrors = false;
                        return;
                    }

                    if (node->children.size() == 4)
                    {
                        if (node->children[2]->type != CPM_ID || node->children[2]->text != R_ARRAY)
                        {
                            compilerLog.Add(LOG_ERROR, "Invalid struct array field declaration.", node->sourceFileName, node->lineNumber);
                            noErrors = false;
                            return;
                        }
                        countNode = node->children[3];
                    }

                    structSymbol->fields[nameNode->text].name = nameNode->text;
                    CPMDataSymbol* sfield = &(structSymbol->fields[nameNode->text]);
                    sfield->owner = structSymbol->owner;
                    string typeName = typeNode->text;

                    /*
                    if (typeName[0] == PtrPrefix)
                    {
                        sfield->isPtr = true;
                        typeName = typeName.substr(1, typeName.size() - 1);
                    }
                    */
                    sfield->type = resolveDataTypeName(typeName, sfield->isPtr, &sources[node->sourceFileName], structSymbol->owner);

                    if (sfield->type == CPM_DATATYPE_UNDEFINED)
                    {
                        compilerLog.Add(LOG_ERROR, "Undefined type '" + typeName + "'.", node->sourceFileName, node->lineNumber);
                        noErrors = false;
                        return;
                    }
                    if (sfield->type == CPM_DATATYPE_AMBIGUOUS)
                    {
                        compilerLog.Add(LOG_ERROR, "Type reference '" + typeName + "' is ambiguous in this context. See the message above.", node->sourceFileName, node->lineNumber);
                        noErrors = false;
                        return;
                    }
                    if (sfield->type == CPM_DATATYPE_VOID)
                    {
                        compilerLog.Add(LOG_ERROR, "Void type is not allowed for structures fields.", node->sourceFileName, node->lineNumber);
                        noErrors = false;
                        return;
                    }

                    if (countNode)
                    {
                        sfield->count = parseArraySizeDecl(countNode, structSymbol->owner);
                    }
                    else
                        sfield->count = 1;

                    if (sizeOfData(sfield) > DataMaxSize)
                    {
                        compilerLog.Add(LOG_ERROR, "Field '" + structSymbol->name + "." + nameNode->text + "' is too large.", node->sourceFileName, node->lineNumber);
                        noErrors = false;
                        return;
                    }
                }
                else
                {
                    compilerLog.Add(LOG_ERROR, "Field '" + structSymbol->name + "." + nameNode->text + "' is already declared.", node->sourceFileName, node->lineNumber);
                    noErrors = false;
                    return;
                }
            }
            else
            {
                compilerLog.Add(LOG_ERROR, "Unexpected syntax inside struct declaration.", node->sourceFileName, node->lineNumber);
                noErrors = false;
                return;
            }
        }
        if (structSymbol->fields.size() == 0)
        {
            compilerLog.Add(LOG_ERROR, "Empty structs are not allowed.", structSymbol->node->sourceFileName, structSymbol->node->lineNumber);
            noErrors = false;
            return;
        }
        compilerLog.Add(LOG_MESSAGE, "Struct fields " + structSymbol->name, structSymbol->node->sourceFileName, structSymbol->node->lineNumber);
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
        else if (countNode->type == CPM_ID)
        {
            CPMStaticSymbol* sizeConst = resolveStaticSymbolName(countNode->text, &sources[countNode->sourceFileName], currentNS);
            if (sizeConst)
            {
                if (sizeConst->isconst)
                {
                    if (IsIntDataType(sizeConst->field.type) && sizeConst->field.count == 1)
                    {
                        int size = (int)sizeConst->field.data[0];
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

    void CPMCompiler::computeStructDataSize(CPMStructSymbol* structSymbol, unordered_set<CPMDataType> &visitedStructs)
    {
        structSymbol->size = -1;
        int sizeAccum = 0;
        for (map<string, CPMDataSymbol>::iterator ifield = structSymbol->fields.begin(); ifield != structSymbol->fields.end(); ++ifield)
        {
            int fieldSize = 0;

            CPMDataType fieldType = ifield->second.type;
            if (!ifield->second.isPtr && (fieldType == CPM_DATATYPE_BOOL || fieldType == CPM_DATATYPE_CHAR || fieldType == CPM_DATATYPE_INT8 || fieldType == CPM_DATATYPE_UINT8))
                fieldSize = 1;
            else if (ifield->second.isPtr || fieldType == CPM_DATATYPE_STRING || fieldType == CPM_DATATYPE_INT16 || fieldType == CPM_DATATYPE_UINT16)
                fieldSize = 2;
            else if (fieldType >= CPM_DATATYPE_USER)
            {
                unordered_set<CPMDataType>::iterator visitedStruct = visitedStructs.find(fieldType);
                if (visitedStruct != visitedStructs.end())
                {                    
                    compilerLog.Add(LOG_ERROR, "Circular struct declaration in '" + structSymbol->name + "." + ifield->first + "'.", structSymbol->node->sourceFileName, structSymbol->node->lineNumber);
                    noErrors = false;
                    return;
                }
                if (structTypes[fieldType]->size < 0)
                {
                    visitedStructs.insert(structSymbol->type);
                    computeStructDataSize(structTypes[fieldType], visitedStructs);
                    visitedStructs.erase(structSymbol->type);
                }
                fieldSize = structTypes[fieldType]->size;
            }

            fieldSize *= ifield->second.count;

            if (structSymbol->isUnion)
            {
                if (sizeAccum == 0)
                    sizeAccum = fieldSize;
                else if (sizeAccum != fieldSize)
                {
                    compilerLog.Add(LOG_ERROR, "Field '" + structSymbol->name + "." + ifield->second.name + "' size does not match with the union size.", structSymbol->node->sourceFileName, structSymbol->node->lineNumber);
                    noErrors = false;
                    return;
                }
            }
            else
                sizeAccum += fieldSize;

        }
        structSymbol->size = sizeAccum;
        buildStructLayout(structSymbol);
        compilerLog.Add(LOG_MESSAGE, structSymbol->name + " size = ", structSymbol->node->sourceFileName, structSymbol->node->lineNumber)
            ->Add(structSymbol->size);
    }

    void CPMCompiler::readFunctions()
    {
        for (map<string, CPMNamespace>::iterator ins = namespaces.begin(); ins != namespaces.end(); ++ins)
        {
            CPMNamespace* ns = &(*ins).second;
            for (int i = 0; i < ns->nodes.size(); i++)
                detectFunction(ns->nodes[i], ns);
        }
    }

    void CPMCompiler::detectFunction(CPMSyntaxTreeNode* node, CPMNamespace* owner)
    {
        if (node->type == CPM_LINE && node->children.size() == 4)
        {
            CPMSyntaxTreeNode* typeNode = node->children[0];
            CPMSyntaxTreeNode* nameNode = node->children[1];
            CPMSyntaxTreeNode* argsNode = node->children[2];
            CPMSyntaxTreeNode* bodyNode = node->children[3];

            if (typeNode->type == CPM_ID && nameNode->type == CPM_ID && (argsNode->type == CPM_EXPR || argsNode->type == CPM_BLOCK) && bodyNode->type == CPM_BLOCK)
            {
                string typeName = typeNode->text;
                
                bool isPtr = false;
                /*
                if (typeName[0] == PtrPrefix)
                {
                    isPtr = true;
                    typeName = typeName.substr(1, typeName.size() - 1);
                }
                */
                CPMDataType type = resolveDataTypeName(typeName, isPtr, &sources[node->sourceFileName], owner);
                if (type == CPM_DATATYPE_UNDEFINED)
                {
                    compilerLog.Add(LOG_ERROR, "Undefined type '" + typeNode->text + "'.", node->sourceFileName, node->children[0]->lineNumber);
                    noErrors = false;
                    return;
                }
                if (type == CPM_DATATYPE_AMBIGUOUS)
                {
                    compilerLog.Add(LOG_ERROR, "Type reference '" + typeNode->text + "' is ambiguous in this context. See the message above.", node->sourceFileName, node->children[0]->lineNumber);
                    noErrors = false;
                    return;
                }

                CPMFunctionSignature signature;
                signature.name = nameNode->text;

                if (argsNode->children.size() > 0)
                {
                    CPMSyntaxTreeNode* argNode = argsNode->type == CPM_EXPR ? argsNode : argsNode->children[1];
                    for (int i = 1; i < argsNode->children.size() - 1; ++i)
                    {
                        CPMArgumentSignature argSign;
                        //argSign.isRef = false;
                        argSign.isPtr = false;
                        argSign.count = 1;
                        if (argNode->children.size() < 2 || argNode->children.size() > 5)
                        {
                            compilerLog.Add(LOG_ERROR, "Invalid function argument declaration syntax.", argNode->sourceFileName, argNode->lineNumber);
                            noErrors = false;
                            return;
                        }

                        string argType = argNode->children[0]->text;
                        /*
                        if (argType[0] == PtrPrefix)
                        {
                            argSign.isPtr = true;
                            argType = argType.substr(1, argType.size() - 1);
                        }
                        */
                        argSign.type = resolveDataTypeName(argType, argSign.isPtr, &sources[node->sourceFileName], owner);
                        if (argSign.type == CPM_DATATYPE_UNDEFINED)
                        {
                            compilerLog.Add(LOG_ERROR, "Undefined type '" + argType + "'.", argNode->sourceFileName, argNode->lineNumber);
                            noErrors = false;
                            return;
                        }
                        if (argSign.type == CPM_DATATYPE_AMBIGUOUS)
                        {
                            compilerLog.Add(LOG_ERROR, "Type reference '" + argType + "' is ambiguous in this context. See the message above.", argNode->sourceFileName, argNode->lineNumber);
                            noErrors = false;
                            return;
                        }

                        argSign.name = argNode->children[1]->text;

                        if (argNode->children.size() > 2)
                        {
                            CPMSyntaxTreeNode* countNode = NULL;
                            if (argNode->children.size() >= 4)
                            {
                                if (argNode->children[2]->text == R_ARRAY)
                                {
                                    countNode = argNode->children[3];
                                    argSign.count = parseArraySizeDecl(countNode, owner);
                                }
                                else
                                {
                                    compilerLog.Add(LOG_ERROR, "Invalid function argument '" + argNode->children[2]->text + "' declaration syntax.", argNode->sourceFileName, argNode->lineNumber);
                                    noErrors = false;
                                    return;
                                }
                            }
                            /*
                            if (argNode->children[argNode->children.size()-1]->text == R_REF)
                            {
                                argSign.isRef = true;
                            }
                            
                            else*/ if (argNode->children.size() != 4)
                            {
                                compilerLog.Add(LOG_ERROR, "Invalid symbol attribute '" + argNode->children[argNode->children.size() - 1]->text + "' for argument " + argSign.name + " .", argNode->sourceFileName, argNode->lineNumber);
                                noErrors = false;
                                return;
                            }
                        }

                        signature.arguments.push_back(argSign);

                        if (argsNode->type == CPM_EXPR)
                            break;
                        argNode = argsNode->children[i+1];
                    }
                }

                map<CPMFunctionSignature, CPMFunctionSymbol>::iterator fi = owner->functions.find(signature);
                if (fi == owner->functions.end())
                {
                    owner->functions[signature].signature = signature;
                    CPMFunctionSymbol* func = &owner->functions[signature];
                    func->owner = owner;
                    func->type = type;
                    func->isPtr = isPtr;
                    func->compiler = this;
                    func->bodyNode = bodyNode;
                    func->arguments.resize(signature.arguments.size());
                    compilerLog.Add(LOG_MESSAGE, "Function '" + nameNode->text + "' : " + typeName + " (", node->sourceFileName, node->lineNumber);
                    for (int i = 0; i < signature.arguments.size(); ++i)
                    {
                        func->arguments[i].name = signature.arguments[i].name;
                        compilerLog.Add(" " + func->arguments[i].name + ":");
                        func->arguments[i].owner = owner;
                        func->arguments[i].count = signature.arguments[i].count;
                        compilerLog.Add("[" + to_string(func->arguments[i].count) + "]");
                        func->arguments[i].type = signature.arguments[i].type;
                        compilerLog.Add(to_string(func->arguments[i].type));
                        func->arguments[i].isPtr = signature.arguments[i].isPtr;
                        if (signature.arguments[i].isPtr)
                            compilerLog.Add(PtrPrefix);
                        //if (signature.arguments[i].isRef)
                        //    compilerLog.Add("&");
                    }
                    compilerLog.Add(" )");
                }
                else
                {
                    compilerLog.Add(LOG_ERROR, "Function '" + nameNode->text + "' with the same argument list is already declared in " + owner->name + " namespace.", node->sourceFileName, node->children[0]->lineNumber);
                    noErrors = false;
                    return;
                }
            }
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
                                namespaces[nsname].name = nsname;
                                for (int i = 0; i < node->children[blockChildIndex]->children.size(); i++)
                                    if (node->children[blockChildIndex]->children[i]->type != CPM_CHAR)
                                        namespaces[nsname].nodes.push_back(node->children[blockChildIndex]->children[i]);

                                if (node->children.size() == 5)
                                {
                                    if (node->children[2]->type == CPM_ID && node->children[2]->text == R_IMPORT && node->children[3]->type == CPM_STR)
                                    {
                                        namespaces[nsname].importSource = CPMSTRContent(node->children[3]);
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
                        namespaces[GlobalNamespace].nodes.push_back(node);
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
                map<string, CPMNamespace>::iterator ins = namespaces.find(isrc->second.usingNamespaces[i]);
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
            return structTypes[type]->size;
    }

    int CPMCompiler::sizeOfData(CPMDataSymbol* dataSymbol)
    {
        if (dataSymbol->isPtr)
            return 2 * dataSymbol->count;
        else
            return sizeOfType(dataSymbol->type)*dataSymbol->count;
    }

    void CPMCompiler::buildStructLayout(CPMStructSymbol* structSymbol)
    {
        unsigned int position = 0;
        for (map<string, CPMDataSymbol>::iterator fi = structSymbol->fields.begin(); fi != structSymbol->fields.end(); ++fi)
        {
            if (structSymbol->isUnion)
                fi->second.offset = 0;
            else
            {
                fi->second.offset = position;
                //compilerLog.Add(LOG_MESSAGE, "Field '" + structSymbol->name + "." + fi->second.name + "' offset = ", structSymbol->node->sourceFileName, structSymbol->node->lineNumber)->Add(fi->second.offset);
                position += sizeOfData(&fi->second);
                if (position > DataMaxSize)
                {
                    compilerLog.Add(LOG_ERROR, "Struct '" + structSymbol->name + "' is too large.", structSymbol->node->sourceFileName, structSymbol->node->lineNumber);
                    noErrors = false;
                }
            }
        }
    }

    vector<string> CPMCompiler::ParseSymbolName(const string& name)
    {
        vector<string> parsedName;
        stringstream sname;
        sname.str(name);
        string subname;
        while (getline(sname, subname, NameSeparator))
            parsedName.push_back(subname);
        return parsedName;
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
                for (auto i = nsi->second.datatypes.begin(); i != nsi->second.datatypes.end(); ++i)
                {
                    if (i->second == typeId)
                        return i->first;
                }
            }
        }
        return "<undefined>";
    }

    CPMDataType CPMCompiler::resolveDataTypeName(const string &name, bool& isPtr, CPMSourceFile* sourceFile, CPMNamespace* currentNS)
    {
        string typeName = name;
        isPtr = false;
        if (typeName[0] == PtrPrefix)
        {
            isPtr = true;
            typeName = typeName.substr(1, typeName.size() - 1);
        }

        vector<string> parsedName = CPMCompiler::ParseSymbolName(typeName);

        CPMDataType result = CPM_DATATYPE_UNDEFINED;

        if (parsedName.size() == 2)
        {
            map<string, CPMNamespace>::iterator ins = namespaces.find(parsedName[0]);
            if (ins != namespaces.end())
            {
                map<string, CPMDataType>::iterator idt = ins->second.datatypes.find(parsedName[1]);
                if (idt != ins->second.datatypes.end())
                {
                    return idt->second;
                }
            }
        }
        else if (parsedName.size() == 1)
        {
            vector<string> candidatesNamespaces;
            map<string, CPMDataType>::iterator idt;
            if (currentNS)
            {
                idt = currentNS->datatypes.find(typeName);
                if (idt != currentNS->datatypes.end())
                    return idt->second;
            }
            idt = namespaces[GlobalNamespace].datatypes.find(typeName);
            if (idt != namespaces[GlobalNamespace].datatypes.end())
            {
                result = idt->second;
                candidatesNamespaces.push_back(GlobalNamespace);
            }

            for (int i = 0; i < sourceFile->usingNamespaces.size(); i++)
            {
                string ns = sourceFile->usingNamespaces[i];
                idt = namespaces[ns].datatypes.find(typeName);
                if (idt != namespaces[ns].datatypes.end())
                {
                    result = idt->second;
                    candidatesNamespaces.push_back(ns);
                }
            }

            if (candidatesNamespaces.size() > 1)
            {
                compilerLog.Add(LOG_ERROR, "Ambiguous type reference '" + typeName + "'. Candidates are: ", sourceFile->name);
                for (int i = 0; i < candidatesNamespaces.size(); i++)
                {
                    compilerLog.Add(candidatesNamespaces[i] + NameSeparator + typeName);
                    if (i < candidatesNamespaces.size() - 1)
                        compilerLog.Add(", ");
                }
                return CPM_DATATYPE_AMBIGUOUS;
            }
        }

        return result;
    }

    CPMStaticSymbol* CPMCompiler::resolveStaticSymbolName(const string &name, CPMSourceFile* sourceFile, CPMNamespace* currentNS)
    {
        vector<string> parsedName;
        stringstream sname;
        sname.str(name);
        string subname;
        while (getline(sname, subname, NameSeparator))
            parsedName.push_back(subname);

        CPMStaticSymbol* result = NULL;

        if (parsedName.size() == 2)
        {
            map<string, CPMNamespace>::iterator ins = namespaces.find(parsedName[0]);
            if (ins != namespaces.end())
            {
                map<string, CPMStaticSymbol>::iterator iss = ins->second.statics.find(parsedName[1]);
                if (iss != ins->second.statics.end())
                {
                    return &iss->second;
                }
            }
        }
        else if (parsedName.size() == 1)
        {
            vector<string> candidatesNamespaces;
            map<string, CPMStaticSymbol>::iterator iss;
            if (currentNS)
            {
                iss = currentNS->statics.find(name);
                if (iss != currentNS->statics.end())
                    return &iss->second;
            }
            iss = namespaces[GlobalNamespace].statics.find(name);
            if (iss != namespaces[GlobalNamespace].statics.end())
            {
                result = &iss->second;
                candidatesNamespaces.push_back(GlobalNamespace);
            }

            for (int i = 0; i < sourceFile->usingNamespaces.size(); i++)
            {
                string ns = sourceFile->usingNamespaces[i];
                iss = namespaces[ns].statics.find(name);
                if (iss != namespaces[ns].statics.end())
                {
                    result = &iss->second;
                    candidatesNamespaces.push_back(ns);
                }
            }

            if (candidatesNamespaces.size() > 1)
            {
                compilerLog.Add(LOG_ERROR, "Ambiguous static symbol reference '" + name + "'. Candidates are: ", sourceFile->name);
                for (int i = 0; i < candidatesNamespaces.size(); i++)
                {
                    compilerLog.Add(candidatesNamespaces[i] + NameSeparator + name);
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
        return strtol(num.c_str(), nullptr, base);
    }

    bool CPMCompiler::parseExpression(CPMSyntaxTreeNode* root, vector<CPMUnfoldedExpressionNode> &unfolded)
    {
        CPM_ASSERT(root->type == CPM_EXPR || root->type == CPM_LINE || root->type == CPM_ID || root->type == CPM_NUM);

        if (root->type == CPM_EXPR || root->type == CPM_LINE)
        {
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
    
    int CPMCompiler::evalNum(vector<CPMUnfoldedExpressionNode> &unfolded, bool& ok, CPMNamespace* currentNS, bool silent)
    {
        ok = true;
        stack<int> evalStack;
        for (int i = unfolded.size() - 1; i >= 0; --i)
        {
            if (unfolded[i].operandsNumber == 0)
            {
                if (unfolded[i].syntaxNode->type == CPM_NUM)
                    evalStack.push(parseNum(unfolded[i].syntaxNode->text));
                else if (unfolded[i].syntaxNode->type == CPM_ID)
                {
                    CPMStaticSymbol* sym = resolveStaticSymbolName(unfolded[i].syntaxNode->text, &sources[unfolded[i].syntaxNode->sourceFileName], currentNS);
                    if (sym)
                    {
                        if (IsIntDataType(sym->field.type) && sym->field.count == 1)
                        {
                            evalStack.push((int)sym->field.data[0]);
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
                if (op == "+")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                        result += args[argi];
                }
                else if (op == "-")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                        result -= args[argi];
                }
                else if (op == "*")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                        result *= args[argi];
                }
                else if (op == "/")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                    {
                        if (args[i] == 0)
                        {
                            compilerLog.Add(LOG_ERROR, "Division by zero.", unfolded[i].syntaxNode->sourceFileName, unfolded[i].syntaxNode->lineNumber);
                            noErrors = false;
                            return 0;
                        }
                        result /= args[argi];
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
                else if (op == ">>")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                        result >>= args[argi];
                }
                else if (op == "<<")
                {
                    result = args[0];
                    for (int argi = 1; argi < args.size(); ++argi)
                        result <<= args[argi];
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

    bool CPMCompiler::parseLiteralStruct(CPMDataSymbol* symbol, CPMSyntaxTreeNode* valueNode, int index)
    {
        if (valueNode->type == CPM_BLOCK)
        {
            CPMStructSymbol* structInstance = new CPMStructSymbol(*structTypes[symbol->type]);//(FRIDGE_WORD*)malloc(structsymbol->size);           
            symbol->data[index] = (FRIDGE_WORD*)structInstance;

            for (int i = 1; i < valueNode->children.size() - 1; ++i)
            {                
                CPMSyntaxTreeNode* fieldLine = valueNode->children[i];
                if (fieldLine->type == CPM_LINE && fieldLine->children.size() == 2)
                {
                    CPMSyntaxTreeNode* fieldName = fieldLine->children[0];
                    CPMSyntaxTreeNode* fieldValue = fieldLine->children[1];

                    map<string, CPMDataSymbol>::iterator fi = structInstance->fields.find(fieldName->text);
                    if (fi != structInstance->fields.end())
                    {
                        parseLiteralValue(&fi->second, fieldValue);
                    }
                    else
                    {
                        compilerLog.Add(LOG_ERROR, "Structure " + structInstance->name + " does not contain '" + fieldName->text + "' field.", fieldLine->sourceFileName, fieldLine->lineNumber);
                        noErrors = false;
                        return false;
                    }
                }
                else
                {
                    compilerLog.Add(LOG_ERROR, "Unexpected syntax in literal struct declaration.", fieldLine->sourceFileName, fieldLine->lineNumber);
                    noErrors = false;
                    return false;
                }
            }
        }
        else
        {
            compilerLog.Add(LOG_ERROR, "Invalid literal struct syntax.", valueNode->sourceFileName, valueNode->lineNumber);
            noErrors = false;
            return false;
        }
        return true;
    }

    bool CPMCompiler::parseLiteralValue(CPMDataSymbol* symbol, CPMSyntaxTreeNode* valueNode)
    {
        if (symbol->count > 1)
        {
            if (valueNode->type != CPM_BLOCK && !(symbol->type == CPM_DATATYPE_CHAR && valueNode->type == CPM_STR))
            {
                compilerLog.Add(LOG_ERROR, "Invalid literal array syntax.", valueNode->sourceFileName, valueNode->lineNumber);
                noErrors = false;
                return false;
            }
            if (valueNode->children.size() != symbol->count + 2)
            {
                compilerLog.Add(LOG_ERROR, "Declared items count does not match with the array size.", valueNode->sourceFileName, valueNode->lineNumber);
                noErrors = false;
                return false;
            }
        }

        symbol->data.resize(symbol->count);

        CPMSyntaxTreeNode* itemNode = valueNode;
        for (int index = 0; index < symbol->count; ++index)
        {
            if (symbol->count > 1)
                itemNode = valueNode->children[index + 1];

            if (IsIntDataType(symbol->type) || symbol->isPtr || symbol->type == CPM_DATATYPE_BOOL)
            {
                if (!parseLiteralNumber(symbol, itemNode, index))
                    return false;
            }
            else
            {
                if (symbol->count > 1 && valueNode->type == CPM_BLOCK) {
                    if (itemNode->children.size() == 1)
                        itemNode = itemNode->children[0];
                    else
                    {
                        compilerLog.Add(LOG_ERROR, "Invalid array item syntax.", itemNode->sourceFileName, itemNode->lineNumber);
                        noErrors = false;
                        return false;
                    }
                }

                if (symbol->type == CPM_DATATYPE_STRING)
                {
                    if (!parseLiteralString(symbol, itemNode, index))
                        return false;
                }
                else if (symbol->type == CPM_DATATYPE_CHAR)
                {
                    if (!parseLiteralChar(symbol, itemNode, index))
                        return false;
                }
                else if (symbol->type >= CPM_DATATYPE_USER)
                {
                    if (!parseLiteralStruct(symbol, itemNode, index))
                        return false;
                }
            }
        }

        return true;
    }

    bool CPMCompiler::parseLiteralNumber(CPMDataSymbol* symbol, CPMSyntaxTreeNode* valueNode, int index, bool autoType)
    {
        if (!autoType)
            CPM_ASSERT(IsIntDataType(symbol->type) || symbol->isPtr || symbol->type == CPM_DATATYPE_BOOL)

        if (valueNode->type == CPM_NUM || valueNode->type == CPM_ID || valueNode->type == CPM_EXPR || valueNode->type == CPM_LINE)
        {
            int val;
            if (valueNode->type == CPM_NUM)
                val = parseNum(valueNode->text);
            else
            {
                vector<CPMUnfoldedExpressionNode> unfolded;
                parseExpression(valueNode, unfolded);
                bool ok;
                val = evalNum(unfolded, ok, symbol->owner);
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
                        symbol->type = CPM_DATATYPE_UINT8;
                    else
                        symbol->type = CPM_DATATYPE_UINT16;
                }
                else
                {
                    if (val >= INT8_Min)
                        symbol->type = CPM_DATATYPE_INT8;
                    else
                        symbol->type = CPM_DATATYPE_INT16;
                }
            }

            if (symbol->type == CPM_DATATYPE_BOOL)
                val = val > 0 ? 1 : 0;

            bool outofrange = false;
            if (!symbol->isPtr)
            {
                switch (symbol->type)
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
                symbol->data[index] = (FRIDGE_WORD*)val;
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

    bool CPMCompiler::parseLiteralString(CPMDataSymbol* symbol, CPMSyntaxTreeNode* valueNode, int index)
    {
        CPM_ASSERT(symbol->type == CPM_DATATYPE_STRING);

        if (valueNode->type == CPM_STR)
        {
            string s = CPMSTRContent(valueNode);
            FRIDGE_WORD* s_ptr = (FRIDGE_WORD*)malloc(s.size() + 1);
            memcpy(s_ptr, s.c_str(), s.size());
            s_ptr[s.size()] = 0;

            symbol->data[index] = s_ptr;
            return true;
        }
        else
        {
            compilerLog.Add(LOG_ERROR, "Cannot parse '" + valueNode->text + "' as a string literal.", valueNode->sourceFileName, valueNode->lineNumber);
            noErrors = false;
            return false;
        }
    }

    bool CPMCompiler::parseLiteralChar(CPMDataSymbol* symbol, CPMSyntaxTreeNode* valueNode, int index)
    {
        CPM_ASSERT(symbol->type == CPM_DATATYPE_CHAR);

        if (valueNode->type == CPM_STR && valueNode->text.size() == 3)
        {
            symbol->data[index] = (FRIDGE_WORD*)valueNode->text[1];
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
            symbol->data[index] = (FRIDGE_WORD*)ccode;
        }
        else if (valueNode->type == CPM_CHAR)
        {
            symbol->data[index] = (FRIDGE_WORD*)valueNode->text[0];
        }
        else
        {
            compilerLog.Add(LOG_ERROR, "Cannot parse '" + valueNode->text + "' as a char literal.", valueNode->sourceFileName, valueNode->lineNumber);
            noErrors = false;
            return false;
        }
        return true;
    }
    
    /*
    void CPMCompiler::writeStaticNum(CPMStaticSymbol* symbol, int value)
    {
        switch (symbol->field.type)
        {
        case CPM_DATATYPE_INT16:
            symbol->data = (FRIDGE_WORD*)malloc(sizeof(CPM_INT16));
            (*(CPM_INT16*)symbol->data) = (CPM_INT16)value;
            break;
        case CPM_DATATYPE_INT8:
            symbol->data = (FRIDGE_WORD*)malloc(sizeof(CPM_INT8));
            (*(CPM_INT8*)symbol->data) = (CPM_INT8)value;
            break;
        case CPM_DATATYPE_UINT16:
            symbol->data = (FRIDGE_WORD*)malloc(sizeof(CPM_UINT16));
            (*(CPM_UINT16*)symbol->data) = (CPM_UINT16)value;
            break;
        case CPM_DATATYPE_UINT8:
            symbol->data = (FRIDGE_WORD*)malloc(sizeof(CPM_UINT8));
            (*(CPM_UINT8*)symbol->data) = (CPM_UINT8)value;
            break;
        case CPM_DATATYPE_CHAR:
            symbol->data = (FRIDGE_WORD*)malloc(sizeof(char));
            (*(char*)symbol->data) = (char)value;
            break;
        }
    }

    int CPMCompiler::readStaticNum(CPMStaticSymbol* symbol)
    {

    }
    */

    void CPMCompiler::getNamespaces(vector<CPMNamespace*>& nslist)
    {
        nslist.clear();
        for (auto i = namespaces.begin(); i != namespaces.end(); ++i)
            nslist.push_back(&i->second);
    }

    string CPMCompiler::printStaticValue(CPMDataSymbol* symbol)
    {
        string s = symbol->name + " = ";

        if (symbol->data.size() == 0)
        {
            s += "<undefined>";
            return s;
        }

        if (symbol->count > 1)
            s += "(";

        for (int i = 0; i < symbol->count; ++i)
        {
            if (IsIntDataType(symbol->type) || symbol->isPtr || symbol->type == CPM_DATATYPE_BOOL)
                s += printStaticNumber(symbol, i);
            else if (symbol->type == CPM_DATATYPE_STRING)
                s += printStaticString(symbol, i);
            else if (symbol->type == CPM_DATATYPE_CHAR)
                s += printStaticChar(symbol, i);
            else if (symbol->type >= CPM_DATATYPE_USER)
                s += printStaticStruct(symbol, i);

            if (i < symbol->count-1)
                s += "; ";
        }

        if (symbol->count > 1)
            s += ")";
        return s;
    }

    string CPMCompiler::printStaticNumber(CPMDataSymbol* symbol, int index)
    {
        return to_string((int)symbol->data[index]);
    }

    string CPMCompiler::printStaticString(CPMDataSymbol* symbol, int index)
    {
        return (char*)symbol->data[index];
    }

    string CPMCompiler::printStaticChar(CPMDataSymbol* symbol, int index)
    {
        if (index < symbol->data.size())
            return string(1, (char)symbol->data[index]);
        else
            return "";
    }

    string CPMCompiler::printStaticStruct(CPMDataSymbol* symbol, int index)
    {
        string s = "(";
        CPMStructSymbol* structInstance = (CPMStructSymbol*)symbol->data[index];
        for (map<string, CPMDataSymbol>::iterator fi = structInstance->fields.begin(); fi != structInstance->fields.end(); ++fi)
        {
            s += printStaticValue(&fi->second) + "; ";
        }
        s += ")";
        return s;
    }

    void CPMCompiler::PrintStaticData()
    {
        compilerLog.Add(LOG_MESSAGE, "Static data:\n");
        for (map<string, CPMNamespace>::iterator nsi = namespaces.begin(); nsi != namespaces.end(); ++nsi)
        {
            compilerLog.Add("\n" + nsi->first + ":\n");
            for (map<string, CPMStaticSymbol>::iterator ssi = nsi->second.statics.begin(); ssi != nsi->second.statics.end(); ++ssi)
            {
                compilerLog.Add("   " + printStaticValue(&ssi->second.field) + "\n");
            }            
        }
    }

    CPMCompiler::~CPMCompiler()
    {
    }

    bool IsIntDataType(CPMDataType dtype)
    {
        return dtype == CPM_DATATYPE_UINT8 || dtype == CPM_DATATYPE_UINT16 || dtype == CPM_DATATYPE_INT8 || dtype == CPM_DATATYPE_INT16;
    }
}
