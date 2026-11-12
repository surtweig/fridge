#include "stdafx.h"
#include "CPMIR.h"
#include "CPMSemantic.h"

#include <algorithm>

namespace CPM
{
    namespace
    {
        CPMType baseType(CPMDataType base)
        {
            CPMType type;
            type.kind = CPM_TYPE_BASE;
            type.base = base;
            return type;
        }

        CPMType pointerTo(const CPMType& pointee)
        {
            CPMType type;
            type.kind = CPM_TYPE_PTR;
            type.child = make_unique<CPMType>(pointee);
            return type;
        }

        void error(CPMCompiler& compiler, const CPMSyntaxTreeNode* syntax, const string& message)
        {
            compiler.Error();
            compiler.CompilerLog()->Add(LOG_ERROR, message, syntax->sourceFileName, syntax->lineNumber);
        }

        struct ProgramBuilder
        {
            CPMCompiler& compiler;
            CPMIRProgram program;
            map<const CPMStaticSymbol*, size_t> statics;

            size_t data(const string& name, size_t size, int address = -1)
            {
                const size_t id = program.data.size();
                program.data.push_back({name, vector<FRIDGE_WORD>(address < 0 ? size : 0), address, {}});
                return id;
            }

            size_t staticData(const CPMStaticSymbol& symbol)
            {
                auto existing = statics.find(&symbol);
                if (existing != statics.end()) return existing->second;
                const auto& field = symbol.field;
                const size_t size = compiler.sizeOfTypeExpr(field.typeExpr);
                const size_t id = data(field.owner->name + "." + field.name, size, symbol.importSource);
                statics.emplace(&symbol, id);
                if (symbol.importSource >= 0 && field.owner->hasRelocatableImport())
                    program.data[id].relocatableImport = field.owner;
                if (symbol.importSource < 0)
                {
                    if (symbol.initializer)
                        initializeData(id, 0, *symbol.initializer);
                    else if (!symbol.declNode)
                    {
                        if (size == 2) program.data[id].bytes[0] = FRIDGE_HIGH_WORD(symbol.immediateData);
                        program.data[id].bytes.back() = FRIDGE_LOW_WORD(symbol.immediateData);
                    }
                }
                return id;
            }

            // Resolve addresses with a statically selected field or array element.
            pair<size_t, size_t> constantObject(const CPMCheckedExpression& value,
                                               const map<const CPMDataSymbol*, size_t>& localData)
            {
                if (value.kind == CPMExprKind::Name)
                {
                    if (value.staticObject) return {staticData(*value.staticObject), 0};
                    auto local = localData.find(value.object);
                    if (local != localData.end()) return {local->second, 0};
                }
                if (value.kind == CPMExprKind::Field)
                {
                    auto base = constantObject(value.operands.front(), localData);
                    base.second += value.field->offset;
                    return base;
                }
                if (value.kind == CPMExprKind::Operator && value.syntax->text == R_AT &&
                    value.operands[0].type.isArray() && !value.operands[1].bytes.empty())
                {
                    auto base = constantObject(value.operands[0], localData);
                    size_t index = 0;
                    for (auto byte : value.operands[1].bytes) index = (index << 8) | byte;
                    base.second += index * compiler.sizeOfTypeExpr(value.type);
                    return base;
                }
                return {CPMIRNone, 0};
            }

