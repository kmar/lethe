#pragma once

#include "../Sys/Types.h"
#include "../Sys/Singleton.h"
#include "../Thread/Lock.h"

namespace lethe
{

class LETHE_API ObjectHeap
{
	AtomicUInt count;		// object count (FIXME: can't have more than 4 billion objects)
	LETHE_SINGLETON(ObjectHeap)
public:
	ObjectHeap();
	~ObjectHeap();

	// allocate/deallocate/reallocate
	void *Alloc(size_t size, size_t align = 16);
	void Dealloc(void *ptr);
	void *Realloc(void *ptr, size_t newSize, size_t align = 16);

	// get number of allocated blocks
	inline size_t GetCount() const
	{
		return (size_t)count;
	}
};

}
