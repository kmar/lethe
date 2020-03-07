#pragma once

#include "../Common.h"

#include <Lethe/Core/Collect/ArrayInterface.h>

namespace lethe
{

class CompiledProgram;
class ScriptContext;
class DataType;

class LETHE_API NativeHelpers
{
public:
	static void Init(CompiledProgram &p);

	// array interface (to support script dynamic array changes
	static Int ArrayInterface(ScriptContext &ctx, const DataType &elemType, ArrayIntfCommand cmd, void *aptr, void *pparam, Int iparam);
};

}
