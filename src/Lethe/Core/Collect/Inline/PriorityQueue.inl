template<typename T, typename S, typename A>
inline PriorityQueue<T,S,A> &PriorityQueue<T,S,A>::Add(const T &key)
{
	InsertKey(key);
	return *this;
}

template<typename T, typename S, typename A>
inline PriorityQueue<T,S,A> &PriorityQueue<T,S,A>::Insert(const T &key)
{
	return Add(key);
}

template<typename T, typename S, typename A>
inline PriorityQueue<T,S,A> &PriorityQueue<T,S,A>::Push(const T &key)
{
	return Add(key);
}

template<typename T, typename S, typename A>
inline bool PriorityQueue<T,S,A>::IsEmpty() const
{
	return data.IsEmpty();
}

template<typename T, typename S, typename A>
inline S PriorityQueue<T,S,A>::GetSize() const
{
	return data.GetSize();
}

template<typename T, typename S, typename A>
inline const T &PriorityQueue<T,S,A>::Top() const
{
	return data[0];
}

template<typename T, typename S, typename A>
inline PriorityQueue<T,S,A> &PriorityQueue<T,S,A>::Pop()
{
	DeleteMin();
	return *this;
}

template<typename T, typename S, typename A>
inline T PriorityQueue<T,S,A>::PopTop()
{
	T res = Top();
	Pop();
	return res;
}

template<typename T, typename S, typename A>
inline PriorityQueue<T,S,A> &PriorityQueue<T,S,A>::Clear()
{
	data.Clear();
	return *this;
}

template<typename T, typename S, typename A>
inline PriorityQueue<T,S,A> &PriorityQueue<T,S,A>::Shrink()
{
	data.Shrink();
	return *this;
}

template<typename T, typename S, typename A>
inline PriorityQueue<T,S,A> &PriorityQueue<T,S,A>::Reset()
{
	data.Reset();
	return *this;
}

template<typename T, typename S, typename A>
bool PriorityQueue<T,S,A>::IsValid() const
{
	for (S i=0; i<data.GetSize(); i++)
	{
		S pi = GetParent(i);

		if (pi <= i)
			continue;

		if (Greater(data[pi], data[i]))
			return 0;
	}

	return 1;
}

template<typename T, typename S, typename A>
size_t PriorityQueue<T,S,A>::GetMemUsage() const
{
	return data.GetMemUsage() + sizeof(*this);
}

template<typename T, typename S, typename A>
inline void PriorityQueue<T,S,A>::SwapWith(PriorityQueue &o)
{
	Swap(data, o.data);
}

template<typename T, typename S, typename A>
inline S PriorityQueue<T,S,A>::GetParent(S index)
{
	return ((index+1) >> 1)-1;
}

template<typename T, typename S, typename A>
inline S PriorityQueue<T,S,A>::GetChild(S index)
{
	return 2*index+1;
}

template<typename T, typename S, typename A>
inline S PriorityQueue<T,S,A>::GetRightChild(S index)
{
	return 2*index+2;
}

template<typename T, typename S, typename A>
void PriorityQueue<T,S,A>::InsertKey(const T &key)
{
	S index = data.GetSize();
	data.Add(key);
	T * const ldata = data.GetData();
	// percolate up
	S pindex;

	while (index > 0 && key < ldata[pindex = GetParent(index)])
	{
		Swap(ldata[index], ldata[pindex]);
		index = pindex;
	}
}

template<typename T, typename S, typename A>
inline void PriorityQueue<T,S,A>::DeleteMin()
{
	data.EraseFast(0);
	PercolateDown(0);
}

template<typename T, typename S, typename A>
void PriorityQueue<T,S,A>::PercolateDown(S index)
{
	S child;
	const S sz = GetSize();
	T * const ldata = data.GetData();

	while ((child = GetRightChild(index)) < sz)
	{
		if (ldata[child-1] < ldata[child])
			child--;

		if (LETHE_UNLIKELY(!(ldata[child] < ldata[index])))
			return;

		Swap(ldata[index], ldata[child]);
		index = child;
	}

	--child;
	LETHE_ASSERT(child >= 0);

	if (child < sz && ldata[child] < ldata[index])
		Swap(ldata[index], ldata[child]);
}
