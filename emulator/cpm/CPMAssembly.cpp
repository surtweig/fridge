#include "stdafx.h"
#include "CPMAssembly.h"

#include <filesystem>
#include <iomanip>

namespace CPM
{
    namespace
    {
        string hexOperand(size_t value, int digits = 4)
        {
            ostringstream text;
            text << "0x" << uppercase << hex << setfill('0') << setw(digits) << value;
            return text.str();
        }

        bool signedInteger(const CPMType& type)
        {
            return type.isBase() && (type.base == CPM_DATATYPE_INT8 || type.base == CPM_DATATYPE_INT16);
        }

        bool boolean(const CPMType& type) { return type.isBase() && type.base == CPM_DATATYPE_BOOL; }

        string dataLabel(size_t data, size_t offset)
        {
            // FALC orders resources by name. Fixed-width offsets keep each
            // object's bytes in address order, including relocation slots.
            return "cpm_data_" + to_string(data) + "_" + hexOperand(offset).substr(2);
        }

        class AssemblyEmitter
        {
            struct Library
            {
                size_t begin;
                size_t end;
                string label;
            };

            CPMCompiler& compiler;
            const CPMIRProgram& program;
            map<const CPMFunctionSymbol*, string> labels;
            map<const CPMNamespace*, Library> libraries;
            const CPMIRFunction* function = nullptr;
            string functionLabel;
            ostringstream listing;
            size_t uniqueLabels = 0;
            size_t imageBytes = 0;

            void error(const CPMSyntaxTreeNode* syntax, const string& message)
            {
                compiler.Error();
                if (syntax)
                    compiler.CompilerLog()->Add(LOG_ERROR, message, syntax->sourceFileName, syntax->lineNumber);
                else
                    compiler.CompilerLog()->Add(LOG_ERROR, message);
            }

            void op(const string& text)
            {
                listing << "    " << text << '\n';
                ++imageBytes;
            }

            void word(const string& mnemonic, const string& operand)
            {
                op(mnemonic + operand);
                imageBytes += 2;
            }

            void word(const string& mnemonic, size_t operand)
            {
                word(mnemonic, hexOperand(operand & 0xFFFF));
            }

            void byte(const string& mnemonic, FRIDGE_WORD operand)
            {
                op(mnemonic + hexOperand(operand, 2));
                ++imageBytes;
            }

            void entry(const string& label) { listing << "entry " << label << '\n'; }

            string label() { return "cpm_label_" + to_string(uniqueLabels++); }

            string blockLabel(size_t block) const { return functionLabel + "_block_" + to_string(block); }

            string epilogue() const { return functionLabel + "_return"; }

            string libraryAddress(const CPMNamespace* owner, size_t address) const
            {
                const auto& label = libraries.at(owner).label;
                return label.empty() ? hexOperand(address) : label + "+" + hexOperand(address);
            }

            string dataAddress(size_t id, size_t offset = 0) const
            {
                const auto& data = program.data[id];
                if (data.importAddress < 0) return dataLabel(id, offset);
                const size_t address = data.importAddress + offset;
                return data.relocatableImport ? libraryAddress(data.relocatableImport, address) : hexOperand(address);
            }

            void spAddress(size_t offset)
            {
                op("HLSP");
                if (offset != 0) word("DAI ", offset);
            }

            void adjustSP(int amount)
            {
                if (amount == 0) return;
                op("HLSP");
                word("DAI ", amount);
                op("SPHL");
            }

            const CPMIRSlot& slot(size_t id) const { return function->slots[id]; }

            void address(size_t id, size_t offset = 0, size_t depth = 0)
            {
                const auto& storage = slot(id);
                offset += storage.offset;
                if (storage.storage == CPMIRStorage::Static)
                    word("LXI HL, ", dataAddress(storage.data, offset));
                else
                {
                    if (storage.storage == CPMIRStorage::Incoming) offset += function->frameBytes;
                    spAddress(offset + depth);
                }
            }

