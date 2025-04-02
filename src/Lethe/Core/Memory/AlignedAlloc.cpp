#include "AlignedAlloc.h"
#include "../Sys/Assert.h"
#include "../Math/Templates.h"
#include <memory.h>
#include <stdlib.h>

/*
	Block layout:
	[-4]
	^		^
	|		ptr to block
	UInt number of bytes to go back until we hit original alloc pointer (-4)
*/

namespace lethe
{

CustomAllocator StringAllocator;
// generic allocator (used for arrays, ...)
CustomAllocator GenericAllocator;
// bucket allocator (used for AST nodes)
CustomAllocator BucketAllocator;
// script object allocator
CustomAllocator ObjectAllocator;

#if !LETHE_USE_HEAP
static void *GetNativePtr(void *block)
{
	char *pblock = static_cast< char * >(block);
	pblock -= sizeof(UInt);
	union u
	{
		char c;
		UInt v;
	};
	UInt val = reinterpret_cast< u * >(pblock)->v;
	pblock -= val;
	return pblock;
}

static void SetNativePtr(void *block, void *ptr)
{
#	if !defined(__clang_analyzer__)
	char *pblock = static_cast< char * >(ptr);
	UInt val = static_cast< UInt >(static_cast< const char * >(ptr) - static_cast< const char * >(block));
	val -= sizeof(UInt);
	pblock -= sizeof(UInt);
	union u
	{
		char c;
		UInt v;
	};
	reinterpret_cast< u * >(pblock)->v = val;
#	endif
}
#endif

void *FallbackAlignedAlloc::Alloc(size_t size, size_t align, size_t groupKey)
{
	(void)groupKey;
#if LETHE_USE_HEAP
	return Heap::Get().Alloc(size, align);
#else
#	if !defined(__clang_analyzer__)
	align = Max(sizeof(UInt), align);
	LETHE_ASSERT(align >= sizeof(UInt) && IsPowerOfTwo(align));
	size_t sz = size + align-1 + sizeof(UInt);
	LETHE_ASSERT(sz >= size);
	void *blk = malloc(sz);
	uintptr_t ptr = reinterpret_cast<uintptr_t>(static_cast< char * >(blk));
	ptr += sizeof(UInt) + align -1;
	ptr &= ~(static_cast<uintptr_t>(align-1));
#	else
	void *blk = nullptr;
	uintptr_t ptr = 0;
#	endif
	void *res = reinterpret_cast<void *>(ptr);
	SetNativePtr(blk, res);
	return res;
#endif
}

void *FallbackAlignedAlloc::Realloc(void *ptr, size_t newSize, size_t align, size_t groupKey)
{
	(void)groupKey;
#if LETHE_USE_HEAP

	if (ptr && Heap::Get().SmartRealloc(ptr, newSize, align))
		return ptr;

#endif
	(void)ptr;
	return Alloc(newSize, align, groupKey);
}

void FallbackAlignedAlloc::Free(void *ptr)
{
	if (!ptr)
		return;

#if LETHE_USE_HEAP
	Heap::Get().Free(ptr);
#else
	free(GetNativePtr(ptr));
#endif
}

}
