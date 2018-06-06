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
        CPM_NUM,
        CPM_STR,
        CPM_ID,
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
    const char CPM_STR_DELIM = '"';
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

    class CPMSD_STR : public CPMSyntaxDetector
    {
    private:
        bool closed;
    public:
        CPMSD_STR();
        bool PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog) override;
        //bool Complete() override;
        CPMSyntaxNodeType Type() override { return CPM_STR; }
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

    class CPMSD_EXPR : public CPMSyntaxDetector
    {
    private:
        //bool closed;
        int pcounter;
        int itemsCount;
        bool opened;
    public:
        CPMSD_EXPR();
        bool PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog) override;
        //bool Complete() override;
        CPMSyntaxNodeType Type() override { return CPM_EXPR; }
    };

    class CPMSD_LINE : public CPMSyntaxDetector
    {
    private:
        int itemsCount;
    public:
        CPMSD_LINE();
        bool PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog) override;
        //bool Complete() override;
        CPMSyntaxNodeType Type() override { return CPM_LINE; }
    };

    class CPMSD_BLOCK :public CPMSyntaxDetector
    {
    private:
        int pcounter;
        bool opened;
    public:
        CPMSD_BLOCK();
        bool PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog) override;
        //bool Complete() override;
        CPMSyntaxNodeType Type() override { return CPM_BLOCK; }
    };

    inline bool CharIsDigit(char c) { return c >= '0' && c <= '9'; }
    inline bool CharIsHexDigit(char c) { return CharIsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
    inline bool CharIsWhiteSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
    inline bool CharIsDelimiter(char c) { return c == CPM_LINE_END || c == ',' || c == CPM_BLOCK_OPEN || c == CPM_BLOCK_CLOSE || c == CPM_STR_DELIM || c == '\''; }
    string CPMSyntaxTreeNodeToString(CPMSyntaxTreeNode* node);
    string CPMSyntaxTreeNodeToStringRecoursive(CPMSyntaxTreeNode* node, string indent = "");
    string CPMSTRContent(CPMSyntaxTreeNode* node);

    void DeleteSyntaxTree(CPMSyntaxTreeNode* root);

    class CPMParser
    {
    private:
        vector<string> text;
        list<CPMSyntaxTreeNode*> layer;
        bool noErrors;
        Logger* compilerLog;

        void pass_CHAR(string sourceFileName);
        void pass_Comments();
        template <typename DetectorType> bool pass_Detector(CPMSyntaxNodeType NodeType);

        /*void pass_STR();
        void pass_ID_NUM();
        void pass_EXPR();
        void pass_LINE();
        void pass_BLOCK();*/

        void pushSyntaxTreeNode(CPMSyntaxTreeNode* node, list<CPMSyntaxTreeNode*>::iterator childrenSeqStart, list<CPMSyntaxTreeNode*>::iterator childrenSeqFinish, CPMSyntaxDetector* detector);
    public:
        CPMParser(string rootFolder, string sourceFileName, Logger* compilerLog);
        void GetLayer(vector<CPMSyntaxTreeNode*>& layerCopy);
        inline bool NoErrors() { return noErrors; }
        void Clear();
        string LayerToString();
        ~CPMParser();
    };

}