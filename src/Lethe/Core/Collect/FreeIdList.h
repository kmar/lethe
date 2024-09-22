#pragma once

#include "PriorityQueue.h"


namespace lethe
{

LETHE_API_BEGIN

class LETHE_API FreeIdList
{
public:
	FreeIdList();

	// alloc id
	Int Alloc();
	// free id
	void Free(Int idx);
	// clear but keep preallocated mem
	// start: assume start indices already allocated
	void Clear(Int start = 0);
	// free memory
	void Reset(Int start = 0);

	inline Int GetCounter() const {return counter;}

	void SwapWith(FreeIdList &o);
private:
	PriorityQueue<Int> freeList;
	// anything below bottom is free
	Int bottom;
	// anything above (and including) counter is free
	Int counter;
};

// this should give the most compact results with guaranteed allocation of lowest possible indices
// however doesn't scale above 10k allocated elements + worst case deletion
class LETHE_API FreeIdListRange
{
public:
	FreeIdListRange();

	// alloc id
	Int Alloc();
	// alloc sequential range of ids
	// perf note: worst case of O(n) wrt free list size
	Int AllocSequence(Int count);
	void FreeSequence(Int start, Int count);

	// free id
	void Free(Int idx);
	// clear but keep preallocated mem
	void Clear();
	// free memory
	void Reset();

	// get minimum free id
	Int GetMinFreeId() const;

	inline Int GetCounter() const {return counter;}
	inline Int GetNumRanges() const {return freeList.GetSize();}

	void SwapWith(FreeIdListRange &o);

private:
	struct Range
	{
		Int from, to;

		// sorting operator
		inline bool operator <(const Range &o) const
		{
			return from < o.from;
		}
	};

	Array<Range> freeList;

	// anything above (and including) counter is free
	Int counter;
};

LETHE_API_END

}
