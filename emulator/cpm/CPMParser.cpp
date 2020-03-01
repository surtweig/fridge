#include "stdafx.h"
#include "CPMParser.h"
#include "CPMCompiler.h"
#include <iostream>
#include <fstream>

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

    CPMSD_STR::CPMSD_STR() : CPMSyntaxDetector()
    {
        closed = false;
    }

    bool CPMSD_STR::PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog)
    {
        if (complete)
            return false;

        if (cur->type != CPM_CHAR)
            return false;

        char c = cur->text[0];

        if (seq.size() == 0 && c != CPM_STR_DELIM)
            return false;

        if (seq.size() > 0 && c == CPM_STR_DELIM)
            complete = true;

        seq.push_back(cur);

        if (complete)
            return false;

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
            compilerLog->Add("ID " + cur->text)->Endl();
            valid = true;
        }

        if (seq.size() > 0 && cur->type == CPM_INDEX)
        {
            compilerLog->Add("INDEX " + cur->text)->Endl();
            valid = seq[seq.size() - 1]->type == CPM_ID;
        }

        if (next)
        {
            if (next->type == CPM_CHAR)
            {
                if (next->text[0] == CPM_BLOCK_OPEN || next->text[0] == CPM_INDEX_OPEN)
                    return false;
            }
            if (seq.size() > 0 &&
                (next->type == CPM_ID || next->type == CPM_INDEX) &&
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
                if (next->type == CPM_CHAR)
                {
                    char c = next->text[0];
                    if (c == CPM_OPERAND_DELIM || c == CPM_LINE_END || c == CPM_BLOCK_CLOSE || c == CPM_INDEX_CLOSE || CharIsWhiteSpace(c))
                    {
                        compilerLog->Add("complete " + next->text)->Endl();
                        complete = seq.size() > 1;
                        if (seq.size() == 1)
                            seq.clear();
                        return false;
                    }
                }
            }
        }

        if (!valid)
            compilerLog->Add("not valid " + CPMSyntaxTreeNodeToString(cur))->Endl();
        return valid;
    }

    CPMSD_EXPR::CPMSD_EXPR() : CPMSyntaxDetector()
    {
        pcounter = 0;
        itemsCount = 0;
        opened = false;
    }

    bool CPMSD_EXPR::PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog)
    {
        if (complete)
            return false;

        if (cur->type == CPM_CHAR)
        {
            char c = cur->text[0];
            if (c == CPM_BLOCK_OPEN)
            {
                if (!opened)
                {
                    opened = true;
                    return true;
                }
                //else
                //    return false;
            }

            if (!opened && c != CPM_BLOCK_OPEN)
                return false;

            if (c == CPM_LINE_END)
                return false;

            if (!CharIsDelimiter(c) && !CharIsWhiteSpace(c))
            {
                compilerLog->Add(LOG_ERROR, "Invalid character in the expression: ", cur->sourceFileName, cur->lineNumber)->Add(c);
                return false;
            }

            // if (opened && CharIsWhiteSpace(c))
            //     return true;

            /*if (c == CPM_BLOCK_OPEN)
                pcounter++;
                if (c == CPM_BLOCK_CLOSE)
                pcounter--;

                if (pcounter < 0)
                {
                CPMCompilerLog.Add(LOG_ERROR, "Parenthesis parse error.");
                return false;
                }*/

            if (opened && c == CPM_BLOCK_CLOSE)
            {
                complete = true;
                opened = false;
                return false;
            }
        }
        else
        {
            /*if (pcounter > 0)
                itemsCount++;
                else
                return false;*/
        }

        /*if (itemsCount == 0 && n->type != CPM_ID)
        {
        CPMCompilerLog.Add(LOG_ERROR, "Token '")->Add(n->text)->Add("' is not an ID.");
        return false;
        }*/

        if (opened && !(cur->type == CPM_CHAR && CharIsWhiteSpace(cur->text[0])))
            seq.push_back(cur);

        if (next != NULL)
        {
            if (next->type == CPM_CHAR && 
                (next->text[0] == CPM_BLOCK_OPEN || next->text[0] == CPM_INDEX_OPEN))
                return false;
        }

        return true;
    }

    CPMSD_INDEX::CPMSD_INDEX() : CPMSyntaxDetector()
    {
        pcounter = 0;
        itemsCount = 0;
        opened = false;
    }

    bool CPMSD_INDEX::PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog)
    {
        if (complete)
            return false;

        if (cur->type == CPM_CHAR)
        {
            char c = cur->text[0];
            if (c == CPM_INDEX_OPEN)
            {
                if (!opened)
                {
                    opened = true;
                    compilerLog->Add("Index opened " + CPMSyntaxTreeNodeToString(cur))->Endl();
                    return true;
                }
            }

            if (!opened && c != CPM_INDEX_OPEN)
                return false;

            if (c == CPM_LINE_END)
                return false;

            if (!CharIsDelimiter(c) && !CharIsWhiteSpace(c))
            {
                compilerLog->Add(LOG_ERROR, "Invalid character in the index: ", cur->sourceFileName, cur->lineNumber)->Add(c);
                return false;
            }

            if (opened && c == CPM_INDEX_CLOSE)
            {
                compilerLog->Add("Index complete " + CPMSyntaxTreeNodeToString(cur))->Endl();
                complete = true;
                opened = false;
                return false;
            }
        }
        else
        {
            /*if (pcounter > 0)
                itemsCount++;
                else
                return false;*/
        }

        if (opened && !(cur->type == CPM_CHAR && CharIsWhiteSpace(cur->text[0])))
            seq.push_back(cur);

        if (next != NULL)
        {
            if (next->type == CPM_CHAR &&
                (next->text[0] == CPM_BLOCK_OPEN || next->text[0] == CPM_INDEX_OPEN))
                return false;
        }

        return true;
    }

    CPMSD_LINE::CPMSD_LINE() : CPMSyntaxDetector()
    {
        itemsCount = 0;
    }

    bool CPMSD_LINE::PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog)
    {
        if (complete)
            return false;

        if (cur->type == CPM_LINE)
            return false;

        if (cur->type == CPM_CHAR)
        {
            char c = cur->text[0];

            if (c == CPM_BLOCK_OPEN)
                return false;

            if (seq.size() == 0 && (c == CPM_BLOCK_CLOSE || c == CPM_LINE_END))
                return false;

            if (!CharIsWhiteSpace(c) && !CharIsDelimiter(c))
            {
                compilerLog->Add(LOG_ERROR, "Invalid character in the operator line: ")->Add(cur->text[0]);
                return false;
            }

            if (c == CPM_LINE_END)
            {
                complete = true;
                return false;
            }
        }
        else
            itemsCount++;

        /* if (itemsCount == 0 && cur->type != CPM_ID)
         {
         CPMCompilerLog.Add(LOG_ERROR, "Token '")->Add(cur->text)->Add("' is not an ID.");
         return false;
         }*/

        if (cur->type != CPM_CHAR && !CharIsWhiteSpace(cur->text[0]))
            seq.push_back(cur);

        if (next != NULL)
        {
            if (next->type == CPM_CHAR)
            {
                char cn = next->text[0];

                if (cn == CPM_BLOCK_OPEN)
                    return false;

                if (cn == CPM_BLOCK_CLOSE)
                {
                    complete = true;
                    return false;
                }
            }
        }

        return true;
    }

    CPMSD_BLOCK::CPMSD_BLOCK()
    {
        pcounter = 0;
    }

    bool CPMSD_BLOCK::PutNode(CPMSyntaxTreeNode* cur, CPMSyntaxTreeNode* next, Logger* compilerLog)
    {
        if (complete)
            return false;

        if (cur->type == CPM_CHAR)
        {
            char c = cur->text[0];
            if (seq.size() == 0 && c != CPM_BLOCK_OPEN)
                return false;

            if (seq.size() > 0 && !CharIsDelimiter(c) && !CharIsWhiteSpace(c))
            {
                compilerLog->Add(LOG_ERROR, "Invalid character in the block: ")->Add(c);                
                return false;
            }
            /*
            if (c == CPM_BLOCK_OPEN)
            pcounter++;
            if (c == CPM_BLOCK_CLOSE)
            pcounter--;

            if (pcounter < 0)
            {
            CPMCompilerLog.Add(LOG_ERROR, "Parenthesis parse error.");
            return false;
            }

            if (pcounter == 0)
            closed = true;*/
            if (c == CPM_BLOCK_CLOSE)
            {
                complete = true;
                seq.push_back(cur);
                return false;
            }
        }
        else
        {
            if (seq.size() == 0)
                return false;
        }

        if (next != NULL)
        {
            if (next->type == CPM_CHAR)
            {
                if (next->text[0] == CPM_BLOCK_OPEN)
                    return false;
            }
        }

        seq.push_back(cur);
        return true;
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

        //this->text = text;
        pass_CHAR(sourceFileName);
        pass_Detector<CPMSD_STR>(CPM_STR);
        pass_Comments();
        pass_Detector<CPMSD_ID>(CPM_ID);
        pass_Detector<CPMSD_NUM>(CPM_NUM);

        //while (pass_Detector<CPMSD_EXPR>(CPM_EXPR)) {}
        bool continueDetect = true;
        while (continueDetect)
        {
            continueDetect = false;
            continueDetect |= pass_Detector<CPMSD_REF>(CPM_REF);
            continueDetect |= pass_Detector<CPMSD_EXPR>(CPM_EXPR);
            continueDetect |= pass_Detector<CPMSD_INDEX>(CPM_INDEX);
        }

        //pass_Detector<CPMSD_LINE>(CPM_LINE);
        //pass_Detector<CPMSD_BLOCK>(CPM_BLOCK);


        while (true)
        {
            if (!pass_Detector<CPMSD_LINE>(CPM_LINE))
                break;
            if (!pass_Detector<CPMSD_BLOCK>(CPM_BLOCK))
                break;
        }
        /*pass_Detector<CPMSD_LINE>(CPM_LINE);
        pass_Detector<CPMSD_BLOCK>(CPM_BLOCK);*/
    }

    void CPMParser::Clear()
    {
        for (list<CPMSyntaxTreeNode*>::iterator li = layer.begin(); li != layer.end(); ++li)
        {
            DeleteSyntaxTree(*li);
        }
    }

    void CPMParser::pushSyntaxTreeNode(CPMSyntaxTreeNode* node, list<CPMSyntaxTreeNode*>::iterator childrenSeqStart, list<CPMSyntaxTreeNode*>::iterator childrenSeqFinish, CPMSyntaxDetector* detector)
    {
        node->parent = NULL;
        node->text = "";
        for (list<CPMSyntaxTreeNode*>::iterator li = childrenSeqStart; li != next(childrenSeqFinish); ++li)
        {
            node->text += (*li)->text;
            (*li)->parent = node;
            //node->children.push_back(*li);
        }
        detector->CopySeqTo(node->children);
        /*for (vector<CPMSyntaxTreeNode*>::iterator li = node->children.begin(); li != node->children.end(); ++li)
        {
        node->text += (*li)->text + ",";
        }*/

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
        return layerWasChanged;
    }

    void CPMParser::pass_CHAR(string sourceFileName)
    {
        Clear();
        for (int lineNumber = 1; lineNumber <= text.size(); lineNumber++)
        {
            string line = text[lineNumber-1] + ' ';
            for (int i = 0; i < line.size(); i++)
            {
                CPMSyntaxTreeNode* charNode = new CPMSyntaxTreeNode();
                charNode->parent = NULL;
                char c = line[i];
                if (c == '\t' || c == '\n' || c == '\r')
                    c = ' ';
                charNode->text = c;
                charNode->type = CPM_CHAR;
                charNode->lineNumber = lineNumber;
                charNode->sourceFileName = sourceFileName;
                layer.push_back(charNode);
            }
        }
    }

    void CPMParser::pass_Comments()
    {
        bool commentBlock = false;
        bool commentLine = false;
        list<CPMSyntaxTreeNode*>::iterator seqStart = layer.end();
        list<CPMSyntaxTreeNode*>::iterator seqFinish = layer.end();

        for (list<CPMSyntaxTreeNode*>::iterator li = layer.begin(); li != layer.end();)
        {
            list<CPMSyntaxTreeNode*>::iterator nextli = next(li);

            if (seqStart != layer.end() && seqFinish != layer.end())
            {
                layer.erase(seqStart, next(seqFinish));
                seqStart = layer.end();
                seqFinish = layer.end();
            }

            if (nextli == layer.end())
                break;

            CPMSyntaxTreeNode* c = (*li);
            CPMSyntaxTreeNode* nextc = (*nextli);

            if (c->type == CPM_CHAR && nextc->type == CPM_CHAR)
            {
                if (!commentBlock && !commentLine)
                {
                    if (c->text[0] == CPM_COMMENT_LINE[0] && nextc->text[0] == CPM_COMMENT_LINE[1])
                    {
                        seqStart = li;
                        commentLine = true;
                    }
                    else
                    if (c->text[0] == CPM_COMMENT_BLOCK_OPEN[0] && nextc->text[0] == CPM_COMMENT_BLOCK_OPEN[1])
                    {
                        seqStart = li;
                        commentBlock = true;
                    }
                }
                else
                {
                    if (commentBlock && c->text[0] == CPM_COMMENT_BLOCK_CLOSE[0] && nextc->text[0] == CPM_COMMENT_BLOCK_CLOSE[1])
                    {
                        commentBlock = false;
                        seqFinish = nextli;
                        nextli = next(nextli);
                    }
                }
            }

            if (commentLine)
            {
                if (c->lineNumber != nextc->lineNumber)
                {
                    commentLine = false;
                    seqFinish = li;
                }
            }

            li = nextli;
        }
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
        case CPM_NUM:
            s += "NUM"; break;
        case CPM_STR:
            s += "STR"; break;
        case CPM_ID:
            s += "ID"; break;
        case CPM_REF:
            s += "REF"; break;
        case CPM_EXPR:
            s += "EXPR"; break;
        case CPM_INDEX:
            s += "INDEX"; break;
        case CPM_LINE:
            s += "LINE"; break;
        case CPM_BLOCK:
            s += "BLOCK"; break;
        }
        
        s += " " + to_string(node->lineNumber) + ": '" + node->text + "'>";
        return s;
    }

}