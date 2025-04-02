#pragma once

#include "../Sys/Types.h"

namespace lethe
{

// default fallback aligned allocator
struct LETHE_API FallbackAlignedAlloc
{
	// allocation
	static void *Alloc(size_t size, size_t align, size_t groupKey);
	// in-place reallocation
	// if can't be done in-place, new block is allocated
	// old block is NOT freed (unlike stupid C realloc)
	static void *Realloc(void *ptr, size_t newSize, size_t align, size_t groupKey);
	// deallocation
	static void Free(void *ptr);
};

struct LETHE_API CustomAllocator
{
	// allocate block
	// group key: class name hash if available
	void *(*Alloc)(size_t size, size_t align, size_t groupKey) = nullptr;
	// reallocate, unlike C realloc, it must keep old memory block if a new block was allocated
	// if this is not supported, simply call Alloc
	void *(*Realloc)(void *ptr, size_t newSize, size_t align, size_t groupKey) = nullptr;
	// free block
	void (*Free)(void *ptr) = nullptr;

	inline void *CallAlloc(size_t size, size_t align, size_t groupKey = 0)
	{
		return Alloc ? Alloc(size, align, groupKey) : FallbackAlignedAlloc::Alloc(size, align, groupKey);
	}

	inline void *CallRealloc(void *ptr, size_t newSize, size_t align, size_t groupKey = 0)
	{
		return Realloc ? Realloc(ptr, newSize, align, groupKey) : CallAlloc(newSize, align, groupKey);
	}

	inline void CallFree(void *ptr)
	{
		return Free ? Free(ptr) : FallbackAlignedAlloc::Free(ptr);
	}
};

// string allocator
extern CustomAllocator LETHE_API StringAllocator;
// generic allocator (used for arrays, ...)
extern CustomAllocator LETHE_API GenericAllocator;
// bucket allocator (used for AST nodes and anything that used to be pooled)
extern CustomAllocator LETHE_API BucketAllocator;
// script object allocator
extern CustomAllocator LETHE_API ObjectAllocator;

// aligned allocator
struct LETHE_API AlignedAlloc
{
	// allocation
	static inline void *Alloc(size_t size, size_t align) {return GenericAllocator.CallAlloc(size, align);}
	// in-place reallocation
	// if can't be done in-place, new block is allocated
	// old block is NOT freed (unlike stupid C realloc)
	static inline void *Realloc(void *ptr, size_t newSize, size_t align) {return GenericAllocator.CallRealloc(ptr, newSize, align);}
	// deallocation
	static inline void Free(void *ptr) {return GenericAllocator.CallFree(ptr);}
};

}
