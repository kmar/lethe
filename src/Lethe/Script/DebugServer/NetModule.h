#pragma once

#include "../Common.h"

#include <Lethe/Core/Thread/Atomic.h>

namespace lethe
{

class LETHE_API NetModule
{
	static AtomicInt refCount;
public:
	static void Init();
	static void Done();
};

}
