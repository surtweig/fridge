#pragma once

#include "CPMCompiler.h"

namespace CPM
{
    constexpr size_t CPMIRNone = static_cast<size_t>(-1);

    enum class CPMIRStorage { Frame, Incoming, Static };

    struct CPMIRSlot
    {
        CPMType type;
        CPMIRStorage storage;
        size_t offset; // Incoming offsets start at the saved PC, before the local frame.
        size_t size;
        size_t data = CPMIRNone;
        string name;
    };

    enum class CPMIROp
    {
        Constant, Copy, Address, Load, Store, Index, Convert, Arithmetic,
        Call, Jump, Branch, Return
    };

    struct CPMIRInstruction
    {
        CPMIROp op;
        const CPMSyntaxTreeNode* syntax;
        size_t output;
        vector<size_t> inputs;
        size_t offset = 0;  // Byte offset within a source or destination object.
        size_t stride = 1;  // Element size for Index.
        string operation;
        vector<FRIDGE_WORD> bytes;
        const CPMFunctionSymbol* callee = nullptr;
        size_t target = CPMIRNone;
        size_t alternative = CPMIRNone;
    };

    struct CPMIRFunction
    {
        const CPMFunctionSymbol* symbol;
        vector<CPMIRSlot> slots;
        vector<size_t> parameters;
        size_t result = CPMIRNone;
        size_t argumentBytes = 0;
        size_t localBytes = 0;
        size_t frameBytes = 0;
        vector<vector<CPMIRInstruction>> blocks;
    };

    struct CPMIRRelocation
    {
        size_t offset;
        size_t target;
        size_t targetOffset;
    };

    struct CPMIRData
    {
        string name;
        vector<FRIDGE_WORD> bytes;
        int fixedAddress = -1;
        vector<CPMIRRelocation> relocations;
    };

    struct CPMIRProgram
    {
        vector<CPMIRFunction> functions;
        vector<CPMIRData> data;
    };

    CPMIRProgram CPMBuildIR(CPMCompiler& compiler);
}
