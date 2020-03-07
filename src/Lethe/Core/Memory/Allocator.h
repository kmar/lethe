#pragma once

#include "../Sys/Types.h"
#include "../Sys/Align.h"
#include "../Sys/Platform.h"
#include "AlignedAlloc.h"

namespace lethe
{

// hybrid heap/small buffer allocator
template< typename T >
struct HAlloc
{
	template<typename T2>
	struct Rebind
	{
		typedef HAlloc<T2> Type;
	};

	LETHE_NOINLINE T *GetSmallBuffer(size_t stackCapacity)
	{
		T *buf = nullptr;

		if (stackCapacity > 0)
		{
			// note: this is potentially dangerous, making assumptions about compiler's alignment
			// but we assert in StackArray
			char *c = reinterpret_cast<char *>(this) + 2 * sizeof(Int) + sizeof(void *);
			buf = static_cast<T *>(AlignPtr(c, AlignOf<T>::align));
		}

		return buf;
	}

	LETHE_NOINLINE T *Realloc(T *ptr, size_t sz, size_t stackCapacity)
	{
		auto buf = GetSmallBuffer(stackCapacity);

		if (sz <= (size_t)(stackCapacity * sizeof(T)) && (!ptr || ptr == buf))
			return buf;

		return static_cast<T *>(AlignedAlloc::Realloc(ptr == buf ? static_cast<T *>(nullptr) : ptr, sz, AlignOf<T>::align));
	}

	inline void Free(T *ptr)
	{
		AlignedAlloc::Free(ptr);
	}
};

// cache-aligned allocator
template< typename T >
struct CAAlloc
{
	template<typename T2>
	struct Rebind
	{
		typedef CAAlloc<T2> Type;
	};

	LETHE_NOINLINE T *Realloc(T *ptr, size_t sz, size_t stackCapacity)
	{
		(void)stackCapacity;
		LETHE_ASSERT(!stackCapacity);
		return static_cast<T *>(AlignedAlloc::Realloc(ptr, sz, LETHE_CACHELINE_SIZE));
	}
	inline void Free(T *ptr)
	{
		AlignedAlloc::Free(ptr);
	}
};

}
