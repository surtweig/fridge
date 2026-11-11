#pragma once

#include "CPMCompiler.h"

namespace CPM
{
    bool CPMIsUserIdentifier(const CPMSyntaxTreeNode* node);

    enum class CPMValueCategory { Value, ReadOnly, Writable };
    enum class CPMExprKind
    {
        Invalid, Literal, Name, Field, Operator, Call, Initializer, StringInitializer
    };

    struct CPMCheckedExpression
    {
        CPMExprKind kind = CPMExprKind::Invalid;
        const CPMSyntaxTreeNode* syntax = nullptr;
        CPMType type;
        CPMValueCategory category = CPMValueCategory::Value;
        const CPMDataSymbol* object = nullptr;
        const CPMStaticSymbol* staticObject = nullptr;
        const CPMFunctionSymbol* function = nullptr;
        const CPMDataSymbol* field = nullptr;
        bool contextualNull = false;
        bool constant = false;
        bool constantAddress = false;
        // Empty for runtime values and for constants requiring relocation.
        vector<FRIDGE_WORD> bytes;
        vector<CPMCheckedExpression> operands;
        vector<size_t> initializerOffsets;

        explicit operator bool() const { return kind != CPMExprKind::Invalid; }
    };

    enum class CPMStatementKind { Block, Declaration, Expression, If, For, Return, Break, Continue };

    struct CPMCheckedLocal
    {
        CPMDataSymbol symbol;
        bool isConst = false;
        bool isStatic = false;
        vector<FRIDGE_WORD> constantBytes;
    };

    struct CPMCheckedStatement
    {
        CPMStatementKind kind = CPMStatementKind::Block;
        const CPMSyntaxTreeNode* syntax = nullptr;
        const CPMCheckedLocal* declaration = nullptr;
        // If: condition and then/else bodies. For: condition and optional step,
        // with initialization and body in statements.
        vector<CPMCheckedExpression> expressions;
        vector<CPMCheckedStatement> statements;
        bool fallsThrough = true;
        bool breaksLoop = false;
    };

    struct CPMCheckedFunction
    {
        deque<CPMCheckedLocal> locals;
        CPMCheckedStatement body;
    };

    class CPMSemanticAnalyzer
    {
        struct Binding
        {
            const CPMDataSymbol* symbol;
            const CPMCheckedLocal* local;
        };

        CPMCompiler& compiler;
        CPMNamespace* owner;
        const CPMSyntaxTreeNode* before;
        CPMFunctionSymbol* function = nullptr;
        CPMCheckedFunction* checkedFunction = nullptr;
        vector<map<string, Binding>> scopes;
        unsigned loopDepth = 0;
        unsigned unevaluatedDepth = 0;
        bool failed = false;

        void error(const CPMSyntaxTreeNode* node, const string& message);
        vector<CPMNamespace*> visibleNamespaces(const CPMSyntaxTreeNode* node) const;
        const Binding* lookupLocal(const string& name) const;
        bool checkName(CPMSyntaxTreeNode* node);
        bool expect(CPMCheckedExpression& value, const CPMType& type, const CPMSyntaxTreeNode* node);
        bool validateCall(CPMSyntaxTreeNode* sequence, size_t begin, size_t end);
        CPMCheckedExpression name(CPMSyntaxTreeNode* node, bool allowZeroArgCall);
        CPMCheckedExpression member(CPMCheckedExpression base, CPMSyntaxTreeNode* field);
        CPMCheckedExpression call(CPMSyntaxTreeNode* head, vector<CPMCheckedExpression> operands);
        CPMCheckedExpression operation(CPMSyntaxTreeNode* head, vector<CPMCheckedExpression> operands);
        CPMCheckedExpression sequence(CPMSyntaxTreeNode* node, size_t begin, size_t end, bool asStatement = false);
        CPMCheckedExpression initializerElement(CPMSyntaxTreeNode* node, const CPMType& type);
        CPMCheckedStatement block(CPMSyntaxTreeNode* node, bool introduceScope);
        CPMCheckedStatement statement(CPMSyntaxTreeNode* node, size_t begin, size_t end);
        CPMCheckedStatement conditional(CPMSyntaxTreeNode* node, size_t begin, size_t end);
        CPMCheckedStatement loop(CPMSyntaxTreeNode* node, size_t begin, size_t end);
        CPMCheckedStatement declaration(CPMSyntaxTreeNode* node, size_t begin, size_t end);

    public:
        CPMSemanticAnalyzer(CPMCompiler& compiler, CPMNamespace* owner,
                            const CPMSyntaxTreeNode* before = nullptr);
        void checkFunction(CPMFunctionSymbol& symbol);
        CPMCheckedExpression expression(CPMSyntaxTreeNode* node);
        CPMCheckedExpression initializer(CPMSyntaxTreeNode* node, const CPMType& type);
        int arrayCount(CPMSyntaxTreeNode* node);
    };
}
