#pragma once

#include "falcfrontend.h"

#include <fridge.h>
#include "fridgemulib.h"

struct App;

void DrawAliasesPanel(const DebugInfo& debug);
void DrawStaticsPanel(const DebugInfo& debug, const FRIDGE_CPU* cpu);
