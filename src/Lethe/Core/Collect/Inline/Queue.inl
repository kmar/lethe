template< typename T, typename S, typename A >
Queue<T,S,A>::Queue()
	: size(0)
	, first(0)
	, last(0)
{
}

template< typename T, typename S, typename A >
Queue<T,S,A>::Queue(const Queue &o)
	: size(0)
	, first(0)
	, last(0)
{
	*this = o;
}

template< typename T, typename S, typename A >
Queue<T,S,A>::Queue(S newReserve)
	: size(0)
	, first(0)
	, last(newReserve > 0 ? newReserve-1 : 0)
{
	data.Resize(newReserve);
}

template< typename T, typename S, typename A >
size_t Queue<T,S,A>::GetMemUsage() const
{
	return data.GetMemUsage() + sizeof(*this);
}

template< typename T, typename S, typename A >
inline S Queue<T,S,A>::GetSize() const
{
	return size;
}

template< typename T, typename S, typename A >
inline bool Queue<T,S,A>::IsEmpty() const
{
	return size == 0;
}

template< typename T, typename S, typename A >
Queue<T,S,A> &Queue<T,S,A>::Clear()
{
	data.Clear();
	size = first = last = 0;
	return *this;
}

template< typename T, typename S, typename A >
Queue<T,S,A> &Queue<T,S,A>::Shrink()
{
	data.Shrink();
	return *this;
}

template< typename T, typename S, typename A >
Queue<T,S,A> &Queue<T,S,A>::Reset()
{
	data.Reset();
	Clear();
	return Shrink();
}

template< typename T, typename S, typename A >
Queue<T,S,A> &Queue<T,S,A>::AddBack(const T &elem)
{
	if (LETHE_UNLIKELY(size >= data.GetSize()))
		Grow();

	last = IncIndex(last);
	data[last] = elem;
	size++;
	return *this;
}

template< typename T, typename S, typename A >
Queue<T,S,A> &Queue<T,S,A>::AddFront(const T &elem)
{
	if (LETHE_UNLIKELY(size >= data.GetSize()))
		Grow();

	first = DecIndex(first);
	data[first] = elem;
	size++;
	return *this;
}

template< typename T, typename S, typename A >
inline Queue<T,S,A> &Queue<T,S,A>::PushBack(const T &elem)
{
	return AddBack(elem);
}

template< typename T, typename S, typename A >
inline Queue<T,S,A> &Queue<T,S,A>::AppendBack(const T &elem)
{
	return AddBack(elem);
}

template< typename T, typename S, typename A >
Queue<T,S,A> &Queue<T,S,A>::PopBack()
{
	LETHE_ASSERT(size > 0);
	DestroyObjectRange<T,S>(&data[last]);
	ConstructObjectRange<T,S>(&data[last]);
	last = DecIndex(last);

	if (!--size)
		Clear();

	return *this;
}

template< typename T, typename S, typename A >
inline Queue<T,S,A> &Queue<T,S,A>::PushFront(const T &elem)
{
	return AddFront(elem);
}

template< typename T, typename S, typename A >
inline Queue<T,S,A> &Queue<T,S,A>::AppendFront(const T &elem)
{
	return AddFront(elem);
}

template< typename T, typename S, typename A >
Queue<T,S,A> &Queue<T,S,A>::PopFront()
{
	LETHE_ASSERT(size > 0);
	DestroyObjectRange<T,S>(&data[first]);
	ConstructObjectRange<T,S>(&data[first]);
	first = IncIndex(first);

	if (!--size)
		Clear();

	return *this;
}

template< typename T, typename S, typename A >
inline const T &Queue<T,S,A>::Front() const
{
	LETHE_ASSERT(size > 0);
	return data[first];
}

template< typename T, typename S, typename A >
inline T &Queue<T,S,A>::Front()
{
	LETHE_ASSERT(size > 0);
	return data[first];
}

template< typename T, typename S, typename A >
inline const T &Queue<T,S,A>::Back() const
{
	LETHE_ASSERT(size > 0);
	return data[last];
}

template< typename T, typename S, typename A >
inline T &Queue<T,S,A>::Back()
{
	LETHE_ASSERT(size > 0);
	return data[last];
}

template< typename T, typename S, typename A >
inline const T &Queue<T,S,A>::operator[](S index) const
{
	return data[GetAbsIndex(index)];
}

template< typename T, typename S, typename A >
inline T &Queue<T,S,A>::operator[](S index)
{
	return data[GetAbsIndex(index)];
}

template< typename T, typename S, typename A >
inline void Queue<T,S,A>::SwapWith(Queue &o)
{
	Swap(data, o.data);
	Swap(size, o.size);
	Swap(first, o.first);
	Swap(last, o.last);
}

template< typename T, typename S, typename A >
inline S Queue<T,S,A>::GetAbsIndex(S index) const
{
	LETHE_ASSERT(index >= 0 && index < size);
	index += first;
	const S sz = data.GetSize();
	index -= sz * (index >= sz);
	return index;
}

template< typename T, typename S, typename A >
inline S Queue<T,S,A>::FixIndex(S index) const
{
	const S sz = data.GetSize();

	if (index < 0)
		index += sz;
	else if (index >= sz)
		index -= sz;

	return index;
}

template< typename T, typename S, typename A >
inline S Queue<T,S,A>::IncIndex(S index) const
{
	if (++index >= data.GetSize())
		index -= data.GetSize();

	return index;
}

template< typename T, typename S, typename A >
inline S Queue<T,S,A>::DecIndex(S index) const
{
	if (--index < 0)
		index += data.GetSize();

	return index;
}

template< typename T, typename S, typename A >
void Queue<T,S,A>::Grow()
{
	LETHE_ASSERT(data.GetSize() == size);
	auto newSize = size < 2 ? size+1 : size*3/2;
	// rotate data, adjust indices

	if (!data.IsEmpty() && first != 0)
		RotateArray(data.GetData(), data.GetData()+first, data.GetData() + data.GetSize());

	last = FixIndex(last - first);
	first = 0;

	data.Resize(newSize);
}

template< typename T, typename S, typename A >
S Queue<T,S,A>::FindIndex(const T &val) const
{
	S index = first;

	for (S i=0; i < size; i++)
	{
		if (data[index] == val)
			return i;

		index = IncIndex(index);
	}

	return -1;
}

template< typename T, typename S, typename A >
Queue<T,S,A> &Queue<T,S,A>::Insert(S index, const T &elem)
{
	if (index <= (size >> 1))
	{
		AddFront(elem);
		const S target = GetAbsIndex(index);
		index = GetAbsIndex(0);

		while (index != target)
		{
			S src = IncIndex(index);
			Swap(data[index], data[src]);
			index = src;
		}
	}
	else
	{
		AddBack(elem);
		const S target = GetAbsIndex(index);
		index = GetAbsIndex(size-1);

		while (index != target)
		{
			S src = DecIndex(index);
			Swap(data[index], data[src]);
			index = src;
		}
	}

	return *this;
}

template< typename T, typename S, typename A >
Queue<T,S,A> &Queue<T,S,A>::operator =(const Queue &o)
{
	data = o.data;
	size = o.size;
	first = o.first;
	last = o.last;
	return *this;
}