            // DE holds the source address and HL holds the destination.
            void copyBytes(size_t size)
            {
                if (size <= 3)
                {
                    for (size_t i = 0; i < size; ++i)
                    {
                        op("LDAX DE");
                        op("MOV M, A");
                        if (i + 1 < size)
                        {
                            op("INX DE");
                            op("INX HL");
                        }
                    }
                    return;
                }
                const auto again = label();
                word("LXI BC, ", size);
                entry(again);
                op("LDAX DE");
                op("MOV M, A");
                op("INX DE");
                op("INX HL");
                op("DCX BC");
                op("MOV A, B");
                op("ORA C");
                word("JNZ ", again);
            }

            void copySlot(size_t output, size_t input, size_t offset = 0)
            {
                address(input);
                op("XCNG");
                address(output, offset);
                copyBytes(slot(input).size);
            }

            void loadHL()
            {
                op("MOV A, M");
                op("INX HL");
                op("MOV L, M");
                op("MOV H, A");
            }

            void storeHL(size_t output)
            {
                op("XCNG");
                address(output);
                op("MOV M, D");
                op("INX HL");
                op("MOV M, E");
            }

            void writeConstant(size_t output, const vector<FRIDGE_WORD>& bytes)
            {
                address(output);
                for (size_t i = 0; i < bytes.size(); ++i)
                {
                    byte("MVI M, ", bytes[i]);
                    if (i + 1 < bytes.size()) op("INX HL");
                }
            }

            void index(const CPMIRInstruction& instruction)
            {
                const bool indexed = instruction.inputs.size() == 2;
                if (indexed)
                {
                    const size_t index = instruction.inputs[1];
                    address(index);
                    if (slot(index).size == 2)
                    {
                        op("MOV B, M");
                        op("INX HL");
                        op("MOV C, M");
                    }
                    else
                    {
                        op("MOV C, M");
                        byte("MVI B, ", 0);
                        if (signedInteger(slot(index).type))
                        {
                            const auto extended = label();
                            op("MOV A, C");
                            byte("CPI ", 0x80);
                            word("JM ", extended);
                            byte("MVI B, ", 0xFF);
                            entry(extended);
                        }
                    }
                    word("LXI HL, ", 0);
                    size_t bit = 1;
                    while (bit <= instruction.stride / 2) bit <<= 1;
                    for (; bit != 0; bit >>= 1)
                    {
                        op("DAD HL");
                        if ((instruction.stride & bit) != 0) op("DAD BC");
                    }
                    if (instruction.operation == "-")
                    {
                        op("MOV A, H");
                        op("CMA");
                        op("MOV H, A");
                        op("MOV A, L");
                        op("CMA");
                        op("MOV L, A");
                        op("INX HL");
                    }
                    op("XCNG");
                }
                address(instruction.inputs[0]);
                loadHL();
                if (indexed) op("DAD DE");
                if (instruction.offset != 0) word("DAI ", instruction.offset);
                storeHL(instruction.output);
            }

            void testSlot(size_t id)
            {
                address(id);
                op("MOV A, M");
                if (slot(id).size == 2)
                {
                    op("INX HL");
                    op("ORA M");
                }
                else op("ORA A");
            }

            void normalizeA()
            {
                const auto done = label();
                op("ORA A");
                word("JZ ", done);
                byte("MVI A, ", 1);
                entry(done);
            }

            void convert(size_t output, size_t input)
            {
                const auto& source = slot(input);
                const auto& destination = slot(output);
                if (boolean(destination.type) && !boolean(source.type))
                {
                    testSlot(input);
                    normalizeA();
                    address(output);
                    op("MOV M, A");
                }
                else if (source.size == destination.size && !boolean(source.type))
                    copySlot(output, input);
                else if (source.type == destination.type)
                    copySlot(output, input);
                else
                {
                    address(input, source.size - 1);
                    op("MOV A, M");
                    if (boolean(source.type)) normalizeA();
                    address(output, destination.size - 1);
                    op("MOV M, A");
                    if (destination.size == 2)
                    {
                        op("DCX HL");
                        byte("MVI M, ", 0);
                        if (signedInteger(source.type))
                        {
                            const auto extended = label();
                            byte("CPI ", 0x80);
                            word("JM ", extended);
                            byte("MVI M, ", 0xFF);
                            entry(extended);
                        }
                    }
                }
            }

