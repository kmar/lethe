#pragma once

#include <Lethe/Core/Memory/Heap.h>
#include <Lethe/Core/Memory/Allocator.h>
#include <Lethe/Core/Collect/Array.h>

namespace lethe
{

// cache-aligned allocator
template< typename T >
struct JITPageAlloc
{
	struct LiveBlock
	{
		T *ptr;
		size_t size;
	};
	Array<LiveBlock> liveBlocks;

	~JITPageAlloc()
	{
		LETHE_ASSERT(liveBlocks.IsEmpty());
	}

	LETHE_NOINLINE T *Realloc(T *ptr, size_t sz, size_t stackCapacity)
	{
		if (!liveBlocks.IsEmpty() && sz <= liveBlocks.Back().size)
			return ptr;

		(void)ptr;
		(void)stackCapacity;
		LETHE_ASSERT(!stackCapacity && sz);
		auto *res = static_cast<T *>(Heap::AllocateExecutableMemory(sz));

		liveBlocks.Add(LiveBlock{res, sz});

		return res;
	}

	LETHE_NOINLINE Int FindLiveBlock(T *ptr) const
	{
		for (Int i=0; i<liveBlocks.GetSize(); i++)
		{
			if (liveBlocks[i].ptr == ptr)
				return i;
		}

		return -1;
	}

	LETHE_NOINLINE void Free(T *ptr)
	{
		if (!ptr)
			return;

		auto idx = FindLiveBlock(ptr);
		LETHE_ASSERT(idx >= 0);

		const auto &lb = liveBlocks[idx];

		Heap::FreeExecutableMemory(lb.ptr, lb.size);
		liveBlocks.EraseIndex(idx);
	}
};

template<typename T>
class JITPageAlignedArray : public Array<T, Int, JITPageAlloc<T>>
{
public:
	inline void SwapWith(JITPageAlignedArray &o)
	{
		Swap(this->liveBlocks, o.liveBlocks);
		Array<T, Int, JITPageAlloc<T>>::SwapWith(o);
	}
};


}
