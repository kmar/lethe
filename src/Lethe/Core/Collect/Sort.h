#pragma once

#include "RotateArray.h"

namespace lethe
{

// FIXME: const C &cmp everywhere is actually less predicate

// O: optimize bounds, default is true
template< typename T, typename S = Int, bool O = 1 >
struct QuickSort
{
	static inline void Sort(T *ptr, S size)
	{
		Sort(ptr, size, LessPredicate<T>());
	}
	static inline void Sort(T *ptr, const T *end)
	{
		Sort(ptr, end, LessPredicate<T>());
	}
	template< typename C > static void Sort(T *ptr, S size, const C &cmp);
	template< typename C > static void Sort(T *ptr, const T *end, const C &cmp);
private:
	static const int MAX_SORT_DEPTH = 512;
	// use insertion sort if # of elems <= this
	static const int INSERTION_THRESHOLD = 8;

	struct SortStack
	{
		S start;
		S end;
	};

	static inline S PickPivot(S start, S end);

	// return new pivot position
	template< typename C > static S Partition(T *ptr, S start, S end, const C &cmp);
	template< typename C > static void Sort(T *ptr, S start, S end, const C &cmp);
};

template< typename T, typename S = Int >
struct InsertionSort
{
	// allow to start at an arbitrary element
	static inline void Sort(T *ptr, S size, S start = 1)
	{
		Sort(ptr, size, LessPredicate<T>(), start);
	}
	static inline void Sort(T *ptr, const T *end)
	{
		Sort(ptr, end, LessPredicate<T>());
	}
	template< typename C > static void Sort(T *ptr, S size, const C &cmp, S start = 1);
	template< typename C > static void Sort(T *ptr, const T *end, const C &cmp);
};

// MergeSort
// allocates extra O(N) memory unless sorted array fits in 8000 bytes
// in that case, stack is used as scratch pad
template< typename T, typename S = Int >
struct MergeSort
{
	static inline void Sort(T *ptr, S size)
	{
		Sort(ptr, size, LessPredicate<T>());
	}
	static inline void Sort(T *ptr, const T *end)
	{
		Sort(ptr, end, LessPredicate<T>());
	}
	template< typename C > static void Sort(T *ptr, S size, const C &cmp);
	template< typename C > static void Sort(T *ptr, const T *end, const C &cmp);
private:
	// use insertion sort if # of elems <= this
	static const int INSERTION_THRESHOLD = 8;

	template< typename C >
	static void Merge(T *ptr, S mid, S size, T *scratch, const C &cmp);
	template< typename C >
	static void SortRecursive(T *ptr, S size, T *scratch, const C &cmp);
};

// ~1.5x slower than std::stable_sort
// do not use this to sort huge objects/objects with complex ctors
template< typename T, typename S = Int >
class InplaceMergeSort
{
public:

	static inline void Sort(T *buf, S size)
	{
		Sort(buf, size, LessPredicate<T>());
	}
	static inline void Sort(T *buf, const T *end)
	{
		Sort(buf, end, LessPredicate<T>());
	}
	template< typename C > static void Sort(T *buf, S size, const C &cmp);
	template< typename C > static void Sort(T *buf, const T *end, const C &cmp);

private:
	// use insertion sort if # of elems <= this
	static const int INSERTION_THRESHOLD = 16;
	static const int SCRATCH_SIZE = (int)(14000/sizeof(T));

	static inline void Rotate(T *buf, S mid, S len);
	template< typename C > static void SortInternal(T *buf, S size, T *sbuf, const C &cmp);
	template< typename C > static void Merge(T *buf, S mid, S size, T *sbuf, const C &cmp);
};

#include "Inline/Sort.inl"

}
