#pragma once
#include <string>
#include <vector>
#include <list>
#include <map>

using namespace std;

namespace CPM
{
    typedef enum CPMSyntaxNodeType
    {
        CPM_UNDEFINED,
        CPM_CHAR,
        CPM_COMMENT,     // trivia boundary; never satisfies required whitespace
        CPM_NUM,
        CPM_STR,
        CPM_CHARLIT,
        CPM_ID,
        CPM_REF,        // "."-joined identifier sequence: NS.Name, a.b.c
                        // (lexical joiner only — NOT a pointer/ref indicator;
                        // pointer types are expressed as `(T ptr)` type-exprs)
        CPM_EXPR,
        CPM_LINE,
        CPM_BLOCK,
    } CPMSyntaxNodeType;

    typedef enum CPMNumberFormat
    {
        CPM_DEC,
        CPM_HEX,
    } CPMNumberFormat;

    const char CPM_BLOCK_OPEN = '(';
    const char CPM_BLOCK_CLOSE = ')';
    const char CPM_LINE_END = ';';
    const char CPM_OPERAND_DELIM = ',';
    const char CPM_SUBSCRIPT_DELIM = '.';
    const char CPM_STR_DELIM = '"';
    const char CPM_CHAR_DELIM = '\'';
    const string CPM_COMMENT_LINE = "//";
    const string CPM_COMMENT_BLOCK_OPEN = "/*";
    const string CPM_COMMENT_BLOCK_CLOSE = "*/";

    struct CPMSyntaxTreeNode
    {
        CPMSyntaxTreeNode* parent;
        vector<CPMSyntaxTreeNode*> children;
        CPMSyntaxNodeType type;
        string text;
        string sourceFileName;
        int lineNumber;
        // Parallel to children: separators retained after punctuation is removed.
        vector<bool> commaBefore;
        vector<bool> whitespaceBefore;
    };

    class Logger;

    class CPMSyntaxDetector
    {
    protected:
        vector<CPMSyntaxTreeNode*> seq;
        bool complete;
    public:
        CPMSyntaxDetector();

        // returns:
        //    true - if the sequence starts or continues at cur node;
        //    false - if the sequence has not started or stops at cur node;
        virtual bool PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog) = 0;

        // returns true if the sequence is not empty and valid
        virtual bool Complete() { return complete; }

        virtual CPMSyntaxNodeType Type() = 0;

        void CopySeqTo(vector<CPMSyntaxTreeNode*> & list);
    };

    class CPMSD_NUM : public CPMSyntaxDetector
    {
    private:
        CPMNumberFormat format;
    public:
        CPMSD_NUM();
        bool PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog) override;
        //bool Complete() override;
        CPMSyntaxNodeType Type() override { return CPM_NUM; }
        inline CPMNumberFormat Format() { return format; }
    };

    class CPMSD_ID : public CPMSyntaxDetector
    {
    private:
        bool isNumber;
    public:
        CPMSD_ID();
        bool PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog) override;
        //bool Complete() override;
        CPMSyntaxNodeType Type() override { return CPM_ID; }
    };

    // Joins dot-separated identifiers (NS.Name, a.b.c) into a single CPM_REF
    // node. The name CPM_REF is historical; this detector is NOT about pointer
    // or reference types — it only does lexical `.`-joining. Pointer types are
    // written `(T ptr)` in the new grammar.
    class CPMSD_REF : public CPMSyntaxDetector
    {
    private:
    public:
        CPMSD_REF();
        bool PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog) override;
        CPMSyntaxNodeType Type() override { return CPM_REF; }
    };

    inline bool CharIsDigit(char c) { return c >= '0' && c <= '9'; }
    inline bool CharIsHexDigit(char c) { return CharIsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
    inline bool CharIsWhiteSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
    inline bool CharIsDelimiter(char c) {
        return c == CPM_LINE_END ||
               c == CPM_OPERAND_DELIM ||
               c == CPM_SUBSCRIPT_DELIM ||
               c == CPM_BLOCK_OPEN ||
               c == CPM_BLOCK_CLOSE ||
               c == CPM_STR_DELIM ||
               c == CPM_CHAR_DELIM; }
    string CPMSyntaxTreeNodeToString(CPMSyntaxTreeNode* node);
    string CPMSyntaxTreeNodeToStringRecoursive(CPMSyntaxTreeNode* node, string indent = "");
    string CPMSTRContent(CPMSyntaxTreeNode* node);

    void DeleteSyntaxTree(CPMSyntaxTreeNode* root);

    // Validate a sequence after its semantic context identifies it as a call.
    // Declarations and named struct-field bindings do not use call separators.
    bool CPMValidateCall(CPMSyntaxTreeNode* node, Logger* logger);
    bool CPMValidateDeclaration(CPMSyntaxTreeNode* node, Logger* logger);

    class CPMParser
    {
    private:
        vector<string> text;
        list<CPMSyntaxTreeNode*> layer;
        bool noErrors;
        Logger* compilerLog;

        void pass_CHAR(string sourceFileName);
        void pass_LiteralsAndComments();
        void pass_Groups();
        CPMSyntaxTreeNode* groupSequence(const vector<CPMSyntaxTreeNode*>& nodes,
                                        CPMSyntaxNodeType type);
        void syntaxError(CPMSyntaxTreeNode* node, const string& message);
        template <typename DetectorType> bool pass_Detector(CPMSyntaxNodeType NodeType);

        void pushSyntaxTreeNode(CPMSyntaxTreeNode* node, list<CPMSyntaxTreeNode*>::iterator childrenSeqStart, list<CPMSyntaxTreeNode*>::iterator childrenSeqFinish, CPMSyntaxDetector* detector);
    public:
        CPMParser(string rootFolder, string sourceFileName, Logger* compilerLog);
        void GetLayer(vector<CPMSyntaxTreeNode*>& layerCopy);
        inline bool NoErrors() { return noErrors; }
        void Clear();
        string LayerToString(bool recursive = true);
        ~CPMParser();
    };

}
