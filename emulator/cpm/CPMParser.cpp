#include "stdafx.h"
#include "CPMParser.h"
#include "CPMCompiler.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <functional>

#define COMPLETE_SEQ_NOT_EMPTY { complete = seq.size() > 0; return false; }

namespace CPM
{
    CPMSyntaxDetector::CPMSyntaxDetector() : seq()
    {
        complete = false;
    }

    void CPMSyntaxDetector::CopySeqTo(vector<CPMSyntaxTreeNode*> & list)
    {
        list.clear();
        list.insert(list.begin(), seq.begin(), seq.end());
    }

    CPMSD_NUM::CPMSD_NUM() : CPMSyntaxDetector()
    {
        format = CPM_DEC;
    }

    bool CPMSD_NUM::PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog)
    {
        if (cur->type != CPM_CHAR)
            return false;

        char c = cur->text[0];

        if (!CharIsDigit(c) && seq.size() == 0)
            return false;

        seq.push_back(cur);

        if (next == NULL) COMPLETE_SEQ_NOT_EMPTY
            if (next->type != CPM_CHAR) COMPLETE_SEQ_NOT_EMPTY

                char cn = next->text[0];

        if (!CharIsDigit(cn))
        {
            if (cn == 'x' && seq.size() == 1)
                format = CPM_HEX;

            if (format == CPM_DEC) COMPLETE_SEQ_NOT_EMPTY
            else
            if (!CharIsHexDigit(cn) && !(seq.size() == 1 && cn == 'x'))
            {
                complete = seq.size() > 2;
                return false;
            }
        }

        return true;
    }

    CPMSD_ID::CPMSD_ID() : CPMSyntaxDetector()
    {
        complete = false;
        isNumber = false;
    }

    bool CPMSD_ID::PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog)
    {
        if (cur->type != CPM_CHAR)
            return false;

        char c = cur->text[0];

        if (CharIsDigit(c) && seq.size() == 0)
        {
            isNumber = true;
            return true;
        }

        if (CharIsWhiteSpace(c) || CharIsDelimiter(c))
        {
            isNumber = false;
            return false;
        }

        if (isNumber)
            return false;

        seq.push_back(cur);

        if (next == NULL)
            COMPLETE_SEQ_NOT_EMPTY
            if (next->type != CPM_CHAR)
                COMPLETE_SEQ_NOT_EMPTY

                char cn = next->text[0];

        if (CharIsWhiteSpace(cn) || CharIsDelimiter(cn))
            COMPLETE_SEQ_NOT_EMPTY

            return true;
    }

    CPMSD_REF::CPMSD_REF() : CPMSyntaxDetector()
    {

    }

    bool CPMSD_REF::PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog)
    {
        if (complete)
            return false;

        bool valid = false;
        if (cur->type == CPM_ID)
        {
            valid = true;
        }

        if (seq.size() > 0 && cur->type == CPM_ID)
        {
            valid = seq[seq.size() - 1]->type == CPM_ID;
        }

        if (next)
        {
            if (seq.size() > 0 &&
                next->type == CPM_ID &&
                cur->type == CPM_CHAR)
            {
                if (cur->text[0] == CPM_SUBSCRIPT_DELIM)
                    valid = true;
            }
        }

        if (valid)
        {
            if (cur->type != CPM_CHAR)
                seq.push_back(cur);
            if (next)
            {
                if (next->type == CPM_TYPE_OPEN || next->type == CPM_TYPE_CLOSE)
                {
                    complete = seq.size() > 1;
                    if (!complete) seq.clear();
                    return false;
                }
                if (next->type == CPM_CHAR)
                {
                    char c = next->text[0];
                    if (c == CPM_OPERAND_DELIM || c == CPM_LINE_END || c == CPM_BLOCK_CLOSE || CharIsWhiteSpace(c))
                    {
                        complete = seq.size() > 1;
                        if (seq.size() == 1)
                            seq.clear();
                        return false;
                    }
                }
            }
        }

        return valid;
    }

    void DeleteSyntaxTree(CPMSyntaxTreeNode* root)
    {
        for (int i = 0; i < root->children.size(); i++)
            DeleteSyntaxTree(root->children[i]);
        delete root;
    }

    CPMParser::CPMParser(string rootFolder, string sourceFileName, Logger* compilerLog)
    {
        noErrors = true;
        this->compilerLog = compilerLog;
        string line;
        ifstream f(rootFolder+sourceFileName);

        if (f.is_open())
        {
            while (getline(f, line))
                text.push_back(line);
            f.close();
        }
        else
        {
            compilerLog->Add(LOG_ERROR, "Cannot open file: '")->Add(rootFolder + sourceFileName)->Add("'.");
            noErrors = false;
        }

        pass_CHAR(sourceFileName);
        pass_LiteralsAndComments();
        pass_Angles();
        pass_Detector<CPMSD_ID>(CPM_ID);
        pass_Detector<CPMSD_NUM>(CPM_NUM);
        while (pass_Detector<CPMSD_REF>(CPM_REF)) {}
        pass_Groups();
    }

    void CPMParser::Clear()
    {
        for (list<CPMSyntaxTreeNode*>::iterator li = layer.begin(); li != layer.end(); ++li)
        {
            DeleteSyntaxTree(*li);
        }
        layer.clear();
    }

    void CPMParser::pushSyntaxTreeNode(CPMSyntaxTreeNode* node, list<CPMSyntaxTreeNode*>::iterator childrenSeqStart, list<CPMSyntaxTreeNode*>::iterator childrenSeqFinish, CPMSyntaxDetector* detector)
    {
        node->parent = nullptr;
        detector->CopySeqTo(node->children);
        for (auto li = childrenSeqStart; li != next(childrenSeqFinish); ++li)
        {
            node->text += (*li)->text;
            if (find(node->children.begin(), node->children.end(), *li) != node->children.end())
                (*li)->parent = node;
            else
                DeleteSyntaxTree(*li);
        }
        layer.insert(childrenSeqStart, node);
        layer.erase(childrenSeqStart, next(childrenSeqFinish));
    }

    template <typename DetectorType> bool CPMParser::pass_Detector(CPMSyntaxNodeType NodeType)
    {
        DetectorType* detector = NULL;
        list<CPMSyntaxTreeNode*>::iterator seqStart = layer.end();
        list<CPMSyntaxTreeNode*>::iterator seqFinish = layer.end();
        bool layerWasChanged = false;
        for (list<CPMSyntaxTreeNode*>::iterator li = layer.begin(); li != layer.end();)
        {
            list<CPMSyntaxTreeNode*>::iterator nextli = next(li);
            if (detector == NULL)
                detector = new DetectorType();

            bool isFinished = false;

            if (detector->PutNode(*li, nextli == layer.end() ? NULL : *nextli, compilerLog))
            {
                if (seqStart == layer.end())
                    seqStart = li;
            }
            else if (seqStart != layer.end())
            {
                seqFinish = li;
                isFinished = true;
            }

            if (detector->Complete())
            {
                if (seqStart == layer.end()) // this happens if the valid sequence consists of one node
                {
                    seqStart = li;
                    seqFinish = li;
                    isFinished = true;
                }

                CPMSyntaxTreeNode* node = new CPMSyntaxTreeNode();
                node->type = NodeType;
                node->lineNumber = (*seqFinish)->lineNumber;
                node->sourceFileName = (*seqStart)->sourceFileName;
                pushSyntaxTreeNode(node, seqStart, seqFinish, dynamic_cast<CPMSyntaxDetector*>(detector));
                //compilerLog->Add(">>> " + LayerToString())->Endl();
                layerWasChanged = true;
            }

            if (isFinished)
            {
                delete detector;
                detector = NULL;
                seqStart = layer.end();
                seqFinish = layer.end();
            }
            li = nextli;
        }
        delete detector;
        return layerWasChanged;
    }

    void CPMParser::pass_CHAR(string sourceFileName)
    {
        Clear();
        for (int lineNumber = 1; lineNumber <= text.size(); lineNumber++)
        {
            string line = text[lineNumber-1] + '\n';
            for (int i = 0; i < line.size(); i++)
            {
                CPMSyntaxTreeNode* charNode = new CPMSyntaxTreeNode();
                charNode->parent = NULL;
                char c = line[i];
                charNode->text = c;
                charNode->type = CPM_CHAR;
                charNode->lineNumber = lineNumber;
                charNode->sourceFileName = sourceFileName;
                layer.push_back(charNode);
            }
        }
    }

    static bool IsChar(CPMSyntaxTreeNode* node, char c)
    {
        return node->type == CPM_CHAR && node->text.size() == 1 && node->text[0] == c;
    }

    static bool IsTrivia(CPMSyntaxTreeNode* node)
    {
        return node->type == CPM_COMMENT ||
            (node->type == CPM_CHAR && CharIsWhiteSpace(node->text[0]));
    }

    void CPMParser::syntaxError(CPMSyntaxTreeNode* node, const string& message)
    {
        compilerLog->Add(LOG_ERROR, message, node->sourceFileName, node->lineNumber);
        noErrors = false;
    }

    bool CPMSD_TYPEEXPR::PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode*, Logger*)
    {
        if (complete) return false;
        if (!opened)
        {
            opened = cur->type == CPM_TYPE_OPEN;
            return opened;
        }
        if (cur->type == CPM_TYPE_CLOSE)
        {
            complete = true;
            return false;
        }
        if (!IsTrivia(cur) && !IsChar(cur, CPM_OPERAND_DELIM))
            seq.push_back(cur);
        return true;
    }

    void CPMParser::pass_LiteralsAndComments()
    {
        vector<CPMSyntaxTreeNode*> raw(layer.begin(), layer.end());
        layer.clear();
        for (size_t i = 0; i < raw.size();)
        {
            const bool lineComment = IsChar(raw[i], '/') && i + 1 < raw.size() && IsChar(raw[i + 1], '/');
            const bool blockComment = IsChar(raw[i], '/') && i + 1 < raw.size() && IsChar(raw[i + 1], '*');
            const bool quoted = IsChar(raw[i], CPM_STR_DELIM) || IsChar(raw[i], CPM_CHAR_DELIM);
            if (!lineComment && !blockComment && !quoted)
            {
                layer.push_back(raw[i++]);
                continue;
            }

            size_t start = i;
            bool closed = lineComment;
            char quote = raw[i]->text[0];
            i += quoted ? 1 : 2;
            while (i < raw.size())
            {
                if (lineComment && IsChar(raw[i], '\n'))
                    break;
                if (blockComment && IsChar(raw[i], '*') && i + 1 < raw.size() && IsChar(raw[i + 1], '/'))
                {
                    i += 2;
                    closed = true;
                    break;
                }
                if (quoted && IsChar(raw[i], quote))
                {
                    ++i;
                    closed = true;
                    break;
                }
                ++i;
            }

            auto node = new CPMSyntaxTreeNode{};
            node->type = quoted ? (quote == CPM_STR_DELIM ? CPM_STR : CPM_CHARLIT) : CPM_COMMENT;
            node->sourceFileName = raw[start]->sourceFileName;
            node->lineNumber = raw[start]->lineNumber;
            for (size_t j = start; j < i; ++j)
            {
                node->children.push_back(raw[j]);
                raw[j]->parent = node;
                if (quoted)
                    node->text += raw[j]->text;
            }
            if (!closed)
                syntaxError(node, quoted ? "Unclosed literal." : "Unclosed block comment.");
            if (node->type == CPM_CHARLIT && node->text.size() != 3)
                syntaxError(node, "A char literal must contain exactly one character.");
            // Comments remain opaque boundaries until grouping. They cannot
            // merge identifiers or supply missing source whitespace.
            layer.push_back(node);
        }
    }

    void CPMParser::pass_Angles()
    {
        vector<CPMSyntaxTreeNode*> raw(layer.begin(), layer.end());
        layer.clear();
        vector<char> delimiters;
        for (size_t i = 0; i < raw.size(); ++i)
        {
            auto node = raw[i];
            if (IsChar(node, CPM_BLOCK_OPEN))
                delimiters.push_back(CPM_BLOCK_OPEN);
            else if (IsChar(node, CPM_BLOCK_CLOSE))
            {
                // Grouping reports mismatches. Recover the lexical context so
                // later comparisons do not inherit an unclosed type context.
                while (!delimiters.empty() && delimiters.back() == CPM_TYPE_OPEN_DELIM)
                    delimiters.pop_back();
                if (!delimiters.empty()) delimiters.pop_back();
            }
            else if (IsChar(node, CPM_LINE_END))
            {
                while (!delimiters.empty() && delimiters.back() == CPM_TYPE_OPEN_DELIM)
                    delimiters.pop_back();
            }
            else if (IsChar(node, CPM_TYPE_OPEN_DELIM) || IsChar(node, CPM_TYPE_CLOSE_DELIM))
            {
                const char angle = node->text[0];
                auto following = i + 1 < raw.size() ? raw[i + 1] : nullptr;
                const bool closeType = angle == CPM_TYPE_CLOSE_DELIM && !delimiters.empty() &&
                    delimiters.back() == CPM_TYPE_OPEN_DELIM;
                size_t typeStart = i + 1;
                while (typeStart < raw.size() && IsChar(raw[typeStart], CPM_TYPE_OPEN_DELIM)) ++typeStart;
                const bool openType = angle == CPM_TYPE_OPEN_DELIM && typeStart < raw.size() &&
                    raw[typeStart]->type == CPM_CHAR && CharIsIdentifierStart(raw[typeStart]->text[0]);
                if (openType)
                {
                    node->type = CPM_TYPE_OPEN;
                    delimiters.push_back(CPM_TYPE_OPEN_DELIM);
                }
                else if (closeType)
                {
                    node->type = CPM_TYPE_CLOSE;
                    delimiters.pop_back();
                    if (following && IsChar(following, '='))
                        syntaxError(node, "A type closes with '>', not '>='; comparisons in array sizes must be parenthesized.");
                }
                else
                {
                    // Comparison tokens stay IDs for existing expression users.
                    // Only a real source whitespace byte can start an operand.
                    node->type = CPM_ID;
                    if (following && (IsChar(following, '=') || IsChar(following, angle)))
                    {
                        node->text += following->text;
                        DeleteSyntaxTree(following);
                        ++i;
                        following = i + 1 < raw.size() ? raw[i + 1] : nullptr;
                    }
                    if (!following || following->type != CPM_CHAR || !CharIsWhiteSpace(following->text[0]))
                        syntaxError(node, "Whitespace is required after a comparison operator; a type must start immediately with its type name or '<'.");
                    if (!layer.empty())
                    {
                        auto previous = layer.back();
                        if (!(previous->type == CPM_CHAR && CharIsWhiteSpace(previous->text[0])) &&
                            !IsChar(previous, CPM_BLOCK_OPEN))
                            syntaxError(node, "A comparison operator must be a separate token.");
                    }
                }
            }
            layer.push_back(node);
        }
    }

    CPMSyntaxTreeNode* CPMParser::groupSequence(const vector<CPMSyntaxTreeNode*>& nodes,
                                               CPMSyntaxNodeType type)
    {
        auto node = new CPMSyntaxTreeNode{};
        node->type = type;
        if (!nodes.empty())
        {
            node->sourceFileName = nodes.front()->sourceFileName;
            node->lineNumber = nodes.front()->lineNumber;
        }
        bool whitespace = false;
        bool comma = false;
        for (auto child : nodes)
        {
            node->text += child->text;
            if (IsTrivia(child))
            {
                if (child->type == CPM_CHAR)
                    whitespace = true;
                DeleteSyntaxTree(child);
            }
            else if (IsChar(child, CPM_OPERAND_DELIM))
            {
                if (comma || node->children.size() < 2)
                    syntaxError(child, "A comma must separate two operands, not precede the first operand.");
                comma = true;
                whitespace = false;
                DeleteSyntaxTree(child);
            }
            else
            {
                if (comma && !whitespace)
                    syntaxError(child, "Whitespace is required after an operand comma.");
                if (!node->children.empty() &&
                    (node->children.back()->type == CPM_TYPEEXPR || child->type == CPM_TYPEEXPR) &&
                    !whitespace && !comma)
                    syntaxError(child, "Whitespace is required after a type expression before a name or type component.");
                if (node->children.empty())
                {
                    node->sourceFileName = child->sourceFileName;
                    node->lineNumber = child->lineNumber;
                }
                node->commaBefore.push_back(comma);
                node->whitespaceBefore.push_back(whitespace);
                child->parent = node;
                node->children.push_back(child);
                comma = false;
                whitespace = false;
            }
        }
        if (comma)
            syntaxError(node, "Trailing commas are not allowed in operator calls.");
        if (type == CPM_LINE && !node->children.empty())
        {
            const string& head = node->children[0]->text;
            if ((head == "<" || head == ">" || head == "<=" || head == ">=") &&
                !CPMValidateCall(node, compilerLog))
                noErrors = false;
        }
        return node;
    }

    bool CPMValidateCall(CPMSyntaxTreeNode* node, Logger* logger)
    {
        bool valid = true;
        for (size_t i = 1; i < node->children.size(); ++i)
        {
            const bool comma = i < node->commaBefore.size() && node->commaBefore[i];
            const bool whitespace = i < node->whitespaceBefore.size() && node->whitespaceBefore[i];
            string error;
            if (i == 1 && comma)
                error = "Use whitespace, not a comma, between an operator and its first operand.";
            else if (i > 1 && !comma)
                error = "A comma is required between operator operands.";
            else if (!whitespace)
                error = i == 1 ? "Whitespace is required after the operator." :
                    "Whitespace is required after an operand comma.";
            if (!error.empty())
            {
                logger->Add(LOG_ERROR, error, node->sourceFileName, node->children[i]->lineNumber);
                valid = false;
            }
        }
        return valid;
    }

    bool CPMValidateDeclaration(CPMSyntaxTreeNode* node, Logger* logger)
    {
        if (find(node->commaBefore.begin(), node->commaBefore.end(), true) == node->commaBefore.end())
            return true;
        logger->Add(LOG_ERROR, "Commas separate call operands, not declaration components or parameter declarations.",
                    node->sourceFileName, node->lineNumber);
        return false;
    }

    void CPMParser::pass_Groups()
    {
        vector<CPMSyntaxTreeNode*> tokens(layer.begin(), layer.end());
        layer.clear();
        size_t position = 0;
        function<CPMSyntaxTreeNode*(CPMSyntaxTreeNode*)> group;
        group = [&](CPMSyntaxTreeNode* opening) -> CPMSyntaxTreeNode*
        {
            const bool typeGroup = opening->type == CPM_TYPE_OPEN;
            CPMSD_TYPEEXPR typeDetector;
            if (typeGroup) typeDetector.PutNode(opening, nullptr, compilerLog);
            vector<CPMSyntaxTreeNode*> contents;
            CPMSyntaxTreeNode* closing = nullptr;
            while (position < tokens.size())
            {
                auto token = tokens[position++];
                if (IsChar(token, CPM_BLOCK_OPEN) || token->type == CPM_TYPE_OPEN)
                {
                    auto child = group(token);
                    contents.push_back(child);
                    if (typeGroup) typeDetector.PutNode(child, nullptr, compilerLog);
                }
                else if ((!typeGroup && IsChar(token, CPM_BLOCK_CLOSE)) ||
                         (typeGroup && token->type == CPM_TYPE_CLOSE))
                {
                    closing = token;
                    if (typeGroup) typeDetector.PutNode(token, nullptr, compilerLog);
                    break;
                }
                else if (IsChar(token, CPM_BLOCK_CLOSE) || token->type == CPM_TYPE_CLOSE ||
                         (typeGroup && IsChar(token, CPM_LINE_END)))
                {
                    // Leave the mismatched delimiter for the enclosing group or
                    // top-level recovery; do not swallow the next declaration.
                    --position;
                    break;
                }
                else
                {
                    contents.push_back(token);
                    if (typeGroup) typeDetector.PutNode(token, nullptr, compilerLog);
                }
            }
            if (!closing)
            {
                syntaxError(opening, typeGroup ? "Unclosed type expression; expected '>'." :
                                                "Unclosed parenthesized group; expected ')'.");
                closing = new CPMSyntaxTreeNode{};
                closing->type = typeGroup ? CPM_TYPE_CLOSE : CPM_CHAR;
                closing->sourceFileName = opening->sourceFileName;
                closing->lineNumber = opening->lineNumber;
            }
            const bool block = !typeGroup && any_of(contents.begin(), contents.end(),
                [](CPMSyntaxTreeNode* n) { return IsChar(n, CPM_LINE_END); });
            CPMSyntaxTreeNode* result;
            if (typeGroup)
            {
                result = groupSequence(contents, typeDetector.Type());
                // The detector and shared sequence builder see the same grouped
                // atoms; only the latter retains their source separator metadata.
                typeDetector.CopySeqTo(result->children);
                const auto& c = result->children;
                if (c.empty() || (c[0]->type != CPM_ID && c[0]->type != CPM_REF && c[0]->type != CPM_TYPEEXPR))
                    syntaxError(opening, "A type expression must start with a type name or nested type.");
                if (find(result->commaBefore.begin(), result->commaBefore.end(), true) != result->commaBefore.end())
                    syntaxError(opening, "Type components use whitespace, not operand commas.");
                for (size_t i = 1; i < c.size(); ++i)
                {
                    if (!result->whitespaceBefore[i])
                        syntaxError(c[i], "Whitespace is required between type components.");
                    if (c[i]->type != CPM_ID || (c[i]->text != "ptr" && c[i]->text != "array"))
                    {
                        syntaxError(c[i], "Expected 'ptr' or 'array' in a type expression.");
                        break;
                    }
                    if (c[i]->text == "array")
                    {
                        if (++i == c.size())
                        {
                            syntaxError(opening, "An array modifier requires a count.");
                            break;
                        }
                        if (!result->whitespaceBefore[i])
                            syntaxError(c[i], "Whitespace is required before an array count.");
                        if (c[i]->type != CPM_ID && c[i]->type != CPM_REF && c[i]->type != CPM_NUM && c[i]->type != CPM_EXPR)
                            syntaxError(c[i], "An array count must be a constant name, number, or parenthesized expression.");
                    }
                }
                result->text = opening->text + result->text + closing->text;
                result->lineNumber = opening->lineNumber;
                result->sourceFileName = opening->sourceFileName;
                DeleteSyntaxTree(opening);
                DeleteSyntaxTree(closing);
            }
            else if (!block)
            {
                result = groupSequence(contents, CPM_EXPR);
                const auto& c = result->children;
                const bool typedParameter = c.size() == 2 && c[0]->type == CPM_TYPEEXPR && c[1]->type == CPM_ID;
                const bool arrayParameter = c.size() >= 3 && c[2]->type == CPM_ID && c[2]->text == "array";
                if (!typedParameter && !arrayParameter && c.size() > 1)
                {
                    if (c[0]->type != CPM_ID && c[0]->type != CPM_REF)
                        syntaxError(opening, "Initializer elements require semicolons; a call must start with an operator.");
                    else if (!CPMValidateCall(result, compilerLog))
                        noErrors = false;
                }
                result->text = opening->text + result->text + closing->text;
                result->lineNumber = opening->lineNumber;
                result->sourceFileName = opening->sourceFileName;
                DeleteSyntaxTree(opening);
                DeleteSyntaxTree(closing);
            }
            else
            {
                result = new CPMSyntaxTreeNode{};
                result->type = CPM_BLOCK;
                result->sourceFileName = opening->sourceFileName;
                result->lineNumber = opening->lineNumber;
                result->children.push_back(opening);
                result->text = opening->text;
                vector<CPMSyntaxTreeNode*> line;
                for (auto token : contents)
                {
                    result->text += token->text;
                    if (IsChar(token, CPM_LINE_END))
                    {
                        result->children.push_back(groupSequence(line, CPM_LINE));
                        line.clear();
                        DeleteSyntaxTree(token);
                    }
                    else
                        line.push_back(token);
                }
                if (any_of(line.begin(), line.end(), [](CPMSyntaxTreeNode* n) { return !IsTrivia(n); }))
                    result->children.push_back(groupSequence(line, CPM_LINE));
                else
                    for (auto token : line) DeleteSyntaxTree(token);
                result->children.push_back(closing);
                result->text += closing->text;
                for (auto child : result->children) child->parent = result;
            }
            return result;
        };

        vector<CPMSyntaxTreeNode*> line;
        auto finishLine = [&]()
        {
            if (any_of(line.begin(), line.end(), [](CPMSyntaxTreeNode* n) { return !IsTrivia(n); }))
                layer.push_back(groupSequence(line, CPM_LINE));
            else
                for (auto token : line) DeleteSyntaxTree(token);
            line.clear();
        };
        while (position < tokens.size())
        {
            auto token = tokens[position++];
            if (IsChar(token, CPM_BLOCK_OPEN) || token->type == CPM_TYPE_OPEN)
                line.push_back(group(token));
            else if (IsChar(token, CPM_LINE_END))
            {
                finishLine();
                DeleteSyntaxTree(token);
            }
            else if (IsChar(token, CPM_BLOCK_CLOSE) || token->type == CPM_TYPE_CLOSE)
            {
                syntaxError(token, "Unexpected closing delimiter.");
                DeleteSyntaxTree(token);
            }
            else
                line.push_back(token);
        }
        finishLine();
    }

    void CPMParser::GetLayer(vector<CPMSyntaxTreeNode*>& layerCopy)
    {
        //return vector<CPMSyntaxTreeNode*>(layer.begin(), layer.end());
        layerCopy.clear();
        layerCopy.insert(layerCopy.end(), layer.begin(), layer.end());
    }

    string CPMParser::LayerToString(bool recursive)
    {
        string s = "";
        for (list<CPMSyntaxTreeNode*>::iterator li = layer.begin(); li != layer.end(); ++li)
        {
            if (recursive)
                s += CPMSyntaxTreeNodeToStringRecoursive(*li);// + "\n";
            else
                s += CPMSyntaxTreeNodeToString(*li);// + "\n";
        }
        return s;
    }

    string CPMSyntaxTreeNodeToStringRecoursive(CPMSyntaxTreeNode* node, string indent)
    {
        string s = indent + CPMSyntaxTreeNodeToString(node) + "\n";
        if (node->type != CPM_ID && node->type != CPM_NUM && node->type != CPM_STR)
            for (vector<CPMSyntaxTreeNode*>::iterator li = node->children.begin(); li != node->children.end(); ++li)
                s += CPMSyntaxTreeNodeToStringRecoursive(*li, indent + "   ");
        return s;
    }

    string CPMSTRContent(CPMSyntaxTreeNode* node)
    {
        return node->text.substr(1, node->text.size() - 2);
    }

    CPMParser::~CPMParser()
    {
        Clear();
    }

    string CPMSyntaxTreeNodeToString(CPMSyntaxTreeNode* node)
    {
        string s = "<";
        switch (node->type)
        {
        case CPM_UNDEFINED:
            s += "?"; break;
        case CPM_CHAR:
            s += "CHAR"; break;
        case CPM_COMMENT:
            s += "COMMENT"; break;
        case CPM_NUM:
            s += "NUM"; break;
        case CPM_STR:
            s += "STR"; break;
        case CPM_CHARLIT:
            s += "CHARLIT"; break;
        case CPM_ID:
            s += "ID"; break;
        case CPM_REF:
            s += "REF"; break;
        case CPM_TYPE_OPEN:
            s += "TYPE_OPEN"; break;
        case CPM_TYPE_CLOSE:
            s += "TYPE_CLOSE"; break;
        case CPM_TYPEEXPR:
            s += "TYPEEXPR"; break;
        case CPM_EXPR:
            s += "EXPR"; break;
        case CPM_LINE:
            s += "LINE"; break;
        case CPM_BLOCK:
            s += "BLOCK"; break;
        }
        
        s += " " + to_string(node->lineNumber) + ": '" + node->text + "'>";
        return s;
    }

}
