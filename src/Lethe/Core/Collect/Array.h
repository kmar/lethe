#pragma once

#include "../Sys/Types.h"
#include "../Sys/Assert.h"
#include "../Sys/Likely.h"
#include "../Sys/Inline.h"
#include "../Math/Templates.h"
#include "../Collect/Sort.h"
#include "../Memory/Memory.h"
#include "../Memory/Allocator.h"
#include "ArrayRef.h"

namespace lethe
{

template< typename T, typename S = Int, typename A = HAlloc<T> >
class Queue;

template<typename T, typename S = Int>
struct ArrayBase_
{
	T *data = nullptr;
	S size = 0;
	S reserve = 0;

	void Clear()
	{
		DestroyObjectRange(data, size);
		size = 0;
	}
};

// dynamic array
template< typename T, typename S = Int, typename A = HAlloc<T> >
class Array : protected A, protected ArrayBase_<T,S>
{
	friend class Queue<T,S,A>;
public:
	Array();
	Array(const Array &o);
	Array(const ArrayRef<T,S> &o);
	explicit Array(S newSize);
	// with initializer
	explicit Array(S newSize, const T &ini);
	~Array();

	// get size
	inline S GetSize() const;
	// get capacity
	inline S GetCapacity() const;

	// empty test
	inline bool IsEmpty() const;

	// get data
	inline T *GetData();
	inline const T *GetData() const;

	// resize (optionally reserves space)
	LETHE_NOINLINE Array &Resize(S newSize);
	// resize (use ini as initializer)
	LETHE_NOINLINE Array &Resize(S newSize, const T &ini);
	// resize to fit index
	inline Array &ResizeToFit(S index);
	inline Array &ResizeToFit(S index, const T &ini);
	// this doesn't do any shrinking
	// grows exponentially by a factor of 1.5x unless capacity was previously zero (or small buffer)
	LETHE_NOINLINE Array &Reserve(S newReserve);
	// clear
	Array &Clear();
	// shrink reserve to fit size
	LETHE_NOINLINE Array &Shrink();
	// reset => clears and frees data
	LETHE_NOINLINE Array &Reset();
	// add or push
	// returns index of newly added element
	S Add(const T &elem);
	// add unique element, perf note: does linear scan!
	LETHE_NOINLINE S AddUnique(const T &elem);
	// add and swap (=move)
	// FIXME: I should really learn C++11 move semantics and switch
	S AddSwap(T &elem);
	S AddSwapUnique(const T &elem);
	// returns pointer to newly allocated count elements; must be filled externally
	// intended use is to fill manually instead of doing multiple adds
	// this is more flexible than adding an array because it avoids copying
	T *Alloc(S count);
	// push (back)
	inline S Push(const T &elem);
	// aliases
	inline S Append(const T &elem);
	inline S AppendBack(const T &elem);
	inline S AddBack(const T &elem);
	inline S PushBack(const T &elem);
	// pop back
	Array &Pop();
	// append, returns index where newly appended array starts
	inline S Append(const Array &o);
	inline S Append(const ArrayRef<T,S> &o);
	LETHE_NOINLINE S Append(const T *buf, S sz);

	// append and swap
	inline S AppendSwap(Array &o);
	inline S AppendSwap(ArrayRef<T,S> o);
	LETHE_NOINLINE S AppendSwap(T *buf, S sz);

	// reverse elements
	LETHE_NOINLINE Array &Reverse();

	// get memory usage
	LETHE_NOINLINE size_t GetMemUsage() const;

	// get references to front/back elements
	inline const T &Front() const;
	inline T &Front();
	inline const T &Back() const;
	inline T &Back();

	// iterators
	typedef const T	*ConstIterator;
	typedef T *Iterator;
	typedef const T	*ConstReverseIterator;
	typedef T *ReverseIterator;

	// find (beware: O(n))
	S FindIndex(const T &elem) const;
	ConstIterator Find(const T &elem) const;
	Iterator Find(const T &elem);

	// find maximum element (beware: O(n))
	S FindMaxIndex() const;
	ConstIterator FindMax() const;
	Iterator FindMax();

	// find minimum element (beware: O(n))
	S FindMinIndex() const;
	ConstIterator FindMin() const;
	Iterator FindMin();