            void initializeData(size_t id, size_t offset, const CPMCheckedExpression& value,
                                const map<const CPMDataSymbol*, size_t>& localData = {})
            {
                if (!compiler.NoErrors()) return;
                if (!value.bytes.empty())
                {
                    copy(value.bytes.begin(), value.bytes.end(), program.data[id].bytes.begin() + offset);
                    return;
                }
                if (value.kind == CPMExprKind::Initializer)
                {
                    for (size_t i = 0; i < value.operands.size(); ++i)
                        initializeData(id, offset + value.initializerOffsets[i], value.operands[i], localData);
                    return;
                }
                if (value.kind == CPMExprKind::StringInitializer)
                {
                    const auto& bytes = value.operands.front().bytes;
                    const size_t buffer = data(program.data[id].name + " characters", bytes.size());
                    program.data[buffer].bytes = bytes;
                    program.data[id].relocations.push_back({offset, buffer, 0});
                    return;
                }
                if (value.kind == CPMExprKind::Operator && value.syntax->text == R_CAST)
                {
                    initializeData(id, offset, value.operands.front(), localData);
                    return;
                }
                if (value.kind == CPMExprKind::Operator && value.syntax->text == R_ADDR)
                {
                    const auto [target, targetOffset] = constantObject(value.operands.front(), localData);
                    if (target != CPMIRNone)
                    {
                        program.data[id].relocations.push_back({offset, target, targetOffset});
                        return;
                    }
                }
                else
                {
                    const auto [source, sourceOffset] = constantObject(value, localData);
                    if (source != CPMIRNone)
                    {
                        const auto& object = program.data[source];
                        const size_t size = compiler.sizeOfTypeExpr(value.type);
                        if (sourceOffset + size <= object.bytes.size())
                        {
                            vector<CPMIRRelocation> relocations;
                            for (const auto& relocation : object.relocations)
                            {
                                if (relocation.offset >= sourceOffset + size ||
                                    relocation.offset + 2 <= sourceOffset) continue;
                                if (relocation.offset < sourceOffset ||
                                    relocation.offset + 2 > sourceOffset + size)
                                {
                                    error(compiler, value.syntax, "Cannot initialize data from part of a symbolic address.");
                                    return;
                                }
                                relocations.push_back({offset + relocation.offset - sourceOffset,
                                                       relocation.target, relocation.targetOffset});
                            }
                            copy_n(object.bytes.begin() + sourceOffset, size, program.data[id].bytes.begin() + offset);
                            auto& destination = program.data[id].relocations;
                            destination.insert(destination.end(), relocations.begin(), relocations.end());
                            return;
                        }
                    }
                }
                error(compiler, value.syntax, "This symbolic initializer is not supported by assembly generation.");
            }
        };

        class FunctionBuilder
        {
            ProgramBuilder& program;
            CPMIRFunction function;
            map<const CPMDataSymbol*, size_t> objects;
            map<const CPMDataSymbol*, size_t> localData;
            map<const CPMCheckedExpression*, size_t> buffers;
            size_t current = CPMIRNone;
            vector<pair<size_t, size_t>> loops;

            size_t sizeOf(const CPMType& type) const { return program.compiler.sizeOfTypeExpr(type); }

            size_t slot(const CPMType& type, CPMIRStorage storage, size_t offset,
                        const string& name = "", size_t data = CPMIRNone)
            {
                function.slots.push_back({type, storage, offset, sizeOf(type), data, name});
                return function.slots.size() - 1;
            }

            size_t frame(const CPMType& type, const string& name = "")
            {
                const auto id = slot(type, CPMIRStorage::Frame, function.frameBytes, name);
                function.frameBytes += function.slots[id].size;
                return id;
            }

            size_t temporary(const CPMType& type) { return type.isVoid() ? CPMIRNone : frame(type); }

            size_t object(const CPMCheckedExpression& value)
            {
                const auto found = objects.find(value.object);
                if (found != objects.end()) return found->second;
                const size_t data = program.staticData(*value.staticObject);
                const size_t id = slot(value.type, CPMIRStorage::Static, 0, "", data);
                objects.emplace(value.object, id);
                return id;
            }

            size_t block()
            {
                function.blocks.emplace_back();
                return function.blocks.size() - 1;
            }

            CPMIRInstruction& emit(CPMIROp op, const CPMSyntaxTreeNode* syntax,
                                   size_t output = CPMIRNone, vector<size_t> inputs = {})
            {
                auto& instructions = function.blocks[current];
                instructions.push_back({op, syntax, output, std::move(inputs), 0, 1, "", {},
                                        nullptr, CPMIRNone, CPMIRNone});
                return instructions.back();
            }

            void jump(size_t target, const CPMSyntaxTreeNode* syntax)
            {
                emit(CPMIROp::Jump, syntax).target = target;
                current = CPMIRNone;
            }

            void branch(size_t condition, size_t yes, size_t no, const CPMSyntaxTreeNode* syntax)
            {
                auto& instruction = emit(CPMIROp::Branch, syntax, CPMIRNone, {condition});
                instruction.target = yes;
                instruction.alternative = no;
                current = CPMIRNone;
            }

