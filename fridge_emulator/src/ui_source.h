#pragma once

#include "falcfrontend.h"

#include <fridge.h>
#include "fridgemulib.h"

struct App;

void InitSourceBreakpoints();
void DrawSourcePanel(App& app, const DebugInfo& debug, const FRIDGE_CPU* cpu);
