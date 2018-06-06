#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "CPMParser.h"
#include "CPMCompiler.h"
//#define COMPILE
#define PARSE
using namespace std;
using namespace CPM;

int _tmain(int argc, _TCHAR* argv[])
{
   // extern Logger CPMCompilerLog;

    string name;
   /* ifstream fsrc("test.cpm");
    if (fsrc.is_open())
    {
        stringstream buffer;
        buffer << fsrc.rdbuf();

        CPMParser parser(buffer.str());
        cout << buffer.str();
        cout << CPMCompilerLog.GetText() << "\n";
        cout << parser.LayerToString();

        ofstream fout("test.parsed");
        fout << parser.LayerToString();
        fout.close();
    }*/

#ifdef PARSE
    Logger logger;
    CPMParser* parser = new CPMParser("", "test.cpm", &logger);
    
    ofstream fout("test.parsed");
    fout << parser->LayerToString();
    //fout << comp.CompilerLog()->GetText();
    fout.close();

    //getline (std::cin, name);
    delete parser;
#endif

#ifdef COMPILE
    CPMCompiler* comp = new CPMCompiler("", "test.cpm", "test.bin", vector<string>());
    comp->PrintStaticData();
    ofstream fout("test.log");
    fout << comp->CompilerLog()->GetText();
    fout.close();
    delete comp;
#endif    

    return 0;
}



