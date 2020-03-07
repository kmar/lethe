#pragma once

#include "../Sys/Types.h"
#include "../Sys/Singleton.h"
#include "../Thread/Lock.h"
#include "../Collect/Array.h"

namespace lethe
{

class LETHE_API Heap
{
public:
	// JIT support, doesn't matter if enabled or not
	// size is rounded up to nearest page
	// returns null on error
	static void *AllocateExecutableMemory(size_t &size);
	// size = rounded size from previous call to AllocateExecutablePages
	static void FreeExecutableMemory(void *ptr, size_t size);
};

}
