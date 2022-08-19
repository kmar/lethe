#include "FreeIdList.h"

namespace lethe
{

// FreeIdList

FreeIdList::FreeIdList()
	: bottom(0)
	, counter(0)
{
}

Int FreeIdList::Alloc()
{
	Int res;

	// top-down alloc from bottom
	if (bottom > 0)
		return --bottom;

	// bottom-up alloc, either counter or freelist
	if (freeList.IsEmpty())
		return counter++;

	res = freeList.PopTop();
	LETHE_ASSERT(res >= bottom);
	return res;
}

void FreeIdList::Free(Int idx)
{
	LETHE_ASSERT(idx >= bottom && idx < counter);

	if (idx+1 == counter)
	{
		if (--counter <= bottom)
		{
			LETHE_ASSERT(freeList.IsEmpty());
			bottom = counter = 0;
		}

		return;
	}

	if (idx != bottom)
	{
		freeList.Add(idx);
		return;
	}

	if (++bottom >= counter)
	{
		LETHE_ASSERT(freeList.IsEmpty());
		bottom = counter = 0;
		return;
	}

	// try to compress
	// FIXME: not sure this is a great idea because it can take very long in theory
	if (counter - bottom == freeList.GetSize())
	{
		bottom = counter = 0;
		freeList.Clear();
		return;
	}

	while (!freeList.IsEmpty())
	{
		if (freeList.Top() != bottom)
			break;

		freeList.Pop();

		if (++bottom >= counter)
		{
			LETHE_ASSERT(freeList.IsEmpty());
			bottom = counter = 0;
			break;
		}
	}
}

void FreeIdList::Clear(Int start)
{
	freeList.Clear();
	bottom = 0;
	counter = start;
}

void FreeIdList::Reset(Int start)
{
	freeList.Reset();
	Clear(start);
}

void FreeIdList::SwapWith(FreeIdList &o)
{
	Swap(freeList, o.freeList);
	Swap(bottom, o.bottom);
	Swap(counter, o.counter);
}

// FreeIdListRange

FreeIdListRange::FreeIdListRange()
	: counter(0)
{
}

Int FreeIdListRange::GetMinFreeId() const
{
	return freeList.IsEmpty() ? counter : freeList[0].from;
}

Int FreeIdListRange::Alloc()
{
	if (freeList.IsEmpty())
		return counter++;

	auto &head = freeList[0];

	auto res = head.from++;

	if (head.from >= head.to)
		freeList.EraseIndex(0);

	return res;
}

Int FreeIdListRange::AllocSequence(Int count)
{
	LETHE_ASSERT(count > 0);

	for (Int i=0; i<freeList.GetSize(); i++)
	{
		auto &it = freeList[i];

		if (it.to - it.from >= count)
		{
			auto res = it.from;
			it.from += count;

			if (it.from >= it.to)
				freeList.EraseIndex(i);

			return res;
		}
	}

	auto res = counter;
	counter += count;
	return res;
}

void FreeIdListRange::FreeSequence(Int start, Int count)
{
	const Int idx = start;

	LETHE_ASSERT(idx >= 0 && count > 0 && idx + count <= counter);

	if (idx+count == counter)
	{
		// just decrease counter BUT still may need to compact last range
		counter -= count;
	}
	else
	{
		Range r = {idx, idx+count};

		auto *it = LowerBound(freeList.Begin(), freeList.End(), r);

		auto prev = it;

		if (prev != freeList.Begin())
			--prev;

		if (prev != freeList.End() && prev->to == r.from)
		{
			// expand prev
			prev->to = r.to;

			// still may want to merge with next
			if (it != freeList.End() && r.to == it->from)
			{
				prev->to = it->to;
				freeList.Erase(it);
			}
		}
		else if (it != freeList.End() && r.to == it->from)
		{
			// expand from
			it->from = r.from;
		}
		else
		{
			// finally simply insert if cannot merge
			freeList.Insert(it, r);
		}
	}

	// remove last range if possible
	while (!freeList.IsEmpty())
	{
		auto &fl = freeList.Back();

		if (fl.to != counter)
			break;

		counter = fl.from;
		freeList.Pop();
	}
}

void FreeIdListRange::Free(Int idx)
{
	FreeSequence(idx, 1);
}

void FreeIdListRange::Clear()
{
	freeList.Clear();
	counter = 0;
}

void FreeIdListRange::Reset()
{
	freeList.Reset();
	counter = 0;
}

void FreeIdListRange::SwapWith(FreeIdListRange &o)
{
	Swap(freeList, o.freeList);
	Swap(counter, o.counter);
}

}