	// erase element
	Array &Erase(S index);
	// fast erase element (doesn't preserve order)
	Array &EraseFast(S index);
	// erase using indices (note that these are redundant)
	inline Array &EraseIndex(S index);
	inline Array &EraseIndexFast(S index);
	// erase using iterators
	inline Iterator Erase(Iterator it);
	inline Iterator EraseFast(Iterator it);

	// erase range
	Array &Erase(S index, S count);
	// fast erase element (doesn't preserve order)
	Array &EraseFast(S index, S count);
	// erase using indices (note that these are redundant)
	inline Array &EraseIndex(S index, S count);
	inline Array &EraseIndexFast(S index, S count);
	// erase using iterators
	inline Iterator Erase(Iterator it, Iterator itEnd);
	inline Iterator EraseFast(Iterator it, Iterator itEnd);

	// erase based on a predicate
	// returns new size
	template<typename F>
	S EraseIf(F func);

	// insert element
	Array &Insert(S index, const T &elem);
	inline Array &InsertIndex(S index, const T &elem);
	inline Array &Insert(Iterator it, const T &elem);
	// insert range
	Array &Insert(S index, const T *elem, S count);
	inline Array &InsertIndex(S index, const T *elem, S count);
	inline Array &Insert(Iterator it, const T *elem, S count);

	// access operators
	inline const T &operator[](S index) const;
	inline T &operator[](S index);

	// assignment
	LETHE_NOINLINE Array &operator =(const Array &o);
	LETHE_NOINLINE Array &operator =(const ArrayRef<T,S> &o);

	// comparison
	inline bool operator ==(const Array &o) const;
	inline bool operator !=(const Array &o) const;

	// swap
	inline void SwapWith(Array &o);

	// these enable range based loop BUT msc currently cannot vectorize loops like that
	inline T *begin();
	inline T *end();
	inline const T *begin() const;
	inline const T *end() const;

	inline T *Begin();
	inline T *End();
	inline const T *Begin() const;
	inline const T *End() const;

	inline T *RBegin();
	inline T *REnd();
	inline const T *RBegin() const;
	inline const T *REnd() const;

	// in-place unstable sort (using less predicate, least element goes first)
	Array &Sort();
	// stable version of the above
	Array &StableSort();
	// same as above but using custom predicate
	template< typename C > Array &Sort(C cmp);
	template< typename C > Array &StableSort(C cmp);
	// in-place stable sort where array is already almost sorted (incrementally after insertion)
	Array &SortInsertion();
	// same as above but using custom predicate
	template< typename C > Array &SortInsertion(C cmp);
	// is sorted (using less predicate)
	bool IsSorted() const;
	// is sorted (using custom predicate)
	template< typename C > bool IsSorted(C cmp) const;

	// index sorting (unstable)
	// note: we don't use SortInsertion which is probably useless in this case
	const Array &SortIndex(S *arr) const;
	template<typename B>
	inline const Array &SortIndex(Array<S,S,B> &arr) const;
	template< typename C > const Array &SortIndex(S *arr, C cmp) const;
	template< typename B, typename C > inline const Array &SortIndex(Array<S,S,B> &arr, C cmp) const;

	// stable version
	const Array &StableSortIndex(S *arr) const;
	template<typename B>
	inline const Array &StableSortIndex(Array<S,S,B> &arr) const;
	template< typename C > const Array &StableSortIndex(S *arr, C cmp) const;
	template<typename B, typename C>
	inline const Array &StableSortIndex(Array<S,S,B> &arr, C cmp) const;

	// fill with value
	Array &Fill(const T &value);
	// fast but unsafe(!) fill using MemSet
	Array &MemSet(int value);

	ArrayRef<const T,S> Slice(S from, S to) const;
	ArrayRef<const T,S> Slice(S from) const;
	ArrayRef<T,S> Slice(S from, S to);
	ArrayRef<T,S> Slice(S from);

	inline operator ArrayRef<const T,S>() const
	{
		return Slice(0);
	}

	inline operator ArrayRef<T,S>()
	{
		return Slice(0);
	}

	inline bool IsValidIndex(S idx) const
	{
		return IsValidArrayIndex(idx, this->size);
	}

protected:

	// required for index sorting
	struct IndexCompare
	{
		const T *arr;
		inline bool operator ()(S x, S y) const
		{
			return arr[x] < arr[y];
		}
	};

