#pragma once

#include "CPMIR.h"

namespace CPM
{
    // Produce FALC source for a checked program. Errors leave the listing empty.
    string CPMEmitAssembly(CPMCompiler& compiler, const CPMIRProgram& program);
}
