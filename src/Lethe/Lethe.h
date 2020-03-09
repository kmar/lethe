#pragma once

#include "Script/ScriptInit.h"
#include "Script/ScriptEngine.h"
#include "Script/Utils/FormatStr.h"

#if LETHE_COMPILER_GCC || LETHE_COMPILER_CLANG
// disable silly warning about offsetof + nonstd layout,
// I've yet to meet a programmer who ever used virtual inheritance
#	pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