            void compare(size_t left, size_t right, bool ordered,
                         const string& less, const string& equal, const string& greater)
            {
                for (size_t offset = 0; offset < slot(left).size; ++offset)
                {
                    const bool bias = offset == 0 && ordered && signedInteger(slot(left).type);
                    const auto load = [&](size_t id)
                    {
                        address(id, offset);
                        op("MOV A, M");
                        if (boolean(slot(id).type)) normalizeA();
                        if (bias) byte("XRI ", 0x80);
                    };
                    load(right);
                    op("MOV B, A");
                    load(left);
                    op("CMP B");
                    word("JM ", less); // Fridge CMP sets sign for unsigned less-than.
                    word("JNZ ", greater);
                }
                word("JMP ", equal);
            }

            void comparison(const CPMIRInstruction& instruction)
            {
                const auto& operation = instruction.operation;
                const auto& inputs = instruction.inputs;
                const auto failed = label();
                const auto done = label();
                const bool equality = operation == "==" || operation == "!=";
                for (size_t i = 0; i + 1 < inputs.size(); ++i)
                {
                    const size_t limit = operation == "!=" ? inputs.size() : i + 2;
                    for (size_t j = i + 1; j < limit; ++j)
                    {
                        const auto passed = label();
                        const bool less = operation == "<" || operation == "<=" || operation == "!=";
                        const bool equal = operation == "==" || operation == "<=" || operation == ">=";
                        const bool greater = operation == ">" || operation == ">=" || operation == "!=";
                        compare(inputs[i], inputs[j], !equality, less ? passed : failed,
                                equal ? passed : failed, greater ? passed : failed);
                        entry(passed);
                    }
                }
                writeConstant(instruction.output, {1});
                word("JMP ", done);
                entry(failed);
                writeConstant(instruction.output, {0});
                entry(done);
            }

            void fold(size_t output, size_t input, const string& operation)
            {
                const size_t width = slot(output).size;
                address(input, width - 1);
                op("XCNG");
                address(output, width - 1);
                op("MOV A, M");
                op("MOV B, A");
                op("XCNG");
                op(operation + " M");
                // Derive carry from the wrapped byte before adding the high
                // bytes. CMP also works on emulator versions with ADD carry bugs.
                if (width == 2 && operation == "ADD")
                {
                    op("MOV C, A");
                    op("MOV A, B");
                    op("CMP C");
                    op("MOV A, C");
                }
                op("XCNG");
                op("MOV M, A");
                if (width == 2)
                {
                    op("DCX HL");
                    op("MOV A, M");
                    op("XCNG");
                    op("DCX HL");
                    op((operation == "ADD" ? "ADC" : operation == "SUB" ? "SBB" : operation) + " M");
                    op("XCNG");
                    op("MOV M, A");
                }
            }

            void arithmetic(const CPMIRInstruction& instruction)
            {
                const auto& operation = instruction.operation;
                const auto& inputs = instruction.inputs;
                const size_t output = instruction.output;
                if (operation == "==" || operation == "!=" || operation == "<" ||
                    operation == ">" || operation == "<=" || operation == ">=")
                {
                    comparison(instruction);
                    return;
                }
                if (operation == "!")
                {
                    const auto isZero = label();
                    const auto done = label();
                    testSlot(inputs[0]);
                    word("JZ ", isZero);
                    writeConstant(output, {0});
                    word("JMP ", done);
                    entry(isZero);
                    writeConstant(output, {1});
                    entry(done);
                    return;
                }
                if (operation == "~")
                {
                    copySlot(output, inputs[0]);
                    address(output);
                    for (size_t i = 0; i < slot(output).size; ++i)
                    {
                        if (i != 0) op("INX HL");
                        op("MOV A, M");
                        op("CMA");
                        op("MOV M, A");
                    }
                    return;
                }
                const string machine = operation == "+" ? "ADD" : operation == "-" ? "SUB" :
                                       operation == "&" ? "ANA" : operation == "|" ? "ORA" :
                                       operation == "^" ? "XRA" : "";
                if (machine.empty())
                {
                    error(instruction.syntax, "Assembly generation does not yet support runtime '" + operation + "'.");
                    return;
                }
                if (operation == "-" && inputs.size() == 1)
                {
                    writeConstant(output, vector<FRIDGE_WORD>(slot(output).size));
                    fold(output, inputs[0], machine);
                    return;
                }
                copySlot(output, inputs[0]);
                for (size_t i = 1; i < inputs.size(); ++i) fold(output, inputs[i], machine);
            }

