#include "../Sys/Types.h"
#include "../Sys/Assert.h"
#include "../Memory/AlignedAlloc.h"
#include "../Thread/Atomic.h"
#include "ObjectHeap.h"

namespace lethe
{

// ObjectHeap

LETHE_SINGLETON_INSTANCE(ObjectHeap)

// allocate/deallocate
void *ObjectHeap::Alloc(size_t size, size_t align)
{
	void *res = ObjectAllocator.CallAlloc(size, align);
	UInt inc = Atomic::Increment(count);
	(void)inc;
	LETHE_ASSERT(inc);

	return res;
}

void ObjectHeap::Dealloc(void *ptr)
{
	ObjectAllocator.CallFree(ptr);
	Atomic::Decrement(count);
}

void *ObjectHeap::Realloc(void *ptr, size_t newSize, size_t align)
{
	LETHE_ASSERT(newSize > 0);
	return ObjectAllocator.CallRealloc(ptr, newSize, align);
}

ObjectHeap::ObjectHeap() : count(0)
{
}

ObjectHeap::~ObjectHeap()
{
	LETHE_ASSERT(!count && "Object heap contains live blocks!");
}

}
