// doubly-linked list helper macros (FIXME: put them elsewhere? didn't want a separate file)
#define LETHE_DLIST_UNLINK(obj, head, tail, prev, next) \
	do { \
		LETHE_ASSERT(obj); \
		auto _prevptr_ = (obj)->prev; \
		auto _nextptr_ = (obj)->next; \
		auto _thisobj_ = (obj); \
		if (_prevptr_) { \
			_prevptr_->next = _nextptr_; \
		} else { \
			LETHE_ASSERT((obj) == (head)); \
			(head)=_nextptr_; \
		} \
		if (_nextptr_) { \
			_nextptr_->prev = _prevptr_; \
		} else { \
			LETHE_ASSERT((tail) == (obj)); \
			(tail) = _prevptr_; \
		} \
		_thisobj_->prev = _thisobj_->next = 0; \
	} while(0)

#define LETHE_DLIST_ADD(obj, head, tail, prev, next) \
	if ((tail) != (obj) && (head) != (obj) && !(obj)->prev && !(obj)->next) { \
		if (!(tail)) { \
			(head) = (tail) = (obj); \
		} else { \
			(tail)->next = (obj); \
			(obj)->prev = (tail); \
			(tail) = (obj); \
		} \
	}

template< typename T, typename S, typename A >
Array<T,S,A>::Array()
{
}

template< typename T, typename S, typename A >
Array<T,S,A>::Array(const Array &o)
{
	*this = o;
}

template< typename T, typename S, typename A >
Array<T,S,A>::Array(const ArrayRef<T,S> &o)
{
	*this = o;
}

template< typename T, typename S, typename A >
Array<T,S,A>::Array(S s)
{
	Resize(s);
}

template< typename T, typename S, typename A >
Array<T,S,A>::Array(S s, const T &ini)
{
	Resize(s, ini);
}

template< typename T, typename S, typename A >
LETHE_NOINLINE Array<T,S,A>::~Array()
{
	if (this->data)
	{
		DestroyObjectRange(this->data, this->size);

		if (this->reserve > 0)
			this->Free(this->data);
	}
}

template< typename T, typename S, typename A >
inline S Array<T,S,A>::GetSize() const
{
	return this->size;
}

template< typename T, typename S, typename A >
inline S Array<T,S,A>::GetCapacity() const
{
	return Abs(this->reserve);
}

template< typename T, typename S, typename A >
inline bool Array<T,S,A>::IsEmpty() const
{
	return this->size == 0;
}

template< typename T, typename S, typename A >
inline Array<T,S,A> &Array<T,S,A>::ResizeToFit(S index)
{
	if (index >= this->size)
		Resize(index+1);

	return *this;
}

