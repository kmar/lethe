#include "ScriptInit.h"

#include <Lethe/Core/Thread/Atomic.h>
#include <Lethe/Core/Thread/Lock.h>
#include <Lethe/Core/Time/Timer.h>
#include <Lethe/Core/String/Name.h>
#include <Lethe/Core/String/CharConv.h>
#include <Lethe/Core/Classes/ObjectHeap.h>

namespace lethe
{

static AtomicUInt letheInitCounter = 0;

void Init(const InitOptions *opts)
{
	if (Atomic::Increment(letheInitCounter) != 1)
		return;

	if (opts)
	{
		StringAllocator = opts->stringAllocator;
		GenericAllocator = opts->genericAllocator;
		BucketAllocator = opts->bucketAllocator;
		ObjectAllocator = opts->objectAllocator;
	}

	Platform::Init();
	Timer::Init();
	CharConv::Init();
	NameTable::Init();
	ObjectHeap::Init();
}

void Done()
{
	if (Atomic::Decrement(letheInitCounter))
		return;

	ObjectHeap::Done();
	NameTable::Done();
	CharConv::Done();
	Timer::Done();
	Platform::Done();
}

}
