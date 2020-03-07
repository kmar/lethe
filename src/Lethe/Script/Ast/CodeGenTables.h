#pragma once

#include <Lethe/Script/Vm/Opcodes.h>
#include <Lethe/Script/TypeInfo/DataTypes.h>

namespace lethe
{

extern const Int LETHE_API opcodeRefLoadOfs[DT_FUNC_PTR+1];
extern const Int LETHE_API opcodeGlobalLoad[DT_FUNC_PTR + 1];
extern const Int LETHE_API opcodeLocalLoad[DT_FUNC_PTR + 1];
extern const Int LETHE_API opcodeLocalStore[2][DT_FUNC_PTR + 1];
extern const Int LETHE_API opcodeGlobalStore[2][DT_FUNC_PTR + 1];
extern const Int LETHE_API opcodeRefStore[2][DT_FUNC_PTR + 1];
extern const Int LETHE_API opcodeRefInc[DT_NAME];
extern const Int LETHE_API opcodeRefIncPost[DT_NAME];

// FIXME: currently this is necessary for functions returning shorts/bytes/bools
static constexpr Int OPC_PUSH_NOZERO = OPC_PUSH_RAW;

}
