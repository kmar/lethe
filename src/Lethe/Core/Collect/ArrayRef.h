#pragma once

#include "../Sys/Types.h"

namespace lethe
{

template<typename T, typename S = Int>
class ArrayRef
{
public:
	ArrayRef();
	ArrayRef(T *ptr, S nsize);

	void Init(T *ptr, S nsize);

	const T *GetData() const;
	T *GetData();
	S GetSize() const;

	const T &operator[](S index) const;
	T &operator[](S index);

	typedef const T *ConstIterator;
	typedef T *Iterator;

	ConstIterator Begin() const;
	Iterator Begin();
	ConstIterator End() const;
	Iterator End();

	ConstIterator begin() const;
	Iterator begin();
	ConstIterator end() const;
	Iterator end();

	// get slice
	ArrayRef Slice(S from, S to) const;
	ArrayRef Slice(S from) const;

	inline bool IsValidIndex(S idx) const
	{
		return idx >= 0 && idx < this->size;
	}

private:
	T *data;
	S size;
};

template<typename T, typename S>
inline ArrayRef<T,S>::ArrayRef()
	: data(nullptr)
	, size(0)
{
}

template<typename T, typename S>
inline ArrayRef<T,S>::ArrayRef(T *ptr, S nsize)
	: data(ptr)
	, size(nsize)
{
}

template<typename T, typename S>
inline void ArrayRef<T,S>::Init(T *ptr, S nsize)
{
	data = ptr;
	size = nsize;
	LETHE_ASSERT(nsize >= 0);
	LETHE_ASSERT(!nsize || ptr);
}

template<typename T, typename S>
inline const T *ArrayRef<T,S>::GetData() const
{
	return data;
}

template<typename T, typename S>
inline T *ArrayRef<T, S>::GetData()
{
	return data;
}

template<typename T, typename S>
inline S ArrayRef<T,S>::GetSize() const
{
	return size;
}

template<typename T, typename S>
inline const T &ArrayRef<T,S>::operator[](S index) const
{
	LETHE_ASSERT(index >= 0 && index < size);
	return data[index];
}

template<typename T, typename S>
inline T &ArrayRef<T,S>::operator[](S index)
{
	LETHE_ASSERT(index >= 0 && index < size);
	return data[index];
}

template<typename T, typename S>
inline typename ArrayRef<T,S>::ConstIterator ArrayRef<T,S>::Begin() const
{
	return data;
}

template<typename T, typename S>
inline typename ArrayRef<T,S>::Iterator ArrayRef<T,S>::Begin()
{
	return data;
}

template<typename T, typename S>
inline typename ArrayRef<T,S>::ConstIterator ArrayRef<T,S>::End() const
{
	return data + size;
}

template<typename T, typename S>
inline typename ArrayRef<T,S>::Iterator ArrayRef<T,S>::End()
{
	return data + size;
}

template<typename T, typename S>
inline typename ArrayRef<T,S>::ConstIterator ArrayRef<T,S>::begin() const
{
	return data;
}

template<typename T, typename S>
inline typename ArrayRef<T,S>::Iterator ArrayRef<T,S>::begin()
{
	return data;
}

template<typename T, typename S>
inline typename ArrayRef<T,S>::ConstIterator ArrayRef<T,S>::end() const
{
	return data + size;
}

template<typename T, typename S>
inline typename ArrayRef<T,S>::Iterator ArrayRef<T,S>::end()
{
	return data + size;
}

template<typename T, typename S>
inline ArrayRef<T,S> ArrayRef<T,S>::Slice(S from, S to) const
{
	LETHE_ASSERT(from >= 0 && to <= size && to >= from);
	return ArrayRef(data + from, to - from);
}

template<typename T, typename S>
inline ArrayRef<T, S> ArrayRef<T, S>::Slice(S from) const
{
	return Slice(from, size);
}

}
