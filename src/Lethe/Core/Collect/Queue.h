#pragma once

// this is actually a double-ended queue without insertion/deletion
// implemented as a circular-buffer dynamic array

#include "Array.h"

namespace lethe
{

// dynamic array
template< typename T, typename S, typename A >
class Queue
{
public:
	Queue();

	Queue(const Queue &o);

	// this is actually reserve
	explicit Queue(S newReserve);

	// get memory usage
	size_t GetMemUsage() const;

	// get size
	inline S GetSize() const;

	// empty test
	inline bool IsEmpty() const;

	// clear
	Queue &Clear();
	// shrink reserve to fit size
	Queue &Shrink();
	// reset => clears and frees data
	Queue &Reset();

	Queue &AddBack(const T &elem);
	Queue &AddFront(const T &elem);
	// aliases:
	inline Queue &AppendBack(const T &elem);
	inline Queue &AppendFront(const T &elem);
	inline Queue &PushBack(const T &elem);
	inline Queue &PushFront(const T &elem);

	Queue &Insert(S index, const T &elem);

	Queue &PopBack();
	Queue &PopFront();

	// get references to front/back elements
	inline const T &Front() const;
	inline T &Front();
	inline const T &Back() const;
	inline T &Back();

	// access operators
	inline const T &operator[](S index) const;
	inline T &operator[](S index);

	// swap
	inline void SwapWith(Queue &o);

	// slow find index
	S FindIndex(const T &val) const;

	Queue &operator =(const Queue &o);

	struct Iterator
	{
		Queue *queue;
		S index;

		inline bool operator ==(const Iterator &o) const
		{
			LETHE_ASSERT(queue == o.queue);
			return index == o.index;
		}

		inline bool operator !=(const Iterator &o) const
		{
			return !(*this == o);
		}

		Iterator &operator ++()
		{
			++index;
			return *this;
		}

		T &operator *()
		{
			return (*queue)[index];
		}
	};

	struct ConstIterator
	{
		const Queue *queue;
		S index;

		inline bool operator ==(const ConstIterator &o) const
		{
			LETHE_ASSERT(queue == o.queue);
			return index == o.index;
		}

		inline bool operator !=(const ConstIterator &o) const
		{
			return !(*this == o);
		}

		ConstIterator &operator ++()
		{
			++index;
			return *this;
		}

		const T &operator *() const
		{
			return (*queue)[index];
		}
	};

	ConstIterator begin() const
	{
		return Begin();
	}

	ConstIterator end() const
	{
		return End();
	}

	Iterator begin()
	{
		return Begin();
	}

	Iterator end()
	{
		return End();
	}

	ConstIterator Begin() const
	{
		ConstIterator res;
		res.index = 0;
		res.queue = this;
		return res;
	}

	ConstIterator End() const
	{
		ConstIterator res;
		res.index = GetSize();
		res.queue = this;
		return res;
	}

	Iterator Begin()
	{
		Iterator res;
		res.index = 0;
		res.queue = this;
		return res;
	}

	Iterator End()
	{
		Iterator res;
		res.index = GetSize();
		res.queue = this;
		return res;
	}

private:
	inline S GetAbsIndex(S index) const;
	inline S IncIndex(S index) const;
	inline S DecIndex(S index) const;
	inline S FixIndex(S index) const;
	void Grow();

	Array<T, S, A> data;
	S size;
	S first;			// points to first element
	S last;				// points to last element
};

template<typename T, typename A = HAlloc<T> > class BigQueue : public Queue<T, Long, A>
{
public:
	inline void SwapWith(BigQueue &o)
	{
		Queue<T,Long,A>::SwapWith(o);
	}
};

#include "Inline/Queue.inl"

}