            void call(const CPMIRInstruction& instruction)
            {
                const auto found = labels.find(instruction.callee);
                if (found == labels.end())
                {
                    error(instruction.syntax, "Assembly generation cannot link '" +
                          instruction.callee->owner->name + "." + instruction.callee->signature.name + "'.");
                    return;
                }
                size_t arguments = 0;
                for (const auto input : instruction.inputs) arguments += slot(input).size;
                const size_t result = instruction.output == CPMIRNone ? 0 : slot(instruction.output).size;
                const size_t depth = arguments + result;
                const size_t incoming = function->argumentBytes + 2 +
                    (function->result == CPMIRNone ? 0 : slot(function->result).size);
                if (depth + function->frameBytes + incoming > DataMaxSize)
                {
                    error(instruction.syntax, "The outgoing call area exceeds stack address space.");
                    return;
                }
                adjustSP(-static_cast<int>(depth));
                size_t argumentOffset = arguments;
                for (const auto input : instruction.inputs)
                {
                    argumentOffset -= slot(input).size;
                    address(input, 0, depth);
                    op("XCNG");
                    spAddress(argumentOffset);
                    copyBytes(slot(input).size);
                }
                word("CALL ", found->second);
                if (result != 0)
                {
                    spAddress(arguments);
                    op("XCNG");
                    address(instruction.output, 0, depth);
                    copyBytes(result);
                }
                adjustSP(static_cast<int>(depth));
            }

            void intrinsic(const CPMIRInstruction& instruction)
            {
                size_t value = 0;
                for (auto byte : instruction.bytes) value = (value << 8) | byte;
                if (instruction.intrinsic == CPMIntrinsic::CallSub)
                {
                    const auto* owner = function->symbol->owner;
                    const auto library = libraries.find(owner);
                    if (library != libraries.end() && (value < library->second.begin || value >= library->second.end))
                    {
                        error(instruction.syntax, "__callsub address is outside its imported binary.");
                        return;
                    }
                    word("CALL ", library == libraries.end() ? hexOperand(value) : libraryAddress(owner, value));
                    return;
                }
                const bool immediate = instruction.inputs.empty();
                if (instruction.intrinsic == CPMIntrinsic::LxiHL)
                {
                    if (immediate) word("LXI HL, ", value);
                    else
                    {
                        op("PUSH AF");
                        address(instruction.inputs.front(), 0, 2);
                        loadHL();
                        op("POP AF");
                    }
                    return;
                }
                const string reg = instruction.intrinsic == CPMIntrinsic::MviA ? "A" :
                                   instruction.intrinsic == CPMIntrinsic::MviB ? "B" : "C";
                if (immediate) byte("MVI " + reg + ", ", static_cast<FRIDGE_WORD>(value));
                else
                {
                    op("PUSH HL");
                    address(instruction.inputs.front(), 0, 2);
                    op("MOV " + reg + ", M");
                    op("POP HL");
                }
            }

