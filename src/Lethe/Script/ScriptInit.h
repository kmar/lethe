#pragma once

#include "Common.h"

#include <Lethe/Core/Memory/AlignedAlloc.h>
#include <Lethe/Core/Collect/Array.h>

namespace lethe
{

struct InitOptions
{
	CustomAllocator stringAllocator;
	CustomAllocator genericAllocator;
	CustomAllocator bucketAllocator;
	CustomAllocator objectAllocator;
};

// one time static Init/Done, main thread
void LETHE_API Init(const InitOptions *opts = nullptr);
void LETHE_API Done();

}
