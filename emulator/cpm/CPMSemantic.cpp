#include "stdafx.h"
#include "CPMSemantic.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <set>

namespace CPM
{
    namespace
    {
        CPMType baseType(CPMDataType base)
        {
            CPMType type;
            type.kind = base == CPM_DATATYPE_VOID ? CPM_TYPE_VOID : CPM_TYPE_BASE;
            type.base = base;
            return type;
        }

        CPMType compoundType(CPMTypeKind kind, const CPMType& child, int count = 0)
        {
            CPMType type;
            type.kind = kind;
            type.child = make_unique<CPMType>(child);
            type.arrayCount = count;
            return type;
        }

        CPMCheckedExpression checked(CPMExprKind kind, const CPMSyntaxTreeNode* node,
                                     const CPMType& type)
        {
            CPMCheckedExpression value;
            value.kind = kind;
            value.syntax = node;
            value.type = type;
            return value;
        }

        bool integer(const CPMType& type)
        {
            return type.isBase() && IsIntDataType(type.base);
        }

        vector<FRIDGE_WORD> encode(int64_t value, size_t size)
        {
            vector<FRIDGE_WORD> bytes(size);
            auto bits = static_cast<uint64_t>(value);
            for (size_t i = size; i > 0; --i)
            {
                bytes[i - 1] = static_cast<FRIDGE_WORD>(bits);
                bits >>= 8;
            }
            return bytes;
        }

        int64_t scalar(const CPMCheckedExpression& value)
        {
            int64_t bits = 0;
            for (auto byte : value.bytes) bits = (bits << 8) | byte;
            if (value.type == baseType(CPM_DATATYPE_BOOL)) return bits != 0;
            if (value.type.isBase() &&
                (value.type.base == CPM_DATATYPE_INT8 || value.type.base == CPM_DATATYPE_INT16))
            {
                const int64_t limit = int64_t(1) << (8 * value.bytes.size());
                if (bits >= limit / 2) bits -= limit;
            }
            return bits;
        }

        int64_t floorDivide(int64_t numerator, int64_t denominator)
        {
            const int64_t quotient = numerator / denominator;
            const int64_t remainder = numerator % denominator;
            return quotient - (remainder != 0 && (remainder < 0) != (denominator < 0));
        }

        bool parseInteger(const string& text, CPMType& type, int64_t& value)
        {
            const bool hex = text.compare(0, 2, "0x") == 0;
            const bool signedLiteral = !hex && (text.front() == '-' || text.front() == '+');
            const bool negative = signedLiteral && text.front() == '-';
            const size_t start = hex ? 2 : signedLiteral ? 1 : 0;
            const bool suffix = !hex && text.back() == 'd';
            const size_t end = text.size() - (suffix ? 1 : 0);
            if (end <= start || (hex && end - start != 2 && end - start != 4)) return false;
            const bool word = suffix || (hex && end - start == 4);
            unsigned magnitude = 0;
            auto result = from_chars(text.data() + start, text.data() + end, magnitude, hex ? 16 : 10);
            if (result.ec != errc{} || result.ptr != text.data() + end) return false;
            const unsigned limit = signedLiteral ? (word ? 32767u : 127u) + negative
                                                 : word ? 65535u : 255u;
            if (magnitude > limit) return false;
            type = baseType(signedLiteral ? (word ? CPM_DATATYPE_INT16 : CPM_DATATYPE_INT8)
                                          : (word ? CPM_DATATYPE_UINT16 : CPM_DATATYPE_UINT8));
            value = negative ? -int64_t(magnitude) : magnitude;
            return true;
        }

        bool identifier(const CPMSyntaxTreeNode* node)
        {
            return node->type == CPM_ID && !node->text.empty() &&
                CharIsIdentifierStart(node->text.front()) &&
                all_of(node->text.begin() + 1, node->text.end(), [](char c)
                { return CharIsIdentifierStart(c) || CharIsDigit(c); });
        }

        bool reserved(const string& name)
        {
            static const set<string> keywords{
                "namespace", "using", "imports", "include", "struct", "union", "static", "const",
                "ptr", "array", "if", "else", "for", "return", "break", "continue", "at", "addr",
                "cast", "convert", "void", "bool", "char", "uint8", "int8", "uint16", "int16",
                "string", "null", "true", "false"
            };
            return keywords.count(name) != 0 || name.compare(0, 2, "__") == 0;
        }

        bool isGroup(const CPMSyntaxTreeNode* node)
        {
            return node->type == CPM_LINE || node->type == CPM_EXPR;
        }

        CPMSyntaxTreeNode* unwrapSingle(CPMSyntaxTreeNode* node)
        {
            while (isGroup(node) && node->children.size() == 1)
                node = node->children.front();
            return node;
        }

        void append(CPMCheckedStatement& block, CPMCheckedStatement item)
        {
            if (block.fallsThrough)
            {
                block.breaksLoop |= item.breaksLoop;
                block.fallsThrough = item.fallsThrough;
            }
            block.statements.push_back(std::move(item));
        }
    }

    bool CPMIsUserIdentifier(const CPMSyntaxTreeNode* node)
    {
        return identifier(node) && !reserved(node->text);
    }

    CPMSemanticAnalyzer::CPMSemanticAnalyzer(CPMCompiler& compiler, CPMNamespace* owner,
                                              const CPMSyntaxTreeNode* before)
        : compiler(compiler), owner(owner ? owner : compiler.namespaces.at(GlobalNamespace)), before(before)
    {
    }

    void CPMSemanticAnalyzer::error(const CPMSyntaxTreeNode* node, const string& message)
    {
        failed = true;
        compiler.Error();
        compiler.CompilerLog()->Add(LOG_ERROR, message, node->sourceFileName, node->lineNumber);
    }