            void emit(const CPMIRInstruction& instruction)
            {
                switch (instruction.op)
                {
                case CPMIROp::Constant:
                    writeConstant(instruction.output, instruction.bytes);
                    break;
                case CPMIROp::Copy:
                    copySlot(instruction.output, instruction.inputs[0], instruction.offset);
                    break;
                case CPMIROp::Address:
                    address(instruction.inputs[0], instruction.offset);
                    storeHL(instruction.output);
                    break;
                case CPMIROp::Load:
                    address(instruction.inputs[0]);
                    loadHL();
                    op("XCNG");
                    address(instruction.output);
                    copyBytes(slot(instruction.output).size);
                    break;
                case CPMIROp::Store:
                    address(instruction.inputs[1]);
                    op("XCNG");
                    address(instruction.inputs[0]);
                    loadHL();
                    copyBytes(slot(instruction.inputs[1]).size);
                    break;
                case CPMIROp::Index:
                    index(instruction);
                    break;
                case CPMIROp::Convert:
                    convert(instruction.output, instruction.inputs[0]);
                    break;
                case CPMIROp::Arithmetic:
                    arithmetic(instruction);
                    break;
                case CPMIROp::Call:
                    call(instruction);
                    break;
                case CPMIROp::Intrinsic:
                    intrinsic(instruction);
                    break;
                case CPMIROp::Jump:
                    word("JMP ", blockLabel(instruction.target));
                    break;
                case CPMIROp::Branch:
                    testSlot(instruction.inputs[0]);
                    word("JZ ", blockLabel(instruction.alternative));
                    word("JMP ", blockLabel(instruction.target));
                    break;
                case CPMIROp::Return:
                    if (!instruction.inputs.empty())
                        copySlot(instruction.output, instruction.inputs[0]);
                    word("JMP ", epilogue());
                    break;
                }
            }

            void emitFunction(const CPMIRFunction& body)
            {
                function = &body;
                const auto& symbol = *body.symbol;
                functionLabel = labels.at(&symbol);
                listing << "\n// " << symbol.owner->name << "." << symbol.signature.name << "(";
                for (size_t i = 0; i < symbol.arguments.size(); ++i)
                {
                    if (i != 0) listing << ", ";
                    listing << compiler.GetTypeName(symbol.arguments[i].typeExpr, true);
                }
                listing << ") -> " << compiler.GetTypeName(symbol.returnType, true) << "\n"
                        << "// Frame uses " << body.localBytes << " local bytes and "
                        << body.frameBytes - body.localBytes << " temporary bytes.\n";
                for (const auto& storage : body.slots)
                {
                    if (storage.storage == CPMIRStorage::Static || storage.name.empty()) continue;
                    const size_t offset = storage.offset +
                        (storage.storage == CPMIRStorage::Incoming ? body.frameBytes : 0);
                    listing << "// " << storage.name << " at SP+" << offset << " (" << storage.size
                            << (storage.size == 1 ? " byte)\n" : " bytes)\n");
                }
                listing << "subroutine " << functionLabel << '\n';
                adjustSP(-static_cast<int>(body.frameBytes));
                for (size_t block = 0; block < body.blocks.size(); ++block)
                {
                    if (body.blocks[block].empty()) continue;
                    entry(blockLabel(block));
                    for (const auto& instruction : body.blocks[block])
                    {
                        emit(instruction);
                        if (!compiler.NoErrors()) return;
                    }
                }
                entry(epilogue());
                adjustSP(static_cast<int>(body.frameBytes));
                listing << "endsub\n";
                ++imageBytes;
            }

            bool emitLibraries()
            {
                namespace fs = std::filesystem;
                vector<CPMNamespace*> spaces;
                compiler.getNamespaces(spaces);
                map<pair<string, int>, string> sections;
                for (const auto* space : spaces)
                {
                    const auto* node = space->importNode;
                    if (!node) continue;
                    const bool automatic = space->hasRelocatableImport();
                    const size_t base = automatic ? 0 : space->importAddress;
                    const auto* source = compiler.getSourceFile(node->sourceFileName);
                    const auto directory = fs::path(source->rootFolder) / fs::path(source->name).parent_path();
                    std::error_code ec;
                    const auto path = fs::canonical(directory / space->importSource, ec);
                    if (space->importSource.empty() || ec)
                    {
                        error(node, "Cannot find imported binary '" + space->importSource + "'.");
                        return false;
                    }
                    const auto size = fs::file_size(path, ec);
                    if (ec || size == 0 || (!automatic && base < compiler.getGlobalOffset()) ||
                        size > FRIDGE_MAX_DWORD - base)
                    {
                        error(node, "Invalid size or placement for imported binary '" + space->importSource + "'.");
                        return false;
                    }
                    const auto name = path.generic_string();
                    if (name.find_first_of("\"\r\n") != string::npos)
                    {
                        error(node, "Imported binary path cannot be written as a FALC string literal.");
                        return false;
                    }
                    if (automatic && !fs::is_regular_file(path.string() + ".rel", ec))
                    {
                        error(node, "Cannot find relocation metadata for '" + space->importSource + "'.");
                        return false;
                    }
                    const size_t end = base + size;
                    for (const auto& entry : space->statics)
                    {
                        const auto& symbol = entry.second;
                        if (symbol.importSource < 0) continue;
                        const size_t address = symbol.importSource;
                        if (address < base || address + compiler.sizeOfTypeExpr(symbol.field.typeExpr) > end)
                        {
                            error(node, "Imported static '" + entry.first + "' is outside its imported binary.");
                            return false;
                        }
                    }
                    const string label = automatic ? "cpm_lib_" + to_string(sections.size()) : "";
                    const auto section = sections.emplace(make_pair(name, space->importAddress), label);
                    libraries.emplace(space, Library{base, end, section.first->second});
                    if (section.second)
                        listing << "incbin \"" << name << '"' << (automatic ? " as " + label : " at " + hexOperand(base)) << '\n';
                }
                return true;
            }