            void constant(size_t destination, vector<FRIDGE_WORD> bytes, const CPMSyntaxTreeNode* syntax)
            {
                emit(CPMIROp::Constant, syntax, destination).bytes = std::move(bytes);
            }

            size_t directAddress(size_t source, const CPMType& pointee, const CPMSyntaxTreeNode* syntax)
            {
                const size_t result = frame(pointerTo(pointee));
                emit(CPMIROp::Address, syntax, result, {source});
                return result;
            }

            size_t indexAddress(size_t base, size_t index, const CPMType& type, size_t stride,
                                const CPMSyntaxTreeNode* syntax, bool subtract = false, size_t offset = 0)
            {
                const size_t result = frame(type);
                auto& instruction = emit(CPMIROp::Index, syntax, result,
                                         index == CPMIRNone ? vector<size_t>{base} : vector<size_t>{base, index});
                instruction.offset = offset;
                instruction.stride = stride;
                instruction.operation = subtract ? "-" : "+";
                return result;
            }

            size_t address(const CPMCheckedExpression& value)
            {
                const auto* syntax = value.syntax;
                if (value.kind == CPMExprKind::Name)
                    return directAddress(object(value), value.type, syntax);
                if (value.kind == CPMExprKind::Field)
                {
                    const size_t base = address(value.operands[0]);
                    return indexAddress(base, CPMIRNone, pointerTo(value.type), 1, syntax, false,
                                        value.field->offset);
                }
                if (value.kind == CPMExprKind::Operator && syntax->text == R_AT)
                {
                    const auto& receiver = value.operands.front();
                    const size_t base = receiver.type.isArray() ? address(receiver) : expression(receiver);
                    const size_t index = value.operands.size() == 2 ? expression(value.operands[1]) : CPMIRNone;
                    return indexAddress(base, index, pointerTo(value.type), sizeOf(value.type), syntax);
                }
                return directAddress(expression(value), value.type, syntax);
            }

            size_t shortCircuit(const CPMCheckedExpression& value)
            {
                const auto* syntax = value.syntax;
                const bool conjunction = syntax->text == "&&";
                const size_t result = frame(value.type);
                const size_t decisive = block();
                const size_t opposite = block();
                const size_t done = block();
                for (size_t i = 0; i < value.operands.size(); ++i)
                {
                    const auto& operand = value.operands[i];
                    const size_t next = i + 1 == value.operands.size() ? opposite : block();
                    if (!operand.bytes.empty())
                    {
                        if (!operand.constant) expression(operand);
                        const bool stops = (operand.bytes[0] == 0) == conjunction;
                        jump(stops ? decisive : next, syntax);
                        if (stops) break;
                    }
                    else
                        branch(expression(operand), conjunction ? next : decisive,
                               conjunction ? decisive : next, syntax);
                    current = next;
                }
                current = decisive;
                constant(result, {static_cast<FRIDGE_WORD>(!conjunction)}, syntax);
                jump(done, syntax);
                current = opposite;
                constant(result, {static_cast<FRIDGE_WORD>(conjunction)}, syntax);
                jump(done, syntax);
                current = done;
                return result;
            }

            size_t arithmetic(const CPMType& type, const string& op, vector<size_t> inputs,
                              const CPMSyntaxTreeNode* syntax)
            {
                if (type.isPtr() && (op == "+" || op == "-"))
                {
                    size_t result = inputs.front();
                    for (size_t i = 1; i < inputs.size(); ++i)
                        result = indexAddress(result, inputs[i], type, sizeOf(*type.child), syntax, op == "-");
                    return result;
                }
                const size_t result = frame(type);
                emit(CPMIROp::Arithmetic, syntax, result, std::move(inputs)).operation = op;
                return result;
            }