    vector<CPMNamespace*> CPMSemanticAnalyzer::visibleNamespaces(const CPMSyntaxTreeNode* node) const
    {
        vector<CPMNamespace*> result{owner};
        auto add = [&](const string& name)
        {
            auto found = compiler.namespaces.find(name);
            if (found != compiler.namespaces.end() &&
                find(result.begin(), result.end(), found->second) == result.end())
                result.push_back(found->second);
        };
        auto source = compiler.sources.find(node->sourceFileName);
        if (source != compiler.sources.end())
            for (const auto& name : source->second.usingNamespaces) add(name);
        add(GlobalNamespace);
        return result;
    }

    const CPMSemanticAnalyzer::Binding* CPMSemanticAnalyzer::lookupLocal(const string& name) const
    {
        for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope)
        {
            auto found = scope->find(name);
            if (found != scope->end()) return &found->second;
        }
        return nullptr;
    }

    bool CPMSemanticAnalyzer::checkName(CPMSyntaxTreeNode* node)
    {
        if (!CPMIsUserIdentifier(node))
        {
            error(node, "Invalid local or parameter name '" + node->text + "'.");
            return false;
        }
        for (auto ns : visibleNamespaces(node))
        {
            const bool functionName = any_of(ns->functions.begin(), ns->functions.end(),
                [&](const auto& entry) { return entry.first.name == node->text; });
            if (ns->statics.count(node->text) || ns->datatypes.count(node->text) || functionName)
            {
                error(node, "Name '" + node->text + "' conflicts with a visible declaration.");
                return false;
            }
        }
        if (lookupLocal(node->text) || compiler.namespaces.count(node->text))
        {
            error(node, "Name '" + node->text + "' conflicts with a visible declaration.");
            return false;
        }
        return true;
    }

    bool CPMSemanticAnalyzer::expect(CPMCheckedExpression& value, const CPMType& type,
                                     const CPMSyntaxTreeNode* node)
    {
        if (!value) return false;
        if (value.contextualNull && type.isPtr())
        {
            value.type = type;
            value.contextualNull = false;
            value.bytes = {0, 0};
        }
        if (value.contextualNull || value.type != type || type.isVoid())
        {
            error(node, "Expected " + compiler.GetTypeName(type) + ", got " +
                        (value.contextualNull ? "untyped null" : compiler.GetTypeName(value.type)) + ".");
            return false;
        }
        return true;
    }

    bool CPMSemanticAnalyzer::validateCall(CPMSyntaxTreeNode* node, size_t begin, size_t end)
    {
        for (size_t i = begin + 1; i < end; ++i)
        {
            const bool comma = node->commaBefore[i];
            if ((i == begin + 1 && comma) || (i > begin + 1 && !comma) ||
                !node->whitespaceBefore[i])
            {
                error(node->children[i], i == begin + 1 ? "Separate the first operand with whitespace." :
                                                         "Separate operands with a comma and whitespace.");
                return false;
            }
        }
        return true;
    }

    CPMCheckedExpression CPMSemanticAnalyzer::member(CPMCheckedExpression base, CPMSyntaxTreeNode* field)
    {
        if (!base) return {};
        if (field->type == CPM_REF)
        {
            for (auto part : field->children) base = member(std::move(base), part);
            return base;
        }
        if (!base.type.isStruct() || base.category == CPMValueCategory::Value)
        {
            error(field, "Field access requires an addressable struct or union.");
            return {};
        }
        auto structure = compiler.structTypes.at(base.type.base);
        auto found = structure->fields.find(field->text);
        if (found == structure->fields.end())
        {
            error(field, "Unknown field '" + field->text + "' in '" + structure->name + "'.");
            return {};
        }
        auto result = checked(CPMExprKind::Field, field, found->second.typeExpr);
        result.category = base.category;
        result.object = base.object;
        result.staticObject = base.staticObject;
        result.field = &found->second;
        result.constant = base.constant;
        result.constantAddress = base.constantAddress;
        const size_t offset = found->second.offset;
        const int size = compiler.sizeOfTypeExpr(result.type);
        if (size > 0 && offset + size <= base.bytes.size())
            result.bytes.assign(base.bytes.begin() + offset, base.bytes.begin() + offset + size);
        result.operands.push_back(std::move(base));
        return result;
    }

    CPMCheckedExpression CPMSemanticAnalyzer::name(CPMSyntaxTreeNode* node, bool allowZeroArgCall)
    {
        vector<CPMSyntaxTreeNode*> parts = node->type == CPM_REF ? node->children :
                                                               vector<CPMSyntaxTreeNode*>{node};
        size_t index = 0;
        const auto* local = lookupLocal(parts[0]->text);
        const CPMStaticSymbol* global = nullptr;
        if (!local)
        {
            auto qualified = parts.size() > 1 ? compiler.namespaces.find(parts[0]->text) :
                                               compiler.namespaces.end();
            if (qualified != compiler.namespaces.end())
            {
                ++index;
                global = compiler.findStatic(qualified->second, parts[index]->text, before);
            }
            else
            {
                global = compiler.findStatic(owner, parts[0]->text, before);
                if (!global)
                {
                    for (auto ns : visibleNamespaces(node))
                    {
                        if (ns == owner || ns->name == GlobalNamespace) continue;
                        auto found = compiler.findStatic(ns, parts[0]->text, before);
                        if (!found) continue;
                        if (global)
                        {
                            error(node, "Ambiguous name '" + parts[0]->text + "'.");
                            return {};
                        }
                        global = found;
                    }
                    if (!global)
                    {
                        auto root = compiler.namespaces.at(GlobalNamespace);
                        global = compiler.findStatic(root, parts[0]->text, before);
                    }
                }
            }
        }
        if (!local && !global)
        {
            if (allowZeroArgCall) return call(node, {});
            error(node, "Unknown name '" + node->text + "'.");
            return {};
        }
        const auto* object = local ? local->symbol : &global->field;
        auto result = checked(CPMExprKind::Name, parts[index], object->typeExpr);
        result.object = object;
        result.staticObject = global;
        const bool isConst = local ? local->local && local->local->isConst : global->isconst;
        result.category = isConst ? CPMValueCategory::ReadOnly : CPMValueCategory::Writable;
        result.constant = isConst;
        result.constantAddress = global || (local && local->local && local->local->isStatic) || isConst;
        if (isConst)
        {
            if (local)
                result.bytes = local->local->constantBytes;
            else if (global->initializer)
                result.bytes = global->initializer->bytes;
            else if (object->typeExpr.isPrimitive() && !object->typeExpr.isString())
                result.bytes = encode(global->immediateData, compiler.sizeOfTypeExpr(object->typeExpr));
            else if (!object->typeExpr.isString() && global->staticData)
                result.bytes.assign(global->staticData, global->staticData + compiler.sizeOfTypeExpr(object->typeExpr));
        }
        for (++index; index < parts.size(); ++index)
            result = member(std::move(result), parts[index]);
        return result;
    }

    CPMCheckedExpression CPMSemanticAnalyzer::call(CPMSyntaxTreeNode* head,
                                                   vector<CPMCheckedExpression> operands)
    {
        auto candidates = visibleNamespaces(head);
        string name = head->text;
        if (head->type == CPM_REF)
        {
            auto ns = head->children.size() == 2 ? compiler.namespaces.find(head->children[0]->text) :
                                                  compiler.namespaces.end();
            if (ns == compiler.namespaces.end())
            {
                error(head, "Invalid namespace qualifier in call to '" + name + "'.");
                return {};
            }
            candidates = {ns->second};
            name = head->children[1]->text;
        }
        const CPMFunctionSymbol* match = nullptr;
        for (auto ns : candidates)
        {
            for (const auto& entry : ns->functions)
            {
                const auto& signature = entry.first;
                if (signature.name != name || signature.arguments.size() != operands.size()) continue;
                bool matches = true;
                for (size_t i = 0; i < operands.size(); ++i)
                    matches &= operands[i].contextualNull ? signature.arguments[i].typeExpr.isPtr() :
                                                           operands[i].type == signature.arguments[i].typeExpr;
                if (!matches) continue;
                if (match)
                {
                    error(head, "Ambiguous call to '" + head->text + "'. Qualify the function name or type its arguments explicitly.");
                    return {};
                }
                match = &entry.second;
            }
        }
        if (!match)
        {
            string signature = head->text + "(";
            for (size_t i = 0; i < operands.size(); ++i)
            {
                if (i > 0) signature += ", ";
                signature += operands[i].contextualNull ? "null" : compiler.GetTypeName(operands[i].type, true);
            }
            signature += ")";
            error(head, "No matching overload for '" + signature + "'.");
            return {};
        }
        for (size_t i = 0; i < operands.size(); ++i)
            expect(operands[i], match->signature.arguments[i].typeExpr, head);
        auto result = checked(CPMExprKind::Call, head, match->returnType);
        result.function = match;
        result.operands = std::move(operands);
        return result;
    }

    CPMCheckedExpression CPMSemanticAnalyzer::expression(CPMSyntaxTreeNode* node)
    {
        if (isGroup(node))
            return sequence(node, 0, node->children.size());
        if (node->type == CPM_MEMBER)
        {
            auto receiver = expression(node->children[0]);
            if (!receiver) return {};
            if (receiver.kind != CPMExprKind::Operator || receiver.syntax->text != R_AT)
            {
                error(node, "Computed field access requires an at expression as its receiver.");
                return {};
            }
            return member(std::move(receiver), node->children[1]);
        }
        if (node->type == CPM_ID || node->type == CPM_REF)
        {
            if (node->type == CPM_ID && node->text == R_NULL)
            {
                auto result = checked(CPMExprKind::Literal, node, {});
                result.contextualNull = result.constant = true;
                return result;
            }
            return name(node, false);
        }
        if (node->type == CPM_NUM)
        {
            CPMType type;
            int64_t value = 0;
            if (!parseInteger(node->text, type, value))
            {
                error(node, "Invalid or out-of-range integer literal '" + node->text + "'.");
                return {};
            }
            auto result = checked(CPMExprKind::Literal, node, type);
            result.constant = true;
            result.bytes = encode(value, compiler.sizeOfTypeExpr(type));
            return result;
        }
        if (node->type == CPM_CHARLIT)
        {
            auto result = checked(CPMExprKind::Literal, node, baseType(CPM_DATATYPE_CHAR));
            result.constant = true;
            result.bytes = {static_cast<FRIDGE_WORD>(node->text[1])};
            return result;
        }
        if (node->type == CPM_STR)
        {
            auto content = CPMSTRContent(node);
            if (content.size() >= 65535)
            {
                error(node, "Quoted literal and terminator exceed the maximum array size.");
                return {};
            }
            auto result = checked(CPMExprKind::Literal, node,
                compoundType(CPM_TYPE_ARRAY, baseType(CPM_DATATYPE_CHAR), static_cast<int>(content.size() + 1)));
            result.category = CPMValueCategory::ReadOnly;
            result.constant = true;
            result.bytes.assign(content.begin(), content.end());
            result.bytes.push_back(0);
            return result;
        }
        error(node, "Expected a value expression.");
        return {};
    }

    CPMCheckedExpression CPMSemanticAnalyzer::sequence(CPMSyntaxTreeNode* node, size_t begin,
                                                       size_t end, bool asStatement)
    {
        if (begin == end)
        {
            error(node, "Expected an expression.");
            return {};
        }
        auto head = node->children[begin];
        if (end - begin == 1)
        {
            if ((head->type == CPM_ID || head->type == CPM_REF) && head->text != R_NULL &&
                (asStatement || node->type == CPM_EXPR))
                return name(head, true);
            return expression(head);
        }
        if ((head->type != CPM_ID && head->type != CPM_REF) || !validateCall(node, begin, end))
        {
            if (head->type != CPM_ID && head->type != CPM_REF)
                error(head, "Expected a function or operator name.");
            return {};
        }
        const string& op = head->text;
        if (op == R_CAST || op == R_CONVERT)
        {
            if (end - begin != 3)
            {
                error(head, op + " expects a type and a value.");
                return {};
            }
            CPMType target;
            if (!compiler.parseTypeExpr(node->children[begin + 1], target,
                                        compiler.getSourceFile(head->sourceFileName), owner, this))
            {
                failed = true;
                return {};
            }
            auto value = expression(node->children[begin + 2]);
            if (!value) return {};
            if (op == R_CAST && value.contextualNull && target.isPtr())
                expect(value, target, head);
            if (value.contextualNull || value.type.isVoid() || target.isVoid())
            {
                error(head, op + " requires a typed, non-void value and target.");
                return {};
            }
            if (!compiler.validTypeLayout(target))
            {
                error(head, "The target type exceeds the maximum value size.");
                return {};
            }
            auto result = checked(CPMExprKind::Operator, head, target);
            if (op == R_CAST)
            {
                if (compiler.sizeOfTypeExpr(target) != compiler.sizeOfTypeExpr(value.type) ||
                    compiler.sizeOfTypeExpr(target) <= 0)
                {
                    error(head, "Cast requires equal-size value types.");
                    return {};
                }
                result.bytes = value.bytes;
            }
            else
            {
                const auto boolean = baseType(CPM_DATATYPE_BOOL);
                const auto character = baseType(CPM_DATATYPE_CHAR);
                const auto byte = baseType(CPM_DATATYPE_UINT8);
                const auto& from = value.type;
                if (!((integer(from) && integer(target)) ||
                      (integer(from) && target == boolean) ||
                      (from == boolean && integer(target)) ||
                      (from == character && target == byte) ||
                      (from == byte && target == character) ||
                      (from == target && (from == boolean || from == character))))
                {
                    error(head, "Unsupported scalar conversion from " + compiler.GetTypeName(from) +
                                " to " + compiler.GetTypeName(target) + ".");
                    return {};
                }
                if (from == target)
                    result.bytes = value.bytes;
                else if (!value.bytes.empty())
                {
                    auto number = scalar(value);
                    if (target == boolean) number = number != 0;
                    result.bytes = encode(number, compiler.sizeOfTypeExpr(target));
                }
            }
            result.constant = value.constant;
            result.operands.push_back(std::move(value));
            return result;
        }
        vector<CPMCheckedExpression> operands;
        bool shortCircuited = false;
        bool valid = true;
        for (size_t i = begin + 1; i < end; ++i)
        {
            if (shortCircuited) ++unevaluatedDepth;
            auto value = expression(node->children[i]);
            if (shortCircuited) --unevaluatedDepth;
            valid &= bool(value);
            if (value && value.type == baseType(CPM_DATATYPE_BOOL) && !value.bytes.empty())
                shortCircuited |= (op == "&&" && scalar(value) == 0) ||
                                  (op == "||" && scalar(value) != 0);
            operands.push_back(std::move(value));
        }
        if (!valid) return {};
        static const set<string> operators{
            R_AT, R_ADDR, "+", "-", "*", "/", "%", "&", "|", "^", "~", "<<", ">>",
            "==", "!=", "<", ">", "<=", ">=", "&&", "||", "!", "++", "--",
            "=", "+=", "-=", "*=", "/=", "%="
        };
        return operators.count(op) ? operation(head, std::move(operands)) : call(head, std::move(operands));
    }

    CPMCheckedExpression CPMSemanticAnalyzer::operation(CPMSyntaxTreeNode* head,
                                                        vector<CPMCheckedExpression> args)
    {
        const string& op = head->text;
        const auto boolean = baseType(CPM_DATATYPE_BOOL);
        auto reject = [&](const string& reason) -> CPMCheckedExpression
        {
            error(head, reason);
            return {};
        };
        auto make = [&](const CPMType& type)
        {
            auto result = checked(CPMExprKind::Operator, head, type);
            result.operands = std::move(args);
            return result;
        };
        if (op == R_AT)
        {
            if (args.empty() || args.size() > 2)
                return reject("at expects one or two operands.");
            const auto& receiver = args[0];
            const bool array = receiver.type.isArray();
            if (array && args.size() != 2)
                return reject("An array access requires an index.");
            if (!array && !receiver.type.isPtr() && !receiver.type.isString())
                return reject("at requires an array, pointer, or string.");
            if (args.size() == 2 && args[1].type != baseType(CPM_DATATYPE_UINT8) &&
                                    args[1].type != baseType(CPM_DATATYPE_UINT16))
                return reject("An index must have type uint8 or uint16.");
            const bool knownIndex = args.size() == 1 || !args[1].bytes.empty();
            const size_t index = args.size() == 2 && knownIndex ? scalar(args[1]) : 0;
            if (array && knownIndex && index >= static_cast<size_t>(receiver.type.arrayCount))
                return reject("Constant array index is out of bounds.");
            auto element = receiver.type.isString() ? baseType(CPM_DATATYPE_CHAR) : *receiver.type.child;
            auto result = checked(CPMExprKind::Operator, head, element);
            result.category = array ? receiver.category : CPMValueCategory::Writable;
            if (receiver.type.isString() && receiver.category == CPMValueCategory::ReadOnly)
                result.category = CPMValueCategory::ReadOnly;
            if (array)
            {
                result.object = receiver.object;
                result.staticObject = receiver.staticObject;
                result.constant = receiver.constant && args[1].constant;
                result.constantAddress = receiver.constantAddress && args[1].constant && knownIndex;
                const int size = compiler.sizeOfTypeExpr(element);
                if (size > 0 && knownIndex && (index + 1) * size <= receiver.bytes.size())
                    result.bytes.assign(receiver.bytes.begin() + index * size,
                                        receiver.bytes.begin() + (index + 1) * size);
            }
            result.operands = std::move(args);
            return result;
        }
        if (op == R_ADDR)
        {
            if (args.size() != 1 || args[0].type.isVoid() ||
                args[0].category == CPMValueCategory::Value ||
                (args[0].category == CPMValueCategory::ReadOnly && !args[0].object))
                return reject("addr requires addressable storage. A quoted literal cannot yield a writable pointer.");
            auto type = compoundType(CPM_TYPE_PTR, args[0].type);
            const bool staticAddress = args[0].constantAddress;
            auto result = make(type);
            result.constant = staticAddress;
            return result;
        }
        if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=")
        {
            if (args.size() != 2 || args[0].category != CPMValueCategory::Writable)
                return reject("Assignment requires two operands and a writable destination.");
            const auto type = args[0].type;
            if (type.isPtr() && (op == "+=" || op == "-="))
            {
                if (!integer(args[1].type))
                    return reject("Pointer adjustment requires an integer offset.");
            }
            else
            {
                if (op != "=" && !integer(type))
                    return reject("Compound arithmetic assignment requires an integer destination.");
                if (!expect(args[1], type, head)) return {};
                if ((op == "/=" || op == "%=") && !unevaluatedDepth &&
                    !args[1].bytes.empty() && scalar(args[1]) == 0)
                    return reject("Division by zero in a constant operand.");
            }
            return make(type);
        }
        if (op == "++" || op == "--")
        {
            if (args.size() != 1 || args[0].category != CPMValueCategory::Writable ||
                !(integer(args[0].type) || args[0].type.isPtr()))
                return reject(op + " requires one writable integer or pointer.");
            auto type = args[0].type;
            return make(type);
        }

        const bool logic = op == "&&" || op == "||" || op == "!";
        const bool equality = op == "==" || op == "!=";
        const bool ordering = op == "<" || op == ">" || op == "<=" || op == ">=";
        const bool shift = op == "<<" || op == ">>";
        const size_t minimum = op == "+" || op == "-" || op == "~" || op == "!" ? 1 : 2;
        const bool exact = op == "~" || op == "!" || shift || op == "%";
        if (args.size() < minimum || (exact && args.size() != minimum))
            return reject("Wrong number of operands for '" + op + "'.");
        if (any_of(args.begin(), args.end(), [](const auto& arg)
            { return arg.type.isVoid() || arg.contextualNull; }))
            return reject("Operator '" + op + "' requires typed, non-void operands.");

        const auto type = args[0].type;
        const bool pointerArithmetic = type.isPtr() && (op == "+" || op == "-");
        if (pointerArithmetic)
        {
            if (args.size() < 2 || !all_of(args.begin() + 1, args.end(), [](const auto& arg)
                { return integer(arg.type); }))
                return reject("Pointer arithmetic requires one or more integer offsets.");
        }
        else if (shift)
        {
            if (!integer(type) || !integer(args[1].type))
                return reject("A shift requires an integer value and an integer count.");
        }
        else
        {
            const bool validType = logic ? type == boolean :
                equality ? (type.isPrimitive() || type.isPtr()) : integer(type);
            if (!validType || !all_of(args.begin(), args.end(), [&](const auto& arg) { return arg.type == type; }))
                return reject("Operator '" + op + "' requires matching " +
                              (logic ? "bool" : equality ? "scalar" : "integer") + " operands.");
        }

        const int width = compiler.sizeOfTypeExpr(type) * 8;
        if (!unevaluatedDepth)
        {
            if ((op == "/" || op == "%") &&
                any_of(args.begin() + 1, args.end(), [](const auto& arg)
                { return !arg.bytes.empty() && scalar(arg) == 0; }))
                return reject("Division by zero in a constant operand.");
            if (shift && !args[1].bytes.empty() &&
                (scalar(args[1]) < 0 || scalar(args[1]) >= width))
                return reject("Constant shift count is out of range.");
        }

        auto result = make(logic || equality || ordering ? boolean : type);
        auto& values = result.operands;
        result.constant = all_of(values.begin(), values.end(), [](const auto& arg) { return arg.constant; });
        if (op == "&&" || op == "||")
        {
            for (const auto& value : values)
            {
                if (value.bytes.empty()) break;
                if ((op == "&&" && scalar(value) == 0) || (op == "||" && scalar(value) != 0))
                {
                    result.bytes = {static_cast<FRIDGE_WORD>(op == "||")};
                    return result;
                }
            }
        }
        if (!all_of(values.begin(), values.end(), [](const auto& arg) { return !arg.bytes.empty(); }))
            return result;
        // Only short-circuited paths can reach here with an invalid constant
        // divisor or shift count. Keep their types without executing the fold.
        if ((op == "/" || op == "%") &&
            any_of(values.begin() + 1, values.end(), [](const auto& arg) { return scalar(arg) == 0; }))
            return result;
        if (shift && (scalar(values[1]) < 0 || scalar(values[1]) >= width))
            return result;

        int64_t value = scalar(values[0]);
        if (op == "~") value = ~value;
        if (op == "!") value = !value;
        if (op == "-" && values.size() == 1) value = -value;
        if (equality || ordering) value = 1;
        set<int64_t> seen{scalar(values[0])};
        const bool signedType = type == baseType(CPM_DATATYPE_INT8) || type == baseType(CPM_DATATYPE_INT16);
        const int64_t modulus = int64_t(1) << width;
        for (size_t i = 1; i < values.size(); ++i)
        {
            int64_t next = scalar(values[i]);
            if (pointerArithmetic) next *= compiler.sizeOfTypeExpr(*type.child);
            if (op == "+") value += next;
            else if (op == "-") value -= next;
            else if (op == "*") value *= next;
            else if (op == "/") value = floorDivide(value, next);
            else if (op == "%") value -= floorDivide(value, next) * next;
            else if (op == "&") value &= next;
            else if (op == "|") value |= next;
            else if (op == "^") value ^= next;
            else if (op == "<<") value *= int64_t(1) << next;
            else if (op == ">>") value = floorDivide(value, int64_t(1) << next);
            else if (op == "&&") value = value && next;
            else if (op == "||") value = value || next;
            else if (op == "==") value &= scalar(values[0]) == next;
            else if (op == "!=") value &= seen.insert(next).second;
            else if (op == "<") value &= scalar(values[i - 1]) < next;
            else if (op == ">") value &= scalar(values[i - 1]) > next;
            else if (op == "<=") value &= scalar(values[i - 1]) <= next;
            else if (op == ">=") value &= scalar(values[i - 1]) >= next;
            if (!equality && !ordering && !logic)
            {
                value = (value % modulus + modulus) % modulus;
                if (signedType && value >= modulus / 2) value -= modulus;
            }
        }
        result.bytes = encode(value, compiler.sizeOfTypeExpr(result.type));
        return result;
    }

    CPMCheckedExpression CPMSemanticAnalyzer::initializerElement(CPMSyntaxTreeNode* node, const CPMType& type)
    {
        // Lines in a list wrap one element, or contain a paren-less call.
        return initializer(node->type == CPM_LINE && node->children.size() == 1 ?
                           node->children[0] : node, type);
    }

    CPMCheckedExpression CPMSemanticAnalyzer::initializer(CPMSyntaxTreeNode* node, const CPMType& type)
    {
        auto literal = unwrapSingle(node);
        if (type.isString() && literal->type == CPM_STR)
        {
            auto characters = expression(literal);
            if (!characters) return {};
            auto result = checked(CPMExprKind::StringInitializer, literal, type);
            result.constant = true;
            result.operands.push_back(std::move(characters));
            return result;
        }

        auto result = checked(CPMExprKind::Initializer, node, type);
        auto add = [&](CPMCheckedExpression value, size_t offset)
        {
            if (!value) return false;
            result.initializerOffsets.push_back(offset);
            result.operands.push_back(std::move(value));
            return true;
        };
        auto finish = [&]()
        {
            result.constant = all_of(result.operands.begin(), result.operands.end(),
                                     [](const auto& value) { return value.constant; });
            if (all_of(result.operands.begin(), result.operands.end(),
                       [](const auto& value) { return !value.bytes.empty(); }))
            {
                result.bytes.resize(compiler.sizeOfTypeExpr(type));
                for (size_t i = 0; i < result.operands.size(); ++i)
                {
                    const auto& bytes = result.operands[i].bytes;
                    copy(bytes.begin(), bytes.end(), result.bytes.begin() + result.initializerOffsets[i]);
                }
            }
            return std::move(result);
        };

        if (type.isArray() && node->type == CPM_BLOCK)
        {
            if (node->children.size() - 2 != static_cast<size_t>(type.arrayCount))
            {
                error(node, "Array initializer must supply exactly " + to_string(type.arrayCount) + " elements.");
                return {};
            }
            const size_t stride = compiler.sizeOfTypeExpr(*type.child);
            for (size_t i = 0; i < static_cast<size_t>(type.arrayCount); ++i)
                if (!add(initializerElement(node->children[i + 1], *type.child), i * stride))
                    return {};
            return finish();
        }
        if (type.isStruct())
        {
            auto structure = compiler.structTypes.at(type.base);
            const bool singleBinding = isGroup(node) && node->children.size() == 2 &&
                structure->fields.count(node->children[0]->text) &&
                !node->commaBefore[1];
            if (node->type == CPM_BLOCK || singleBinding)
            {
                vector<CPMSyntaxTreeNode*> entries{node};
                if (node->type == CPM_BLOCK)
                    entries.assign(node->children.begin() + 1, node->children.end() - 1);
                set<string> initialized;
                for (auto entry : entries)
                {
                    if (entry->children.size() != 2 || !identifier(entry->children[0]) ||
                        !CPMValidateDeclaration(entry, compiler.CompilerLog()))
                    {
                        error(entry, "Expected a named field and one value.");
                        return {};
                    }
                    auto fieldName = entry->children[0];
                    auto field = structure->fields.find(fieldName->text);
                    if (field == structure->fields.end() || !initialized.insert(fieldName->text).second)
                    {
                        error(fieldName, "Unknown or repeated initializer field '" + fieldName->text + "'.");
                        return {};
                    }
                    if (!add(initializer(entry->children[1], field->second.typeExpr), field->second.offset))
                        return {};
                }
                const bool completeView = any_of(structure->views.begin(), structure->views.end(),
                    [&](const auto& view)
                    {
                        return view.size() == initialized.size() &&
                            all_of(view.begin(), view.end(), [&](const auto& name)
                            { return initialized.count(name) != 0; });
                    });
                if (!completeView)
                {
                    error(node, structure->isUnion ? "Initialize exactly one complete union view." :
                                                     "Initialize every struct field exactly once.");
                    return {};
                }
                return finish();
            }
        }
        if (type.isArray() && type.arrayCount == 1 && node->type == CPM_EXPR && node->children.size() == 1)
        {
            auto element = node->children.front();
            const bool fieldBinding = type.child->isStruct() && isGroup(element) &&
                element->children.size() == 2 && !element->commaBefore[1] &&
                compiler.structTypes.at(type.child->base)->fields.count(element->children[0]->text);
            if (element->type == CPM_BLOCK || fieldBinding || (type.child->isString() && literal->type == CPM_STR))
            {
                if (!add(initializer(element, *type.child), 0)) return {};
                return finish();
            }
        }
        auto value = expression(node);
        if (!value) return {};
        if (type.isArray() && type.arrayCount == 1 && node->type == CPM_EXPR &&
            (value.type == *type.child || (value.contextualNull && type.child->isPtr())))
        {
            if (!expect(value, *type.child, node) || !add(std::move(value), 0)) return {};
            return finish();
        }
        return expect(value, type, node) ? std::move(value) : CPMCheckedExpression{};
    }

    int CPMSemanticAnalyzer::arrayCount(CPMSyntaxTreeNode* node)
    {
        auto value = expression(node);
        if (!value) return 0;
        if (!integer(value.type) || value.bytes.empty() || scalar(value) <= 0)
        {
            error(node, "An array size must be a positive compile-time integer.");
            return 0;
        }
        return static_cast<int>(scalar(value));
    }

    CPMCheckedStatement CPMSemanticAnalyzer::block(CPMSyntaxTreeNode* node, bool introduceScope)
    {
        CPMCheckedStatement result;
        result.syntax = node;
        if (introduceScope) scopes.emplace_back();
        if (node->type == CPM_BLOCK)
        {
            for (size_t i = 1; i + 1 < node->children.size(); ++i)
            {
                auto line = node->children[i];
                if (line->children.empty()) continue;
                const auto& head = line->children[0]->text;
                if ((head == "if" || head == "for") && i + 2 < node->children.size() &&
                    !node->children[i + 1]->children.empty() &&
                    node->children[i + 1]->children[0]->text == "else")
                {
                    // Else clauses can follow a semicolon-terminated simple
                    // statement. Keep the original tokens and locations.
                    auto chain = *line;
                    while (i + 2 < node->children.size())
                    {
                        auto next = node->children[i + 1];
                        if (next->children.empty() || next->children[0]->text != "else") break;
                        chain.children.insert(chain.children.end(), next->children.begin(), next->children.end());
                        chain.commaBefore.insert(chain.commaBefore.end(), next->commaBefore.begin(), next->commaBefore.end());
                        chain.whitespaceBefore.insert(chain.whitespaceBefore.end(),
                                                      next->whitespaceBefore.begin(), next->whitespaceBefore.end());
                        ++i;
                    }
                    append(result, statement(&chain, 0, chain.children.size()));
                }
                else
                    append(result, statement(line, 0, line->children.size()));
            }
        }
        else if (!node->children.empty())
            append(result, statement(node, 0, node->children.size()));
        if (introduceScope) scopes.pop_back();
        return result;
    }

    CPMCheckedStatement CPMSemanticAnalyzer::statement(CPMSyntaxTreeNode* node, size_t begin, size_t end)
    {
        CPMCheckedStatement result;
        if (begin == end) return result;
        auto head = node->children[begin];
        result.syntax = head;
        if (end - begin == 1 && head->type == CPM_BLOCK)
            return block(head, true);
        if (end - begin == 1 && head->type == CPM_EXPR)
            return head->children.empty() ? block(head, true) : statement(head, 0, head->children.size());
        const auto& op = head->text;
        if (op == "if") return conditional(node, begin, end);
        if (op == "for") return loop(node, begin, end);
        if (op == "else")
        {
            error(head, "else requires a preceding if branch.");
            return result;
        }
        if (op == "return")
        {
            result.kind = CPMStatementKind::Return;
            result.fallsThrough = false;
            if (end - begin == 2 && (node->commaBefore[begin + 1] || !node->whitespaceBefore[begin + 1]))
                error(head, "Separate return from its value with whitespace.");
            if (end - begin > 2 || (end - begin == 1) != function->returnType.isVoid())
                error(head, function->returnType.isVoid() ? "A void function must return without a value." :
                                                            "A non-void function must return one value.");
            else if (end - begin == 2)
            {
                auto value = expression(node->children[begin + 1]);
                if (expect(value, function->returnType, head))
                    result.expressions.push_back(std::move(value));
            }
            return result;
        }
        if (op == "break" || op == "continue")
        {
            if (end - begin != 1 || !loopDepth)
                error(head, op + " takes no operands and requires an enclosing loop.");
            result.kind = op == "break" ? CPMStatementKind::Break : CPMStatementKind::Continue;
            result.fallsThrough = false;
            result.breaksLoop = op == "break";
            return result;
        }
        CPMSyntaxTreeNode probe{};
        probe.sourceFileName = head->sourceFileName;
        probe.children = {head};
        if (op == R_STATIC || op == R_CONST || compiler.isTypeDeclaration(&probe, owner))
            return declaration(node, begin, end);

        auto value = sequence(node, begin, end, true);
        if (value && value.contextualNull)
            error(head, "null requires a pointer type from its context.");
        else if (value)
        {
            result.kind = CPMStatementKind::Expression;
            result.expressions.push_back(std::move(value));
        }
        return result;
    }

    CPMCheckedStatement CPMSemanticAnalyzer::conditional(CPMSyntaxTreeNode* node, size_t begin, size_t end)
    {
        CPMCheckedStatement result;
        result.kind = CPMStatementKind::If;
        result.syntax = node->children[begin];
        if (end - begin < 3 || node->children[begin + 1]->type != CPM_EXPR)
        {
            error(result.syntax, "if requires a parenthesized condition and a body.");
            return result;
        }
        auto condition = expression(node->children[begin + 1]);
        expect(condition, baseType(CPM_DATATYPE_BOOL), node->children[begin + 1]);
        result.expressions.push_back(std::move(condition));
        auto alternative = begin + 2;
        unsigned nested = 0;
        for (; alternative < end; ++alternative)
        {
            const auto& token = node->children[alternative]->text;
            if (token == "if") ++nested;
            if (token == "else")
            {
                if (nested == 0) break;
                --nested;
            }
        }
        if (alternative == begin + 2)
            error(result.syntax, "if requires a body before else.");
        scopes.emplace_back();
        result.statements.push_back(statement(node, begin + 2, alternative));
        scopes.pop_back();
        if (alternative < end)
        {
            if (alternative + 1 == end)
                error(node->children[alternative], "else requires a body.");
            scopes.emplace_back();
            result.statements.push_back(statement(node, alternative + 1, end));
            scopes.pop_back();
        }
        const auto& test = result.expressions.front();
        const bool known = test.type == baseType(CPM_DATATYPE_BOOL) && !test.bytes.empty();
        const bool canBeTrue = !known || scalar(test) != 0;
        const bool canBeFalse = !known || scalar(test) == 0;
        const auto& thenBranch = result.statements[0];
        const auto* elseBranch = result.statements.size() == 2 ? &result.statements[1] : nullptr;
        result.fallsThrough = (canBeTrue && thenBranch.fallsThrough) ||
                              (canBeFalse && (!elseBranch || elseBranch->fallsThrough));
        result.breaksLoop = (canBeTrue && thenBranch.breaksLoop) ||
                            (canBeFalse && elseBranch && elseBranch->breaksLoop);
        return result;
    }

    CPMCheckedStatement CPMSemanticAnalyzer::loop(CPMSyntaxTreeNode* node, size_t begin, size_t end)
    {
        CPMCheckedStatement result;
        result.kind = CPMStatementKind::For;
        result.syntax = node->children[begin];
        if (end - begin < 3 || node->children[begin + 1]->type != CPM_BLOCK ||
            node->children[begin + 1]->semicolonCount != 2)
        {
            error(result.syntax, "for requires (initializer; condition; step) and a body.");
            return result;
        }
        auto header = node->children[begin + 1];
        auto init = header->children[1];
        auto test = header->children[2];
        scopes.emplace_back();
        auto initial = statement(init, 0, init->children.size());
        if (!init->children.empty() && initial.kind != CPMStatementKind::Declaration &&
            !(initial.kind == CPMStatementKind::Expression && !initial.expressions.empty() &&
              initial.expressions[0].kind == CPMExprKind::Operator &&
              initial.expressions[0].syntax->text == OP_ASSIGN))
            error(init, "A for initializer must be a declaration or assignment.");
        result.statements.push_back(std::move(initial));
        auto condition = expression(test);
        expect(condition, baseType(CPM_DATATYPE_BOOL), test);
        result.expressions.push_back(std::move(condition));
        if (header->children.size() == 5)
        {
            auto step = expression(header->children[3]);
            if (step.contextualNull) error(header->children[3], "A for step requires a typed expression.");
            result.expressions.push_back(std::move(step));
        }
        ++loopDepth;
        scopes.emplace_back();
        result.statements.push_back(statement(node, begin + 2, end));
        scopes.pop_back();
        --loopDepth;
        scopes.pop_back();
        const auto& checkedCondition = result.expressions.front();
        result.fallsThrough = checkedCondition.type != baseType(CPM_DATATYPE_BOOL) ||
                              checkedCondition.bytes.empty() || scalar(checkedCondition) == 0 ||
                              result.statements.back().breaksLoop;
        return result;
    }

    CPMCheckedStatement CPMSemanticAnalyzer::declaration(CPMSyntaxTreeNode* node, size_t begin, size_t end)
    {
        CPMCheckedStatement result;
        result.kind = CPMStatementKind::Declaration;
        result.syntax = node->children[begin];
        const bool isConst = result.syntax->text == R_CONST;
        const bool isStatic = result.syntax->text == R_STATIC;
        if (isConst || isStatic) ++begin;

        CPMSyntaxTreeNode parts{};
        parts.type = CPM_LINE;
        parts.sourceFileName = result.syntax->sourceFileName;
        parts.lineNumber = result.syntax->lineNumber;
        parts.children.assign(node->children.begin() + begin, node->children.begin() + end);
        parts.commaBefore.assign(node->commaBefore.begin() + begin, node->commaBefore.begin() + end);
        CPMType type;
        if (!compiler.parseDataDeclaration(&parts, type, owner, this))
        {
            failed = true;
            return result;
        }
        auto name = parts.children[1];
        if (!checkName(name)) return result;
        if (parts.children.size() == 3)
        {
            auto value = initializer(parts.children[2], type);
            if (!value) return result;
            if (isConst && !value.constant)
                error(parts.children[2], "A const initializer must be computable at compile time.");
            result.expressions.push_back(std::move(value));
        }
        auto& local = checkedFunction->locals.emplace_back();
        local.symbol.name = name->text;
        local.symbol.owner = owner;
        local.symbol.typeExpr = type;
        local.isStatic = isStatic;
        local.isConst = isConst;
        if (isConst)
        {
            if (result.expressions.empty())
                local.constantBytes.resize(compiler.sizeOfTypeExpr(type));
            else
                local.constantBytes = result.expressions[0].bytes;
        }
        result.declaration = &local;
        scopes.back().emplace(name->text, Binding{&local.symbol, &local});
        return result;
    }

    void CPMSemanticAnalyzer::checkFunction(CPMFunctionSymbol& symbol)
    {
        function = &symbol;
        auto result = make_shared<CPMCheckedFunction>();
        checkedFunction = result.get();
        scopes.emplace_back();
        for (size_t i = 0; i < symbol.arguments.size(); ++i)
        {
            auto node = symbol.signature.arguments[i].nameNode;
            if (checkName(node))
                scopes.back().emplace(node->text, Binding{&symbol.arguments[i], nullptr});
        }
        result->body = block(symbol.bodyNode, false);
        scopes.pop_back();
        if (!failed && !symbol.returnType.isVoid() && result->body.fallsThrough)
            error(symbol.bodyNode, "A reachable end of non-void function '" + symbol.signature.name + "' requires a return value.");
        size_t frameBytes = 0;
        for (const auto& local : result->locals)
            if (!local.isConst && !local.isStatic)
                frameBytes += compiler.sizeOfTypeExpr(local.symbol.typeExpr);
        if (frameBytes > DataMaxSize)
            error(symbol.bodyNode, "Local values exceed the available frame size.");
        if (!failed) symbol.checkedBody = std::move(result);
    }
}