        public:
            AssemblyEmitter(CPMCompiler& compiler, const CPMIRProgram& program)
                : compiler(compiler), program(program)
            {
            }

            string run()
            {
                if (!compiler.NoErrors()) return {};
                const CPMIRFunction* main = nullptr;
                for (const auto& body : program.functions)
                {
                    const auto* symbol = body.symbol;
                    const string id = to_string(labels.size());
                    labels.emplace(symbol, "cpm_" + id + "_" + symbol->owner->name + "_" + symbol->signature.name);
                    if (symbol->owner->name == GlobalNamespace && symbol->signature.name == "main" &&
                        symbol->arguments.empty() && symbol->returnType.isVoid())
                        main = &body;
                }
                if (!main)
                {
                    error(nullptr, "Assembly generation requires a definition of void main().");
                    return {};
                }

                const auto origin = compiler.getGlobalOffset();
                listing << "offset " << hexOperand(origin) << "\nmain cpm_start\n";
                if (!emitLibraries()) return {};
                for (size_t i = 0; i < program.data.size(); ++i)
                {
                    for (const auto& relocation : program.data[i].relocations)
                    {
                        word("LXI HL, ", dataAddress(relocation.target, relocation.targetOffset));
                        word("SHLD ", dataAddress(i, relocation.offset));
                    }
                }
                const auto mainReturn = compiler.getMainReturn();
                if (mainReturn == CPMMainReturn::None)
                {
                    // Enter main directly so its ordinary RET returns to the caller.
                    emitFunction(*main);
                    if (!compiler.NoErrors()) return {};
                }
                else
                {
                    word("CALL ", labels.at(main->symbol));
                    if (mainReturn == CPMMainReturn::Bootloader) word("JMP ", 0);
                    else op("HLT");
                }
                for (const auto& body : program.functions)
                {
                    if (mainReturn == CPMMainReturn::None && &body == main) continue;
                    emitFunction(body);
                    if (!compiler.NoErrors()) return {};
                }
                size_t dataBytes = 0;
                for (size_t i = 0; i < program.data.size(); ++i)
                {
                    const auto& data = program.data[i];
                    if (data.bytes.empty()) continue;
                    listing << "\n// " << data.name << '\n';
                    for (size_t offset = 0; offset < data.bytes.size(); ++offset)
                        listing << "static " << dataLabel(i, offset) << ' ' << hexOperand(data.bytes[offset], 2) << '\n';
                    dataBytes += data.bytes.size();
                }
                const size_t resourceGuard = dataBytes == 0 ? 0 : 3; // FALC appends a jump before resources.
                if (imageBytes + resourceGuard + dataBytes > FRIDGE_MAX_DWORD - origin)
                {
                    error(nullptr, "The assembled image exceeds Fridge address space.");
                    return {};
                }
                return listing.str();
            }
        };
    }

    string CPMEmitAssembly(CPMCompiler& compiler, const CPMIRProgram& program)
    {
        return AssemblyEmitter(compiler, program).run();
    }
}
