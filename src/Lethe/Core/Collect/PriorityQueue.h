#pragma once

// unlike STL priority_queue, this is a min-heap which makes more sense (because of A*, Dijkstra)
// reference: http://www.eecs.wsu.edu/~ananth/CptS223/Lectures/heaps.pdf

#include "Array.h"

namespace lethe
{

template<typename T, typename S = Int, typename A = HAlloc<T>> class PriorityQueue
{
public:

	inline PriorityQueue &Add(const T &key);
	// aliases
	inline PriorityQueue &Insert(const T &key);
	inline PriorityQueue &Push(const T &key);

	inline bool IsEmpty() const;

	inline S GetSize() const;

	inline const T &Top() const;

	inline PriorityQueue &Pop();

	inline T PopTop();

	inline PriorityQueue &Clear();
	inline PriorityQueue &Shrink();
	inline PriorityQueue &Reset();

	bool IsValid() const;

	// get memory usage
	size_t GetMemUsage() const;

	inline void SwapWith(PriorityQueue &o);

private:
	// get parent index
	static inline S GetParent(S index);
	// get (left) child index
	// right child is left child+1
	static inline S GetChild(S index);

	// get right child index
	static inline S GetRightChild(S index);

	void InsertKey(const T &key);

	inline void DeleteMin();

	// FIXME: this is currently slower than std::priority_queue
	// with the code that follows it's only about 9% slower but still...
	void PercolateDown(S index);

	Array<T,S,A> data;
};

#include "Inline/PriorityQueue.inl"

}