	template< typename C >
	struct IndexCompareCmp
	{
		C *cmp;
		const T *arr;
		inline bool operator ()(S x, S y) const
		{
			return (*cmp)(arr[x], arr[y]);
		}
	};

	// force reserve reallocation
	LETHE_NOINLINE Array &Reallocate(S newReserve);
	LETHE_NOINLINE void ReallocateInternal(T *newData, S newSize);

	static inline S GrowCapacity(S cap)
	{
		return cap<2 ? cap+1 : cap*3/2;
	}
};

#if LETHE_COMPILER_MSC
#	pragma warning(push)
#	pragma warning(disable: 4324) // alignment (padding)
#endif

// be careful not to swap when passed as Array type!
template<typename T, Int SZ >
class StackArray : public Array<T, Int >
{
	inline T *GetBuffer() const
	{
		return (T *)(this->buf);
	}

	void InitBuffer()
	{
		LETHE_ASSERT(this->GetBuffer() == this->GetSmallBuffer(SZ));
		this->data = this->GetBuffer();
		this->reserve = -SZ;
	}

public:
	inline StackArray()
	{
		InitBuffer();
	}
	explicit StackArray(Int newSize)
	{
		InitBuffer();
		this->Resize(newSize);
	}
	// with initializer
	explicit StackArray(Int newSize, const T &ini)
	{
		InitBuffer();
		this->Resize(newSize, ini);
	}

	StackArray(const Array<T, Int> &o)
		: Array<T, Int>()
	{
		InitBuffer();
		*this = o;
	}

	StackArray(const StackArray &o)
		: Array<T, Int>()
	{
		InitBuffer();
		*this = o;
	}

	inline StackArray &operator =(const Array<T, Int> &o)
	{
		Array<T, Int>::operator =(o);
		return *this;
	}

	inline StackArray &operator =(const StackArray &o)
	{
		Array<T, Int>::operator =(o);
		return *this;
	}

	inline StackArray &operator =(const ArrayRef<T,Int> &o)
	{
		Array<T, Int>::operator =(o);
		return *this;
	}

	LETHE_NOINLINE void SwapWith(StackArray &o)
	{
		Swap(this->data, o.data);
		Swap(this->size, o.size);
		Swap(this->reserve, o.reserve);

		// avoid small buffer swap if both already heap allocated
		if (this->reserve >= 0 && o.reserve >= 0)
			return;

		auto ssz0 = (this->reserve < 0)*this->size;
		auto ssz1 = (o.reserve < 0)*o.size;

		if (ssz0 | ssz1)
		{
			// perform swap on buffers
			// note: this is reversed because we already swapped sizes
			auto *buf0 = o.GetBuffer();
			auto *buf1 = this->GetBuffer();

			if (ssz0 < ssz1)
			{
				Swap(ssz0, ssz1);
				Swap(buf0, buf1);
			}

			auto swapMin = Min<Int>(ssz0, ssz1);
			auto swapMax = Max<Int>(ssz0, ssz1);

			for (Int i=0; i<swapMin; i++)
				Swap(buf0[i], buf1[i]);

			if (ssz0 != ssz1)
			{
				ConstructObjectRange(buf1 + swapMin, swapMax-swapMin);

				for (Int i=swapMin; i<swapMax; i++)
					Swap(buf0[i], buf1[i]);

				DestroyObjectRange(buf0 + swapMin, swapMax-swapMin);
			}
		}

		// don't forget to fixup small buffer data pointers
		if (o.data == GetBuffer())
			o.data = o.GetBuffer();

		if (this->data == o.GetBuffer())
			this->data = GetBuffer();
	}

private:
	LETHE_ALIGN(AlignOf<T>::align) Byte buf[SZ*sizeof(T)];
};

#if LETHE_COMPILER_MSC
#	pragma warning(pop)
#endif

template<typename T>
class CacheAlignedArray : public Array<T, Int, CAAlloc<T> >
{
public:
	inline void SwapWith(CacheAlignedArray &o)
	{
		Array<T,Int, CAAlloc<T>>::SwapWith(o);
	}
};

template<typename T, typename A = HAlloc<T> >
class BigArray : public Array<T, Long, A>
{
public:
	inline void SwapWith(BigArray &o)
	{
		Array<T,Long, A>::SwapWith(o);
	}
};

#include "Inline/Array.inl"

}