            size_t expression(const CPMCheckedExpression& value)
            {
                const auto* syntax = value.syntax;
                if (value.constant && !value.bytes.empty())
                {
                    const size_t result = frame(value.type);
                    constant(result, value.bytes, syntax);
                    return result;
                }
                switch (value.kind)
                {
                case CPMExprKind::Name:
                {
                    const size_t source = object(value);
                    const size_t result = frame(value.type);
                    emit(CPMIROp::Copy, syntax, result, {source});
                    return result;
                }
                case CPMExprKind::Field:
                {
                    const size_t source = address(value);
                    const size_t result = frame(value.type);
                    emit(CPMIROp::Load, syntax, result, {source});
                    return result;
                }
                case CPMExprKind::Initializer:
                {
                    const size_t result = frame(value.type);
                    for (size_t i = 0; i < value.operands.size(); ++i)
                    {
                        const size_t source = expression(value.operands[i]);
                        emit(CPMIROp::Copy, syntax, result, {source}).offset = value.initializerOffsets[i];
                    }
                    return result;
                }
                case CPMExprKind::StringInitializer:
                {
                    const size_t buffer = buffers.at(&value);
                    constant(buffer, value.operands.front().bytes, syntax);
                    const size_t result = frame(value.type);
                    emit(CPMIROp::Address, syntax, result, {buffer});
                    return result;
                }
                case CPMExprKind::Call:
                {
                    if (value.intrinsic != CPMIntrinsic::None)
                    {
                        const auto& argument = value.operands.front();
                        size_t input = CPMIRNone;
                        if (argument.bytes.empty() || !argument.constant)
                        {
                            // A bit cast can read the same slot directly. Generated
                            // wrappers load named arguments without using scratch registers.
                            const auto* source = &argument;
                            while (source->kind == CPMExprKind::Operator && source->syntax->text == R_CAST)
                                source = &source->operands.front();
                            input = source->kind == CPMExprKind::Name ? object(*source) : expression(argument);
                        }
                        auto& instruction = emit(CPMIROp::Intrinsic, syntax, CPMIRNone,
                                                 input == CPMIRNone ? vector<size_t>{} : vector<size_t>{input});
                        instruction.intrinsic = value.intrinsic;
                        if (input == CPMIRNone) instruction.bytes = argument.bytes;
                        return CPMIRNone;
                    }
                    vector<size_t> arguments;
                    for (const auto& argument : value.operands) arguments.push_back(expression(argument));
                    const size_t result = temporary(value.type);
                    emit(CPMIROp::Call, syntax, result, std::move(arguments)).callee = value.function;
                    return result;
                }
                default:
                    break;
                }
                const string& op = syntax->text;
                if (op == R_ADDR) return address(value.operands[0]);
                if (op == R_AT)
                {
                    const size_t source = address(value);
                    const size_t result = frame(value.type);
                    emit(CPMIROp::Load, syntax, result, {source});
                    return result;
                }
                if (op == R_CAST || op == R_CONVERT)
                {
                    const size_t source = expression(value.operands[0]);
                    const size_t result = frame(value.type);
                    emit(op == R_CAST ? CPMIROp::Copy : CPMIROp::Convert, syntax, result, {source});
                    return result;
                }
                if (op == "&&" || op == "||") return shortCircuit(value);
                if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" ||
                    op == "%=" || op == "++" || op == "--")
                {
                    const size_t destination = address(value.operands[0]);
                    size_t result;
                    if (op == "=")
                        result = expression(value.operands[1]);
                    else
                    {
                        const size_t old = frame(value.type);
                        emit(CPMIROp::Load, syntax, old, {destination});
                        size_t right;
                        if (value.operands.size() == 2)
                            right = expression(value.operands[1]);
                        else
                        {
                            right = frame(value.type.isPtr() ? baseType(CPM_DATATYPE_UINT8) : value.type);
                            auto bytes = vector<FRIDGE_WORD>(function.slots[right].size);
                            bytes.back() = 1;
                            constant(right, std::move(bytes), syntax);
                        }
                        result = arithmetic(value.type, op.substr(0, 1), {old, right}, syntax);
                    }
                    emit(CPMIROp::Store, syntax, CPMIRNone, {destination, result});
                    return result;
                }
                vector<size_t> inputs;
                for (const auto& operand : value.operands) inputs.push_back(expression(operand));
                return arithmetic(value.type, op, std::move(inputs), syntax);
            }