template< typename T, typename S, typename A >
inline Array<T,S,A> &Array<T,S,A>::ResizeToFit(S index, const T &ini)
{
	if (index >= this->size)
		Resize(index+1, ini);

	return *this;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Resize(S newSize)
{
	LETHE_ASSERT(newSize >= 0);
	newSize = MaxZero(newSize);

	if (this->size > newSize)
		DestroyObjectRange(this->data+newSize, this->size - newSize);
	else
	{
		Reserve(newSize);
		ConstructObjectRange(this->data+this->size, newSize - this->size);
	}

	this->size = newSize;
	return *this;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Resize(S newSize, const T &ini)
{
	LETHE_ASSERT(newSize >= 0);
	newSize = MaxZero(newSize);

	if (this->size > newSize)
		DestroyObjectRange(this->data+newSize, this->size - newSize);
	else
	{
		Reserve(newSize);
		ConstructObjectRange(this->data+this->size, newSize - this->size);
	}

	for (S i=this->size; i<newSize; i++)
		this->data[i] = ini;

	this->size = newSize;
	return *this;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Reserve(S newReserve)
{
	LETHE_ASSERT(newReserve >= 0);

	S cap = GetCapacity();

	if (cap >= newReserve)
		return *this;

	cap = GrowCapacity(cap);

	if (cap < newReserve)
		cap = newReserve;

	while (newReserve > cap)
		cap = GrowCapacity(cap);

	return Reallocate(cap);
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Clear()
{
	ArrayBase_<T,S>::Clear();
	return *this;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Shrink()
{
	return Reallocate(this->size);
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Reset()
{
	Clear();
	return Shrink();
}

template< typename T, typename S, typename A >
void Array<T,S,A>::ReallocateInternal(T *newData, S newSize)
{
	ConstructObjectRange(newData, newSize);

	if (this->data)
	{
		if (MemCopyTraits<T>::VALUE)
		{
			if (newSize > 0)
				MemCpy(newData, this->data, (size_t)newSize*sizeof(T));
		}
		else
		{
			for (S i=0; i<newSize; i++)
				SwapCopy(newData[i], this->data[i]);
		}

		DestroyObjectRange(this->data, this->size);

		if (this->reserve > 0)
			this->Free(this->data);
	}
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Reallocate(S newReserve)
{
// FIXME: I was unable to fix this (or Add) using asserts
// I don't like this hack at all, but at the moment it's the best I've got
#if !defined(__clang_analyzer__)
	LETHE_ASSERT(newReserve >= 0);

	if (this->reserve < 0 && newReserve <= -this->reserve)
	{
		LETHE_ASSERT(this->size <= newReserve);
		return *this;
	}

	if (newReserve == GetCapacity())
		return *this;

	T *newData = newReserve ?
				 static_cast<T *>(this->Realloc(
					this->reserve < 0 ? static_cast<T *>(0) : this->data,
					(size_t)(newReserve) * sizeof(T),
					(size_t)Max<S>(-this->reserve, 0))) : nullptr;
	S newSize = Min(this->size, newReserve);

	if (newData != this->data)
	{
		if (!newData)
		{
			newSize = 0;		// keep SA happy
		}

		ReallocateInternal(newData, newSize);
	}
	else
		LETHE_ASSERT(newSize == this->size);

	this->reserve = newReserve;
	this->size = newSize;
	this->data = newData;
#endif
	return *this;
}

template< typename T, typename S, typename A >
S Array<T,S,A>::Add(const T &elem)
{
	if (LETHE_UNLIKELY(this->size >= GetCapacity()))
		Reserve(this->size + 1);

	ConstructObjectRange<T,S>(this->data + this->size);
	LETHE_ASSERT(this->data);
	this->data[this->size] = elem;
	return this->size++;
}

template< typename T, typename S, typename A >
S Array<T,S,A>::AddUnique(const T &elem)
{
	auto res = FindIndex(elem);

	if (res < 0)
		res = Add(elem);

	return res;
}

template< typename T, typename S, typename A >
S Array<T, S, A>::AddSwap(T &elem)
{
	S res = Add(T());
	Swap(Back(), elem);
	return res;
}

template< typename T, typename S, typename A >
S Array<T, S, A>::AddSwapUnique(const T &elem)
{
	auto res = FindIndex(elem);

	if (res < 0)
		res = AddSwap(elem);

	return res;
}

template< typename T, typename S, typename A >
T *Array<T,S,A>::Alloc(S count)
{
	if (LETHE_UNLIKELY(this->size + count > GetCapacity()))
		Reserve(this->size + count);

	ConstructObjectRange<T,S>(this->data + this->size, count);
	T *res = this->data + this->size;
	this->size += count;
	return res;
}

template< typename T, typename S, typename A >
inline S Array<T,S,A>::Push(const T &elem)
{
	return Add(elem);
}

template< typename T, typename S, typename A >
inline S Array<T,S,A>::Append(const T &elem)
{
	return Add(elem);
}

template< typename T, typename S, typename A >
inline S Array<T,S,A>::AppendBack(const T &elem)
{
	return Add(elem);
}

template< typename T, typename S, typename A >
inline S Array<T,S,A>::AddBack(const T &elem)
{
	return Add(elem);
}

template< typename T, typename S, typename A >
inline S Array<T,S,A>::PushBack(const T &elem)
{
	return Add(elem);
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Pop()
{
	LETHE_ASSERT(this->size > 0);
	this->size--;
	DestroyObjectRange<T,S>(this->data + this->size);
	return *this;
}

template< typename T, typename S, typename A >
inline S Array<T,S,A>::Append(const Array &o)
{
	return Append(o.GetData(), o.GetSize());
}

template< typename T, typename S, typename A >
inline S Array<T,S,A>::Append(const ArrayRef<T,S> &o)
{
	return Append(o.GetData(), o.GetSize());
}

template< typename T, typename S, typename A >
S Array<T,S,A>::Append(const T *nbuf, S sz)
{
	LETHE_ASSERT(sz >= 0);

	if (LETHE_UNLIKELY(this->size + sz > GetCapacity()))
		Reserve(this->size + sz);

	ConstructObjectRange<T,S>(this->data + this->size, sz);

	if constexpr (MemCopyTraits<T>::VALUE)
	{
		if (sz > 0)
			MemCpy(this->data + this->size, nbuf, (size_t)sz * sizeof(T));
	}
	else
	{
		for (S i=0; i<sz; i++)
			this->data[this->size + i] = nbuf[i];
	}

	S res = this->size;
	this->size += sz;
	return res;
}

template< typename T, typename S, typename A >
inline S Array<T, S, A>::AppendSwap(Array &o)
{
	return AppendSwap(o.GetData(), o.GetSize());
}

template< typename T, typename S, typename A >
inline S Array<T, S, A>::AppendSwap(ArrayRef<T,S> o)
{
	return AppendSwap(o.GetData(), o.GetSize());
}

template< typename T, typename S, typename A >
S Array<T, S, A>::AppendSwap(T *nbuf, S sz)
{
	LETHE_ASSERT(sz >= 0);

	if (LETHE_UNLIKELY(this->size + sz > GetCapacity()))
		Reserve(this->size + sz);

	ConstructObjectRange<T, S>(this->data + this->size, sz);

	for (S i = 0; i < sz; i++)
		Swap(this->data[this->size + i], nbuf[i]);

	Int res = this->size;
	this->size += sz;
	return res;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Reverse()
{
	S sz = GetSize();

	for (S i=0; i<sz/2; i++)
		Swap(this->data[i], this->data[sz-i-1]);

	return *this;
}

template< typename T, typename S, typename A >
size_t Array<T,S,A>::GetMemUsage() const
{
	// guess block overhead to be 32 bytes
	size_t blockOverhead = IsEmpty() ? 0 : 32;
	return sizeof(*this) + AlignOf<T>::align + (size_t)GetCapacity()*sizeof(T) + blockOverhead;
}

template< typename T, typename S, typename A >
inline const T &Array<T,S,A>::operator[](S index) const
{
	LETHE_ASSERT(index >= 0	&& index < this->size);
	return this->data[index];
}

template< typename T, typename S, typename A >
inline T &Array<T,S,A>::operator[](S index)
{
	LETHE_ASSERT(index >= 0	&& index < this->size);
	return this->data[index];
}

template< typename T, typename S, typename A >
inline T *Array<T,S,A>::begin()
{
	return this->data;
}

template< typename T, typename S, typename A >
inline const T *Array<T,S,A>::begin() const
{
	return this->data;
}

template< typename T, typename S, typename A >
inline T *Array<T,S,A>::end()
{
	return this->data + this->size;
}

template< typename T, typename S, typename A >
inline const T *Array<T,S,A>::end() const
{
	return this->data + this->size;
}

template< typename T, typename S, typename A >
inline T *Array<T,S,A>::Begin()
{
	return this->data;
}

template< typename T, typename S, typename A >
inline const T *Array<T,S,A>::Begin() const
{
	return this->data;
}

template< typename T, typename S, typename A >
inline T *Array<T,S,A>::End()
{
	return this->data + this->size;
}

template< typename T, typename S, typename A >
inline const T *Array<T,S,A>::End() const
{
	return this->data + this->size;
}

template< typename T, typename S, typename A >
inline T *Array<T,S,A>::RBegin()
{
	return this->data + this->size - 1;
}

template< typename T, typename S, typename A >
inline const T *Array<T,S,A>::RBegin() const
{
	return this->data + this->size - 1;
}

template< typename T, typename S, typename A >
inline T *Array<T,S,A>::REnd()
{
	return this->data - 1;
}

template< typename T, typename S, typename A >
inline const T *Array<T,S,A>::REnd() const
{
	return this->data - 1;
}

template< typename T, typename S, typename A >
LETHE_NOINLINE Array<T,S,A> &Array<T,S,A>::operator =(const Array &o)
{
	Resize(o.size);
	LETHE_ASSERT(!o.size || this->data);

	if constexpr (MemCopyTraits<T>::VALUE)
	{
		if (o.size > 0)
			MemCpy(this->data, o.data, (size_t)o.size * sizeof(T));
	}
	else
	{
		for (S i=0; i < o.size; i++)
			this->data[i] = o.data[i];
	}

	return *this;
}

template< typename T, typename S, typename A >
LETHE_NOINLINE Array<T,S,A> &Array<T,S,A>::operator =(const ArrayRef<T,S> &o)
{
	Resize(o.GetSize());
	LETHE_ASSERT(!o.GetSize() || this->data);

	if constexpr (MemCopyTraits<T>::VALUE)
	{
		if (o.GetSize() > 0)
			MemCpy(this->data, o.GetData(), (size_t)o.GetSize() * sizeof(T));
	}
	else
	{
		for (S i=0; i < o.GetSize(); i++)
			this->data[i] = o[i];
	}

	return *this;
}

template< typename T, typename S, typename A >
inline bool Array<T,S,A>::operator ==(const Array &o) const
{
	if (this->size != o.size)
		return 0;

	for (S i=0; i<this->size; i++)
		if (this->data[i] != o.data[i])
			return 0;

	return 1;
}

template< typename T, typename S, typename A >
inline bool Array<T,S,A>::operator !=(const Array &o) const
{
	return !(*this == o);
}

template< typename T, typename S, typename A >
S Array<T,S,A>::FindIndex(const T &elem) const
{
	for (S i=0; i<this->size; i++)
		if (LETHE_LIKELY(elem == this->data[i]))
			return i;

	return -1;
}

template< typename T, typename S, typename A >
typename Array<T,S,A>::ConstIterator Array<T,S,A>::Find(const T &elem) const
{
	S res = FindIndex(elem);
	return res < 0 ? End() : Begin() + res;
}

template< typename T, typename S, typename A >
typename Array<T,S,A>::Iterator Array<T,S,A>::Find(const T &elem)
{
	S res = FindIndex(elem);
	return res < 0 ? End() : Begin() + res;
}

template< typename T, typename S, typename A >
S Array<T,S,A>::FindMaxIndex() const
{
	if (IsEmpty())
		return -1;

	S res = 0;
	T best = this->data[0];

	for (S i=1; i<this->size; i++)
	{
		if (best < this->data[i])
		{
			best = this->data[i];
			res = i;
		}
	}

	return res;
}

template< typename T, typename S, typename A >
typename Array<T,S,A>::ConstIterator Array<T,S,A>::FindMax() const
{
	S res = FindMaxIndex();
	return res < 0 ? End() : Begin() + res;
}
template< typename T, typename S, typename A >
typename Array<T,S,A>::Iterator Array<T,S,A>::FindMax()
{
	S res = FindMaxIndex();
	return res < 0 ? End() : Begin() + res;
}

template< typename T, typename S, typename A >
S Array<T,S,A>::FindMinIndex() const
{
	if (IsEmpty())
		return -1;

	S res = 0;
	T best = this->data[0];

	for (S i=1; i<this->size; i++)
	{
		if (this->data[i] < best)
		{
			best = this->data[i];
			res = i;
		}
	}

	return res;
}

template< typename T, typename S, typename A >
typename Array<T,S,A>::ConstIterator Array<T,S,A>::FindMin() const
{
	S res = FindMinIndex();
	return res < 0 ? End() : Begin() + res;
}
template< typename T, typename S, typename A >
typename Array<T,S,A>::Iterator Array<T,S,A>::FindMin()
{
	S res = FindMinIndex();
	return res < 0 ? End() : Begin() + res;
}

template< typename T, typename S, typename A >
template<typename F>
S Array<T,S,A>::EraseIf(F func)
{
	S dst = 0;

	for (S i=0; i<this->size; i++)
	{
		if (!func(this->data[i]))
		{
			if (i != dst)
				Swap(this->data[i], this->data[dst]);

			dst++;
		}
	}

	Resize(dst);
	return dst;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Erase(S index)
{
	LETHE_ASSERT(index >= 0 && index < this->size);

	for (S i=index+1; i<this->size; i++)
		SwapCopy(this->data[i-1], this->data[i]);

	this->size--;
	DestroyObjectRange<T,S>(this->data+this->size, 1);
	return *this;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::EraseFast(S index)
{
	// swap index and last
	LETHE_ASSERT(index >= 0 && index < this->size);
	--this->size;

	if (LETHE_LIKELY(index != this->size))
		SwapCopy(this->data[index], this->data[this->size]);

	DestroyObjectRange<T,S>(this->data+this->size, 1);
	return *this;
}

template< typename T, typename S, typename A >
inline Array<T,S,A> &Array<T,S,A>::EraseIndex(S index)
{
	return Erase(index);
}

template< typename T, typename S, typename A >
inline Array<T,S,A> &Array<T,S,A>::EraseIndexFast(S index)
{
	return EraseFast(index);
}

template< typename T, typename S, typename A >
inline typename Array<T,S,A>::Iterator Array<T,S,A>::Erase(typename Array::Iterator it)
{
	Erase((S)(it - Begin()));
	return it;
}

template< typename T, typename S, typename A >
inline typename Array<T,S,A>::Iterator Array<T,S,A>::EraseFast(typename Array::Iterator it)
{
	EraseFast((S)(it - Begin()));
	return it;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Erase(S index, S count)
{
	if (count)
	{
		LETHE_ASSERT(index >= 0 && index < this->size);

		if (count < 0 || count > this->size - index)
			count = this->size - index;

		for (S i=index+count; i<this->size; i++)
			SwapCopy(this->data[i-count], this->data[i]);

		this->size -= count;
		DestroyObjectRange<T,S>(this->data+this->size, count);
	}
	return *this;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::EraseFast(S index, S count)
{
	// swap index and last
	LETHE_ASSERT(index >= 0 && index < this->size);

	if (count < 0 || count > this->size - index)
		count = this->size - index;

	S remaining = count;

	while (remaining-- > 0)
	{
		--this->size;

		if (LETHE_LIKELY(index != this->size))
			SwapCopy(this->data[index], this->data[this->size]);
	}

	DestroyObjectRange<T,S>(this->data+this->size, count);
	return *this;
}

template< typename T, typename S, typename A >
inline Array<T,S,A> &Array<T,S,A>::EraseIndex(S index, S count)
{
	return Erase(index, count);
}

template< typename T, typename S, typename A >
inline Array<T,S,A> &Array<T,S,A>::EraseIndexFast(S index, S count)
{
	return EraseFast(index, count);
}

template< typename T, typename S, typename A >
inline typename Array<T,S,A>::Iterator Array<T,S,A>::Erase(typename Array::Iterator it, typename Array::Iterator itEnd)
{
	Erase((S)(it - Begin()), (S)(itEnd - it));
	return it;
}

template< typename T, typename S, typename A >
inline typename Array<T,S,A>::Iterator Array<T,S,A>::EraseFast(typename Array::Iterator it, typename Array::Iterator itEnd)
{
	EraseFast((S)(it - Begin()), (S)(itEnd - it));
	return it;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Insert(S index, const T &elem)
{
	LETHE_ASSERT(index >= 0 && index <= this->size);

	if (LETHE_UNLIKELY(this->size + 1 > GetCapacity()))
		Reserve(this->size + 1);

	ConstructObjectRange<T,S>(this->data + this->size);

	for (S i=this->size; i > index; i--)
		SwapCopy(this->data[i], this->data[i-1]);

	this->data[index] = elem;
	this->size++;
	return *this;
}

template< typename T, typename S, typename A >
inline Array<T,S,A> &Array<T,S,A>::InsertIndex(S index, const T &elem)
{
	return Insert(index, elem);
}

template< typename T, typename S, typename A >
inline Array<T,S,A> &Array<T,S,A>::Insert(typename Array::Iterator it, const T &elem)
{
	return Insert((S)(it - Begin()), elem);
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Insert(S index, const T *elem, S count)
{
	LETHE_ASSERT(count > 0 && index >= 0 && index <= this->size);

	if (LETHE_UNLIKELY(this->size + count > GetCapacity()))
		Reserve(this->size + count);

	ConstructObjectRange<T,S>(this->data + this->size, count);

	for (S i=this->size-1; i >= index; i--)
		SwapCopy(this->data[i+count], this->data[i]);

	for (S i=0; i<count; i++)
		this->data[index+i] = elem[i];

	this->size+=count;
	return *this;
}

template< typename T, typename S, typename A >
inline Array<T,S,A> &Array<T,S,A>::InsertIndex(S index, const T *elem, S count)
{
	return Insert(index, elem, count);
}

template< typename T, typename S, typename A >
inline Array<T,S,A> &Array<T,S,A>::Insert(typename Array::Iterator it, const T *elem, S count)
{
	return Insert((S)(it - Begin()), elem, count);
}

template< typename T, typename S, typename A >
inline const T &Array<T,S,A>::Front() const
{
	LETHE_ASSERT(this->size > 0);
	return this->data[0];
}

template< typename T, typename S, typename A >
inline T &Array<T,S,A>::Front()
{
	LETHE_ASSERT(this->size > 0);
	return this->data[0];
}

template< typename T, typename S, typename A >
inline const T &Array<T,S,A>::Back() const
{
	LETHE_ASSERT(this->size > 0);
	return this->data[this->size-1];
}

template< typename T, typename S, typename A >
inline T &Array<T,S,A>::Back()
{
	LETHE_ASSERT(this->size > 0);
	return this->data[this->size-1];
}

template< typename T, typename S, typename A >
inline T *Array<T,S,A>::GetData()
{
	return this->data;
}

template< typename T, typename S, typename A >
inline const T *Array<T,S,A>::GetData() const
{
	return this->data;
}

template< typename T, typename S, typename A >
inline void Array<T,S,A>::SwapWith(Array &o)
{
	LETHE_ASSERT((this->reserve >= 0) && (o.reserve >= 0));
	Swap(this->data, o.data);
	Swap(this->size,o.size);
	Swap(this->reserve, o.reserve);
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Sort()
{
	if (LETHE_LIKELY(this->size > 0))
		QuickSort<T,S>::Sort(this->data, this->size);

	return *this;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::StableSort()
{
	if (LETHE_LIKELY(this->size > 0))
		InplaceMergeSort<T,S>::Sort(this->data, this->size);

	return *this;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::SortInsertion()
{
	if (LETHE_LIKELY(this->size > 0))
		InsertionSort<T,S>::Sort(this->data, this->size);

	return *this;
}

template< typename T, typename S, typename A >
bool Array<T,S,A>::IsSorted() const
{
	for (S i=1; i<this->size; i++)
	{
		if (Greater(this->data[i-1], this->data[i]))
			return 0;
	}

	return 1;
}

template< typename T, typename S, typename A >
template< typename C >
Array<T,S,A> &Array<T,S,A>::Sort(C cmp)
{
	if (LETHE_LIKELY(this->size > 0))
		QuickSort<T,S>::Sort(this->data, this->size, cmp);

	return *this;
}

template< typename T, typename S, typename A >
template< typename C >
Array<T,S,A> &Array<T,S,A>::StableSort(C cmp)
{
	if (LETHE_LIKELY(this->size > 0))
		InplaceMergeSort<T,S>::Sort(this->data, this->size, cmp);

	return *this;
}

template< typename T, typename S, typename A >
template< typename C >
Array<T,S,A> &Array<T,S,A>::SortInsertion(C cmp)
{
	if (LETHE_LIKELY(this->size > 0))
		InsertionSort<T,S>::Sort(this->data, this->size, cmp);

	return *this;
}

template< typename T, typename S, typename A >
template< typename C >
bool Array<T,S,A>::IsSorted(C cmp) const
{
	for (S i=1; i<this->size; i++)
	{
		if (cmp(this->data[i], this->data[i-1]))
			return 0;
	}

	return 1;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::Fill(const T &value)
{
	for (S i=0; i<this->size; i++)
		this->data[i] = value;

	return *this;
}

template< typename T, typename S, typename A >
Array<T,S,A> &Array<T,S,A>::MemSet(int value)
{
	if (LETHE_LIKELY(this->size > 0))
		lethe::MemSet(this->data, value, sizeof(T)*(size_t)this->size);

	return *this;
}

// index sorting
template< typename T, typename S, typename A >
const Array<T,S,A> &Array<T,S,A>::SortIndex(S *arr) const
{
	IndexCompare icmp;
	icmp.arr = GetData();

	for (S i=0; i<GetSize(); i++)
		arr[i] = i;

	QuickSort<S,S>::Sort(arr, GetSize(), icmp);
	return *this;
}

template< typename T, typename S, typename A >
const Array<T,S,A> &Array<T,S,A>::StableSortIndex(S *arr) const
{
	IndexCompare icmp;
	icmp.arr = GetData();

	for (S i=0; i<GetSize(); i++)
		arr[i] = i;

	InplaceMergeSort<S,S>::Sort(arr, GetSize(), icmp);
	return *this;
}

template< typename T, typename S, typename A >
template<typename B>
inline const Array<T,S,A> &Array<T,S,A>::SortIndex(Array<S,S,B> &arr) const
{
	arr.Resize(GetSize());
	SortIndex(arr.GetData());
	return *this;
}

template< typename T, typename S, typename A >
template<typename B>
inline const Array<T,S,A> &Array<T,S,A>::StableSortIndex(Array<S,S,B> &arr) const
{
	arr.Resize(GetSize());
	StableSortIndex(arr.GetData());
	return *this;
}

template< typename T, typename S, typename A >
template< typename C >
const Array<T,S,A> &Array<T,S,A>::SortIndex(S *arr, C cmp) const
{
	IndexCompareCmp<C> icmp;
	icmp.cmp = &cmp;
	icmp.arr = GetData();

	for (S i=0; i<GetSize(); i++)
		arr[i] = i;

	QuickSort<S,S>::Sort(arr, GetSize(), icmp);
	return *this;
}

template< typename T, typename S, typename A >
template< typename C >
const Array<T,S,A> &Array<T,S,A>::StableSortIndex(S *arr, C cmp) const
{
	IndexCompareCmp<C> icmp;
	icmp.cmp = &cmp;
	icmp.arr = GetData();

	for (S i=0; i<GetSize(); i++)
		arr[i] = i;

	InplaceMergeSort<S,S>::Sort(arr, GetSize(), icmp);
	return *this;
}

template< typename T, typename S, typename A >
template<typename B, typename C>
inline const Array<T,S,A> &Array<T,S,A>::SortIndex(Array<S,S,B> &arr, C cmp) const
{
	arr.Resize(GetSize());
	SortIndex(arr.GetData(), cmp);
	return *this;
}

template< typename T, typename S, typename A >
template<typename B, typename C>
inline const Array<T,S,A> &Array<T,S,A>::StableSortIndex(Array<S,S,B> &arr, C cmp) const
{
	arr.Resize(GetSize());
	StableSortIndex(arr.GetData(), cmp);
	return *this;
}

template<typename T, typename S, typename A>
inline ArrayRef<const T,S> Array<T,S,A>::Slice(S from, S to) const
{
	LETHE_ASSERT(from >= 0 && to <= this->size && to >= from);
	return ArrayRef<const T,S>(this->data + from, to - from);
}

template<typename T, typename S, typename A>
inline ArrayRef<const T, S> Array<T, S, A>::Slice(S from) const
{
	S to = this->size;
	LETHE_ASSERT(from >= 0 && to <= this->size && to >= from);
	return ArrayRef<const T, S>(this->data + from, to - from);
}

template<typename T, typename S, typename A>
inline ArrayRef<T,S> Array<T,S,A>::Slice(S from, S to)
{
	LETHE_ASSERT(from >= 0 && to <= this->size && to >= from);
	return ArrayRef<T,S>(this->data + from, to - from);
}

template<typename T, typename S, typename A>
inline ArrayRef<T, S> Array<T, S, A>::Slice(S from)
{
	S to = this->size;
	LETHE_ASSERT(from >= 0 && to <= this->size && to >= from);
	return ArrayRef<T, S>(this->data + from, to - from);
}