            void statement(const CPMCheckedStatement& value)
            {
                if (current == CPMIRNone || !program.compiler.NoErrors()) return;
                const auto* syntax = value.syntax ? value.syntax : function.symbol->bodyNode;
                switch (value.kind)
                {
                case CPMStatementKind::Block:
                    for (const auto& child : value.statements) statement(child);
                    break;
                case CPMStatementKind::Declaration:
                    if (!value.expressions.empty())
                    {
                        const auto& declaration = *value.declaration;
                        const auto& initializer = value.expressions.front();
                        if (declaration.isConst)
                            program.initializeData(localData.at(&declaration.symbol), 0,
                                                   initializer, localData);
                        else
                        {
                            const size_t destination = objects.at(&declaration.symbol);
                            if (initializer.constant && !initializer.bytes.empty())
                                constant(destination, initializer.bytes, initializer.syntax);
                            else
                            {
                                const size_t source = expression(initializer);
                                emit(CPMIROp::Copy, syntax, destination, {source});
                            }
                        }
                    }
                    break;
                case CPMStatementKind::Expression:
                    expression(value.expressions.front());
                    break;
                case CPMStatementKind::Return:
                {
                    vector<size_t> inputs;
                    if (!value.expressions.empty()) inputs.push_back(expression(value.expressions.front()));
                    emit(CPMIROp::Return, syntax, function.result, std::move(inputs));
                    current = CPMIRNone;
                    break;
                }
                case CPMStatementKind::If:
                    conditional(value, syntax);
                    break;
                case CPMStatementKind::For:
                    loop(value, syntax);
                    break;
                case CPMStatementKind::Break:
                    jump(loops.back().first, syntax);
                    break;
                case CPMStatementKind::Continue:
                    jump(loops.back().second, syntax);
                    break;
                }
            }

            void conditional(const CPMCheckedStatement& value, const CPMSyntaxTreeNode* syntax)
            {
                const auto& test = value.expressions.front();
                if (!test.bytes.empty())
                {
                    if (!test.constant) expression(test);
                    if (test.bytes[0] != 0) statement(value.statements[0]);
                    else if (value.statements.size() == 2) statement(value.statements[1]);
                    return;
                }
                const size_t condition = expression(test);
                const size_t yes = block();
                const size_t no = block();
                const size_t done = block();
                branch(condition, yes, no, syntax);
                current = yes;
                statement(value.statements[0]);
                bool fallsThrough = current != CPMIRNone;
                if (fallsThrough) jump(done, syntax);
                current = no;
                if (value.statements.size() == 2) statement(value.statements[1]);
                if (current != CPMIRNone)
                {
                    fallsThrough = true;
                    jump(done, syntax);
                }
                current = fallsThrough ? done : CPMIRNone;
            }

            void loop(const CPMCheckedStatement& value, const CPMSyntaxTreeNode* syntax)
            {
                statement(value.statements[0]);
                const auto& test = value.expressions.front();
                const bool known = !test.bytes.empty();
                if (known && test.bytes[0] == 0)
                {
                    if (!test.constant) expression(test);
                    return;
                }
                const size_t condition = block();
                const size_t body = block();
                const size_t step = block();
                const size_t done = block();
                jump(condition, syntax);
                current = condition;
                if (known)
                {
                    if (!test.constant) expression(test);
                    jump(body, syntax);
                }
                else branch(expression(test), body, done, syntax);
                current = body;
                loops.emplace_back(done, step);
                statement(value.statements[1]);
                loops.pop_back();
                if (current != CPMIRNone) jump(step, syntax);
                current = step;
                if (value.expressions.size() == 2) expression(value.expressions[1]);
                jump(condition, syntax);
                current = value.fallsThrough ? done : CPMIRNone;
            }

            void pruneUnreachable()
            {
                vector<bool> reachable(function.blocks.size());
                vector<size_t> pending{0};
                while (!pending.empty())
                {
                    const auto id = pending.back();
                    pending.pop_back();
                    if (reachable[id]) continue;
                    reachable[id] = true;
                    const auto& block = function.blocks[id];
                    if (block.empty())
                    {
                        error(program.compiler, function.symbol->bodyNode,
                              "A reachable IR block has no terminator.");
                        return;
                    }
                    const auto& last = block.back();
                    if (last.op == CPMIROp::Jump || last.op == CPMIROp::Branch)
                        pending.push_back(last.target);
                    if (last.op == CPMIROp::Branch) pending.push_back(last.alternative);
                    if (last.op != CPMIROp::Jump && last.op != CPMIROp::Branch && last.op != CPMIROp::Return)
                    {
                        error(program.compiler, last.syntax, "A reachable IR block has no terminator.");
                        return;
                    }
                }
                for (size_t i = 0; i < reachable.size(); ++i)
                    if (!reachable[i]) function.blocks[i].clear();
            }

            void reserveBuffers(const CPMCheckedExpression& value, const CPMCheckedLocal& local)
            {
                if (value.kind == CPMExprKind::StringInitializer)
                {
                    const auto& characters = value.operands.front();
                    const string name = local.symbol.name + " characters";
                    const size_t id = local.isStatic ?
                        slot(characters.type, CPMIRStorage::Static, 0, name,
                             program.data(function.symbol->signature.name + "." + name, characters.bytes.size())) :
                        frame(characters.type, name);
                    buffers.emplace(&value, id);
                }
                for (const auto& child : value.operands) reserveBuffers(child, local);
            }

            void reserveBuffers(const CPMCheckedStatement& value)
            {
                if (value.declaration && !value.declaration->isConst && !value.expressions.empty())
                    reserveBuffers(value.expressions.front(), *value.declaration);
                for (const auto& child : value.statements) reserveBuffers(child);
            }

        public:
            FunctionBuilder(ProgramBuilder& program, const CPMFunctionSymbol& symbol)
                : program(program), function{&symbol, {}, {}, CPMIRNone, 0, 0, 0, {}}
            {
            }

            CPMIRFunction build()
            {
                const auto& symbol = *function.symbol;
                for (const auto& argument : symbol.arguments) function.argumentBytes += sizeOf(argument.typeExpr);
                size_t argumentOffset = function.argumentBytes + 2;
                for (const auto& argument : symbol.arguments)
                {
                    argumentOffset -= sizeOf(argument.typeExpr);
                    const size_t id = slot(argument.typeExpr, CPMIRStorage::Incoming, argumentOffset, argument.name);
                    function.parameters.push_back(id);
                    objects.emplace(&argument, id);
                }
                if (!symbol.returnType.isVoid())
                    function.result = slot(symbol.returnType, CPMIRStorage::Incoming, function.argumentBytes + 2, "result");
                for (const auto& local : symbol.checkedBody->locals)
                {
                    const auto& variable = local.symbol;
                    if (local.isConst || local.isStatic)
                    {
                        const size_t data = program.data(symbol.signature.name + "." + variable.name,
                                                         sizeOf(variable.typeExpr));
                        localData.emplace(&variable, data);
                        objects.emplace(&variable, slot(variable.typeExpr, CPMIRStorage::Static, 0, variable.name, data));
                    }
                    else
                        objects.emplace(&variable, frame(variable.typeExpr, variable.name));
                }
                reserveBuffers(symbol.checkedBody->body);
                function.localBytes = function.frameBytes;
                current = block();
                statement(symbol.checkedBody->body);
                if (current != CPMIRNone && symbol.returnType.isVoid())
                    emit(CPMIROp::Return, symbol.bodyNode);
                if (program.compiler.NoErrors()) pruneUnreachable();
                if (function.frameBytes + function.argumentBytes + sizeOf(symbol.returnType) + 2 > DataMaxSize)
                    error(program.compiler, symbol.bodyNode, "The function frame and incoming slots exceed stack address space.");
                return std::move(function);
            }
        };
    }

    CPMIRProgram CPMBuildIR(CPMCompiler& compiler)
    {
        ProgramBuilder builder{compiler, {}, {}};
        vector<CPMNamespace*> namespaces;
        compiler.getNamespaces(namespaces);
        for (auto space : namespaces)
            for (const auto& entry : space->functions)
            {
                if (!compiler.NoErrors()) return {};
                if (entry.second.checkedBody)
                    builder.program.functions.push_back(FunctionBuilder(builder, entry.second).build());
            }
        return std::move(builder.program);
    }
}
